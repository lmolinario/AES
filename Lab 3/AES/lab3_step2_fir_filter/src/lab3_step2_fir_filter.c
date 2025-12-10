/***************************************************************
 *  Lab 3 – FIR Audio Filtering (Step 2)
 *  Author: Lello Molinario
 *  University of Cagliari – Advanced Embedded Systems (AES)
 *  Student ID: 70/90/000369
 *  Date: November 2025
 *
 *  Description
 *  ------------------------------------------------------------
 *  Real-time FIR audio filtering implemented in software on the
 *  Zybo Z7 platform. The program acquires stereo audio samples
 *  (Left + Right) from the SSM2603 codec via AXI-I2S, stores
 *  them in a sliding window, and applies a configurable FIR
 *  convolution before sending the filtered samples back to the
 *  output FIFO.
 *
 *  The goal of STEP 2 is to:
 *    • implement a generic FIR filter function
 *    • apply convolution over a sliding window of past samples
 *    • allow selection of the filtering mode via board switches
 *    • support both Low-Pass (LP) and High-Pass (HP) filters
 *    • preserve clean passthrough audio when no filter is selected
 *
 *  FIR Filtering
 *  ------------------------------------------------------------
 *  The filtering operation is implemented as a standard FIR:
 *
 *        y[n] = h0·x[n] + h1·x[n−1] + … + hN·x[n−N]
 *
 *  where:
 *     • x[n]   = current input sample
 *     • h[k]   = FIR coefficients (filter kernel)
 *     • N      = number of taps (LP or HP)
 *
 *  A sliding buffer stores the latest N input samples, updated
 *  each iteration by shifting the window and inserting x[n] at
 *  index 0. A flexible FIR function computes the convolution
 *  for any tap size, enabling easy testing of alternative
 *  kernels or more aggressive filter shapes.
 *
 *  Filter Selection (Using Switches)
 *  ------------------------------------------------------------
 *    SW0 = 1   → enable Low-Pass filter  (LP)
 *    SW1 = 1   → enable High-Pass filter (HP)
 *    Otherwise → raw audio passthrough (clean)
 *
 *  Additional Notes
 *  ------------------------------------------------------------
 *  • The FIR implementation is fully software-based.
 *  • Coefficients are defined at the top of main.c.
 *  • More aggressive LP/HP kernels may be substituted as needed.
 *  • External tone generators (e.g. szynalski.com/tone-generator)
 *    can be used to validate filter behavior.
 *
 *
 *  Platform
 *  ------------------------------------------------------------
 *   • Board: Zybo Z7 (Zynq-7000)
 *   • Audio Codec: Analog Devices SSM2603 (I²C control, I²S data)
 *   • Interfaces:
 *        – AXI-I2S for audio RX/TX streaming
 *        – AXI FIFO for sample buffering
 *        – PS I²C for codec configuration
 *   • Tools: Xilinx SDK / Vitis + UART terminal (115200 baud, 8N1)
 *
 ***************************************************************/


#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xiicps.h"
#include "timer_ps.h"
#include <time.h>
#include <stdlib.h>
#include <math.h>


/* I2S Register offsets */
#define I2S_RESET_REG 		0x00
#define I2S_CTRL_REG 		0x04
#define I2S_CLK_CTRL_REG 	0x08

#define I2S_FIFO_STS_REG 	0x20
#define I2S_RX_FIFO_REG 	0x28
#define I2S_TX_FIFO_REG 	0x2C


#define FIFO_ISR ( 0x00)
#define FIFO_IER ( 0x04)
#define FIFO_TDFV ( 0x0C)
#define FIFO_RDFO ( 0x1C)
#define FIFO_TDR ( 0x2C)
#define FIFO_TDFD ( 0x10)
#define FIFO_TLR ( 0x14)


#define FIFO_RLR ( 0x24)
#define FIFO_RDFD ( 0x20)
#define FIFO_RDR ( 0x30)

/* IIC address of the SSM2603 device and the desired IIC clock speed */
#define IIC_SLAVE_ADDR		0b0011010
#define IIC_SCLK_RATE		100000


#define AUDIO_IIC_ID XPAR_XIICPS_0_DEVICE_ID
#define AUDIO_CTRL_BASEADDR XPAR_AXI_I2S_ADI_0_S00_AXI_BASEADDR
#define SCU_TIMER_ID XPAR_SCUTIMER_DEVICE_ID


#define SWI_BASE_ADDR XPAR_AXI_GPIO_2_BASEADDR
#define LED_BASE_ADDR XPAR_AXI_GPIO_1_BASEADDR
#define BUT_BASE_ADDR XPAR_AXI_GPIO_0_BASEADDR

