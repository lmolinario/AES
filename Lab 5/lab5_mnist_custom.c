/***************************************************************
 *  Lab 5 – DNN on MNIST (Custom Network – Group 4)
 *  Author: Lello Molinario
 *  Board: Zybo Z7 – PS7 (ARM Cortex-A9)
 *
 *  Topologia rete custom (gruppo 4):
 *      FC0: 784  -> 64
 *      FC1: 64   -> 32
 *      FC2: 32   -> 16
 *      FC3: 16   -> 10
 *
 *  Formato dati:
 *      - PesI, bias, attivazioni: DATA = short int (16 bit), Q8.8
 *      - Ordine byte su UART: little-endian (LSB prima, poi MSB)
 *
 *  Protocollo:
 *      1) All'avvio:
 *          - il PC invia, in questo ordine:
 *              FC0 weights (64*784)
 *              FC0 bias    (64)
 *              FC1 weights (32*64)
 *              FC1 bias    (32)
 *              FC2 weights (16*32)
 *              FC2 bias    (16)
 *              FC3 weights (10*16)
 *              FC3 bias    (10)
 *      2) Ciclo:
 *          - il PC invia un'immagine 28x28 (784 campioni Q8.8)
 *          - la board esegue la DNN
 *          - invia in UART: RESULT=<digit>
 ***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "platform.h"
#include "xil_printf.h"
#include "xuartps.h"
#include <xtime_l.h>

#include <math.h>

// -----------------------------
// Topologia rete custom (gruppo 4)
// -----------------------------
// 784 -> 64 -> 32 -> 16 -> 10
#define FC0_IN   784
#define FC0_OUT  64

#define FC1_IN   64
#define FC1_OUT  32

#define FC2_IN   32
#define FC2_OUT  16

#define FC3_IN   16
#define FC3_OUT  10

typedef short int DATA;

// -----------------------------
// Fixed-point helpers (Q-format)
// -----------------------------
#define FIXED2FLOAT(a, qf) (((float) (a)) / (1<<qf))
#define FLOAT2FIXED(a, qf) ((short int) round((a) * (1<<qf)))

#define _MAX_ (1 << (sizeof(DATA)*8-1))-1
#define _MIN_ -(_MAX_+1)

// -----------------------------
// Pesi e bias (globali, caricati via UART)
// -----------------------------
DATA wFC0[FC0_OUT * FC0_IN];
DATA bFC0[FC0_OUT];

DATA wFC1[FC1_OUT * FC1_IN];
DATA bFC1[FC1_OUT];

DATA wFC2[FC2_OUT * FC2_IN];
DATA bFC2[FC2_OUT];

DATA wFC3[FC3_OUT * FC3_IN];
DATA bFC3[FC3_OUT];

// -----------------------------
// Prototipi DNN
// -----------------------------
void FC_forward(DATA* input, DATA* output, int in_s, int out_s,
                DATA* weights, DATA* bias, int qf);
static inline long long int saturate(long long int mac);
static inline void relu_forward(DATA* input, DATA* output, int size);
int resultsProcessing(DATA* results, int size);

// -----------------------------
// UART helpers
// -----------------------------

// Legge un DATA (16 bit, Q8.8) dalla UART in formato little-endian
DATA readDATA(void) {
    u8 lo = XUartPs_RecvByte(XPAR_PS7_UART_1_BASEADDR); // LSB
    u8 hi = XUartPs_RecvByte(XPAR_PS7_UART_1_BASEADDR); // MSB
    return (DATA)((hi << 8) | lo);
}

// Legge n valori DATA consecutivi dalla UART
void readArray(DATA *buffer, int n) {
    for (int i = 0; i < n; i++) {
        buffer[i] = readDATA();
    }
}

// Legge un'immagine 28x28 (784 pixel) in formato Q8.8
void receive_image(DATA *buffer) {
    readArray(buffer, FC0_IN);  // FC0_IN = 784
}

// =======================================================
// main()
// =======================================================
int main() {
    init_platform();

    // UART setup
    XUartPs Uart_1_PS;
    u16 DeviceId_1 = XPAR_PS7_UART_1_DEVICE_ID;
    int Status_1;
    XUartPs_Config *Config_1;

    Config_1 = XUartPs_LookupConfig(DeviceId_1);
    if (NULL == Config_1) {
        return XST_FAILURE;
    }

    // inizializza UART
    Status_1 = XUartPs_CfgInitialize(&Uart_1_PS,
                                     Config_1,
                                     Config_1->BaseAddress);
    if (Status_1 != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // Baud rate = 115200
    u32 BaudRate = (u32)115200;
    Status_1 = XUartPs_SetBaudRate(&Uart_1_PS, BaudRate);
    if (Status_1 != (s32)XST_SUCCESS) {
        return XST_FAILURE;
    }

    xil_printf("Lab5 – MNIST Custom DNN (Group 4)\r\n");
    xil_printf("UART ready @ 115200 baud.\r\n");

    // =======================================================
    // Caricamento pesi e bias della rete custom (una sola volta)
    // =======================================================
    xil_printf("Loading custom weights and biases...\r\n");

    // FC0: 784 -> 64
    xil_printf("Waiting for FC0 weights...\r\n");
    readArray(wFC0, FC0_OUT * FC0_IN);     // 64 * 784
    xil_printf("Waiting for FC0 biases...\r\n");
    readArray(bFC0, FC0_OUT);              // 64

    // FC1: 64 -> 32
    xil_printf("Waiting for FC1 weights...\r\n");
    readArray(wFC1, FC1_OUT * FC1_IN);     // 32 * 64
    xil_printf("Waiting for FC1 biases...\r\n");
    readArray(bFC1, FC1_OUT);              // 32

    // FC2: 32 -> 16
    xil_printf("Waiting for FC2 weights...\r\n");
    readArray(wFC2, FC2_OUT * FC2_IN);     // 16 * 32
    xil_printf("Waiting for FC2 biases...\r\n");
    readArray(bFC2, FC2_OUT);              // 16

    // FC3: 16 -> 10
    xil_printf("Waiting for FC3 weights...\r\n");
    readArray(wFC3, FC3_OUT * FC3_IN);     // 10 * 16
    xil_printf("Waiting for FC3 biases...\r\n");
    readArray(bFC3, FC3_OUT);              // 10

    xil_printf("All weights and biases loaded.\r\n");

    // =======================================================
    // Buffer per input e attivazioni
    // =======================================================
    DATA image[FC0_IN];   // 784
    DATA act0[FC0_OUT];   // 64
    DATA act1[FC1_OUT];   // 32
    DATA act2[FC2_OUT];   // 16
    DATA act3[FC3_OUT];   // 10 (logits finali)

    // =======================================================
    // STEP 2–3–4 – Ciclo continuo di classificazione via UART
    // =======================================================
    while (1) {

        xil_printf("\nWaiting for the image...\r\n");

        XTime t_start_total, t_end_total;
        XTime_GetTime(&t_start_total);

        // (1) Ricezione immagine
        XTime t0, t1;
        XTime_GetTime(&t0);

        receive_image(image);

        XTime_GetTime(&t1);
        xil_printf("Time RX image      = %llu cycles\r\n", (t1 - t0));

        // (2) FC0: 784 -> 64
        XTime_GetTime(&t0);
        FC_forward(image, act0, FC0_IN, FC0_OUT, wFC0, bFC0, 8);
        XTime_GetTime(&t1);
        xil_printf("Time FC0           = %llu cycles\r\n", (t1 - t0));

        // (3) ReLU0
        XTime_GetTime(&t0);
        relu_forward(act0, act0, FC0_OUT);   // in-place
        XTime_GetTime(&t1);
        xil_printf("Time ReLU0         = %llu cycles\r\n", (t1 - t0));

        // (4) FC1: 64 -> 32
        XTime_GetTime(&t0);
        FC_forward(act0, act1, FC1_IN, FC1_OUT, wFC1, bFC1, 8);
        XTime_GetTime(&t1);
        xil_printf("Time FC1           = %llu cycles\r\n", (t1 - t0));

        // (5) ReLU1
        XTime_GetTime(&t0);
        relu_forward(act1, act1, FC1_OUT);   // in-place
        XTime_GetTime(&t1);
        xil_printf("Time ReLU1         = %llu cycles\r\n", (t1 - t0));

        // (6) FC2: 32 -> 16
        XTime_GetTime(&t0);
        FC_forward(act1, act2, FC2_IN, FC2_OUT, wFC2, bFC2, 8);
        XTime_GetTime(&t1);
        xil_printf("Time FC2           = %llu cycles\r\n", (t1 - t0));

        // (7) ReLU2
        XTime_GetTime(&t0);
        relu_forward(act2, act2, FC2_OUT);   // in-place
        XTime_GetTime(&t1);
        xil_printf("Time ReLU2         = %llu cycles\r\n", (t1 - t0));

        // (8) FC3: 16 -> 10
        XTime_GetTime(&t0);
        FC_forward(act2, act3, FC3_IN, FC3_OUT, wFC3, bFC3, 8);
        XTime_GetTime(&t1);
        xil_printf("Time FC3           = %llu cycles\r\n", (t1 - t0));

        // (9) Classificazione finale
        XTime_GetTime(&t0);
        int predicted = resultsProcessing(act3, 10);
        XTime_GetTime(&t1);
        xil_printf("Time classification = %llu cycles\r\n", (t1 - t0));

        // Tempo totale iterazione
        XTime_GetTime(&t_end_total);
        xil_printf("TOTAL iteration     = %llu cycles\r\n",
                   (t_end_total - t_start_total));

        xil_printf("RESULT=%d\r\n", predicted);
        fflush(stdout);
    }

    // In pratica non verrà mai eseguito
    cleanup_platform();
    return 0;
}

// =======================================================
// Implementazione funzioni DNN
// =======================================================

void FC_forward(DATA* input, DATA* output, int in_s, int out_s,
                DATA* weights, DATA* bias, int qf) {

    int hkern = 0;
    int wkern = 0;
    long long int mac = 0;
    DATA current = 0;

    for (hkern = 0; hkern < out_s; hkern++) {
        mac = ((long long int)bias[hkern]) << qf;

        for (wkern = 0; wkern < in_s; wkern++) {
            current = input[wkern];
            mac += current * weights[hkern * in_s + wkern];
        }

        // opzionale: mac = saturate(mac);
        output[hkern] = (DATA)(mac >> qf);
    }
}

static inline long long int saturate(long long int mac) {

    if (mac > _MAX_) {
        // opzionale: messaggio di warning
        return _MAX_;
    }

    if (mac < _MIN_) {
        return _MIN_;
    }

    return mac;
}

static inline void relu_forward(DATA* input, DATA* output, int size) {
    int i = 0;
    for (i = 0; i < size; i++) {
        DATA v = input[i];
        v = (v > 0) ? v : 0;
        output[i] = v;
    }
}

// =======================================================
// Post-processing: classificazione (softmax + argmax)
// =======================================================
#define SIZEWA 10

int resultsProcessing(DATA* results, int size) {
    // Semplice classificatore: softmax + top-1
    char *labels[10] = {
        "digit 0", "digit 1", "digit 2", "digit 3", "digit 4",
        "digit 5", "digit 6", "digit 7", "digit 8", "digit 9"
    };

    int size_wa = SIZEWA;
    float  r[SIZEWA];
    int    c[SIZEWA];
    float  results_float[SIZEWA];
    float  sum = 0.0f;

    // Converti da Q8.8 a float e trova max assoluto
    for (int i = 0; i < size_wa; i++) {
        results_float[i] = FIXED2FLOAT(results[i], 8);
    }

    // Softmax
    for (int i = 0; i < size_wa; i++) {
        sum += expf(results_float[i]);
    }

    for (int i = 0; i < size_wa; i++) {
        r[i] = expf(results_float[i]) / sum;
        c[i] = i;
    }

    // Ordina (facoltativo: top-5)
    for (int i = 0; i < size_wa; i++) {
        for (int j = i; j < size_wa; j++) {
            if (r[j] > r[i]) {
                float t  = r[j];  r[j] = r[i];  r[i] = t;
                int   tc = c[j];  c[j] = c[i];  c[i] = tc;
            }
        }
    }

    // Trova top-1 sui logits
    int top0 = 0;
    float topval = results_float[0];
    for (int i = 1; i < size_wa; i++) {
        if (results_float[i] > topval) {
            top0 = i;
            topval = results_float[i];
        }
    }

    xil_printf("max class = %d (%s)\r\n", top0, labels[top0]);
    return top0;
}
