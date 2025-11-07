/***************************************************************
 *  Lab 1 – Polling and Interrupts (Part c: Interrupt v2)
 *  Author: Lello Molinario
 *  University of Cagliari – Embedded Systems Lab
 *  Student ID: 70/90/000369
 *  Date: November 2025
 *
 *  Description:
 *  ------------------------------------------------------------
 *  Extended dual-interrupt version using the AXI Interrupt Controller.
 *
 *   • SW interrupt:
 *        Reads the 4-bit switch value and displays the index of the
 *        most significant switch (MSB) that is ON.
 *
 *   • BTN interrupt:
 *        Adds the last digit of the Student ID (9) to the current
 *        LED pattern, then inverts (~) the result.
 *
 *  Features:
 *   • Two interrupt sources (SW + BTN)
 *   • Software debounce (50 ms)
 *   • UART debug messages for traceability
 *   • Clean and modular structure
 ***************************************************************/


#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "sleep.h"

/* ============================================================
 *  BASE ADDRESSES (from Vivado hardware design)
 *  Each AXI GPIO has two channels:
 *   - DATA register (offset 0x0)
 *   - TRI  register (offset 0x4) for direction
 * ============================================================ */
#define GPIO_LED_BASE   0x40000000U   // LED output GPIO
#define GPIO_SW_BASE    0x40010000U   // Switch input GPIO
#define GPIO_BTN_BASE   0x40020000U   // Button input GPIO
#define GPIO_TRI_OFFSET 0x4U          // Direction register offset
#define INTC_BASE       0x41200000U   // AXI Interrupt Controller

/* ============================================================
 *  INTERRUPT CONTROLLER (AXI INTC) REGISTERS
 *  ------------------------------------------------------------
 *  IISR : Interrupt Status Register
 *  IER  : Interrupt Enable Register
 *  IIAR : Interrupt Acknowledge Register
 *  MER  : Master Enable Register (global enable)
 * ============================================================ */
#define IISR (*(volatile unsigned int*)(INTC_BASE + 0x00U))
#define IER  (*(volatile unsigned int*)(INTC_BASE + 0x08U))
#define IIAR (*(volatile unsigned int*)(INTC_BASE + 0x0CU))
#define MER  (*(volatile unsigned int*)(INTC_BASE + 0x1CU))

/* ============================================================
 *  GPIO INTERRUPT REGISTERS (local interrupt control)
 *  ------------------------------------------------------------
 *  GIER : Global Interrupt Enable
 *  IPIER: Interrupt Enable Register
 *  IPISR: Interrupt Status Register
 * ============================================================ */
#define GIER_SW   (*(volatile unsigned int*)(GPIO_SW_BASE  + 0x011CU))
#define IPIER_SW  (*(volatile unsigned int*)(GPIO_SW_BASE  + 0x0128U))
#define IPISR_SW  (*(volatile unsigned int*)(GPIO_SW_BASE  + 0x0120U))
#define GIER_BTN  (*(volatile unsigned int*)(GPIO_BTN_BASE + 0x011CU))
#define IPIER_BTN (*(volatile unsigned int*)(GPIO_BTN_BASE + 0x0128U))
#define IPISR_BTN (*(volatile unsigned int*)(GPIO_BTN_BASE + 0x0120U))

/* Last digit of Student ID (used in BTN interrupt arithmetic) */
#define STUDENT_ID_DIGIT 9U // My Student ID: 70/90/000369 ---> 9

/* ============================================================
 *  FUNCTION PROTOTYPES
 * ============================================================ */
static void to_bin4(unsigned int value, char out[5]);   // Convert 4-bit integer to binary string
void myISR(void) __attribute__((interrupt_handler));    // Declare ISR with MicroBlaze attribute

/* ============================================================
 *  MAIN PROGRAM
 * ============================================================ */
