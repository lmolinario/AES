/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xiicps.h"
#include "timer_ps.h"
#include "xil_cache.h"   // <-- per Xil_DCacheDisable / Xil_ICacheDisable
#include <time.h>
#include <stdlib.h>
#include <math.h>

/* I2S Register offsets */
#define I2S_RESET_REG      0x00
#define I2S_CTRL_REG       0x04
#define I2S_CLK_CTRL_REG   0x08
#define I2S_FIFO_STS_REG   0x20
#define I2S_RX_FIFO_REG    0x28
#define I2S_TX_FIFO_REG    0x2C

#define FIFO_ISR  (0x00)
#define FIFO_IER  (0x04)
#define FIFO_TDFV (0x0C)
#define FIFO_RDFO (0x1C)
#define FIFO_TDR  (0x2C)
#define FIFO_TDFD (0x10)
#define FIFO_TLR  (0x14)

#define FIFO_RLR  (0x24)
#define FIFO_RDFD (0x20)
#define FIFO_RDR  (0x30)

/* IIC address of the SSM2603 device and the desired IIC clock speed */
#define IIC_SLAVE_ADDR     0b0011010
#define IIC_SCLK_RATE      100000

#define AUDIO_IIC_ID       XPAR_XIICPS_0_DEVICE_ID
#define AUDIO_CTRL_BASEADDR XPAR_AXI_I2S_ADI_0_S00_AXI_BASEADDR
#define SCU_TIMER_ID       XPAR_SCUTIMER_DEVICE_ID

#define SWI_BASE_ADDR      XPAR_AXI_GPIO_2_BASEADDR
#define LED_BASE_ADDR      XPAR_AXI_GPIO_1_BASEADDR
#define BUT_BASE_ADDR      XPAR_AXI_GPIO_0_BASEADDR

#define AUDIO_FIFO         XPAR_AXI_FIFO_MM_S_0_BASEADDR
#define FIR_FIFO           XPAR_AXI_FIFO_MM_S_1_BASEADDR

/* Global Timer base address (AXI peripheral) */
#define GLOBAL_TMR_BASEADDR XPAR_PS7_GLOBALTIMER_0_S_AXI_BASEADDR
#define GTIMER_COUNTER_LOWER_OFFSET  0x00
#define GTIMER_COUNTER_UPPER_OFFSET  0x04
#define GTIMER_CONTROL_OFFSET        0x08

/* ------------------------------------------------------------ */
/*  Low-Pass and High-Pass FIR filter coefficients              */
/* ------------------------------------------------------------ */
#define coeffLP -1.692219e-02, 5.043750e-02,3.935835e-02,4.341238e-02,5.137933e-02,5.982048e-02,6.748827e-02, 7.379049e-02, 7.824605e-02, 8.052560e-02,8.052560e-02, 7.824605e-02, 7.379049e-02, 6.748827e-02,5.982048e-02,5.137933e-02,4.341238e-02,3.935835e-02,5.043750e-02,-0.0169221860
#define coeffHP -3.942071e-02,-5.929114e-03,1.430997e-02,4.069053e-02,5.762197e-02,4.843584e-02,4.349633e-03,-6.932913e-02,-1.527862e-01,-2.187328e-01,7.562085e-01,-2.187328e-01,-1.527862e-01,-6.932913e-02,4.349633e-03,4.843584e-02,5.762197e-02,4.069053e-02,1.430997e-02,-5.929114e-03,-0.0394207089

#define N_LP 20   // ordine filtro LP
#define N_HP 21   // ordine filtro HP

#define N_MEAS 1000  // numero di esecuzioni per la media dei cicli

float LP[] = { coeffLP };
float HP[] = { coeffHP };

/* ------------------------------------------------------------ */
/*  Global Variables                                            */
/* ------------------------------------------------------------ */

XIicPs Iic;     /* Instance of the IIC Device */

/* ------------------------------------------------------------ */
/*  Procedure Definitions                                       */
/* ------------------------------------------------------------ */

int AudioRegSet(XIicPs *IIcPtr, u8 regAddr, u16 regData)
{
    int Status;
    u8 SendBuffer[2];

    SendBuffer[0] = regAddr << 1;
    SendBuffer[0] = SendBuffer[0] | ((regData >> 8) & 0b1);

    SendBuffer[1] = regData & 0xFF;

    Status = XIicPs_MasterSendPolled(IIcPtr, SendBuffer,
                 2, IIC_SLAVE_ADDR);
    if (Status != XST_SUCCESS) {
        xil_printf("IIC send failed\n\r");
        return XST_FAILURE;
    }
    /*
     * Wait until bus is idle to start another transfer.
     */
    while (XIicPs_BusIsBusy(IIcPtr)) {
        /* NOP */
    }
    return XST_SUCCESS;
}

