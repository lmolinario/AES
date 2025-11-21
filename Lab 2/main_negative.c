/***************************************************************
 *  Lab 2 – UART Microserver (Negative Version)
 *  Author: Lello Molinario
 *  University of Cagliari – Advanced Embedded Systems (AES)
 *  Student ID: 70/90/000369
 *  Date: November 2025
 *
 *  Description
 *  ------------------------------------------------------------
 *  UART-based microserver running on the Zybo Z7.
 *  The server receives a PPM (P6) image over UART, computes the
 *  negative of each RGB channel, and sends the processed PPM
 *  image back to the client.
 *
 *  Supported PPM format:
 *   • P6 (binary)
 *   • Arbitrary image size (width × height)
 *   • 1-byte RGB channels (maxval = 255)
 *
 *  Communication protocol:
 *   1. Client sends 3 header lines:
 *        - "P6"
 *        - "<width> <height>"
 *        - "255"
 *   2. Client sends raw RGB data (width × height × 3 bytes)
 *   3. Microserver applies:  p_out = maxval − p_in
 *   4. Microserver responds with:
 *        - Same header (3 lines)
 *        - Processed RGB data
 *
 *  Platform
 *  ------------------------------------------------------------
 *   • Hardware: Zybo Z7 board (ARM Cortex-A9 – PS7)
 *   • UART: PS UART1 @ 115200 baud, 8N1
 *   • Tools: Xilinx SDK / Vitis + RealTerm (Windows) or gtkterm (Linux)
 *
 ***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "platform.h"
#include "xil_printf.h"
#include "xuartps.h"

#define UART_BASE   XPAR_PS7_UART_1_BASEADDR
#define UART_DEVICE XPAR_PS7_UART_1_DEVICE_ID

/* ------------------------------------------------------------
 *  PPM header structure
 * ----------------------------------------------------------*/
typedef struct {
    int width;
    int height;
    int maxval;
} PpmHeader;

/* ------------------------------------------------------------
 *  Global UART instance (PS UART1)
 * ----------------------------------------------------------*/
static XUartPs UartPs_1;

/* ============================================================
 *  UART utility functions
 * ==========================================================*/

/**
 * Initialize PS UART1 at 115200 baud, 8N1.
 */
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

/**
 * Blocking UART receive of one byte.
 */
static u8 uart_recv_byte(void)
{
    return XUartPs_RecvByte(UART_BASE);
}

/**
 * Blocking UART send of one byte.
 */
static void uart_send_byte(u8 c)
{
    XUartPs_SendByte(UART_BASE, c);
}

/* ============================================================
 *  Helper functions for ASCII parsing
 * ==========================================================*/

/**
 * Read a line from UART into 'buf', including the final '\n'.
 * The string is null-terminated. Returns number of chars read.
 */
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

/**
 * Convert a numeric ASCII string (e.g. "255") to integer.
 * Stops at first non-digit character.
 */
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

/* ============================================================
 *  PPM utility functions
 * ==========================================================*/

/**
 * Read PPM header (3 lines) from UART.
 * Stores original header lines (for re-transmission) and
 * parses width, height, maxval into hdr.
 */
static int ppm_read_header(PpmHeader *hdr,
                           char *line_magic,
                           char *line_dim,
                           char *line_max,
                           int   maxlen)
{
    // 1) Magic number (e.g., "P6\n")
    read_line(line_magic, maxlen);

    // 2) Dimensions line: "<width> <height>\n"
    read_line(line_dim, maxlen);

    // 3) Max value line: "255\n"
    read_line(line_max, maxlen);

    // Parse width and height from line_dim
    int i = 0;
    int width = 0;
    int height = 0;

    while (line_dim[i] >= '0' && line_dim[i] <= '9') {
        width = width * 10 + (line_dim[i] - '0');
        i++;
    }

    // Skip separator (space)
    if (line_dim[i] == ' ') {
        i++;
    }

    while (line_dim[i] >= '0' && line_dim[i] <= '9') {
        height = height * 10 + (line_dim[i] - '0');
        i++;
    }

    // Parse maxval from line_max
    int maxval = ascii_to_int(line_max);

    hdr->width  = width;
    hdr->height = height;
    hdr->maxval = maxval;

    return 0;
}

/**
 * Read raw pixel data (RGB) from UART into buffer.
 * Expects exactly num_bytes bytes.
 */
static void ppm_read_pixels(u8 *buffer, int num_bytes)
{
    for (int i = 0; i < num_bytes; i++) {
        buffer[i] = uart_recv_byte();
    }
}

/**
 * Re-send original PPM header over UART.
 */
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

/**
 * Send raw pixel data (RGB) over UART.
 */
static void ppm_write_pixels(const u8 *buffer, int num_bytes)
{
    for (int i = 0; i < num_bytes; i++) {
        uart_send_byte(buffer[i]);
    }
}

/* ============================================================
 *  Image processing – Negative
 * ==========================================================*/

/**
 * Compute negative of image:
 *    p_out = maxval - p_in
 * Works on each byte in-place (RGB channels).
 */
static void image_negative(u8 *buffer, int num_bytes, int maxval)
{
    for (int i = 0; i < num_bytes; i++) {
        buffer[i] = (u8)(maxval - buffer[i]);
    }
}

/* ============================================================
 *  main()
 * ==========================================================*/

int main(void)
{
    init_platform();

    if (uart_init() != XST_SUCCESS) {
        xil_printf("FATAL: UART initialization failed\r\n");
        cleanup_platform();
        return XST_FAILURE;
    }

    xil_printf("Lab 2 – UART Microserver (Negative Version) started.\r\n");

    PpmHeader hdr;
    char line_magic[32];
    char line_dim[32];
    char line_max[32];

    // 1) Read and parse PPM header
    ppm_read_header(&hdr, line_magic, line_dim, line_max, 32);

    int num_bytes = hdr.width * hdr.height * 3;  // RGB

    // 2) Allocate buffer for image data
    u8 *image = (u8 *)malloc(num_bytes);
    if (image == NULL) {
        xil_printf("ERROR: malloc failed (image buffer)\r\n");
        cleanup_platform();
        return XST_FAILURE;
    }

    // 3) Receive RGB pixels
    ppm_read_pixels(image, num_bytes);

    // 4) Apply negative transformation
    image_negative(image, num_bytes, hdr.maxval);

    // 5) Send original header back
    ppm_write_header(line_magic, line_dim, line_max);

    // 6) Send processed pixels
    ppm_write_pixels(image, num_bytes);

    // 7) Cleanup
    free(image);
    xil_printf("Lab 2 – Negative Version completed.\r\n");

    cleanup_platform();
    return 0;
}
