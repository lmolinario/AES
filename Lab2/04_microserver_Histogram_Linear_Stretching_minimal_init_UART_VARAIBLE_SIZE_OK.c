#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xuartps.h"
#include "stdlib.h"

#define UART_BASE XPAR_PS7_UART_1_BASEADDR

// Legge una riga ASCII fino a '\n'
int read_line(char *buf, int maxlen) {
    int i = 0;
    char c;
    while (i < maxlen - 1) {
        c = XUartPs_RecvByte(UART_BASE);
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = 0;
    return i;
}

// Converte stringa ASCII → intero (solo numeri)
int to_int(char *s) {
    int v = 0;
    int i = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        v = v * 10 + (s[i] - '0');
        i++;
    }
    return v;
}

int main()
{
    init_platform();

    XUartPs Uart_1_PS;
    XUartPs_Config *Config_1;

    Config_1 = XUartPs_LookupConfig(XPAR_PS7_UART_1_DEVICE_ID);
    XUartPs_CfgInitialize(&Uart_1_PS, Config_1, Config_1->BaseAddress);
    XUartPs_SetBaudRate(&Uart_1_PS, 115200);

    char line1[32], line2[32], line3[32];

    // -----------------------------
    // 1) Leggi header PPM variabile
    // -----------------------------
    read_line(line1, 32);   // es: "P6\n"
    read_line(line2, 32);   // es: "128 128\n"
    read_line(line3, 32);   // es: "255\n"

    // Parse width e height
    int width = 0, height = 0;
    int idx = 0;

    while (line2[idx] >= '0' && line2[idx] <= '9')
        width = width * 10 + (line2[idx++] - '0');

    idx++; // salta spazio

    while (line2[idx] >= '0' && line2[idx] <= '9')
        height = height * 10 + (line2[idx++] - '0');

    int maxval = to_int(line3);

    int num_pixels = width * height * 3;

    // -----------------------------
    // 2) Alloca immagine dinamica
    // -----------------------------
    u8 *image = malloc(num_pixels);

    // -----------------------------
    // 3) Ricevi pixel
    // -----------------------------
    for (int i = 0; i < num_pixels; i++)
        image[i] = XUartPs_RecvByte(UART_BASE);

    // -----------------------------
    // 4) Histogram Linear Stretching
    // -----------------------------
    u8 I_min = 255;
    u8 I_max = 0;

    for (int i = 0; i < num_pixels; i++) {
        if (image[i] < I_min) I_min = image[i];
        if (image[i] > I_max) I_max = image[i];
    }

    if (I_max == I_min) I_max = I_min + 1;

    float scale = 255.0f / (float)(I_max - I_min);

    for (int i = 0; i < num_pixels; i++) {
        float stretched = (image[i] - I_min) * scale;
        if (stretched < 0) stretched = 0;
        if (stretched > 255) stretched = 255;
        image[i] = (u8) stretched;
    }

    // -----------------------------
    // 5) Rinvia header PPM originale
    // -----------------------------
    for (int i = 0; line1[i] != 0; i++)
        XUartPs_SendByte(UART_BASE, line1[i]);

    for (int i = 0; line2[i] != 0; i++)
        XUartPs_SendByte(UART_BASE, line2[i]);

    for (int i = 0; line3[i] != 0; i++)
        XUartPs_SendByte(UART_BASE, line3[i]);

    // -----------------------------
    // 6) Rinvia pixel
    // -----------------------------
    for (int i = 0; i < num_pixels; i++)
        XUartPs_SendByte(UART_BASE, image[i]);

    free(image);
    cleanup_platform();
    return 0;
}
