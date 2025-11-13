/***************************************************************
 *  Lab 2 – UART Microserver (PPM Generic Parser + Negative)
 *  Board: Zynq / Zybo Z7 (PS UART)
 *
 *  Funzionamento:
 *  ------------------------------------------------------------
 *  - Riceve da UART un file PPM in formato "raw" (P6).
 *  - Effettua il parsing completo dell'header:
 *        * magic number (P6)
 *        * width, height
 *        * maxval (≤ 255)
 *        * commenti '#' e whitespace arbitrario
 *  - Copia l'header esattamente come ricevuto in header_buf[]
 *    (senza modificarlo).
 *  - Legge il raster (width * height * 3 byte, RGB).
 *  - Calcola l'immagine negativa in un buffer separato.
 *        pixel_neg = maxval - pixel_orig
 *  - Invia su UART:
 *        1) header originale;
 *        2) raster negativo.
 *
 *  Nota:
 *  ------------------------------------------------------------
 *  - Il buffer dei pixel è dimensionato per immagini fino a
 *    MAX_WIDTH x MAX_HEIGHT. Il file ricevuto deve rientrare
 *    in questi limiti.
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

/* Header PPM: di solito pochi byte, 256 è più che sufficiente */
#define MAX_HEADER_BYTES 256

/* Buffer globali */
static uint8_t header_buf[MAX_HEADER_BYTES];
static uint32_t header_len = 0;

static uint8_t img_in[MAX_IMAGE_BYTES];
static uint8_t img_out[MAX_IMAGE_BYTES];

/* Flag: 0 = stiamo ancora leggendo l'header, 1 = header finito */
static int header_done = 0;


/* ============================================================
 *  UART – Funzioni di supporto
 * ============================================================ */

/* Inizializzazione UART PS */
static int uart_init(void) {
    int Status;
    XUartPs_Config *Config;

    Config = XUartPs_LookupConfig(UART_DEVICE_ID);
    if (Config == NULL) {
        return XST_FAILURE;
    }

    Status = XUartPs_CfgInitialize(&UartInstance, Config, Config->BaseAddress);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    UartBaseAddr = Config->BaseAddress;

    Status = XUartPs_SetBaudRate(&UartInstance, UART_BAUDRATE);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

/* Riceve un byte dalla UART (blocking). */
static uint8_t uart_recv_byte(void) {
    return XUartPs_RecvByte(UartBaseAddr);
}

/* Invia un byte sulla UART. */
static void uart_send_byte(uint8_t b) {
    XUartPs_SendByte(UartBaseAddr, b);
}


/* ============================================================
 *  LETTURA STREAM + SALVATAGGIO HEADER
 * ============================================================ */

/*
 * read_stream_byte()
 * ------------------
 * Legge un byte dallo stream UART.
 * Se header_done == 0, lo salva anche in header_buf[].
 */
static int read_stream_byte(void) {
    uint8_t b = uart_recv_byte();

    if (!header_done && header_len < MAX_HEADER_BYTES) {
        header_buf[header_len++] = b;
    }

    return (int)b;
}

/*
 * read_token()
 * ------------
 * Legge un "token" ASCII dall'header PPM:
 *   - salta whitespace e commenti '# ... \n'
 *   - accumula caratteri non whitespace nel buffer 'token'
 *   - termina il token su whitespace o inizio commento
 *
 * Tutti i byte letti passano da read_stream_byte(), quindi
 * vengono copiati nell'header finché header_done == 0.
 */
static void read_token(char *token, int max_len) {
    int c;
    int i = 0;

    /* 1) Salta whitespace e commenti, ma copiandoli nel header_buf */
    while (1) {
        c = read_stream_byte();

        if (c == '#') {
            /* Commento: leggi fino a fine riga (inclusa) */
            do {
                c = read_stream_byte();
            } while (c != '\n' && c != '\r');
            /* ricomincia il ciclo: potremmo avere altro whitespace */
            continue;
        }

        if (!isspace(c)) {
            /* Primo carattere non whitespace/non-commento */
            break;
        }
    }

    /* 2) Primo carattere del token */
    if (i < max_len - 1) {
        token[i++] = (char)c;
    }

    /* 3) Continua a leggere caratteri del token */
    while (i < max_len - 1) {
        c = read_stream_byte();

        if (isspace(c) || c == '#') {
            /* Separatore di token trovato */

            if (c == '#') {
                /* Se è un commento, consumalo fino a fine linea */
                do {
                    c = read_stream_byte();
                } while (c != '\n' && c != '\r');
            }
            break;  /* Token terminato */
        }

        token[i++] = (char)c;
    }

    token[i] = '\0';
}

/* Converte una stringa di cifre decimali in intero (>=0). */
static uint32_t parse_uint(const char *s) {
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10u + (uint32_t)(*s - '0');
        s++;
    }
    return v;
}


