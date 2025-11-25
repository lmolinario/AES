/***************************************************************
 *  Lab 3 – Audio DSP on Zybo Z7 (PS + I2S + SSM2603)
 *  Author: Lello Molinario
 *  Board : Zybo Z7 (Zynq-7000)
 *
 *  Features:
 *  ------------------------------------------------------------
 *  • Step 1: Audio passthrough (line-in → line-out).
 *  • Step 2: FIR filtering on PS (software):
 *        - SW0 ON → Low-pass FIR (N_LP tap)
 *        - SW1 ON → High-pass FIR (N_HP tap)
 *        - No switch → clean passthrough
 *  • Step 3: FIR execution timing using Global Timer:
 *        - Measures average cycles per FIR (LP / HP) with cache ON
 *        - Measures average cycles per FIR (LP / HP) with cache OFF
 *        - Computes max usable taps in real-time given Fs ≈ 48 kHz
 ***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xiicps.h"
#include "timer_ps.h"
#include "xil_cache.h"

/* ---------------- I2S Register offsets ---------------- */
#define I2S_RESET_REG        0x00
#define I2S_CTRL_REG         0x04
#define I2S_CLK_CTRL_REG     0x08
#define I2S_FIFO_STS_REG     0x20
#define I2S_RX_FIFO_REG      0x28
#define I2S_TX_FIFO_REG      0x2C

/* ---------------- AXI FIFO offsets -------------------- */
#define FIFO_ISR   0x00
#define FIFO_IER   0x04
#define FIFO_TDFV  0x0C
#define FIFO_RDFO  0x1C
#define FIFO_TDR   0x2C
#define FIFO_TDFD  0x10
#define FIFO_TLR   0x14
#define FIFO_RLR   0x24
#define FIFO_RDFD  0x20
#define FIFO_RDR   0x30

/* ------------ I2C / Codec configuration --------------- */
#define IIC_SLAVE_ADDR   0b0011010
#define IIC_SCLK_RATE    100000

/* ------------ Hardware addresses (Vivado) ------------- */
#define AUDIO_IIC_ID          XPAR_XIICPS_0_DEVICE_ID
#define AUDIO_CTRL_BASEADDR   XPAR_AXI_I2S_ADI_0_S00_AXI_BASEADDR
#define SCU_TIMER_ID          XPAR_SCUTIMER_DEVICE_ID

#define SWI_BASE_ADDR         XPAR_AXI_GPIO_2_BASEADDR
#define LED_BASE_ADDR         XPAR_AXI_GPIO_1_BASEADDR
#define BUT_BASE_ADDR         XPAR_AXI_GPIO_0_BASEADDR

#define AUDIO_FIFO            XPAR_AXI_FIFO_MM_S_0_BASEADDR
#define FIR_FIFO              XPAR_AXI_FIFO_MM_S_1_BASEADDR

#define GLOBAL_TMR_BASEADDR   XPAR_PS7_GLOBALTIMER_0_S_AXI_BASEADDR

/* ------------ FIR Coefficients ------------------------ */
/* Low-Pass (20 tap) & High-Pass (21 tap) FIR */

#define coeffLP  \
    -1.692219e-02,  5.043750e-02,  3.935835e-02,  4.341238e-02, \
     5.137933e-02,  5.982048e-02,  6.748827e-02,  7.379049e-02, \
     7.824605e-02,  8.052560e-02,  8.052560e-02,  7.824605e-02, \
     7.379049e-02,  6.748827e-02,  5.982048e-02,  5.137933e-02, \
     4.341238e-02,  3.935835e-02,  5.043750e-02, -1.69221860e-02

#define coeffHP  \
    -3.942071e-02, -5.929114e-03,  1.430997e-02,  4.069053e-02, \
     5.762197e-02,  4.843584e-02,  4.349633e-03, -6.932913e-02, \
    -1.527862e-01, -2.187328e-01,  7.562085e-01, -2.187328e-01, \
    -1.527862e-01, -6.932913e-02,  4.349633e-03,  4.843584e-02, \
     5.762197e-02,  4.069053e-02,  1.430997e-02, -5.929114e-03, \
    -3.94207089e-02

#define N_LP  20   /* #taps LP */
#define N_HP  21   /* #taps HP */

float LP[] = { coeffLP };
float HP[] = { coeffHP };

/* ---------------- Global Variables -------------------- */
XIicPs Iic;      /* Instance of the IIC Device */

/* Forward declarations */
int  AudioInitialize(u16 timerID, u16 iicID, u32 i2sAddr);
void initialize_FIFO(u32 fifoAddr);
void I2SFifoWrite(u32 i2sBaseAddr, u32 audioData);
u32  I2SFifoRead(u32 i2sBaseAddr);
int  FIR_filter(int *buffer, float *coeff, int taps);
void RunFIRTimingStep3(void);

