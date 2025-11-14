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
    //  PPM RECEIVE + Histogram Linear Stretching
    // -----------------------------

    // Read 15-byte header
    for (int i = 0; i < HEADER_SIZE; i++)
        header[i] = XUartPs_RecvByte(UART_BASE);



    // Read pixels
    for (int i = 0; i < PIXELS; i++)
        image[i] = XUartPs_RecvByte(UART_BASE);


    // -------------------------------------------------
    // Histogram Linear Stretching
    // p_new = (p - I_min) * scale
    // -------------------------------------------------

    // 1) Find I_min and I_max
    u8 I_min = 255;
    u8 I_max = 0;

    for (int i = 0; i < PIXELS; i++) {
        if (image[i] < I_min) I_min = image[i];
        if (image[i] > I_max) I_max = image[i];
    }

    // Avoid division by zero (flat image)
    if (I_max == I_min) I_max = I_min + 1;

    // 2) Compute scale
    float scale = 255.0f / (float)(I_max - I_min);

    // 3) Apply linear stretching
    for (int i = 0; i < PIXELS; i++) {
        float stretched = (image[i] - I_min) * scale;

        // Clamp to [0,255]
        if (stretched < 0) stretched = 0;
        if (stretched > 255) stretched = 255;

        image[i] = (u8) stretched;
    }



    // Send header
    for (int i = 0; i < HEADER_SIZE; i++)
        XUartPs_SendByte(UART_BASE, header[i]);

    // Send pixels
    for (int i = 0; i < PIXELS; i++)
        XUartPs_SendByte(UART_BASE, image[i]);



    cleanup_platform();
    return 0;
}
