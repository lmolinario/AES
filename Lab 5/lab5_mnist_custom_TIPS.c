/***************************************************************
 *  Lab 5 – DNN on MNIST (Custom Network with Tips – Group 4)
 *  ------------------------------------------------------------
 *  File: main_custom.c
 *
 *  Author: Lello Molinario
 *  University of Cagliari – Advanced Embedded Systems (AES)
 *  Academic Year: 2025–2026
 *
 *
 *  Network Topology (Custom – Group 4):
 *    • FC0: 784 → 64
 *    • FC1: 64  → 32
 *    • FC2: 32  → 16
 *    • FC3: 16  → 10
 *
 *  Data Representation:
 *    • Weights, bias, activations: DATA = int16 (fixed-point)
 *    • Baseline format: Q8.8
 *    • Tip 1: input images in Q0.8 (1 byte) reconstructed on-board
 *    • Tip 2: weights/bias in Q1.7 for improved dynamic range
 *
 *  UART Protocol:
 *    1) Startup (optional offline loading):
 *       - FC0 weights (64×784), bias (64)
 *       - FC1 weights (32×64),  bias (32)
 *       - FC2 weights (16×32),  bias (16)
 *       - FC3 weights (10×16),  bias (10)
 *
 *    2) Runtime loop:
 *       - PC sends one 28×28 image (784 samples)
 *       - Board executes DNN inference
 *       - Board returns RESULT=<digit> over UART
 *
 *  Measurement:
 *    • Cycle-level timing via ARM Global Timer
 *    • Per-layer profiling (FC + ReLU)
 *    • Compute-only throughput evaluation
 *
 ***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

#include "platform.h"
#include "xil_printf.h"
#include "xuartps.h"
#include "xtime_l.h"
#include "time.h"

#include "test_images.h"
#include "weights_group4.h"


/* ============================================================
 * NETWORK DIMENSIONS
 * ============================================================ */

#define n_bias0 64
#define n_weights0 50176   // 784*64

#define n_bias1 32
#define n_weights1 2048    // 64*32

#define n_bias2 16
#define n_weights2 512     // 32*16

#define n_bias3 10
#define n_weights3 160     // 16*10


/* ============================================================
 * CONFIGURATION FLAGS (TIPS)
 * ============================================================ */
/* Tip 1: receive input images in Q0.8 (1 byte) */

#define USE_Q0_8_INPUT   0   // 0 = baseline Q8.8, 1 = Tip 1

/* Tip 2: use weights/bias in Q1.7 instead of Q8.8 */
#define USE_Q1_7_WEIGHTS  1   // 0 = baseline, 1 = Tip 2

#if USE_Q1_7_WEIGHTS
    #define QF_WEIGHTS 7
#else
    #define QF_WEIGHTS 8
#endif

#define QF_INPUT 8   // input always reconstructed to Q8.8

/* Fixed-point data type */
typedef short int DATA;

/* ============================================================
 * WEIGHTS AND BIASES SELECTION
 * ============================================================ */
#if USE_Q1_7_WEIGHTS
DATA gemm0_bias[n_bias0]    = {bias0_q17};
DATA gemm0_weights[n_weights0] = {weights0_q17};

DATA gemm1_bias[n_bias1]    = {bias1_q17};
DATA gemm1_weights[n_weights1] = {weights1_q17};

DATA gemm2_bias[n_bias2]    = {bias2_q17};
DATA gemm2_weights[n_weights2] = {weights2_q17};

DATA gemm3_bias[n_bias3]    = {bias3_q17};
DATA gemm3_weights[n_weights3] = {weights3_q17};
#else
DATA gemm0_bias[n_bias0]    = {bias0};
DATA gemm0_weights[n_weights0] = {weights0};

DATA gemm1_bias[n_bias1]    = {bias1};
DATA gemm1_weights[n_weights1] = {weights1};

DATA gemm2_bias[n_bias2]    = {bias2};
DATA gemm2_weights[n_weights2] = {weights2};

