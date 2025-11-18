#include "platform.h"
#include "xuartps.h"
#include "xil_printf.h"

#define UART_BASE XPAR_PS7_UART_1_BASEADDR

#define HEADER_SIZE 15
#define IMG_W 128
#define IMG_H 128
#define PIXELS (IMG_W * IMG_H * 3)   // RGB → 49152 byte

u8 header[HEADER_SIZE];
u8 image[PIXELS];

int main()
{
    init_platform();


    // 1) Read the 15-byte header
    for (int i = 0; i < HEADER_SIZE; i++)
        header[i] = XUartPs_RecvByte(UART_BASE);



    // 2) Read image pixels
    for (int i = 0; i < PIXELS; i++)
        image[i] = XUartPs_RecvByte(UART_BASE);



    // 3) Negative processing
    for (int i = 0; i < PIXELS; i++)
        image[i] = 255 - image[i];


    // 4) Send header back
    for (int i = 0; i < HEADER_SIZE; i++)
        XUartPs_SendByte(UART_BASE, header[i]);

    // 5) Send processed pixels back
    for (int i = 0; i < PIXELS; i++)
        XUartPs_SendByte(UART_BASE, image[i]);


    cleanup_platform();
    return 0;
}