/* ------------------------------------------------------ */
/*             Codec I2C Register Write                   */
/* ------------------------------------------------------ */
int AudioRegSet(XIicPs *IIcPtr, u8 regAddr, u16 regData)
{
    int Status;
    u8 SendBuffer[2];

    SendBuffer[0] = regAddr << 1;
    SendBuffer[0] |= ((regData >> 8) & 0x1);
    SendBuffer[1] = regData & 0xFF;

    Status = XIicPs_MasterSendPolled(IIcPtr, SendBuffer, 2, IIC_SLAVE_ADDR);
    if (Status != XST_SUCCESS) {
        xil_printf("IIC send failed\n\r");
        return XST_FAILURE;
    }

    while (XIicPs_BusIsBusy(IIcPtr)) {
        /* wait */
    }

    return XST_SUCCESS;
}

/* ------------------------------------------------------ */
/*             Audio + I2S Initialization                 */
/* ------------------------------------------------------ */
int AudioInitialize(u16 timerID, u16 iicID, u32 i2sAddr)
{
    int Status;
    XIicPs_Config *Config;
    u32 i2sClkDiv;

    TimerInitialize(timerID);

    Config = XIicPs_LookupConfig(iicID);
    if (Config == NULL) {
        return XST_FAILURE;
    }

    Status = XIicPs_CfgInitialize(&Iic, Config, Config->BaseAddress);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    Status = XIicPs_SelfTest(&Iic);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    Status = XIicPs_SetSClk(&Iic, IIC_SCLK_RATE);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    /* Codec configuration (SSM2603) */
    Status = AudioRegSet(&Iic, 15, 0b000000000); // Reset
    TimerDelay(75000);
    Status |= AudioRegSet(&Iic, 6,  0b000110000); // Power up
    Status |= AudioRegSet(&Iic, 0,  0b000010111);
    Status |= AudioRegSet(&Iic, 1,  0b000010111);
    Status |= AudioRegSet(&Iic, 2,  0b101111001);
    Status |= AudioRegSet(&Iic, 4,  0b000010000);
    Status |= AudioRegSet(&Iic, 5,  0b000000000);
    Status |= AudioRegSet(&Iic, 7,  0b000001010); // 24-bit word length
    Status |= AudioRegSet(&Iic, 8,  0b000000000); // No CLKDIV2
    TimerDelay(75000);
    Status |= AudioRegSet(&Iic, 9,  0b000000001);
    Status |= AudioRegSet(&Iic, 6,  0b000100000);
    Status  = AudioRegSet(&Iic, 4,  0b000010000);

    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    /* I2S clock: BCLK = MCLK / 4, LRCLK = BCLK / 64 */
    i2sClkDiv  = 1;
    i2sClkDiv |= (31 << 16);

    Xil_Out32(i2sAddr + I2S_CLK_CTRL_REG, i2sClkDiv);
    Xil_Out32(AUDIO_CTRL_BASEADDR + I2S_RESET_REG, 0b110); // reset RX/TX FIFOs
    Xil_Out32(AUDIO_CTRL_BASEADDR + I2S_CTRL_REG,  0b011); // enable RX/TX, unmute

    return XST_SUCCESS;
}

/* ------------------------------------------------------ */
/*                   AXI FIFO Helpers                     */
/* ------------------------------------------------------ */
void I2SFifoWrite(u32 i2sBaseAddr, u32 audioData)
{
    Xil_Out32(i2sBaseAddr + FIFO_TDFD, audioData);
    Xil_Out32(i2sBaseAddr + FIFO_TLR,  4);  // 4 bytes

    while ((Xil_In32(i2sBaseAddr + FIFO_ISR) & 0x08000000) != 0x08000000) {
        /* wait TX complete */
    }
    Xil_Out32(i2sBaseAddr + FIFO_ISR, 0x08000000);  // ack
}

u32 I2SFifoRead(u32 i2sBaseAddr)
{
    while (Xil_In32(i2sBaseAddr + FIFO_RDFO) == 0) {
        /* wait data */
    }
    return Xil_In32(i2sBaseAddr + FIFO_RDFD);
}