#define AUDIO_FIFO XPAR_AXI_FIFO_MM_S_0_BASEADDR

#define FIR_FIFO XPAR_AXI_FIFO_MM_S_1_BASEADDR
#define GLOBAL_TMR_BASEADDR XPAR_PS7_GLOBALTIMER_0_S_AXI_BASEADDR
/* ------------------------------------------------------------ */
/*				Low-Pass and High-Pass FIR filter coefficients									*/
/* ------------------------------------------------------------ */

#define coeffLP -1.692219e-02, 5.043750e-02,3.935835e-02,4.341238e-02,5.137933e-02,5.982048e-02,6.748827e-02, 7.379049e-02, 7.824605e-02, 8.052560e-02,8.052560e-02, 7.824605e-02, 7.379049e-02, 6.748827e-02,5.982048e-02,5.137933e-02,4.341238e-02,3.935835e-02,5.043750e-02,-0.0169221860
#define coeffHP -3.942071e-02,-5.929114e-03,1.430997e-02,4.069053e-02,5.762197e-02,4.843584e-02,4.349633e-03,-6.932913e-02,-1.527862e-01,-2.187328e-01,7.562085e-01,-2.187328e-01,-1.527862e-01,-6.932913e-02,4.349633e-03,4.843584e-02,5.762197e-02,4.069053e-02,1.430997e-02,-5.929114e-03,-0.0394207089
#define N_LP 20 // ordine filtro
#define N_HP 21 // ordine filtro

/* Aggressive Low-Pass (29 taps) */
#define coeffLP_AGG  \
-0.008747420411798365, -0.01352684070757768, -0.021069157456114974, \
-0.02821205662046602, -0.03288466862750655, -0.032820056352546804, \
-0.026015856418133178, -0.011326253746998683, 0.01118086152569252, \
0.039926269347420495, 0.07195575020178693, 0.10331516426959793, \
0.12972205191226951, 0.14735052987683003, 0.15353880775461448, \
0.14735052987683003, 0.12972205191226951, 0.10331516426959793, \
0.07195575020178693, 0.039926269347420495, 0.01118086152569252, \
-0.011326253746998683, -0.026015856418133178, -0.032820056352546804, \
-0.03288466862750655, -0.02821205662046602, -0.021069157456114974, \
-0.01352684070757768, -0.008747420411798365
#define N_LP_AGG 29

/* Aggressive High-Pass (25 taps) */
#define coeffHP_AGG \
0.05946436587252379, -0.08266255914551396, -0.032374303236116855, \
0.00216595808715192, 0.02865430955587078, 0.045067235048989344, \
0.04435253660179216, 0.02016730464237364, -0.027703198625664668, \
-0.0913071374380985, -0.15595705304807897, -0.2038996657538856, \
0.7782265468798992, -0.2038996657538856, -0.15595705304807897, \
-0.0913071374380985, -0.027703198625664668, 0.02016730464237364, \
0.04435253660179216, 0.045067235048989344, 0.02865430955587078, \
0.00216595808715192, -0.032374303236116855, -0.08266255914551396, \
0.05946436587252379
#define N_HP_AGG 25

/* Create coefficient arrays */
float LP_AGG[] = { coeffLP_AGG };
float HP_AGG[] = { coeffHP_AGG };

float LP[]={coeffLP};
float HP[]={coeffHP};


/* ------------------------------------------------------------ */
/*				Global Variables								*/
/* ------------------------------------------------------------ */

XIicPs Iic;		/* Instance of the IIC Device */

