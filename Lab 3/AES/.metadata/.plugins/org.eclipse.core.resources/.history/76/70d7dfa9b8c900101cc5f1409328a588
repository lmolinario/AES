/***************************************************************
 *  Lab 3 – Audio Passthrough & Sampling Frequency Measurement
 *  Author: Lello Molinario
 *  University of Cagliari – Advanced Embedded Systems (AES)
 *  Student ID: 70/90/000369
 *  Date: November 2025
 *
 *  Description
 *  ------------------------------------------------------------
 *  Real-time audio loopback application running on the Zybo Z7.
 *  The program acquires stereo audio samples (Left + Right)
 *  from the SSM2603 audio codec via AXI-I2S, and immediately
 *  forwards the samples back to the output FIFO (“passthrough”).
 *
 *  The goal of STEP 1 is to:
 *    • verify correct initialization of the SSM2603 via I²C
 *    • verify correct blocking acquisition via I²S
 *    • test IN → OUT loopback functionality
 *    • measure the actual audio sampling frequency used
 *      by the codec/I²S subsystem
 *
 *  Sampling Frequency Measurement
 *  ------------------------------------------------------------
 *  The ARM Cortex-A9 includes a 64-bit Global Timer clocked at:
 *
 *        CPU_Freq / 2  =  666 MHz / 2  ≈ 333 MHz
 *
 *  Only the lower 32 bits are used here, accessed through:
 *
 *        Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET)
 *
 *  The measurement procedure is:
 *    1) read timer at sample n      → t0
 *    2) read timer at sample n+1    → t1
 *    3) compute Δ = t1 - t0 (timer cycles)
 *    4) sampling period  Ts = Δ / 333 MHz
 *    5) sampling frequency Fs = 1 / Ts
 *
 *  IMPORTANT:
 *    The printing of t0/t1 results is performed *after* the
 *    measurement has completed, ensuring that UART I/O does not
 *    interfere with the timing, as required by the assignment.
 *
 *
 *  Platform
 *  ------------------------------------------------------------
 *   • Board: Zybo Z7 (Zynq-7000)
 *   • Audio Codec: Analog Devices SSM2603 (I²C control, I²S data)
 *   • Interfaces:
 *        – AXI-I2S for audio RX/TX
 *        – AXI FIFO for streaming samples
 *        – PS I²C for codec configuration
 *        – PS Global Timer for frequency measurement
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
#include "xil_io.h"

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

#define GTIMER_COUNTER_LOWER_OFFSET 0x04   // offset per il contatore basso
#define GLOBAL_TMR_FREQ 333000000          // Global Timer = CPU/2 = 333 MHz
/* ------------------------------------------------------------ */
/*				Low-Pass and High-Pass FIR filter coefficients									*/
/* ------------------------------------------------------------ */

#define coeffLP -1.692219e-02, 5.043750e-02,3.935835e-02,4.341238e-02,5.137933e-02,5.982048e-02,6.748827e-02, 7.379049e-02, 7.824605e-02, 8.052560e-02,8.052560e-02, 7.824605e-02, 7.379049e-02, 6.748827e-02,5.982048e-02,5.137933e-02,4.341238e-02,3.935835e-02,5.043750e-02,-0.0169221860
#define coeffHP -3.942071e-02,-5.929114e-03,1.430997e-02,4.069053e-02,5.762197e-02,4.843584e-02,4.349633e-03,-6.932913e-02,-1.527862e-01,-2.187328e-01,7.562085e-01,-2.187328e-01,-1.527862e-01,-6.932913e-02,4.349633e-03,4.843584e-02,5.762197e-02,4.069053e-02,1.430997e-02,-5.929114e-03,-0.0394207089
#define N_LP 20 // ordine filtro
#define N_HP 21 // ordine filtro

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
	    xil_printf("FIFO_ISR:  0x%08x\n",Xil_In32(fifoAddr + FIFO_ISR));
	    print("write FIFO_ISR\n\r");
	    Xil_Out32(fifoAddr + FIFO_ISR, 0xFFFFFFFF);
	    xil_printf("FIFO_ISR:  0x%08x\n",Xil_In32(fifoAddr + FIFO_ISR));
	    xil_printf("FIFO_IER:  0x%08x\n",Xil_In32(fifoAddr + FIFO_IER));
	    xil_printf("FIFO_TDFV: 0x%08x\n",Xil_In32(fifoAddr + FIFO_TDFV));
	    xil_printf("FIFO_RDFO: 0x%08x\n",Xil_In32(fifoAddr + FIFO_RDFO));

	    print("Write IER\n\r");
	    Xil_Out32(fifoAddr + FIFO_IER, 0x0C000000);

	    print("Write TDR\n\r");
	    Xil_Out32(fifoAddr + FIFO_TDR, 0x00000000);


	    xil_printf("FIFO_ISR:  0x%08x\n",Xil_In32(fifoAddr + FIFO_ISR));
		print("write FIFO_ISR\n\r");
		Xil_Out32(fifoAddr + FIFO_ISR, 0xFFFFFFFF);
		xil_printf("FIFO_ISR:  0x%08x\n",Xil_In32(fifoAddr + FIFO_ISR));
		xil_printf("FIFO_IER:  0x%08x\n",Xil_In32(fifoAddr + FIFO_IER));
		xil_printf("FIFO_TDFV: 0x%08x\n",Xil_In32(fifoAddr + FIFO_TDFV));
		xil_printf("FIFO_RDFO: 0x%08x\n",Xil_In32(fifoAddr + FIFO_RDFO));


	    print("write FIFO_IER\n");
	    Xil_Out32(fifoAddr + FIFO_IER, 0x04100000);
	    xil_printf("FIFO_ISR:  0x%08x\n",Xil_In32(fifoAddr + FIFO_ISR));
	    print("write FIFO_ISR\n");
	    Xil_Out32(fifoAddr + FIFO_ISR, 0x00100000);



}

int main()
{

    init_platform();

    print("Started!\n\r");
    xil_printf("\n=== LAB3 – Loopback + Timer ===\n");

    AudioInitialize(SCU_TIMER_ID, AUDIO_IIC_ID, AUDIO_CTRL_BASEADDR);

    initialize_FIFO(AUDIO_FIFO);






    int SampleL, SampleR;

    // ---- Variabili per la misura del sampling interval ----
    u32 t0 = 0;
    u32 t1 = 0;
    int captured = 0;

    xil_printf("Measuring sampling interval...\n");

    while (1) {

        // 1) Read stereo samples (L+R)
        SampleL = (int) I2SFifoRead(AUDIO_FIFO);
        SampleR = (int) I2SFifoRead(AUDIO_FIFO);

        // 2) Misura del periodo di campionamento (una sola volta)
        if (captured == 0) {
            t0 = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
            captured = 1;
        }
        else if (captured == 1) {
            t1 = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
            captured = 2;
        }

        // 3) Stampa una sola volta, senza interferire con la misura
        if (captured == 2) {
            captured = 3; // evita ripetizioni

            u32 delta = t1 - t0;
            float Ts = (float)delta / GLOBAL_TMR_FREQ;  // periodo in secondi
            float Fs = 1.0f / Ts;                       // frequenza di campionamento

            xil_printf("Delta cycles = %u\n", delta);
            xil_printf("Sampling frequency ≈ %.2f Hz\n", Fs);
        }

        // 4) Loopback
        I2SFifoWrite(AUDIO_FIFO, SampleL);
        I2SFifoWrite(AUDIO_FIFO, SampleR);
    }

    cleanup_platform();
    return 0;
}