int main(void)
{
    /* Initialize hardware platform:
     * - UART (for xil_printf)
     * - caches and basic peripherals
     */
        init_platform();
    xil_printf("\r\n[Lab1 - Interrupt v2] Add + Invert logic\r\n");

    /* ------------------------------------------------------------
     *  Configure GPIO directions
     *  TRI = 1 → input
     *  TRI = 0 → output
     * ------------------------------------------------------------ */
    *(volatile unsigned int*)(GPIO_SW_BASE  + GPIO_TRI_OFFSET)  = 0xF; // SW as 4-bit input
    *(volatile unsigned int*)(GPIO_BTN_BASE + GPIO_TRI_OFFSET)  = 0xF; // BTN as input
    *(volatile unsigned int*)(GPIO_LED_BASE + GPIO_TRI_OFFSET)  = 0x0; // LED as output

    /* Clear LEDs at startup to defined state */
    *(volatile unsigned int*)(GPIO_LED_BASE) = 0x0;

    /* ------------------------------------------------------------
     *  Enable GPIO interrupt logic
     *  Each peripheral must enable its own interrupt output
     *  before the INTC can aggregate them.
     * ------------------------------------------------------------ */
    GIER_SW   = 0x80000000;   // Global enable for SW GPIO
    GIER_BTN  = 0x80000000;   // Global enable for BTN GPIO
    IPIER_SW  = 0x1;          // Enable interrupt from channel 1
    IPIER_BTN = 0x1;          // Enable interrupt from channel 1

    /* ------------------------------------------------------------
     *  Configure and enable AXI Interrupt Controller
     * ------------------------------------------------------------ */
    IER = 0x3;   // Enable IRQ0 (SW) and IRQ1 (BTN)
    MER = 0x3;   // Enable master interrupt output and hardware mode
    microblaze_enable_interrupts(); // Enable interrupts globally in CPU

    xil_printf("[System] Interrupt controller configured. Waiting for events...\r\n");

    /* Idle loop: all logic handled by ISR */
    while (1);

    cleanup_platform();
    return 0;
}

/* ============================================================
 *  INTERRUPT SERVICE ROUTINE (ISR)
 *  ------------------------------------------------------------
 *  This function handles both SW and BTN interrupts.
 *  The INTC indicates which sources are pending.
 * ============================================================ */
void myISR(void)
{
    unsigned int pending = IISR;  // snapshot of pending interrupt sources

    /* ----------------------- SW INTERRUPT ----------------------- */
    if (pending & 0x1U) {
        /* Read switch state (4 bits only) */
        unsigned int sw = *((volatile unsigned int*)(GPIO_SW_BASE)) & 0xF;

        /* Determine the most significant bit (MSB) that is ON */
        int msb_index = -1;
        for (int i = 3; i >= 0; --i) {
            if (sw & (1U << i)) {
                msb_index = i;
                break;
            }
        }

        /* LED pattern = binary index of MSB active switch */
        unsigned int pattern = (msb_index < 0) ? 0U : (unsigned int)msb_index;
        *((volatile unsigned int*)(GPIO_LED_BASE)) = pattern;

        /* Convert to human-readable binary strings for UART */
        char sw_str[5], led_str[5];
        to_bin4(sw, sw_str);
        to_bin4(pattern, led_str);

        /* Print debug message */
        if (msb_index < 0)
            xil_printf("[SW IRQ] SW=%s -> LED=%s (index=none)\r\n", sw_str, led_str);
        else
            xil_printf("[SW IRQ] SW=%s -> LED=%s (index=%d)\r\n", sw_str, led_str, msb_index);

        /* Acknowledge interrupt:
         * - Clear GPIO IP interrupt flag
         * - Acknowledge INTC source
         */
        IPISR_SW = 0x1;
        IIAR     = 0x1;
    }

    /* ---------------------- BTN INTERRUPT ----------------------- */
    if (pending & 0x2U) {
        /* Software debounce (~50 ms) */
        usleep(50000);

        /* Read current LED state */
        unsigned int led = *((volatile unsigned int*)(GPIO_LED_BASE)) & 0xF;

        /* Perform arithmetic + inversion
         * Step 1: Add Student ID digit (9)
         * Step 2: Invert result bitwise
         * Step 3: Mask to 4 bits
         */
        unsigned int result = (~(led + STUDENT_ID_DIGIT)) & 0xF;
        *((volatile unsigned int*)(GPIO_LED_BASE)) = result;

        /* Prepare UART log strings */
        char led_str[5], result_str[5];
        to_bin4(led, led_str);
        to_bin4(result, result_str);

        xil_printf("[BTN IRQ] (%s + %d) inverted -> %s\r\n",
                   led_str, STUDENT_ID_DIGIT, result_str);

        /* Acknowledge interrupt (clear local + INTC flag) */
        IPISR_BTN = 0x1;
        IIAR      = 0x2;
    }
}

/* ============================================================
 *  SUPPORT FUNCTION
 *  ------------------------------------------------------------
 *  Converts a 4-bit integer (0–15) into a null-terminated
 *  string of '0' and '1' characters for UART logging.
 * ============================================================ */
static void to_bin4(unsigned int value, char out[5])
{
    for (int bit = 3; bit >= 0; --bit)
        out[3 - bit] = (value & (1U << bit)) ? '1' : '0';
    out[4] = '\0'; // terminate string
}
