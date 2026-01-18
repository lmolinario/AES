/***************************************************************
 * Lab 5 – DNN on MNIST via UART (Software-only inference)
 *
 * Author: Lello Molinario
 * Course: Advanced Embedded Systems – University of Cagliari
 * Board: Zybo Z7 (Zynq-7000, ARM Cortex-A9)
 *
 * Description
 * ------------------------------------------------------------
 * This application implements a fully software-based Deep Neural
 * Network (DNN) for handwritten digit recognition (MNIST).
 *
 * The network is executed on the ARM Cortex-A9 Processing System
 * and receives input images via UART, encoded as fixed-point
 * signed 16-bit values (Q8.8).
 *
 * The code integrates and extends concepts developed in:
 *  - Lab 3: execution-time measurement (Global Timer / XTime)
 *  - Lab 4: parallel/serial computation structure
 *  - Lab 5: DNN inference pipeline and UART-based I/O
 *
 * The execution time is profiled at application level, separating:
 *  - UART reception latency
 *  - DNN inference time
 *  - Output / printing overhead
 *
 * All large buffers and tensors are statically allocated.
 * This avoids dynamic memory usage and guarantees:
 *  - predictable memory layout
 *  - no runtime allocation overhead
 *  - suitability for real-time embedded systems
 ***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "platform.h"
#include "xil_printf.h"
#include "xuartps.h"
#include "weights.h"
#include "test_images.h"
#include <xtime_l.h>
#include <time.h>

#include <math.h>

/* ============================================================
 * NETWORK TOPOLOGY PARAMETERS
 * ============================================================ */

/*
 * Custom fully-connected DNN:
 *   FC0: 784  -> 64
 *   FC1: 64   -> 32
 *   FC2: 32   -> 10
 */

#define n_bias0     64
#define n_weights0  (64 * 784)

#define n_bias1     32
#define n_weights1  (32 * 64)

#define n_bias2     10
#define n_weights2  (10 * 32)

/* Fixed-point data type (Q8.8) */
typedef short int DATA;


/* ============================================================
 * NETWORK PARAMETERS (WEIGHTS & BIASES)
 * ============================================================ */
DATA gemm0_bias[n_bias0] = {bias0};
DATA gemm0_weights[n_weights0] = {weights0} ;
DATA gemm1_bias[n_bias1] = {bias1};
DATA gemm1_weights[n_weights1] = {weights1};
DATA gemm2_bias[n_bias2] = {bias2};
DATA gemm2_weights[n_weights2] = {weights2};

/* ============================================================
 * FIXED-POINT UTILITIES
 * ============================================================ */

#define FIXED2FLOAT(a, qf) (((float) (a)) / (1<<qf))
#define FLOAT2FIXED(a, qf) ((short int) round((a) * (1<<qf)))

#define _MAX_ (1 << (sizeof(DATA)*8-1))-1
#define _MIN_ -(_MAX_+1)



/* ============================================================
 * DNN FUNCTION PROTOTYPES
 * ============================================================ */
void FC_forward(DATA* input, DATA* output, int in_s, int out_s, DATA* weights, DATA* bias, int qf) ;
static inline long long int saturate(long long int mac);
static inline void relu_forward(DATA* input, DATA* output, int size);
int resultsProcessing(DATA* results, int size);


/* ============================================================
 * OFFLINE TEST IMAGES (FUNCTIONAL VALIDATION)
 * ============================================================ */

/*
 * Ten reference MNIST images (digits 0–9) are embedded in the
 * program to validate the correctness of the DNN inference
 * before enabling UART-based input.
 */
DATA immagine[10][28*28] = {
 {imm_test_0},{imm_test_1},{imm_test_2},{imm_test_3},{imm_test_4},
 {imm_test_5},{imm_test_6},{imm_test_7},{imm_test_8},{imm_test_9}
};



/* ============================================================
 * UART HANDLING
 * ============================================================ */

/*
 * UART instance (PS UART1).
 * The UART is used to receive raw MNIST images encoded as
 * 16-bit signed fixed-point values (Q8.8).
 */
XUartPs Uart_1_PS;

/*
 * Blocking UART byte reception.
 * This function waits until exactly one byte is received.
 */
static inline u8 uart_inbyte(void) {
    u8 b;
    // Blocking read: waits indefinitely until 1 byte is received
    while (XUartPs_Recv(&Uart_1_PS, &b, 1) != 1);
    return b;
}


/*
 * Read one 16-bit signed sample from UART.
 * Byte order follows the assignment specification:
 *   - first byte: LSB
 *   - second byte: MSB
 */
DATA readfromUART() {
    u8 data1 = uart_inbyte();   // LSB
    u8 data2 = uart_inbyte();   // MSB
    return (DATA)((data2 << 8) | data1);
}


/* ============================================================
 * MAIN APPLICATION
 * ============================================================ */
