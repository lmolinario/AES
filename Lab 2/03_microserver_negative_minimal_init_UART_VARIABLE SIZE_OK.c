#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xuartps.h"
#include "stdlib.h"


#define UART_BASE XPAR_PS7_UART_1_BASEADDR


// funzione minimale per leggere una riga fino a '\n'
int read_line(char *buf, int maxlen) {
    int i = 0;
    char c;
    while (i < maxlen-1) {
        c = XUartPs_RecvByte(UART_BASE);
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = 0;
    return i;
}

// converte stringa ASCII in intero, minimale
int to_int(char *s) {
    int n = 0;
    int i = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        n = n*10 + (s[i] - '0');
        i++;
    }
    return n;
}



int main()
{
    init_platform();
    XUartPs Uart_1_PS;
    u16 DeviceId_1= XPAR_PS7_UART_1_DEVICE_ID;
    int Status_1;
    XUartPs_Config *Config_1;
    Config_1 = XUartPs_LookupConfig(DeviceId_1);
    if (NULL == Config_1) {
        return XST_FAILURE;
    }
    Status_1 = XUartPs_CfgInitialize(&Uart_1_PS, Config_1, Config_1->BaseAddress); //inizializzo la UART
    if (Status_1 != XST_SUCCESS) {
        return XST_FAILURE;
    }
    u32 BaudRate = (u32)115200;
    Status_1 = XUartPs_SetBaudRate(&Uart_1_PS, BaudRate); //setto il BaudRate = 115200
    if (Status_1 != (s32)XST_SUCCESS) {
        return XST_FAILURE;
    }

    // ---------------------------
    // Lettura header variabile
    // ---------------------------

    char line1[32], line2[32], line3[32];

    read_line(line1, 32);     // "P6\n"
    read_line(line2, 32);     // "128 128\n" (esempio)
    read_line(line3, 32);     // "255\n"

    // estrai width, height
    int width = 0, height = 0;
    int i = 0;

    // parse width
    while (line2[i] >= '0' && line2[i] <= '9') {
        width = width*10 + (line2[i] - '0');
        i++;
    }

    i++; // salta lo spazio

    // parse height
    while (line2[i] >= '0' && line2[i] <= '9') {
        height = height*10 + (line2[i] - '0');
        i++;
    }


    int maxval = to_int(line3);

    int num_pixels = width * height * 3;

    // Allocate image buffer
    u8 *image = malloc(num_pixels);

    // Read image pixels
    for (int i = 0; i < num_pixels; i++)
        image[i] = XUartPs_RecvByte(UART_BASE);

    // Apply negative
    for (int i = 0; i < num_pixels; i++)
        image[i] = 255 - image[i];

    // Send header back
    for (int i = 0; line1[i] != 0; i++)
        XUartPs_SendByte(UART_BASE, line1[i]);

    for (int i = 0; line2[i] != 0; i++)
        XUartPs_SendByte(UART_BASE, line2[i]);

    for (int i = 0; line3[i] != 0; i++)
        XUartPs_SendByte(UART_BASE, line3[i]);

    // Send image back
    for (int i = 0; i < num_pixels; i++)
        XUartPs_SendByte(UART_BASE, image[i]);

    free(image);
    cleanup_platform();
    return 0;
}