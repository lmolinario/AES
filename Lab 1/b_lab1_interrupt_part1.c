/***************************************************************
 *  Lab 1 – Polling and Interrupts (Part b: Interrupt v1)
 *  Author: Lello Molinario
 *  University of Cagliari – Embedded Systems Lab
 *  Student ID: 70/90/000369
 *  Date: November 2025
 *
 *  Description
 *  ------------------------------------------------------------
 *  Dual-interrupt program using the AXI Interrupt Controller (INTC)
 *  on the MicroBlaze processor (Zybo Z7 board).
 *
 *   • Switch interrupt (SW):
 *       On a switch change, reads the 4 input switches and updates
 *       the LEDs with the binary index (0..3) of the most-significant
 *       switch that is ON. If no switches are ON → LEDs = 0x00.
 *
 *   • Button interrupt (BTN):
 *       On a button press or release, negates (bitwise invert, ~)
 *       the current LED pattern (4 least significant bits).
 *
 *  Example cycle:
 *       SW  = 0b0101 → MSB = 2 → LED = 0010
 *       BTN pressed  → invert  → LED = 1101 (final)
 *
 *  Features
 *  ------------------------------------------------------------
 *   • Two interrupt sources: SW (IRQ0) and BTN (IRQ1)
 *   • AXI INTC configuration: IER, MER, IISR, IIAR
 *   • Uses OR-masks (assignment constraint)
 *   • Software debounce for BTN (~50 ms)
 *   • UART logging for traceability and debugging
 ***************************************************************/

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_exception.h"
#include "sleep.h"

/* ============================================================
 *  BASE ADDRESSES (from Vivado hardware design)
 *  Each AXI GPIO has:
 *   - DATA register (offset 0x0)
 *   - TRI register  (offset 0x4) for direction
 *       1 = input, 0 = output
 * ============================================================ */
#define GPIO_LED_BASE   0x40000000U   // AXI GPIO: LED output
#define GPIO_SW_BASE    0x40010000U   // AXI GPIO: Switch input
#define GPIO_BTN_BASE   0x40020000U   // AXI GPIO: Button input
#define GPIO_TRI_OFFSET 0x4U          // Direction register offset
#define INTC_BASE       0x41200000U   // AXI Interrupt Controller

/* ============================================================
 *  INTERRUPT CONTROLLER (AXI INTC) REGISTERS
 * ============================================================ */
#define IISR (*(volatile unsigned int*)(INTC_BASE + 0x00U))  // Status
#define IER  (*(volatile unsigned int*)(INTC_BASE + 0x08U))  // Enable
#define IIAR (*(volatile unsigned int*)(INTC_BASE + 0x0CU))  // Ack source
#define MER  (*(volatile unsigned int*)(INTC_BASE + 0x1CU))  // Master enable

/* ============================================================
 *  GPIO INTERRUPT REGISTERS (local control per peripheral)
 * ============================================================ */
#define GIER_SW   (*(volatile unsigned int*)(GPIO_SW_BASE  + 0x011CU))
#define IPIER_SW  (*(volatile unsigned int*)(GPIO_SW_BASE  + 0x0128U))
#define IPISR_SW  (*(volatile unsigned int*)(GPIO_SW_BASE  + 0x0120U))
#define GIER_BTN  (*(volatile unsigned int*)(GPIO_BTN_BASE + 0x011CU))
#define IPIER_BTN (*(volatile unsigned int*)(GPIO_BTN_BASE + 0x0128U))
#define IPISR_BTN (*(volatile unsigned int*)(GPIO_BTN_BASE + 0x0120U))

/* ============================================================
 *  FUNCTION PROTOTYPES
 * ============================================================ */
static void SystemInit(void);
static void GpioInit(void);
static void to_bin4(unsigned int value, char out[5]); // Convert 4-bit int to "0101"
void myISR(void) __attribute__((interrupt_handler));  // MicroBlaze ISR attribute

/***************************************************************
 *  MAIN PROGRAM
 ***************************************************************/
int main(void)
{
    init_platform();  // Init UART, caches, peripherals
    xil_printf("\r\n[Lab1 - Interrupt v1] MSB index + Invert logic\r\n");

    /* Configure interrupt controller and clear any stale flags */
    SystemInit();

    /* Interrupt mapping:
     *   IRQ0 → Switch GPIO
     *   IRQ1 → Push Button GPIO
     */
    GpioInit();  // Configure GPIO directions

    /* Register ISR in MicroBlaze exception system */
    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(
        XIL_EXCEPTION_ID_INT,
        (Xil_ExceptionHandler)myISR,
        NULL
    );
    Xil_ExceptionEnable();

    microblaze_enable_interrupts();  // Global enable (MSR[IE]=1)
    xil_printf("[System] Ready. Waiting for SW/BTN interrupts...\r\n");

    /* Infinite loop – CPU sleeps between interrupts */
    while (1);

    /* Not reached, but provided for completeness */
    cleanup_platform();
    return 0;
}
/***************************************************************
 *  SystemInit
 *  ------------------------------------------------------------
 *  Initializes the interrupt controller and peripherals in
 *  a known, clean state. Prevents stale interrupt conditions
 *  after reprogramming or soft-reset.
 ***************************************************************/
