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
 *        CPU_Freq / 2  =  667 MHz / 2  ≈ 333 MHz
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
 *    UART printing is performed only after t0/t1 are acquired,
 *    ensuring that serial I/O does not perturb the measurement,
 *    as required by the assignment.
 *
 *
 *  Platform
 *  ------------------------------------------------------------
 *   • Board: Zybo Z7 (Zynq-7000)
 *   • Audio Codec: Analog Devices SSM2603 (I²C control, I²S data)
 *   • Interfaces:
 *        – AXI-I2S for audio RX/TX
 *        – AXI FIFO for sample buffering
 *        – PS I²C for codec configuration
 *        – PS Global Timer for timing analysis
 *   • Tools: Xilinx SDK / Vitis + UART terminal (115200 baud, 8N1)
 *
 ***************************************************************/


#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xiicps.h"
#include <math.h>

/*
 * timer_ps.h
 *
 * This header provides access to the SCU Timer utility functions.
 *
 * In this application, the SCU Timer is used exclusively to generate
 * software delays during the initialization of the SSM2603 audio codec
 * (e.g. after reset and power-up sequences), as required by the device
 * timing specifications.
 * The SCU Timer is NOT used for sampling frequency or performance
 * measurements. All timing measurements related to audio sampling
 * frequency (Fs) and execution time are performed using the ARM
 * Cortex-A9 Global Timer, accessed through memory-mapped registers.
 */
#include "timer_ps.h"



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

#define FIR_FIFO XPAR_AXI_FIFO_MM_S_1_BASEADDR // FIR_FIFO not used in STEP 1


#define GLOBAL_TMR_BASEADDR          XPAR_PS7_GLOBALTIMER_0_S_AXI_BASEADDR

#define GTIMER_CONTROL_OFFSET        0x08   // Offset of the Global Timer control register (enable, auto-increment)
#define CPU_FREQ_HZ        667000000   // ARM Cortex-A9 CPU frequency
#define GLOBAL_TMR_FREQ    (CPU_FREQ_HZ / 2)   // Global Timer = CPU / 2

/* --------------------------------------------------------------------------
 *  GLOBAL TIMER – ENABLE & READOUT
 *
 *  The Cortex-A9 Global Timer is a 64-bit counter incremented at a clock
 *  running at half the CPU frequency (667 MHz / 2 ≈ 333 MHz).
 *
 *  To use it for sampling-frequency (Fs) measurement:
 *      • enable the timer            → bit0 = 1
 *      • enable auto-increment mode  → bit1 = 1
 *
 *  Control register value to enable both features:
 *                      0x03  (0000 0011b)
 *
 *  *** COUNTER READOUT ***
 *  The assignment explicitly requires reading the *lower 32 bits* of the
 *  64-bit counter. These are accessed using:
 *
 *      Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET)
 *
 *  IMPORTANT:
 *      Do NOT read from the timer base address, as it corresponds to the
 *      CONTROL register. The LSB counter is located at offset 0x00.
 * -------------------------------------------------------------------------- */
#define GTIMER_COUNTER_LOWER_OFFSET  0x00   // Offset of the 32-bit LSB counter register



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


/* --------------------------------------------------------------------------
 *  BLOCKING I²S READ
 *
 *  The lab requires verifying that I²S acquisition is blocking:
 *  the function waits until the RX FIFO contains data.
 *
 *  This ensures synchronous sample-by-sample acquisition during loopback.
 * -------------------------------------------------------------------------- */
u32 I2SFifoRead (u32 i2sBaseAddr)
{

	while (Xil_In32(i2sBaseAddr + 0x1C)==0){;} // waits for a sample in the FIFO
	int data = Xil_In32(i2sBaseAddr + 0x20);   // read the sample from the FIFO
return data;

}


// FIR FIFO initialized not used in STEP 1 kept for Step 2–3

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


/* --------------------------------------------------------------------------
 *   LOOPBACK TEST
 *
 *   Tested audio pipeline:
 *        ADC → I²S RX → CPU → I²S TX → DAC
 *
 *   The CPU reads each incoming stereo sample and immediately forwards it
 *   to the I²S TX path. This implements a real-time IN → OUT passthrough.
 *
 *   Successful loopback confirms:
 *      • correct initialization of the SSM2603 codec via I²C
 *      • correct blocking acquisition through the I²S RX FIFO
 *      • proper functionality of the complete audio chain
 *        (ADC → CPU → DAC)
 * -------------------------------------------------------------------------- */

