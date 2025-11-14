#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xuartps.h"

#define UART_BASE XPAR_PS7_UART_1_BASEADDR

#define HEADER_SIZE 15
#define IMG_W 128
#define IMG_H 128
#define PIXELS (IMG_W * IMG_H * 3)

u8 header[HEADER_SIZE];
u8 image[PIXELS];

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

    // -----------------------------
    //  PPM RECEIVE + NEGATIVE
    // -----------------------------

    // Read 15-byte header
    for (int i = 0; i < HEADER_SIZE; i++)
        header[i] = XUartPs_RecvByte(UART_BASE);



    // Read pixels
    for (int i = 0; i < PIXELS; i++)
        image[i] = XUartPs_RecvByte(UART_BASE);


    // Apply negative
    for (int i = 0; i < PIXELS; i++)
        image[i] = 255 - image[i];



    // Send header
    for (int i = 0; i < HEADER_SIZE; i++)
        XUartPs_SendByte(UART_BASE, header[i]);

    // Send pixels
    for (int i = 0; i < PIXELS; i++)
        XUartPs_SendByte(UART_BASE, image[i]);



    cleanup_platform();
    return 0;
}