static void SystemInit(void)
{
    /* --- Step 1: clear pending flags --- */
    IPISR_SW  = 0x1;   // Clear SW GPIO interrupt (write-1-to-clear)
    IPISR_BTN = 0x1;   // Clear BTN GPIO interrupt
    IISR      = 0x3;   // Clear both IRQ0 and IRQ1 in INTC

    /* --- Step 2: disable all interrupt outputs --- */
    GIER_SW = GIER_BTN = 0x0;
    IPIER_SW = IPIER_BTN = 0x0;
    IER = MER = 0x0;

    /* --- Step 3: re-enable GPIO interrupt logic --- */
    /* Use OR-masks as required by the assignment constraint */
    GIER_SW  |= 0x80000000U;    // Global enable for SW GPIO
    GIER_BTN |= 0x80000000U;    // Global enable for BTN GPIO
    IPIER_SW  |= 0x1U;          // Enable channel 1 interrupt
    IPIER_BTN |= 0x1U;

    /* --- Step 4: configure and enable AXI INTC --- */
    IER |= (0x1U | 0x2U);       // Enable IRQ0 (SW) ORed with IRQ1 (BTN)
    MER |= 0x3U;                // Enable master + hardware mode

    xil_printf("[SystemInit] INTC and GPIO interrupt lines configured.\r\n");
}

/***************************************************************
 *  GpioInit
 *  ------------------------------------------------------------
 *  Sets direction of GPIO channels and initializes LEDs.
 ***************************************************************/
static void GpioInit(void)
{
    *(volatile unsigned int*)(GPIO_SW_BASE  + GPIO_TRI_OFFSET)  = 0xF; // SW = input
    *(volatile unsigned int*)(GPIO_BTN_BASE + GPIO_TRI_OFFSET)  = 0xF; // BTN = input
    *(volatile unsigned int*)(GPIO_LED_BASE + GPIO_TRI_OFFSET)  = 0x0; // LED = output
    *(volatile unsigned int*)(GPIO_LED_BASE) = 0x0;                    // Clear LEDs
}

/***************************************************************
 *  myISR
 *  ------------------------------------------------------------
 *  Interrupt handler for both sources (SW:IRQ0, BTN:IRQ1).
 *  Each event is identified using INTC pending bits.
 ***************************************************************/
void myISR(void)
{
    volatile unsigned int pending = IISR;  // Snapshot pending sources

    /* ---------------- SWITCH INTERRUPT (IRQ0) ---------------- */
    if (pending & 0x1U) {
        unsigned int sw = *((volatile unsigned int*)(GPIO_SW_BASE)) & 0xF;

        /* Find index of most significant '1' (3..0), or -1 if none */
        int msb_index = -1;
        for (int i = 3; i >= 0; --i) {
            if (sw & (1U << i)) {
                msb_index = i;
                break;
            }
        }

        /* LED pattern = MSB index or 0x00 if no switch is ON */
        unsigned int led_pattern = (msb_index < 0) ? 0x0U : (unsigned int)msb_index;
        *((volatile unsigned int*)(GPIO_LED_BASE)) = led_pattern;

        /* UART debug (binary strings) */
        char sw_str[5], led_str[5];
        to_bin4(sw, sw_str);
        to_bin4(led_pattern, led_str);
        xil_printf("[SW IRQ] SW=%s → LED=%s (MSB=%d)\r\n",
                   sw_str, led_str, (msb_index < 0 ? -1 : msb_index));

        /* Acknowledge SW interrupt (GPIO + INTC) */
        IPISR_SW = 0x1;
        IIAR     = 0x1;
    }

    /* ---------------- BUTTON INTERRUPT (IRQ1) ---------------- */
    if (pending & 0x2U) {
        /* Debounce delay (~50 ms)
         * Note: button triggers both press & release (active-low). */
        usleep(50000);

        /* Invert 4-bit LED pattern */
        unsigned int led = *((volatile unsigned int*)(GPIO_LED_BASE)) & 0xF;
        unsigned int inv = (~led) & 0xF;
        *((volatile unsigned int*)(GPIO_LED_BASE)) = inv;

        /* UART trace for visibility */
        char inv_str[5];
        to_bin4(inv, inv_str);
        xil_printf("[BTN IRQ] Invert LED -> %s\r\n", inv_str);

        /* Acknowledge BTN interrupt (GPIO + INTC) */
        IPISR_BTN = 0x1;
        IIAR      = 0x2;
    }
}

/***************************************************************
 *  to_bin4
 *  ------------------------------------------------------------
 *  Converts a 4-bit integer into a binary string for UART logs.
 *  Example: 0x5 → "0101"
 ***************************************************************/
static void to_bin4(unsigned int value, char out[5])
{
    for (int bit = 3; bit >= 0; --bit)
        out[3 - bit] = (value & (1U << bit)) ? '1' : '0';
    out[4] = '\0';
}