int main(){
  init_platform();


  /* --------------------------------------------------------
   * UART INITIALIZATION
   * -------------------------------------------------------- */

  u16 DeviceId_1= XPAR_PS7_UART_1_DEVICE_ID;
  int Status_1;
  XUartPs_Config *Config_1;
  Config_1 = XUartPs_LookupConfig(DeviceId_1);
  if (NULL == Config_1) {
    return XST_FAILURE;
  }
  /*the default configuration is stored in Config and it can be used to initialize the controller */
  Status_1 = XUartPs_CfgInitialize(&Uart_1_PS, Config_1, Config_1->BaseAddress);
  if (Status_1 != XST_SUCCESS) {
    return XST_FAILURE;
  }
  // Set the BAUD rate
  u32 BaudRate = (u32)115200;
  Status_1 = XUartPs_SetBaudRate(&Uart_1_PS, BaudRate);
  if (Status_1 != (s32)XST_SUCCESS) {
    return XST_FAILURE;
  }


  //END UART SETUP
  xil_printf ("\n\rStarted\n\r");
  xil_printf("\n\r=== LAB5 === DNN on MNIST===\n\r");

/* --------------------------------------------------------
 * Tensor buffers between layers
 * -------------------------------------------------------- */

/* --------------------------------------------------------
 * TENSOR BUFFERS (STATIC ALLOCATION)
 * -------------------------------------------------------- */

/*
 * All intermediate tensors are declared static to:
 *  - avoid stack overflow
 *  - guarantee deterministic memory placement
 */

  static DATA output_gemm0[64];
  static DATA  input_gemm1[64];
  static DATA output_gemm1[32];
  static DATA  input_gemm2[32];
  static DATA output_gemm2[10];

  /* --------------------------------------------------------
  * OFFLINE FUNCTIONAL TEST (EMBEDDED IMAGES)
  * -------------------------------------------------------- */
    for (int i = 0; i < 10; i++) {

        xil_printf("\r\n===== Test image %d =====\r\n", i);


        /* FC0 + ReLU */
        FC_forward(immagine[i], output_gemm0,
                   784, 64,
                   gemm0_weights, gemm0_bias, 8);
        relu_forward(output_gemm0, input_gemm1, 64);

        /* FC1 + ReLU */
        FC_forward(input_gemm1, output_gemm1,
                   64, 32,
                   gemm1_weights, gemm1_bias, 8);
        relu_forward(output_gemm1, input_gemm2, 32);

        /* FC2 (output layer) */
        FC_forward(input_gemm2, output_gemm2,
                   32, 10,
                   gemm2_weights, gemm2_bias, 8);

        /* Classificazione */
        int pred = resultsProcessing(output_gemm2, 10);
        xil_printf("Expected = %d, Predicted = %d\r\n", i, pred);

    }


    /* --------------------------------------------------------
     * UART-BASED ONLINE INFERENCE + TIMING
     * -------------------------------------------------------- */


    static DATA image_uart[28*28];  // buffer immagine ricevuta via UART


    XTime t0, t1, t2, t3;
    double us_per_tick = 1e6 / (double)COUNTS_PER_SECOND;

    while (1) {

        xil_printf("\r\nWaiting for image...\r\n");



        XTime_GetTime(&t0);
        /*
         * UART reception is deliberately measured separately from
         * the DNN inference.
         *
         * UART latency is dominated by:
         *  - serial bandwidth (115200 baud)
         *  - host-side transmission (RealTerm / PC)
         *
         * Separating RX from computation allows fair evaluation of
         * the neural network execution time, independent of I/O.
         */

        /* ---------------- RX TIMING ---------------- */
        for (int i = 0; i < 784; i++)
            image_uart[i] = readfromUART();

        XTime_GetTime(&t1);


/*
         * DNN inference pipeline
         * ------------------------------------------------------------
         * The inference is executed sequentially on a single core,
         * but the code structure mirrors a stage-based pipeline
         * (FC → ReLU → FC → ReLU → FC).
         *
         * This organization is consistent with Lab 4 concepts and
         * allows future extensions such as:
         *  - manual parallelization
         *  - NEON vectorization
         *  - dual-core partitioning
         */

        /* ---------------- DNN INFERENCE ---------------- */
        FC_forward(image_uart, output_gemm0, 784, 64,
                   gemm0_weights, gemm0_bias, 8);
        relu_forward(output_gemm0, input_gemm1, 64);

        FC_forward(input_gemm1, output_gemm1, 64, 32,
                   gemm1_weights, gemm1_bias, 8);
        relu_forward(output_gemm1, input_gemm2, 32);

        FC_forward(input_gemm2, output_gemm2, 32, 10,
                   gemm2_weights, gemm2_bias, 8);

        XTime_GetTime(&t2);

        int pred = resultsProcessing(output_gemm2, 10);
        xil_printf("Predicted digit = %d\r\n", pred);

        XTime_GetTime(&t3);


        /*
         * Timing strategy
         * ------------------------------------------------------------
         * Execution time is measured using XTime_GetTime(), which
         * internally relies on the ARM Cortex-A9 Global Timer.
         *
         * Unlike Lab 3 (cycle-accurate FIR analysis), here the goal is
         * application-level profiling rather than worst-case cycle
         * estimation.
         *
         * The timing is therefore expressed in microseconds and
         * separated into:
         *  - UART reception latency (I/O bound)
         *  - DNN inference time (compute bound)
         *  - Output/printing overhead
         *
         * This approach provides a realistic performance characterization
         * of the complete embedded inference pipeline.
         */

        /* ---------------- TIMING REPORT ---------------- */
        u32 rx_us   = (u32)((t1 - t0) * us_per_tick);
        u32 dnn_us  = (u32)((t2 - t1) * us_per_tick);
        u32 prn_us  = (u32)((t3 - t2) * us_per_tick);
        u32 iter_us = (u32)((t3 - t0) * us_per_tick);

        xil_printf("\r\nTIMING us: RX=%lu DNN=%lu PRINT=%lu ITER=%lu\r\n\r\n",
                   rx_us, dnn_us, prn_us, iter_us);

    }


    // NOTE: never reached
    cleanup_platform();
    return 0;
}

