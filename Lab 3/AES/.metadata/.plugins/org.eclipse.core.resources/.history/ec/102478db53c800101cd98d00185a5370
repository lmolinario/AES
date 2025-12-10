#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xiicps.h"
#include "timer_ps.h"

/* I2S Registers */
#define FIFO_ISR             0x00
#define FIFO_TDFD            0x10
#define FIFO_TLR             0x14
#define FIFO_RDFO            0x1C
#define FIFO_RDFD            0x20

/* Codec I2C parameters */
#define IIC_SLAVE_ADDR   0b0011010
#define IIC_SCLK_RATE    100000

#define AUDIO_IIC_ID          XPAR_XIICPS_0_DEVICE_ID
#define AUDIO_CTRL_BASEADDR   XPAR_AXI_I2S_ADI_0_S00_AXI_BASEADDR
#define AUDIO_FIFO            XPAR_AXI_FIFO_MM_S_0_BASEADDR
#define SCU_TIMER_ID          XPAR_SCUTIMER_DEVICE_ID
#define SWI_BASE_ADDR         XPAR_AXI_GPIO_2_BASEADDR

XIicPs Iic;

/* ---------------------- FIR COEFFICIENTS ---------------------- */
/* versione semplice, 20 tap */
#define coeffLP -1.692219e-02, 5.043750e-02,3.935835e-02,4.341238e-02,5.137933e-02, \
                5.982048e-02,6.748827e-02,7.379049e-02,7.824605e-02,8.052560e-02, \
                8.052560e-02,7.824605e-02,7.379049e-02,6.748827e-02,5.982048e-02, \
                5.137933e-02,4.341238e-02,3.935835e-02,5.043750e-02,-0.0169221860

#define N_TAP 20

float LP[N_TAP] = { coeffLP };

/* ------------------------- I2C WRITE --------------------------- */
int AudioRegSet(XIicPs *IicPtr, u8 reg, u16 data)
{
    u8 buf[2];
    buf[0] = (reg << 1) | ((data >> 8) & 1);
    buf[1] = data & 0xFF;

    XIicPs_MasterSendPolled(IicPtr, buf, 2, IIC_SLAVE_ADDR);
    while (XIicPs_BusIsBusy(IicPtr)) {}
    return XST_SUCCESS;
}

/* ---------------------- AUDIO INITIALIZE ----------------------- */
int AudioInitialize()
{
    XIicPs_Config *Cfg;

    TimerInitialize(SCU_TIMER_ID);

    Cfg = XIicPs_LookupConfig(AUDIO_IIC_ID);
    XIicPs_CfgInitialize(&Iic, Cfg, Cfg->BaseAddress);
    XIicPs_SetSClk(&Iic, IIC_SCLK_RATE);

    AudioRegSet(&Iic, 15, 0x000);   // Reset
    TimerDelay(75000);
    AudioRegSet(&Iic, 6, 0b000110000);
    AudioRegSet(&Iic, 0, 0b000010111);
    AudioRegSet(&Iic, 1, 0b000010111);
    AudioRegSet(&Iic, 2, 0b101111001);
    AudioRegSet(&Iic, 4, 0b000010000);
    AudioRegSet(&Iic, 5, 0b000000000);
    AudioRegSet(&Iic, 7, 0b000001010);
    AudioRegSet(&Iic, 8, 0b000000000);
    TimerDelay(75000);
    AudioRegSet(&Iic, 9,  0b000000001);
    AudioRegSet(&Iic, 6,  0b000100000);

    /* Clock dividers */
    u32 div = 1 | (31 << 16);
    Xil_Out32(AUDIO_CTRL_BASEADDR + 0x08, div);

    /* Reset + Enable I2S FIFOs */
    Xil_Out32(AUDIO_CTRL_BASEADDR + 0x00, 0b110);
    Xil_Out32(AUDIO_CTRL_BASEADDR + 0x04, 0b011);

    return XST_SUCCESS;
}

/* ----------------------- FIFO READ/WRITE ----------------------- */
u32 I2SFifoRead(u32 fifo)
{
    while (Xil_In32(fifo + FIFO_RDFO) == 0) {}
    return Xil_In32(fifo + FIFO_RDFD);
}

void I2SFifoWrite(u32 fifo, u32 sample)
{
    Xil_Out32(fifo + FIFO_TDFD, sample);
    Xil_Out32(fifo + FIFO_TLR,  4);
    while ((Xil_In32(fifo + FIFO_ISR) & 0x08000000) == 0) {}
    Xil_Out32(fifo + FIFO_ISR, 0x08000000);
}

/* --------------------- FIR FILTER MINIMALE --------------------- */
float fir_filter(float x, float *h, float *buf, int N)
{
    /* shift buffer */
    for(int i = N - 1; i > 0; i--)
        buf[i] = buf[i - 1];

    buf[0] = x;

    /* convolution */
    float y = 0;
    for(int i = 0; i < N; i++)
        y += h[i] * buf[i];

    return y;
}

/* ----------------------------- MAIN ----------------------------- */
int main()
{
    init_platform();
    xil_printf("\n===== LAB 3 – STEP 2: FIR (Minimal) =====\n\r");

    AudioInitialize();

    float bufL[N_TAP] = {0};
    float bufR[N_TAP] = {0};

    volatile int *switches = (int *) SWI_BASE_ADDR;

    xil_printf("SW0 = 1 → FIR ON\nSW0 = 0 → Pass-through\n\r");

    while(1)
    {
        /* read samples (32 bit: 24bit audio valid) */
        int sL = I2SFifoRead(AUDIO_FIFO);
        int sR = I2SFifoRead(AUDIO_FIFO);

        /* extract 24-bit audio */
        float xL = (float)(sL >> 8);
        float xR = (float)(sR >> 8);

        int filteredL, filteredR;

        if (*switches & 0x1)
        {
            /* apply FIR LP */
            float yL = fir_filter(xL, LP, bufL, N_TAP);
            float yR = fir_filter(xR, LP, bufR, N_TAP);

            filteredL = ((int)yL) << 8;
            filteredR = ((int)yR) << 8;
        }
        else
        {
            /* clean audio */
            filteredL = sL;
            filteredR = sR;
        }

        /* output */
        I2SFifoWrite(AUDIO_FIFO, filteredL);
        I2SFifoWrite(AUDIO_FIFO, filteredR);
    }

    cleanup_platform();
    return 0;
}
