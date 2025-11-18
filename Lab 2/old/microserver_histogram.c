/***************************************************************
 *  Lab 2 – UART Microserver (PPM Generic Parser + histogram_stretch)
 *  Board: Zynq / Zybo Z7 (PS UART)
 ***************************************************************/

#include <stdint.h>
#include <ctype.h>

#include "platform.h"
#include "xparameters.h"
#include "xuartps.h"

/* ============================================================
 *  CONFIGURAZIONE UART
 * ============================================================ */
#define UART_DEVICE_ID  XPAR_PS7_UART_1_DEVICE_ID
#define UART_BAUDRATE   115200

static XUartPs UartInstance;
static uint32_t UartBaseAddr;

/* ============================================================
 *  LIMITI IMMAGINE / BUFFER
 * ============================================================ */
#define MAX_WIDTH        128
#define MAX_HEIGHT       128
#define MAX_CHANNELS     3
#define MAX_PIXELS       (MAX_WIDTH * MAX_HEIGHT)
#define MAX_IMAGE_BYTES  (MAX_PIXELS * MAX_CHANNELS)

#define MAX_HEADER_BYTES 256

static uint8_t header_buf[MAX_HEADER_BYTES];
static uint32_t header_len = 0;

static uint8_t img_in[MAX_IMAGE_BYTES];
static uint8_t img_out[MAX_IMAGE_BYTES];

static int header_done = 0;

/* ============================================================
 *  UART – Funzioni di supporto
 * ============================================================ */
static int uart_init(void) {
    int Status;
    XUartPs_Config *Config;

    Config = XUartPs_LookupConfig(UART_DEVICE_ID);
    if (Config == NULL)
        return XST_FAILURE;

    Status = XUartPs_CfgInitialize(&UartInstance, Config, Config->BaseAddress);
    if (Status != XST_SUCCESS)
        return XST_FAILURE;

    UartBaseAddr = Config->BaseAddress;

    Status = XUartPs_SetBaudRate(&UartInstance, UART_BAUDRATE);
    if (Status != XST_SUCCESS)
        return XST_FAILURE;

    return XST_SUCCESS;
}

static uint8_t uart_recv_byte(void) {
    return XUartPs_RecvByte(UartBaseAddr);
}

static void uart_send_byte(uint8_t b) {
    XUartPs_SendByte(UartBaseAddr, b);
}

/* ============================================================
 *  LETTURA STREAM + PARSING HEADER
 * ============================================================ */
static int read_stream_byte(void) {
    uint8_t b = uart_recv_byte();

    if (!header_done && header_len < MAX_HEADER_BYTES)
        header_buf[header_len++] = b;

    return (int)b;
}

static void read_token(char *token, int max_len) {
    int c;
    int i = 0;

    while (1) {
        c = read_stream_byte();

        if (c == '#') {
            do {
                c = read_stream_byte();
            } while (c != '\n' && c != '\r');
            continue;
        }
        if (!isspace(c))
            break;
    }

    if (i < max_len - 1)
        token[i++] = (char)c;

    while (i < max_len - 1) {
        c = read_stream_byte();
        if (isspace(c) || c == '#') {

            if (c == '#') {
                do {
                    c = read_stream_byte();
                } while (c != '\n' && c != '\r');
            }
            break;
        }
        token[i++] = (char)c;
    }
    token[i] = '\0';
}

static uint32_t parse_uint(const char *s) {
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    return v;
}

/* ============================================================
 *  🔥 HISTOGRAM LINEAR STRETCHING — VERSIONE CORRETTA
 * ============================================================ */
void histogram_stretch(uint8_t *src, uint8_t *dst,
                       uint32_t num_bytes, uint8_t maxval)
{
    uint8_t I_min = 255;
    uint8_t I_max = 0;

    /* 1. Trova min e max */
    for (uint32_t i = 0; i < num_bytes; i++) {
        uint8_t p = src[i];
        if (p < I_min) I_min = p;
        if (p > I_max) I_max = p;
    }

    if (I_max == I_min) {
        for (uint32_t i = 0; i < num_bytes; i++)
            dst[i] = src[i];
        return;
    }

    /* 2. Scale factor */
    float scale = (float)maxval / (float)(I_max - I_min);

    /* 3. Stretching */
    for (uint32_t i = 0; i < num_bytes; i++) {
        float p = (float)(src[i] - I_min) * scale;
        if (p < 0) p = 0;
        if (p > maxval) p = maxval;
        dst[i] = (uint8_t)p;
    }
}

/* ============================================================
 *  MICROSERVER: PPM parsing → elaborazione → invio
 * ============================================================ */
static void ppm_microserver_once(void) {
    char token[64];
    uint32_t width, height, maxval;
    uint32_t num_pixels, num_bytes;

    header_len  = 0;
    header_done = 0;

    read_token(token, sizeof(token));        /* P6 */
    read_token(token, sizeof(token)); width  = parse_uint(token);
    read_token(token, sizeof(token)); height = parse_uint(token);
    read_token(token, sizeof(token)); maxval = parse_uint(token);

    header_done = 1;

    if (width  > MAX_WIDTH)  width  = MAX_WIDTH;
    if (height > MAX_HEIGHT) height = MAX_HEIGHT;
    if (maxval > 255)        maxval = 255;

    num_pixels = width * height;
    num_bytes  = num_pixels * MAX_CHANNELS;
    if (num_bytes > MAX_IMAGE_BYTES)
        num_bytes = MAX_IMAGE_BYTES;

    for (uint32_t i = 0; i < num_bytes; i++)
        img_in[i] = uart_recv_byte();

    /* 🔥 Applica Histogram Stretching */
    histogram_stretch(img_in, img_out, num_bytes, maxval);

    /* Invia l'header originale */
    for (uint32_t i = 0; i < header_len; i++)
        uart_send_byte(header_buf[i]);

    /* Invia raster elaborato */
    for (uint32_t i = 0; i < num_bytes; i++)
        uart_send_byte(img_out[i]);
}

/* ============================================================
 *  MAIN
 * ============================================================ */
int main(void) {
    init_platform();

    if (uart_init() != XST_SUCCESS)
        while (1);

    ppm_microserver_once();

    while (1);
    return 0;
}