/* ============================================================
 * FULLY CONNECTED LAYER
 * ============================================================ */
void FC_forward(DATA* input, DATA* output, int in_s, int out_s, DATA* weights, DATA* bias, int qf) {
	// NOTE return W * x
	int hkern = 0;
	int wkern = 0;
	long long int mac = 0;
	DATA current = 0;

	/* foreach row in kernel */
//	#pragma omp parallel for private (hkern, wkern, mac, current)
	for (hkern = 0; hkern < out_s; hkern++) {
		mac = ((long long int)bias[hkern]) << qf;
		for (wkern = 0; wkern < in_s; wkern++) {
			current = input[wkern];
			mac += current * weights[hkern*in_s + wkern];
		}
		output[hkern] = (DATA)(mac >> qf);
	}
}



static inline long long int saturate(long long int mac)
{

	if(mac > _MAX_) {
		printf("[WARNING] Saturation.mac: %lld -> %llx _MAX_: %d  _MIN_: %d  res: %d\n",  mac, mac, _MAX_, _MIN_, _MAX_);
		return _MAX_;
	}

	if(mac < _MIN_){
		printf( "[WARNING] Saturation. mac: %lld -> %llx _MAX_: %d  _MIN_: %d  res: %d\n",  mac, mac, _MAX_, _MIN_, _MIN_);
		return _MIN_;
	}

	//printf("mac: %lld -> %llx _MAX_: %lld  _MIN_: %lld  res: %lld\n", mac, mac, _MAX_, _MIN_, mac);
    return mac;

}

/* ============================================================
 * RELU ACTIVATION
 * ============================================================ */
static inline void relu_forward(DATA* input, DATA* output, int size) {
	int i = 0;
	for(i = 0; i < size; i++) {
		DATA v = input[i];
		v = v > 0 ? v : 0;
		output[i] = v;
	}
}

#define SIZEWA 10
int resultsProcessing(DATA* results, int size){
//What do you want to do with the results of the CNN? Here is the place where you should put the classifier or the detection (see YOLO detection for example)
//The simplest classifier is a maximum search for the results which returns the index value of the maximum

 char *labels[10]={"digit 0", "digit 1", "digit 2", "digit 3", "digit 4", "digit 5", "digit 6", "digit 7", "digit 8", "digit 9"};

// TODO: check the size parameter
  int size_wa = size; // Must be 10 ...
  if (size_wa > SIZEWA) size_wa = SIZEWA;

  float r[SIZEWA];
  int c[SIZEWA];
  float results_float[SIZEWA];
  float sum=0.0;
  DATA max=0;
  int max_i;
  for (int i =0;i<size_wa;i++){
      results_float[i] = FIXED2FLOAT(results[i],8);
    int n;
    if (results[i]>0)
      n=results[i];
    else
      n=-results[i];
    if (n>max){
      max=n;
      max_i=i;
    }
  }
  for (int i =0;i<size_wa;i++)
    sum+=exp(results_float[i]);

  for (int i =0;i<size_wa;i++){
    r[i]=exp(results_float[i]) / sum;
    c[i]=i;
  }
  for (int i =0;i<size_wa;i++){
    for (int j =i;j<size_wa;j++){
      if (r[j]>r[i]){
        float t= r[j];
        r[j]=r[i];
        r[i]=t;
        int tc= c[j];
        c[j]=c[i];
        c[i]=tc;
      }
    }
  }
  int top0=0;
  float topval=results_float[0];
  for (int i =1;i<size_wa;i++){
    if (results_float[i]>topval){
      top0=i;
      topval=results_float[i];
    }
  }
  //xil_printf("\n\n");
  for (int i =0;i<5;i++){
//	  xil_printf("            TOP %d: [%d] %s   \n",i, c[i], labels[c[i]]);
  }
  //xil_printf("max= %x \n",top0);
  return top0;
}
