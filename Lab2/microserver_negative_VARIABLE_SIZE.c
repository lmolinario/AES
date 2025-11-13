/***************************************************************
 *  Lab 2 – UART Microserver (PPM Generic, Variable Size, Negative)
 *  Board: Zynq / Zybo Z7 (PS UART)
 *
 *  Funzionamento:
 *  ------------------------------------------------------------
 *  - Riceve da UART un PPM "raw" P6 di qualunque dimensione.
 *  - Parsing completo dell’header:
 *        * P6
 *        * width, height (ASCII → int)
 *        * maxval (≤255)
 *        * gestione commenti '#'
 *        * gestione whitespace multiplo
 *  - Copia l’header originale byte-per-byte
 *  - Legge (width * height * 3) byte senza limiti predefiniti
 *  - Alloca memoria dinamica per l’immagine
 *  - Applica negativo:
 *        pixel_neg = maxval - pixel
 *  - Invia header + raster negativo
 *
 *  NOTA:
 *  ------------------------------------------------------------
 *  - Nessuna printf di debug (come richiesto).
 *  - Eseguibile per qualunque dimensione di immagine.
 ***************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

#include "platform.h"
#include "xuartps.h"
#include "xparameters.h"

/* ============================================================
 * UART SETUP
 * ============================================================ */
#define UART_ID     XPAR_PS7_UART_1_DEVICE_ID
#define BAUD        115200

static XUartPs uart;
static u32 uart_base;

static int uart_init(void)
{
    XUartPs_Config *cfg = XUartPs_LookupConfig(UART_ID);
    if (!cfg) return XST_FAILURE;

    if (XUartPs_CfgInitialize(&uart, cfg, cfg->BaseAddress) != XST_SUCCESS)
        return XST_FAILURE;

    uart_base = cfg->BaseAddress;

    if (XUartPs_SetBaudRate(&uart, BAUD) != XST_SUCCESS)
        return XST_FAILURE;

    return XST_SUCCESS;
}

static inline uint8_t rx_byte(void) {
    return XUartPs_RecvByte(uart_base);
}

static inline void tx_byte(uint8_t b) {
    XUartPs_SendByte(uart_base, b);
}



/* ============================================================
 * HEADER PARSING (Generic P6)
 *  - salva ogni byte in header[]
 *  - restituisce token testuale (per width,height,maxval)
 * ============================================================ */

static int read_token(char *tok, int max_len, uint8_t *header, int *header_len, int *header_done)
{
    int c, i = 0;

    // Skip whitespace & comments
    while (1) {

        c = rx_byte();

        if (!(*header_done))
            header[(*header_len)++] = (uint8_t)c;

        if (c == '#') {
            // consume comment
            do {
                c = rx_byte();
                if (!(*header_done))
                    header[(*header_len)++] = (uint8_t)c;
            } while (c != '\n' && c != '\r');
            continue;
        }

        if (!isspace(c))
            break;
    }

    tok[i++] = (char)c;

    while (i < max_len - 1) {

        c = rx_byte();

        if (!(*header_done))
            header[(*header_len)++] = (uint8_t)c;

        if (isspace(c) || c == '#') {

            if (c == '#') {
                do {
                    c = rx_byte();
                    if (!(*header_done))
                        header[(*header_len)++] = (uint8_t)c;
                } while (c != '\n' && c != '\r');
            }

            break;
        }

        tok[i++] = (char)c;
    }

    tok[i] = '\0';
    return i;
}

/* Conversione ASCII → int (Lab TIP: digit = byte-48) */
static int ascii_to_int(const char *s)
{
    int v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - 48);   // TIP richiesto: ASCII '0' = 48
        s++;
    }
    return v;
}



/* ============================================================
 * NEGATIVE FILTER
 * ============================================================ */
static void negative(uint8_t *src, uint8_t *dst, int count, int maxval)
{
    for (int i = 0; i < count; i++) {
        uint8_t p = src[i];
        if (p > maxval) p = maxval;     // clamp di sicurezza
        dst[i] = (uint8_t)(maxval - p);
    }
}



/* ============================================================
 *  MICROSERVER – SINGLE IMAGE PROCESSING
 * ============================================================ */

static void ppm_microserver_once(void)
{
    char tok[64];

    uint8_t header[512];
    int header_len = 0;
    int header_done = 0;

    int width, height, maxval;

    /* Magic number */
    read_token(tok, 64, header, &header_len, &header_done); // P6

    /* Width */
    read_token(tok, 64, header, &header_len, &header_done);
    width = ascii_to_int(tok);

    /* Height */
    read_token(tok, 64, header, &header_len, &header_done);
    height = ascii_to_int(tok);

    /* Maxval */
    read_token(tok, 64, header, &header_len, &header_done);
    maxval = ascii_to_int(tok);

    /* Header finito */
    header_done = 1;

    /* Calcolo numero di byte dell’immagine */
    int total = width * height * 3;

    /* Allocazione dinamica buffer */
    uint8_t *img_in  = malloc(total);
    uint8_t *img_out = malloc(total);

    if (!img_in || !img_out)
        while(1);   // fail

    /* Lettura raster */
    for (int i = 0; i < total; i++)
        img_in[i] = rx_byte();

    /* Elaborazione */
    negative(img_in, img_out, total, maxval);

    /* Invio header identico */
    for (int i = 0; i < header_len; i++)
        tx_byte(header[i]);

    /* Invio immagine negativa */
    for (int i = 0; i < total; i++)
        tx_byte(img_out[i]);

    free(img_in);
    free(img_out);
}



/* ============================================================
 * MAIN
 * ============================================================ */
int main(void)
{
    init_platform();

    if (uart_init() != XST_SUCCESS)
        while(1);

    /* Elabora una sola immagine */
    ppm_microserver_once();

    /* Rimani vivo */
    while (1);

    return 0;
}