/* ------------------------------------------------------------ */
/*				Procedure Definitions							*/
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
/***	AudioInitialize(u16 timerID,  u16 iicID, u32 i2sAddr)
**
**	Parameters:
**		timerID - DEVICE_ID for the SCU timer
**		iicID 	- DEVICE_ID for the PS IIC controller connected to the SSM2603
**		i2sAddr - Physical Base address of the I2S controller
**
**	Return Value: int
**		XST_SUCCESS if successful
**
**	Errors:
**
**	Description:
**		Initializes the Audio demo. Must be called once and only once before calling
**		AudioRunDemo
**
*/
int AudioInitialize(u16 timerID,  u16 iicID, u32 i2sAddr) //, u32 i2sTransmAddr, u32 i2sReceivAddr)
{
	int Status;
	XIicPs_Config *Config;
	u32 i2sClkDiv;

	TimerInitialize(timerID);

	/*
	 * Initialize the IIC driver so that it's ready to use
	 * Look up the configuration in the config table,
	 * then initialize it.
	 */
	Config = XIicPs_LookupConfig(iicID);
	if (NULL == Config) {
		return XST_FAILURE;
	}

	Status = XIicPs_CfgInitialize(&Iic, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Perform a self-test to ensure that the hardware was built correctly.
	 */
	Status = XIicPs_SelfTest(&Iic);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Set the IIC serial clock rate.
	 */
	Status = XIicPs_SetSClk(&Iic, IIC_SCLK_RATE);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}


	/*
	 * Write to the SSM2603 audio codec registers to configure the device. Refer to the
	 * SSM2603 Audio Codec data sheet for information on what these writes do.
	 */
	Status = AudioRegSet(&Iic, 15, 0b000000000); //Perform Reset
	TimerDelay(75000);
	Status |= AudioRegSet(&Iic, 6, 0b000110000); //Power up
	Status |= AudioRegSet(&Iic, 0, 0b000010111);
	Status |= AudioRegSet(&Iic, 1, 0b000010111);
	Status |= AudioRegSet(&Iic, 2, 0b101111001);
	Status |= AudioRegSet(&Iic, 4, 0b000010000);
	Status |= AudioRegSet(&Iic, 5, 0b000000000);
	Status |= AudioRegSet(&Iic, 7, 0b000001010); //Changed so Word length is 24
	Status |= AudioRegSet(&Iic, 8, 0b000000000); //Changed so no CLKDIV2
	TimerDelay(75000);
	Status |= AudioRegSet(&Iic, 9, 0b000000001);
	Status |= AudioRegSet(&Iic, 6, 0b000100000);
	Status = AudioRegSet(&Iic, 4, 0b000010000);

	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	i2sClkDiv = 1; //Set the BCLK to be MCLK / 4
	i2sClkDiv = i2sClkDiv | (31 << 16); //Set the LRCLK's to be BCLK / 64

	Xil_Out32(i2sAddr + I2S_CLK_CTRL_REG, i2sClkDiv); //Write clock div register

	Xil_Out32(AUDIO_CTRL_BASEADDR + I2S_RESET_REG, 0b110); //Reset RX and TX FIFOs
	Xil_Out32(AUDIO_CTRL_BASEADDR + I2S_CTRL_REG, 0b011); //Enable RX Fifo and TX FIFOs, disable mute
	return XST_SUCCESS;
}

void I2SFifoWrite (u32 i2sBaseAddr, u32 audioData)
{

	Xil_Out32(i2sBaseAddr + 0x10, audioData); // write DATA
    Xil_Out32(i2sBaseAddr + 0x14, 4);    // write the length of the DATA (4 bytes)

	//xil_printf("%x\n", Xil_In32(i2sBaseAddr + 0x00));
	while ((Xil_In32(i2sBaseAddr + 0x00)&0x08000000)!=0x08000000){;}  // waits for the transmission completes
	Xil_Out32(i2sBaseAddr + 0x00, 0x08000000);  // ack the transmission complete


}

u32 I2SFifoRead (u32 i2sBaseAddr)
{

	while (Xil_In32(i2sBaseAddr + 0x1C)==0){;} // waits for a sample in the FIFO
	int data = Xil_In32(i2sBaseAddr + 0x20);   // read the sample from the FIFO
return data;

}

void initialize_FIFO(u32 fifoAddr){
	Xil_Out32(AUDIO_FIFO + 0x2c, 0);

	    // init
	    Xil_Out32(fifoAddr + FIFO_ISR, 0xFFFFFFFF);
	    Xil_Out32(fifoAddr + FIFO_IER, 0x0C000000);
	    Xil_Out32(fifoAddr + FIFO_TDR, 0x00000000);
		Xil_Out32(fifoAddr + FIFO_ISR, 0xFFFFFFFF);
	    Xil_Out32(fifoAddr + FIFO_IER, 0x04100000);
	    Xil_Out32(fifoAddr + FIFO_ISR, 0x00100000);



}


// GENERIC FIR FILTER: y[n] = sum_{k=0}^{taps-1} h[k] * x[n-k]
int FIR_filter(int *buffer, float *coeff, int taps)
{
    float acc = 0.0f;   // accumulator for the convolution result

    /* Perform the FIR convolution:
     *   For each tap k, multiply the coefficient h[k] by the
     *   corresponding delayed sample buffer[k], and accumulate.
     *
     *   buffer[0] = newest sample  x[n]
     *   buffer[1] = previous       x[n-1]
     *   ...
     *   buffer[k] = x[n-k]
     */
    for (int k = 0; k < taps; k++) {
        acc += coeff[k] * (float)buffer[k];
    }

    /* Return the filtered sample as an integer.
     * The output of the FIR is floating-point, but audio samples
     * are represented as integers in the I²S data path.
     */
    return (int)acc;
}




