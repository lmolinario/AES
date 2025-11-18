/***************************************************************
 *  Lab 2 – UART Microserver (Histogram Stretching Version)
 *  Author: Lello Molinario
 *  University of Cagliari – Advanced Embedded Systems (AES)
 *  Student ID: 70/90/000369
 *  Date: November 2025
 *
 *  Description
 *  ------------------------------------------------------------
 *  UART-based microserver running on the Zybo Z7 (ARM PS UART1).
 *  The server receives a PPM (P6) image over UART, performs
 *  linear histogram stretching on all RGB channels, and sends
 *  the processed image back to the client.
 *
 *  Supported PPM format:
 *   • P6 (binary)
 *   • Arbitrary image size (width × height)
 *   • 1-byte RGB channels (maxval = 255)
 *
 *  Histogram Stretching:
 *   1. Find global minimum and maximum pixel values (I_min, I_max)
 *   2. For each pixel p:
 *        p_out = (p − I_min) * maxval / (I_max − I_min)
 *      Values are clipped to [0, maxval].
 *   3. If I_max == I_min (flat image), no change is applied.
 *
 *  Communication protocol:
 *   • Same as Negative Version (3 header lines + RGB data).
 *
 ***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "platform.h"
#include "xil_printf.h"
#include "xuartps.h"

#define UART_BASE   XPAR_PS7_UART_1_BASEADDR
#define UART_DEVICE XPAR_PS7_UART_1_DEVICE_ID

typedef struct {
    int width;
    int height;
    int maxval;
} PpmHeader;

static XUartPs UartPs_1;

/* ---------------- UART utilities ---------------- */

static int uart_init(void)
{
    XUartPs_Config *Config;
    int Status;

    Config = XUartPs_LookupConfig(UART_DEVICE);
    if (Config == NULL) {
        xil_printf("ERROR: UART config not found\r\n");
        return XST_FAILURE;
    }

    Status = XUartPs_CfgInitialize(&UartPs_1, Config, Config->BaseAddress);
    if (Status != XST_SUCCESS) {
        xil_printf("ERROR: UART init failed\r\n");
        return XST_FAILURE;
    }

    Status = XUartPs_SetBaudRate(&UartPs_1, 115200);
    if (Status != XST_SUCCESS) {
        xil_printf("ERROR: UART set baudrate failed\r\n");
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

static u8 uart_recv_byte(void)
{
    return XUartPs_RecvByte(UART_BASE);
}

static void uart_send_byte(u8 c)
{
    XUartPs_SendByte(UART_BASE, c);
}

/* ---------------- ASCII parsing helpers ---------------- */

static int read_line(char *buf, int maxlen)
{
    int i = 0;
    char c;

    while (i < maxlen - 1) {
        c = (char)uart_recv_byte();
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return i;
}

static int ascii_to_int(const char *s)
{
    int value = 0;
    int i = 0;

    while (s[i] >= '0' && s[i] <= '9') {
        value = value * 10 + (s[i] - '0');
        i++;
    }
    return value;
}

/* ---------------- PPM utilities ---------------- */

static int ppm_read_header(PpmHeader *hdr,
                           char *line_magic,
                           char *line_dim,
                           char *line_max,
                           int   maxlen)
{
    read_line(line_magic, maxlen);
    read_line(line_dim,   maxlen);
    read_line(line_max,   maxlen);

    int i = 0;
    int width = 0;
    int height = 0;

    while (line_dim[i] >= '0' && line_dim[i] <= '9') {
        width = width * 10 + (line_dim[i] - '0');
        i++;
    }

    if (line_dim[i] == ' ') {
        i++;
    }

    while (line_dim[i] >= '0' && line_dim[i] <= '9') {
        height = height * 10 + (line_dim[i] - '0');
        i++;
    }

    int maxval = ascii_to_int(line_max);

    hdr->width  = width;
    hdr->height = height;
    hdr->maxval = maxval;

    return 0;
}

static void ppm_read_pixels(u8 *buffer, int num_bytes)
{
    for (int i = 0; i < num_bytes; i++) {
        buffer[i] = uart_recv_byte();
    }
}

static void ppm_write_header(const char *line_magic,
                             const char *line_dim,
                             const char *line_max)
{
    int i = 0;

    while (line_magic[i] != '\0') {
        uart_send_byte((u8)line_magic[i++]);
    }

    i = 0;
    while (line_dim[i] != '\0') {
        uart_send_byte((u8)line_dim[i++]);
    }

    i = 0;
    while (line_max[i] != '\0') {
        uart_send_byte((u8)line_max[i++]);
    }
}

static void ppm_write_pixels(const u8 *buffer, int num_bytes)
{
    for (int i = 0; i < num_bytes; i++) {
        uart_send_byte(buffer[i]);
    }
}

/* ---------------- Image processing: histogram stretching ---------------- */

/**
 * Linear histogram stretching:
 *  - Find I_min and I_max over the whole image.
 *  - Re-map each pixel to [0, maxval].
 */
static void image_hist_stretch(u8 *buffer, int num_bytes, int maxval)
{
    int I_min = maxval;
    int I_max = 0;

    // 1) Compute global min and max
    for (int i = 0; i < num_bytes; i++) {
        int p = buffer[i];
        if (p < I_min) I_min = p;
        if (p > I_max) I_max = p;
    }

    // Avoid division by zero (flat image)
    if (I_max == I_min) {
        return;
    }

    // 2) Stretch each pixel
    for (int i = 0; i < num_bytes; i++) {
        int p = buffer[i];
        int stretched = (p - I_min) * maxval / (I_max - I_min);

        if (stretched < 0)        stretched = 0;
        if (stretched > maxval)   stretched = maxval;

        buffer[i] = (u8)stretched;
    }
}

/* ---------------- main() ---------------- */

int main(void)
{
    init_platform();

    if (uart_init() != XST_SUCCESS) {
        xil_printf("FATAL: UART initialization failed\r\n");
        cleanup_platform();
        return XST_FAILURE;
    }

    xil_printf("Lab 2 – UART Microserver (Histogram Stretching) started.\r\n");

    PpmHeader hdr;
    char line_magic[32];
    char line_dim[32];
    char line_max[32];

    ppm_read_header(&hdr, line_magic, line_dim, line_max, 32);

    int num_bytes = hdr.width * hdr.height * 3;

    u8 *image = (u8 *)malloc(num_bytes);
    if (image == NULL) {
        xil_printf("ERROR: malloc failed (image buffer)\r\n");
        cleanup_platform();
        return XST_FAILURE;
    }

    ppm_read_pixels(image, num_bytes);

    image_hist_stretch(image, num_bytes, hdr.maxval);

    ppm_write_header(line_magic, line_dim, line_max);
    ppm_write_pixels(image, num_bytes);

    free(image);
    xil_printf("Lab 2 – Histogram Stretching Version completed.\r\n");

    cleanup_platform();
    return 0;
}