DATA gemm3_bias[n_bias3]    = {bias3};
DATA gemm3_weights[n_weights3] = {weights3};
#endif



/* ============================================================
 * FIXED-POINT UTILITIES
 * ============================================================ */
#define FIXED2FLOAT(a, qf) (((float) (a)) / (1<<qf))
#define FLOAT2FIXED(a, qf) ((short int) round((a) * (1<<qf)))


#define _MAX_ INT16_MAX
#define _MIN_ INT16_MIN



static inline u32 ticks_to_us(u64 ticks)
{
    // COUNTS_PER_SECOND è definita in xtime_l.h
    return (u32)((ticks * 1000000ULL) / (u64)COUNTS_PER_SECOND);
}



/* ============================================================
 * DNN FUNCTION PROTOTYPES
 * ============================================================ */

void FC_forward(DATA* input, DATA* output, int in_s, int out_s, DATA* weights, DATA* bias, int qf) ;
static inline long long int saturate(long long int mac);
static inline void relu_forward(DATA* input, DATA* output, int size);
int resultsProcessing(DATA* results, int size);


/* ============================================================
 * UART IMAGE RECEPTION
 * ============================================================ */
DATA readfromUART(){ // reads 2 bytes and composes a short int
    u8 lo = XUartPs_RecvByte(XPAR_PS7_UART_1_BASEADDR); // first byte (LSB)
    u8 hi = XUartPs_RecvByte(XPAR_PS7_UART_1_BASEADDR); // second byte (MSB)
    return (DATA)((hi << 8) | lo);
}


#define SYNC_BYTE 0xA5


/* Receive a full MNIST image (784 pixels) */
void receive_image(DATA *buffer) {



#if USE_Q0_8_INPUT
    /* Tip 1: Q0.8 input → reconstructed to Q8.8 */
    for (int i = 0; i < 784; i++) {
        u8 v = XUartPs_RecvByte(XPAR_PS7_UART_1_BASEADDR);
        buffer[i] = ((DATA)v) << 8;   // Q0.8 → Q8.8

    }
#else
    /* Baseline: direct Q8.8 reception */
    for (int i = 0; i < 784; i++) {
        buffer[i] = readfromUART();
    }
#endif
}


/* Static test images for functional validation */
DATA immagine[10][28*28] = {{imm_test_9},{imm_test_8},{imm_test_7},{imm_test_6},{imm_test_5},{imm_test_4},{imm_test_3},{imm_test_2},{imm_test_1},{imm_test_0}};

/* ============================================================
 * MAIN APPLICATION
 * ============================================================ */