/*** AudioInitialize(u16 timerID,  u16 iicID, u32 i2sAddr)
**
**  Parameters:
**      timerID - DEVICE_ID for the SCU timer
**      iicID   - DEVICE_ID for the PS IIC controller connected to the SSM2603
**      i2sAddr - Physical Base address of the I2S controller
**
**  Return Value: int
**      XST_SUCCESS if successful
**
**  Description:
**      Initializes the Audio demo. Must be called once and only once
**      before calling the main audio loop.
*/
int AudioInitialize(u16 timerID,  u16 iicID, u32 i2sAddr)
{
    int Status;
    XIicPs_Config *Config;
    u32 i2sClkDiv;

    TimerInitialize(timerID);

    /* Initialize the IIC driver */
    Config = XIicPs_LookupConfig(iicID);
    if (NULL == Config) {
        return XST_FAILURE;
    }

    Status = XIicPs_CfgInitialize(&Iic, Config, Config->BaseAddress);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    /* Self-test */
    Status = XIicPs_SelfTest(&Iic);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    /* Set IIC serial clock rate */
    Status = XIicPs_SetSClk(&Iic, IIC_SCLK_RATE);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    /* Configure SSM2603 codec registers */
    Status = AudioRegSet(&Iic, 15, 0b000000000); // Reset
    TimerDelay(75000);
    Status |= AudioRegSet(&Iic, 6, 0b000110000); // Power up
    Status |= AudioRegSet(&Iic, 0, 0b000010111);
    Status |= AudioRegSet(&Iic, 1, 0b000010111);
    Status |= AudioRegSet(&Iic, 2, 0b101111001);
    Status |= AudioRegSet(&Iic, 4, 0b000010000);
    Status |= AudioRegSet(&Iic, 5, 0b000000000);
    Status |= AudioRegSet(&Iic, 7, 0b000001010); // 24-bit word length
    Status |= AudioRegSet(&Iic, 8, 0b000000000); // no CLKDIV2
    TimerDelay(75000);
    Status |= AudioRegSet(&Iic, 9, 0b000000001);
    Status |= AudioRegSet(&Iic, 6, 0b000100000);
    Status  = AudioRegSet(&Iic, 4, 0b000010000);

    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    /* I2S clock dividers */
    i2sClkDiv = 1;                // BCLK = MCLK / 4
    i2sClkDiv = i2sClkDiv | (31 << 16); // LRCLK = BCLK / 64

    Xil_Out32(i2sAddr + I2S_CLK_CTRL_REG, i2sClkDiv);

    Xil_Out32(AUDIO_CTRL_BASEADDR + I2S_RESET_REG, 0b110); // Reset RX/TX FIFOs
    Xil_Out32(AUDIO_CTRL_BASEADDR + I2S_CTRL_REG,  0b011); // Enable RX/TX, unmute
    return XST_SUCCESS;
}

void I2SFifoWrite (u32 i2sBaseAddr, u32 audioData)
{
    Xil_Out32(i2sBaseAddr + 0x10, audioData); // write DATA
    Xil_Out32(i2sBaseAddr + 0x14, 4);         // length (4 bytes)

    while ((Xil_In32(i2sBaseAddr + 0x00) & 0x08000000) != 0x08000000) {
        ; // wait transmission complete
    }
    Xil_Out32(i2sBaseAddr + 0x00, 0x08000000); // ack
}

u32 I2SFifoRead (u32 i2sBaseAddr)
{
    while (Xil_In32(i2sBaseAddr + 0x1C) == 0) {
        ; // wait sample
    }
    return Xil_In32(i2sBaseAddr + 0x20);
}

void initialize_FIFO(u32 fifoAddr)
{
    Xil_Out32(AUDIO_FIFO + 0x2c, 0);

    xil_printf("FIFO_ISR:  0x%08x\n", Xil_In32(fifoAddr + FIFO_ISR));
    print("write FIFO_ISR\n\r");
    Xil_Out32(fifoAddr + FIFO_ISR, 0xFFFFFFFF);
    xil_printf("FIFO_ISR:  0x%08x\n", Xil_In32(fifoAddr + FIFO_ISR));
    xil_printf("FIFO_IER:  0x%08x\n", Xil_In32(fifoAddr + FIFO_IER));
    xil_printf("FIFO_TDFV: 0x%08x\n", Xil_In32(fifoAddr + FIFO_TDFV));
    xil_printf("FIFO_RDFO: 0x%08x\n", Xil_In32(fifoAddr + FIFO_RDFO));

    print("Write IER\n\r");
    Xil_Out32(fifoAddr + FIFO_IER, 0x0C000000);

    print("Write TDR\n\r");
    Xil_Out32(fifoAddr + FIFO_TDR, 0x00000000);

    xil_printf("FIFO_ISR:  0x%08x\n", Xil_In32(fifoAddr + FIFO_ISR));
    print("write FIFO_ISR\n\r");
    Xil_Out32(fifoAddr + FIFO_ISR, 0xFFFFFFFF);
    xil_printf("FIFO_ISR:  0x%08x\n", Xil_In32(fifoAddr + FIFO_ISR));
    xil_printf("FIFO_IER:  0x%08x\n", Xil_In32(fifoAddr + FIFO_IER));
    xil_printf("FIFO_TDFV: 0x%08x\n", Xil_In32(fifoAddr + FIFO_TDFV));
    xil_printf("FIFO_RDFO: 0x%08x\n", Xil_In32(fifoAddr + FIFO_RDFO));

    print("write FIFO_IER\n");
    Xil_Out32(fifoAddr + FIFO_IER, 0x04100000);
    xil_printf("FIFO_ISR:  0x%08x\n", Xil_In32(fifoAddr + FIFO_ISR));
    print("write FIFO_ISR\n");
    Xil_Out32(fifoAddr + FIFO_ISR, 0x00100000);
}