int main()
{
    init_platform();
    /* Initialize the underlying BSP (UART, caches, stdout redirection, etc.)
     * This is required before any I/O or platform-specific operations. */
    print("Started!\n\r");

    xil_printf("\n=== LAB3 – Step 2: FIR Filtering ===\n\r");

    xil_printf("\nFilter selection via board switches:\n\r");

    xil_printf("\n  SW0 = 1  →  Low-Pass filter  (basic kernel)\n\r");
    xil_printf("\n  SW1 = 1  →  High-Pass filter (basic kernel)\n\r");
    xil_printf("\n  SW2 = 1  →  Low-Pass filter  (aggressive kernel)\n\r");
    xil_printf("\n  SW3 = 1  →  High-Pass filter (aggressive kernel)\n\r");
    xil_printf("\n  No switches ON → raw audio passthrough\n");
    xil_printf(" \n\n");


    /* Initialize the audio subsystem:
     *  - configure the SSM2603 codec via I²C
     *  - set up I²S clocking and FIFO control registers
     *  - enable RX/TX streaming
     */
    AudioInitialize(SCU_TIMER_ID, AUDIO_IIC_ID, AUDIO_CTRL_BASEADDR);

    /* Initialize both AXI FIFOs:
     *  - AUDIO_FIFO handles samples coming from / going to the I²S interface
     * The initialization sequence clears pending flags and configures control bits.
     */

    initialize_FIFO(AUDIO_FIFO);

    int SampleL, SampleR; // temporary storage for incoming stereo samples

    // FIR sliding-window buffers (use the largest size: N_HP)
    int bufferL[N_HP], bufferR[N_HP];

    // Initialize buffers to zero (cold start of the convolution state)
    for (int i = 0; i < N_HP; i++) {
        bufferL[i] = 0;
        bufferR[i] = 0;
    }

    while (1) {

        /* -----------------------------
         *      SAMPLE ACQUISITION
         * -----------------------------
         * Read one stereo sample from the I²S RX FIFO.
         * Samples are 32-bit integers (packed audio data).
         */
        SampleL = (int) I2SFifoRead(AUDIO_FIFO);
        SampleR = (int) I2SFifoRead(AUDIO_FIFO);

        /* -----------------------------
         *     SLIDING WINDOW UPDATE
         * -----------------------------
         * Shift all older samples to the right:
         *
         *   buffer[k] ← buffer[k-1]
         *
         * Insert the newest sample at index 0.
         * This produces the structure:
         *   buffer[0] = x[n]
         *   buffer[1] = x[n-1]
         *   ...
         *   buffer[N_HP-1] = x[n-(N_HP-1)]
         */
        for (int i = N_HP - 1; i > 0; i--) {
            bufferL[i] = bufferL[i - 1];
            bufferR[i] = bufferR[i - 1];
        }
        bufferL[0] = SampleL;
        bufferR[0] = SampleR;

        /* -----------------------------
         *       FILTER SELECTION
         * -----------------------------
         * Read switch inputs from the GPIO.
         *   SW0 → Low-Pass filter
         *   SW1 → High-Pass filter
         *   none → raw passthrough
         */
        u32 sw = Xil_In32(SWI_BASE_ADDR);

        int outL, outR;

        if (sw & 0x1) {
            // SW0 → Low-Pass (base)
            outL = FIR_filter(bufferL, LP, N_LP);
            outR = FIR_filter(bufferR, LP, N_LP);
        }
        else if (sw & 0x2) {
            // SW1 → High-Pass (base)
            outL = FIR_filter(bufferL, HP, N_HP);
            outR = FIR_filter(bufferR, HP, N_HP);
        }
        else if (sw & 0x4) {
            // SW2 → Low-Pass (aggressive)
            outL = FIR_filter(bufferL, LP_AGG, N_LP_AGG);
            outR = FIR_filter(bufferR, LP_AGG, N_LP_AGG);
        }
        else if (sw & 0x8) {
            // SW3 → High-Pass (aggressive)
            outL = FIR_filter(bufferL, HP_AGG, N_HP_AGG);
            outR = FIR_filter(bufferR, HP_AGG, N_HP_AGG);
        }
        else {
            // No filter → clean passthrough
            outL = SampleL;
            outR = SampleR;
        }


        /* -----------------------------
         *          OUTPUT STAGE
         * -----------------------------
         * Send the processed (or raw) samples to the I²S TX FIFO
         * for playback through the audio DAC.
         */
        I2SFifoWrite(AUDIO_FIFO, outL);
        I2SFifoWrite(AUDIO_FIFO, outR);
    }

    cleanup_platform();
    return 0;
}