int main(){
  init_platform();


    /* --------------------------------------------------------
     * UART INITIALIZATION
     * -------------------------------------------------------- */
  XUartPs Uart_1_PS;
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
  xil_printf ("Started\n");
  xil_printf("\n=== LAB5 === DNN on MNIST===\n\r");
  xil_printf("\n=== Custom with Tips ===\n\r");

  xil_printf("\n--- CONFIGURATION ---\n\r");

  #if USE_Q0_8_INPUT
  xil_printf("Input format      : Q0.8 (Tip 1 ENABLED)\n\r");
  #else
  xil_printf("Input format      : Q8.8 (Baseline)\n\r");
  #endif

  #if USE_Q1_7_WEIGHTS
  xil_printf("Weights/Bias      : Q1.7 (Tip 2 ENABLED)\n\r");
  #else
  xil_printf("Weights/Bias      : Q8.8 (Baseline)\n\r");
  #endif

  xil_printf("---------------------\n\r\n");



    /* --------------------------------------------------------
     * Tensor buffers between layers
     * -------------------------------------------------------- */

  DATA image[28*28];  // buffer immagine ricevuta via UART

  DATA output_gemm0[64];
  DATA input_gemm1[64];

  DATA output_gemm1[32];
  DATA input_gemm2[32];

  DATA output_gemm2[16];
  DATA input_gemm3[16];

  DATA output_gemm3[10];


    /* ========================================================
     * STEP 1 – Functional validation with static images
     * ======================================================== */
  for (int i = 0; i < 10; i++) {
      xil_printf("\r\n===== Test image %d =====\r\n", 9 - i);

      // Layer 0: FC 784 → 64
      FC_forward(
          immagine[i],
          output_gemm0,
          784,
          64,
          gemm0_weights,
          gemm0_bias,
          QF_WEIGHTS
      );

      // ReLU 0
      relu_forward(output_gemm0, input_gemm1, 64);

      // Layer 1: FC 64 → 32
      FC_forward(
          input_gemm1,
          output_gemm1,
          64,
          32,
          gemm1_weights,
          gemm1_bias,
          QF_WEIGHTS
      );

      // ReLU 1
      relu_forward(output_gemm1, input_gemm2, 32);

      // Layer 2: FC 32 → 16
      FC_forward(
          input_gemm2,
          output_gemm2,
          32,
          16,
          gemm2_weights,
          gemm2_bias,
          QF_WEIGHTS
      );

      // ReLU 2
      relu_forward(output_gemm2, input_gemm3, 16);


      // Layer 3: FC 16 → 10
      FC_forward(
          input_gemm3,
          output_gemm3,
          16,
          10,
          gemm3_weights,
          gemm3_bias,
          QF_WEIGHTS
      );

      // Classificazione finale
      int predicted = resultsProcessing(output_gemm3, 10);
      xil_printf("Predicted digit = %d\r\n", predicted);
  }


    /* ========================================================
     * STEP 2–3–4 – Real-time inference with profiling
     * ======================================================== */
  while (1) {

      xil_printf("\nWaiting for the image...\r\n");

      XTime t_iter0, t_iter1;
      XTime t_rx0, t_rx1;
      XTime t_fc0_0, t_fc0_1;
      XTime t_relu0_0, t_relu0_1;
      XTime t_fc1_0, t_fc1_1;
      XTime t_relu1_0, t_relu1_1;
      XTime t_fc2_0, t_fc2_1;
      XTime t_fc3_0, t_fc3_1;
      XTime t_relu2_0, t_relu2_1;
      XTime t_cls0, t_cls1;

      XTime_GetTime(&t_iter0);

      // (1) RX image (blocking)
      XTime_GetTime(&t_rx0);
      receive_image(image);
      XTime_GetTime(&t_rx1);

      // === DEBUG: verify received image ===
      //xil_printf("First 10 pixels: ");
      //for (int i = 0; i < 10; i++) {
      //    xil_printf("%d ", image[i]);
      //}
      xil_printf("\r\n");

        /* Discard first iteration (warm-up) */
      static int warmup = 1;
      if (warmup) {
          warmup = 0;
          xil_printf("Warm-up image discarded\r\n");
          continue;
      }



        /* Forward pass */
      // (2) FC0
      XTime_GetTime(&t_fc0_0);
      FC_forward(image, output_gemm0, 784, 64, gemm0_weights, gemm0_bias, QF_WEIGHTS);
      XTime_GetTime(&t_fc0_1);

      // (3) ReLU0
      XTime_GetTime(&t_relu0_0);
      relu_forward(output_gemm0, input_gemm1, 64);
      XTime_GetTime(&t_relu0_1);

      // (4) FC1
      XTime_GetTime(&t_fc1_0);
      FC_forward(input_gemm1, output_gemm1, 64, 32, gemm1_weights, gemm1_bias, QF_WEIGHTS);
      XTime_GetTime(&t_fc1_1);

      // (5) ReLU1
      XTime_GetTime(&t_relu1_0);
      relu_forward(output_gemm1, input_gemm2, 32);
      XTime_GetTime(&t_relu1_1);

      // (6) FC2: 32 → 16
      XTime_GetTime(&t_fc2_0);
      FC_forward(input_gemm2, output_gemm2, 32, 16, gemm2_weights, gemm2_bias, QF_WEIGHTS);
      XTime_GetTime(&t_fc2_1);

      // (7) ReLU2: 16
      XTime_GetTime(&t_relu2_0);
      relu_forward(output_gemm2, input_gemm3, 16);
      XTime_GetTime(&t_relu2_1);

      // (8) FC3: 16 → 10
      XTime_GetTime(&t_fc3_0);
      FC_forward(input_gemm3, output_gemm3, 16, 10, gemm3_weights, gemm3_bias, QF_WEIGHTS);
      XTime_GetTime(&t_fc3_1);

      // (9) Classification (softmax + argmax)
      XTime_GetTime(&t_cls0);
      int predicted = resultsProcessing(output_gemm3, 10);
      XTime_GetTime(&t_cls1);


      XTime_GetTime(&t_iter1);

      // Compute deltas (ticks)
      u64 rx_t  = (u64)(t_rx1   - t_rx0);
      u64 fc0_t = (u64)(t_fc0_1 - t_fc0_0);
      u64 r0_t  = (u64)(t_relu0_1 - t_relu0_0);
      u64 fc1_t = (u64)(t_fc1_1 - t_fc1_0);
      u64 r1_t  = (u64)(t_relu1_1 - t_relu1_0);
      u64 fc2_t = (u64)(t_fc2_1 - t_fc2_0);
      u64 cls_t = (u64)(t_cls1 - t_cls0);
      u64 tot_t = (u64)(t_iter1 - t_iter0);
      u64 fc3_t = (u64)(t_fc3_1 - t_fc3_0);
      u64 r2_t  = (u64)(t_relu2_1 - t_relu2_0);

      u64 dnn_t = fc0_t + r0_t + fc1_t + r1_t + fc2_t + r2_t + fc3_t + cls_t;



      // Conversion in microsecond (u32) per "safe" print
      u32 rx_us  = ticks_to_us(rx_t);
      u32 fc0_us = ticks_to_us(fc0_t);
      u32 r0_us  = ticks_to_us(r0_t);
      u32 fc1_us = ticks_to_us(fc1_t);
      u32 r1_us  = ticks_to_us(r1_t);
      u32 fc2_us = ticks_to_us(fc2_t);
      u32 cls_us = ticks_to_us(cls_t);
      u32 dnn_us = ticks_to_us(dnn_t);
      u32 tot_us = ticks_to_us(tot_t);
      u32 fc3_us = ticks_to_us(fc3_t);
      u32 r2_us  = ticks_to_us(r2_t);

      // Only xil_printf, only 32-bit
      xil_printf(
        "RX=%u us | FC0=%u us | R0=%u us | FC1=%u us | R1=%u us | FC2=%u us | R2=%u us | FC3=%u us | CLS=%u us | DNN=%u us | TOT=%u us\r\n",
        rx_us, fc0_us, r0_us, fc1_us, r1_us, fc2_us, r2_us, fc3_us, cls_us, dnn_us, tot_us
      );

      xil_printf("Predicted digit = %d\r\n", predicted);

      static int printed_summary = 0;
      if (!printed_summary) {
    	  xil_printf("Compute-only throughput = %u img/s (compute-only)\r\n",
    	             1000000 / dnn_us);

          xil_printf("Real-time capability: YES (DNN << UART)\r\n");
          printed_summary = 1;
      }

  }


    cleanup_platform();
    return 0;
}

/* ============================================================
 * FULLY CONNECTED LAYER (Fixed-Point)
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
		output[hkern] = (DATA)saturate(mac >> qf);//output[hkern] = (DATA)(mac >> qf);
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
  int size_wa = SIZEWA;
  float  r[SIZEWA];
  int  c[SIZEWA];
  float results_float[SIZEWA];
  float sum=0.0;
  DATA max=0;
  int max_i;
  for (int i =0;i<size_wa;i++){
      results_float[i] = FIXED2FLOAT(results[i],QF_WEIGHTS);
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