void initialize_FIFO(u32 fifoAddr)
{
    xil_printf("\n[INIT FIFO @ 0x%08x]\n\r", fifoAddr);

    Xil_Out32(fifoAddr + FIFO_TDR, 0x00000000);

    xil_printf("FIFO_ISR:  0x%08x\n\r", Xil_In32(fifoAddr + FIFO_ISR));
    xil_printf("FIFO_IER:  0x%08x\n\r", Xil_In32(fifoAddr + FIFO_IER));
    xil_printf("FIFO_TDFV: 0x%08x\n\r", Xil_In32(fifoAddr + FIFO_TDFV));
    xil_printf("FIFO_RDFO: 0x%08x\n\r", Xil_In32(fifoAddr + FIFO_RDFO));

    /* Clear & enable interrupts (even if we use polling) */
    Xil_Out32(fifoAddr + FIFO_ISR, 0xFFFFFFFF);
    Xil_Out32(fifoAddr + FIFO_IER, 0x0C000000);

    xil_printf("FIFO_ISR (after init): 0x%08x\n\r", Xil_In32(fifoAddr + FIFO_ISR));
}

/* ------------------------------------------------------ */
/*                    Generic FIR                         */
/* ------------------------------------------------------ */
/* y[n] = sum_{k=0}^{taps-1} h[k] * x[n-k] */
int FIR_filter(int *buffer, float *coeff, int taps)
{
    float acc = 0.0f;

    for (int k = 0; k < taps; k++) {
        acc += coeff[k] * (float)buffer[k];
    }
    return (int)acc;
}

/* ------------------------------------------------------ */
/*              STEP 3 – FIR Timing (offline)             */
/* ------------------------------------------------------ */void RunFIRTimingStep3(void)
{
    xil_printf("\n=== Step 3: FIR Timing ===\n\r");

    /* Small test buffer (dummy data only for timing) */
    int testBuf[N_HP];
    for (int i = 0; i < N_HP; i++) {
        testBuf[i] = i;   // pattern irrilevante, serve solo per MAC
    }

    /* Enable Global Timer (bit0=enable, bit1=auto-inc) */
    Xil_Out32(GLOBAL_TMR_BASEADDR + 0x08, 0x03);

    u32 t0, t1;
    unsigned long long acc_lp = 0ULL;
    unsigned long long acc_hp = 0ULL;
    int dummy;
    int iterations = 200;

    const float Fs        = 48000.0f;       // ≈ 48 kHz
    const float CPU_FREQ  = 333000000.0f;   // 333 MHz
    float budget_cycles   = CPU_FREQ / Fs;  // ≈ 6937 cycles/sample

    /* Convert float → integer for xil_printf */
    int Fs_int      = (int)Fs;
    int CpuMHz_int  = (int)(CPU_FREQ / 1e6f);
    int budget_int  = (int)budget_cycles;

    xil_printf("Fs ≈ %d Hz, CPU ≈ %d MHz → budget ≈ %d cycles/sample\n\r",
               Fs_int, CpuMHz_int, budget_int);

    /**********************
     *   CACHE ON
     **********************/
    xil_printf("\n[Cache ON]\n\r");

    /* LP FIR timing */
    acc_lp = 0;
    for (int k = 0; k < iterations; k++) {
        t0 = Xil_In32(GLOBAL_TMR_BASEADDR);
        dummy = FIR_filter(testBuf, LP, N_LP);
        t1 = Xil_In32(GLOBAL_TMR_BASEADDR);
        acc_lp += (unsigned long long)(t1 - t0);
    }
    int avg_lp = (int)(acc_lp / iterations);
    float cyc_per_tap_lp = (float)avg_lp / (float)N_LP;
    float max_taps_lp = budget_cycles / cyc_per_tap_lp;

    xil_printf("LP FIR: %d cycles (avg), %.2f cyc/tap → max taps ≈ %.1f\n\r",
               avg_lp, cyc_per_tap_lp, max_taps_lp);

    /* HP FIR timing */
    acc_hp = 0;
    for (int k = 0; k < iterations; k++) {
        t0 = Xil_In32(GLOBAL_TMR_BASEADDR);
        dummy = FIR_filter(testBuf, HP, N_HP);
        t1 = Xil_In32(GLOBAL_TMR_BASEADDR);
        acc_hp += (unsigned long long)(t1 - t0);
    }
    int avg_hp = (int)(acc_hp / iterations);
    float cyc_per_tap_hp = (float)avg_hp / (float)N_HP;
    float max_taps_hp = budget_cycles / cyc_per_tap_hp;

    xil_printf("HP FIR: %d cycles (avg), %.2f cyc/tap → max taps ≈ %.1f\n\r",
               avg_hp, cyc_per_tap_hp, max_taps_hp);

    /**********************
     *   CACHE OFF
     **********************/
    xil_printf("\nDisabling caches...\n\r");
    Xil_DCacheDisable();
    Xil_ICacheDisable();

    xil_printf("\n[Cache OFF]\n\r");

    /* LP FIR timing */
    acc_lp = 0;
    for (int k = 0; k < iterations; k++) {
        t0 = Xil_In32(GLOBAL_TMR_BASEADDR);
        dummy = FIR_filter(testBuf, LP, N_LP);
        t1 = Xil_In32(GLOBAL_TMR_BASEADDR);
        acc_lp += (unsigned long long)(t1 - t0);
    }
    avg_lp = (int)(acc_lp / iterations);
    cyc_per_tap_lp = (float)avg_lp / (float)N_LP;
    max_taps_lp = budget_cycles / cyc_per_tap_lp;

    xil_printf("LP FIR: %d cycles (avg), %.2f cyc/tap → max taps ≈ %.1f\n\r",
               avg_lp, cyc_per_tap_lp, max_taps_lp);

    /* HP FIR timing */
    acc_hp = 0;
    for (int k = 0; k < iterations; k++) {
        t0 = Xil_In32(GLOBAL_TMR_BASEADDR);
        dummy = FIR_filter(testBuf, HP, N_HP);
        t1 = Xil_In32(GLOBAL_TMR_BASEADDR);
        acc_hp += (unsigned long long)(t1 - t0);
    }
    avg_hp = (int)(acc_hp / iterations);
    cyc_per_tap_hp = (float)avg_hp / (float)N_HP;
    max_taps_hp = budget_cycles / cyc_per_tap_hp;

    xil_printf("HP FIR: %d cycles (avg), %.2f cyc/tap → max taps ≈ %.1f\n\r",
               avg_hp, cyc_per_tap_hp, max_taps_hp);

    xil_printf("\n[Step 3 completed]\n\r");

    xil_printf("Re-enabling caches...\n\r");
    Xil_ICacheEnable();
    Xil_DCacheEnable();
}