/* ============================================================
 *  NEGATIVE() – Elabora il raster
 * ============================================================ */

/*
 * negative()
 * ----------
 * src   : buffer di input (raster RGB)
 * dst   : buffer di output (stessa dimensione)
 * count : numero di byte nel raster
 * maxval: valore massimo di intensità (Header PPM)
 *
 * Per ogni byte: dst[i] = maxval - src[i]
 */
static void negative(const uint8_t *src,
                     uint8_t *dst,
                     uint32_t count,
                     uint32_t maxval)
{
    uint32_t i;

    if (maxval == 0) {
        /* Evita divisioni strane: se maxval è 0, copia semplicemente */
        for (i = 0; i < count; ++i) {
            dst[i] = src[i];
        }
        return;
    }

    for (i = 0; i < count; ++i) {
        uint32_t v = (uint32_t)src[i];
        if (v > maxval) {
            v = maxval; /* clamp di sicurezza */
        }
        dst[i] = (uint8_t)(maxval - v);
    }
}


/* ============================================================
 *  PPM MICROSERVER – PARSING + ELABORAZIONE
 * ============================================================ */

static void ppm_microserver_once(void) {
    char token[64];
    uint32_t width, height, maxval;
    uint32_t num_pixels, num_bytes;
    uint32_t i;

    header_len  = 0;
    header_done = 0;

    /* 1) Leggi magic number (es. "P6") */
    read_token(token, sizeof(token));
    /* (opzionale) controllo: ci aspettiamo 'P' '6' */
    /* if (token[0] != 'P' || token[1] != '6') { ... } */

    /* 2) width */
    read_token(token, sizeof(token));
    width = parse_uint(token);

    /* 3) height */
    read_token(token, sizeof(token));
    height = parse_uint(token);

    /* 4) maxval */
    read_token(token, sizeof(token));
    maxval = parse_uint(token);

    /* Dopo aver letto maxval (e il suo separatore), l'header è finito */
    header_done = 1;

    /* 5) Limiti di sicurezza sul buffer */
    if (width  > MAX_WIDTH)  width  = MAX_WIDTH;
    if (height > MAX_HEIGHT) height = MAX_HEIGHT;
    if (maxval > 255)        maxval = 255;  /* supportiamo solo 1 byte per canale */

    num_pixels = width * height;
    num_bytes  = num_pixels * MAX_CHANNELS;

    if (num_bytes > MAX_IMAGE_BYTES) {
        num_bytes = MAX_IMAGE_BYTES;  /* clamp di sicurezza */
    }

    /* 6) Leggi il raster (solo parte che entra nel buffer) */
    for (i = 0; i < num_bytes; ++i) {
        img_in[i] = uart_recv_byte();     /* da qui in poi NON scriviamo più nell'header */
    }

    /* Se lo stream contiene più byte (immagine più grande), andrebbero eventualmente
       scartati leggendo e ignorando i byte rimanenti. . */

    /* 7) Calcola negativo in un buffer separato */
    negative(img_in, img_out, num_bytes, maxval);

    /* 8) Invia header originale così come ricevuto */
    for (i = 0; i < header_len; ++i) {
        uart_send_byte(header_buf[i]);
    }

    /* 9) Invia raster elaborato */
    for (i = 0; i < num_bytes; ++i) {
        uart_send_byte(img_out[i]);
    }
}


/* ============================================================
 *  MAIN
 * ============================================================ */

int main(void) {
    int Status;

    init_platform();

    Status = uart_init();
    if (Status != XST_SUCCESS) {
        /* In caso di errore UART non facciamo nulla:
         * niente stampe, nessun output.
         */
        while (1) {
            /* loop infinito di sicurezza */
        }
    }

    /* Esegui una singola sessione PPM (un'immagine) */
    ppm_microserver_once();

    /* Mantieni il programma attivo (opzionale) */
    while (1) {
        /* Se volessi supportare più immagini una dopo l'altra:
         * ppm_microserver_once();
         */
    }

    /* cleanup_platform();  // mai raggiunto */
    return 0;
}