/* FIR generico: y[n] = sum_{k=0}^{taps-1} h[k] * x[n-k] */
int FIR_filter(int *buffer, float *coeff, int taps)
{
    float acc = 0.0f;
    int k;

    for (k = 0; k < taps; k++) {
        acc += coeff[k] * (float)buffer[k];
    }
    return (int)acc;
}

/* ------------------------------------------------------------ */
/*                          MAIN                               */
/* ------------------------------------------------------------ */

int main()
{
    int i;

    int SampleL, SampleR;
    int outL, outR;

    /* Variabili per timing (Step 3) */
    u32 t0, t1;
    unsigned long long acc_cycles_LP = 0;
    unsigned long long acc_cycles_HP = 0;
    float avg_lp = 0.0f;
    float avg_hp = 0.0f;
    float avg_lp_off = 0.0f;
    float avg_hp_off = 0.0f;
    float cycles_per_sample;
    float fs_measured = 48000.0f;   // Sostituisci con Fs misurata allo Step 1

    int dummy = 0;

    /* Per stampa con xil_printf (solo %d) */
    int avg_lp_i, avg_hp_i;
    int cps100;
    int lp_budget100, hp_budget100;
    int avg_lp_off_i, avg_hp_off_i;
    int slowdown_lp100, slowdown_hp100;

    /* Buffer FIR (uso dimensione N_HP, la più grande) */
    int bufferL[N_HP], bufferR[N_HP];

    init_platform();

    print("Started!\n\r");
    xil_printf("\n=== LAB3 – Step 2: FIR Filtering ===\n");

    AudioInitialize(SCU_TIMER_ID, AUDIO_IIC_ID, AUDIO_CTRL_BASEADDR);

    initialize_FIFO(AUDIO_FIFO);
    initialize_FIFO(FIR_FIFO);

    /* Inizializza i buffer a zero */
    for (i = 0; i < N_HP; i++) {
        bufferL[i] = 0;
        bufferR[i] = 0;
    }

    /**************************************************************
     *  STEP 3 – Misura tempo FIR (OFFLINE, prima del loop audio)
     *  - FIR LP/HP con cache ON
     *  - calcolo cicli disponibili per sample
     *  - FIR LP/HP con cache OFF
     **************************************************************/

    /* Abilita Global Timer (bit0=ENABLE, bit1=AUTO-INCREMENT) */
    Xil_Out32(GLOBAL_TMR_BASEADDR + GTIMER_CONTROL_OFFSET, 0x03);

    xil_printf("\n=== LAB3 – Step 3: FIR Timing ===\n\r");
    xil_printf("Misura FIR (cache ON)...\n\r");

    /* --- Low-pass FIR, cache ON --- */
    acc_cycles_LP = 0;
    for (i = 0; i < N_MEAS; i++) {
        t0 = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
        dummy = FIR_filter(bufferL, LP, N_LP);
        t1 = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
        acc_cycles_LP += (unsigned long long)(t1 - t0);
    }
    avg_lp = (float)acc_cycles_LP / (float)N_MEAS;

    /* --- High-pass FIR, cache ON --- */
    acc_cycles_HP = 0;
    for (i = 0; i < N_MEAS; i++) {
        t0 = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
        dummy = FIR_filter(bufferL, HP, N_HP);
        t1 = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
        acc_cycles_HP += (unsigned long long)(t1 - t0);
    }
    avg_hp = (float)acc_cycles_HP / (float)N_MEAS;

    /* Cicli disponibili per sample (GlobalTimer @ 333 MHz) */
    cycles_per_sample = 333000000.0f / fs_measured;

    /* Conversione in interi per stampa */
    avg_lp_i = (int)(avg_lp + 0.5f);
    avg_hp_i = (int)(avg_hp + 0.5f);

    cps100 = (int)(cycles_per_sample * 100.0f + 0.5f);

    lp_budget100 = (int)((avg_lp / cycles_per_sample) * 10000.0f + 0.5f);
    hp_budget100 = (int)((avg_hp / cycles_per_sample) * 10000.0f + 0.5f);

    xil_printf("\n--- RISULTATI (CACHE ON) ---\n\r");
    xil_printf("LP FIR (%d tap): %d cicli\n\r", N_LP, avg_lp_i);
    xil_printf("HP FIR (%d tap): %d cicli\n\r", N_HP, avg_hp_i);
    xil_printf("Cicli disponibili per sample: %d.%02d\n\r",
               cps100 / 100, cps100 % 100);
    xil_printf("LP usa circa %d.%02d%% del budget\n\r",
               lp_budget100 / 100, lp_budget100 % 100);
    xil_printf("HP usa circa %d.%02d%% del budget\n\r",
               hp_budget100 / 100, hp_budget100 % 100);

    /**************************************************************
     *  Seconda misura: cache DISABILITATE
     **************************************************************/
    xil_printf("\nDisabilito cache I+D e ripeto le misure...\n\r");

    Xil_DCacheDisable();
    Xil_ICacheDisable();

    /* --- Low-pass FIR, cache OFF --- */
    acc_cycles_LP = 0;
    for (i = 0; i < N_MEAS; i++) {
        t0 = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
        dummy = FIR_filter(bufferL, LP, N_LP);
        t1 = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
        acc_cycles_LP += (unsigned long long)(t1 - t0);
    }
    avg_lp_off = (float)acc_cycles_LP / (float)N_MEAS;

    /* --- High-pass FIR, cache OFF --- */
    acc_cycles_HP = 0;
    for (i = 0; i < N_MEAS; i++) {
        t0 = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
        dummy = FIR_filter(bufferL, HP, N_HP);
        t1 = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
        acc_cycles_HP += (unsigned long long)(t1 - t0);
    }
    avg_hp_off = (float)acc_cycles_HP / (float)N_MEAS;

    avg_lp_off_i = (int)(avg_lp_off + 0.5f);
    avg_hp_off_i = (int)(avg_hp_off + 0.5f);

    slowdown_lp100 = (int)((avg_lp_off / avg_lp) * 100.0f + 0.5f);
    slowdown_hp100 = (int)((avg_hp_off / avg_hp) * 100.0f + 0.5f);

    xil_printf("\n--- RISULTATI (CACHE OFF) ---\n\r");
    xil_printf("LP FIR (%d tap): %d cicli\n\r", N_LP, avg_lp_off_i);
    xil_printf("HP FIR (%d tap): %d cicli\n\r", N_HP, avg_hp_off_i);
    xil_printf("Slowdown LP ≈ %d.%02dx\n\r",
               slowdown_lp100 / 100, slowdown_lp100 % 100);
    xil_printf("Slowdown HP ≈ %d.%02dx\n\r",
               slowdown_hp100 / 100, slowdown_hp100 % 100);

    xil_printf("\n[Step 3 completato – entro ora nel loop audio realtime]\n\r");

    /**************************************************************
     *  LOOP AUDIO REALTIME (Step 2) – FIR in tempo reale
     **************************************************************/
    while (1) {

        /* --- ACQUISIZIONE --- */
        SampleL = (int) I2SFifoRead(AUDIO_FIFO);
        SampleR = (int) I2SFifoRead(AUDIO_FIFO);

        /* --- SLIDING WINDOW: shift a destra --- */
        for (i = N_HP - 1; i > 0; i--) {
            bufferL[i] = bufferL[i - 1];
            bufferR[i] = bufferR[i - 1];
        }
        bufferL[0] = SampleL;
        bufferR[0] = SampleR;

        /* --- SELEZIONE FILTRO TRAMITE SWITCH --- */
        {
            u32 sw = Xil_In32(SWI_BASE_ADDR);

            if (sw & 0x1) {
                /* SW0 ON → Low-pass */
                outL = FIR_filter(bufferL, LP, N_LP);
                outR = FIR_filter(bufferR, LP, N_LP);
            }
            else if (sw & 0x2) {
                /* SW1 ON → High-pass */
                outL = FIR_filter(bufferL, HP, N_HP);
                outR = FIR_filter(bufferR, HP, N_HP);
            }
            else {
                /* Nessun filtro → loopback “clean” */
                outL = SampleL;
                outR = SampleR;
            }
        }

        /* --- OUTPUT --- */
        I2SFifoWrite(AUDIO_FIFO, outL);
        I2SFifoWrite(AUDIO_FIFO, outR);
    }

    cleanup_platform();
    return 0;
}