/* ------------------------------------------------------ */
/*                        MAIN                            */
/* ------------------------------------------------------ */
int main(void)
{
    init_platform();

    xil_printf("\n=========================================\n\r");
    xil_printf("   Lab 3 – Audio DSP (PS FIR + Timing)\n\r");
    xil_printf("=========================================\n\r");

    /* Step 3 – Offline FIR timing (no audio yet) */
    RunFIRTimingStep3();

    /* Step 1+2 – Audio initialization and processing */
    xil_printf("\nInitializing Audio + I2S...\n\r");

    if (AudioInitialize(SCU_TIMER_ID, AUDIO_IIC_ID, AUDIO_CTRL_BASEADDR) != XST_SUCCESS) {
        xil_printf("AudioInitialize FAILED!\n\r");
        cleanup_platform();
        return -1;
    }

    initialize_FIFO(AUDIO_FIFO);
    /* FIR_FIFO not used here, ma lo inizializziamo per completezza */
    initialize_FIFO(FIR_FIFO);

    xil_printf("\nEntering audio processing loop...\n\r");

    int SampleL, SampleR;
    int bufferL[N_HP], bufferR[N_HP];

    /* init FIR buffers */
    for (int i = 0; i < N_HP; i++) {
        bufferL[i] = 0;
        bufferR[i] = 0;
    }

    volatile int *led      = (int *)LED_BASE_ADDR;

    while (1) {
        /* Read stereo samples from AUDIO FIFO */
        SampleL = (int)I2SFifoRead(AUDIO_FIFO);
        SampleR = (int)I2SFifoRead(AUDIO_FIFO);

        /* Shift buffers (sliding window) */
        for (int i = N_HP - 1; i > 0; i--) {
            bufferL[i] = bufferL[i - 1];
            bufferR[i] = bufferR[i - 1];
        }
        bufferL[0] = SampleL;
        bufferR[0] = SampleR;

        /* Read switches */
        u32 sw = Xil_In32(SWI_BASE_ADDR);
        int outL, outR;

        if (sw & 0x1) {
            /* SW0 → Low-pass FIR */
            outL = FIR_filter(bufferL, LP, N_LP);
            outR = FIR_filter(bufferR, LP, N_LP);
        } else if (sw & 0x2) {
            /* SW1 → High-pass FIR */
            outL = FIR_filter(bufferL, HP, N_HP);
            outR = FIR_filter(bufferR, HP, N_HP);
        } else {
            /* No filter, clean passthrough */
            outL = SampleL;
            outR = SampleR;
        }

        /* Show current switch config on LEDs */
        *led = (int)sw;

        /* Write back to audio FIFO */
        I2SFifoWrite(AUDIO_FIFO, outL);
        I2SFifoWrite(AUDIO_FIFO, outR);
    }

    cleanup_platform();
    return 0;
}