int main()
{

	init_platform();

	print("Started!\n\r");
    xil_printf("=== LAB 3 === Step 1: Fs Measurement (Global Timer) + LOOPBACK ===\n\r");

    // Initialize the audio codec (I²C), the I²S interface, and clocking
	AudioInitialize(SCU_TIMER_ID, AUDIO_IIC_ID, AUDIO_CTRL_BASEADDR);

    // Initialize and clear the AXI FIFO used for audio streaming
	initialize_FIFO(AUDIO_FIFO);


    /* ----------------------------------------------------------------------
     *  ENABLE GLOBAL TIMER
     *
     *  Required by the assignment:
     *      - the timer must be explicitly enabled
     *      - auto-increment mode must be activated
     *
     *  → CONTROL register value = 0x03
     *        bit0 = 1 → enable timer
     *        bit1 = 1 → enable auto-increment
     *
     *  Without enabling the timer, the counter remains at zero and the
     *  sampling-frequency measurement (Fs) would always return 0 Hz.
     * ---------------------------------------------------------------------- */

    /* Write 0x03 to the Global Timer control register (enable + auto-inc) */
    Xil_Out32(GLOBAL_TMR_BASEADDR + GTIMER_CONTROL_OFFSET, 0x03);

    u32 t0 = 0, t1 = 0;   // timer snapshots
    int j = 0;            // loop counter for delayed measurement
    #define N_SAMPLES  100        // number of samples used for Fs averaging

    u64 accum_ticks = 0;          // accumulator for timer deltas
    int sample_cnt = 0;           // number of measured deltas


    /* --------------------------------------------------------------------------
     *  STEP 1 – SIMPLE LOOPBACK TEST (IN → OUT)
     *
     *  As required by the lab:
     *      - acquire one stereo sample (L, R)
     *      - immediately forward it to the TX FIFO
     *
     *  This verifies the correct end-to-end behavior of the audio chain:
     *      ADC → I²S RX → CPU → I²S TX → DAC
     *
     *  This is the minimum working configuration needed to validate
     *  proper audio hardware operation.
     * -------------------------------------------------------------------------- */

    while (1)
    {
        /* ----------------------------------------------------------------------
         *  AUDIO ACQUISITION (BLOCKING)
         *
         *  One stereo audio frame (Left + Right) is synchronously acquired
         *  from the I²S RX FIFO. The blocking behavior guarantees that
         *  samples are processed at the exact rate imposed by the codec,
         *  which is a necessary condition for reliable timing measurements
         *  and real-time audio loopback.
         * ---------------------------------------------------------------------- */
        u32 L = I2SFifoRead(AUDIO_FIFO);
        u32 R = I2SFifoRead(AUDIO_FIFO);

        /* ----------------------------------------------------------------------
         *  SAMPLING FREQUENCY MEASUREMENT (Fs)
         *
         *  The sampling period is measured using the ARM Cortex-A9 Global
         *  Timer by computing the elapsed timer ticks between two consecutive
         *  audio frames. To improve measurement robustness and reduce jitter,
         *  the measurement is averaged over N_SAMPLES consecutive periods.
         *
         *  A delay of j >= 300 iterations is introduced to ensure that:
         *    • the audio pipeline is fully initialized
         *    • transient effects at startup are avoided
         * ---------------------------------------------------------------------- */
        if (j >= 300 && sample_cnt < N_SAMPLES)
        {
            /* Timer snapshot at the beginning of one sampling period */
            t0 = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);

            /* ------------------------------------------------------------------
             *  Acquisition of the next stereo frame.
             *
             *  The time elapsed between t0 and t1 corresponds to exactly one
             *  sampling period Ts imposed by the I²S/codec subsystem.
             * ------------------------------------------------------------------ */
            u32 L_tmp = I2SFifoRead(AUDIO_FIFO);
            u32 R_tmp = I2SFifoRead(AUDIO_FIFO);

            /* Timer snapshot at the end of the sampling period */
            t1 = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);

            /* Accumulate elapsed timer ticks for averaging */
            accum_ticks += (t1 - t0);
            sample_cnt++;

            /* Forward the additionally acquired samples to preserve continuity
             * of the real-time audio stream during the measurement phase */
            I2SFifoWrite(AUDIO_FIFO, L_tmp);
            I2SFifoWrite(AUDIO_FIFO, R_tmp);
        }

        /* ----------------------------------------------------------------------
         *  FINAL Fs COMPUTATION AND REPORTING
         *
         *  The average sampling period is computed as:
         *
         *      avg_delta = accum_ticks / N_SAMPLES
         *
         *  Since the Global Timer runs at CPU_FREQUENCY / 2, the sampling
         *  frequency is obtained as:
         *
         *      Fs = GLOBAL_TMR_FREQ / avg_delta
         *
         *  UART output is performed only once, outside the timing-critical
         *  measurement window, to avoid perturbing the results.
         * ---------------------------------------------------------------------- */
        if (sample_cnt == N_SAMPLES)
        {
            float avg_delta = (float)accum_ticks / (float)N_SAMPLES;
            float Fs = (float)GLOBAL_TMR_FREQ / avg_delta;

            int Fs100 = (int)(Fs * 100);
            xil_printf("Fs (avg over %d samples) ~= %d.%02d Hz\n\r",
                       N_SAMPLES,
                       Fs100 / 100,
                       Fs100 % 100);

            /* Prevent further printing */
            sample_cnt++;
        }

        /* ----------------------------------------------------------------------
         *  REAL-TIME AUDIO LOOPBACK
         *
         *  The originally acquired stereo sample is immediately forwarded
         *  to the I²S TX FIFO, implementing a direct IN → OUT passthrough.
         *  This confirms correct end-to-end operation of the audio chain:
         *
         *      ADC → I²S RX → CPU → I²S TX → DAC
         * ---------------------------------------------------------------------- */
        I2SFifoWrite(AUDIO_FIFO, L);
        I2SFifoWrite(AUDIO_FIFO, R);

        /* Loop iteration counter */
        j++;
    }

    cleanup_platform();
    return 0;
}
