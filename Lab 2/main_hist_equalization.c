/***************************************************************
 *  Lab 2 – UART Microserver (Histogram Equalization – Bonus)
 *  Author: Lello Molinario
 *  University of Cagliari – Advanced Embedded Systems (AES)
 *  Student ID: 70/90/000369
 *  Date: November 2025
 *
 *  Description
 *  ------------------------------------------------------------
 *  UART-based microserver running on the Zybo Z7 (ARM PS UART1).
 *  The server receives a PPM (P6) image over UART, applies
 *  global histogram equalization on all RGB channels (byte-wise),
 *  and sends the processed image back to the client.
 *
 *  Supported PPM format:
 *   • P6 (binary)
 *   • Arbitrary image size (width × height)
 *   • 1-byte RGB channels (maxval = 255)
 *
 *  Histogram Equalization (simplified):
 *   1. Build histogram over 256 intensity levels [0..255]
 *   2. Compute cumulative distribution function (CDF)
 *   3. Find smallest non-zero CDF (cdf_min)
 *   4. Build mapping:
 *        map[i] = round( (CDF[i] − cdf_min) / (N − cdf_min) * maxval )
 *   5. Remap each pixel via map[]
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

/* ---------------- Image processing: histogram equalization ---------------- */

/**
 * Global histogram equalization on all bytes (RGB channels).
 * Uses a 256-bin histogram and CDF-based mapping.
 */
static void image_hist_equalization(u8 *buffer, int num_bytes, int maxval)
{
    unsigned int hist[256] = {0};
    unsigned int cdf[256]  = {0};
    u8           map[256];

    int N = num_bytes;  // total number of "pixels" (actually bytes)

    // 1) Histogram
    for (int i = 0; i < num_bytes; i++) {
        hist[buffer[i]]++;
    }

    // 2) CDF
    cdf[0] = hist[0];
    for (int i = 1; i < 256; i++) {
        cdf[i] = cdf[i - 1] + hist[i];
    }

    // 3) cdf_min (first non-zero)
    unsigned int cdf_min = 0;
    for (int i = 0; i < 256; i++) {
        if (cdf[i] != 0) {
            cdf_min = cdf[i];
            break;
        }
    }

    if (cdf_min == 0 || N <= (int)cdf_min) {
        // Pathological or flat image – nothing to do
        return;
    }

    // 4) Build mapping
    for (int i = 0; i < 256; i++) {
        float v = (float)(cdf[i] - cdf_min) / (float)(N - cdf_min);
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        map[i] = (u8)(v * (float)maxval + 0.5f);
    }

    // 5) Apply mapping in-place
    for (int i = 0; i < num_bytes; i++) {
        buffer[i] = map[buffer[i]];
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

    xil_printf("Lab 2 – UART Microserver (Histogram Equalization – Bonus) started.\r\n");

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

    image_hist_equalization(image, num_bytes, hdr.maxval);

    ppm_write_header(line_magic, line_dim, line_max);
    ppm_write_pixels(image, num_bytes);

    free(image);
    xil_printf("Lab 2 – Histogram Equalization Version completed.\r\n");

    cleanup_platform();
    return 0;
}
