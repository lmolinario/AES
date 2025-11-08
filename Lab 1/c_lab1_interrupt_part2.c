/***************************************************************
 *  Lab 1 – Polling and Interrupts (Part c: Interrupt v2)
 *  Author: Lello Molinario
 *  University of Cagliari – Embedded Systems Lab
 *  Student ID: 70/90/000369
 *  Date: November 2025
 *
 *  Description
 *  ------------------------------------------------------------
 *  Extended dual-interrupt program using the AXI Interrupt
 *  Controller (INTC) on the MicroBlaze (Zybo Z7 board).
 *
 *   • Switch interrupt (SW - IRQ0):
 *       On a switch change, reads the 4 input switches and updates
 *       the LEDs with the binary index (0..3) of the most-significant
 *       switch that is ON. If no switches are ON → LEDs = 0x00.
 *
 *   • Button interrupt (BTN - IRQ1):
 *       On a button press or release:
 *         1) takes the current 4-bit LED pattern,
 *         2) adds the last digit of the Student ID (9),
 *         3) inverts (~) the result and keeps only 4 bits.
 *
 *  Example behaviour:
 *      LED = 0010 (2)
 *      BTN: (2 + 9) = 11 = 0b1011
 *           ~1011 = ...0100 → 0100 (masked to 4 bits)
 *
 *  Features
 *  ------------------------------------------------------------
 *   • Two interrupt sources: SW (IRQ0) and BTN (IRQ1)
 *   • AXI INTC configuration: IER, MER, IISR, IIAR
 *   • Uses OR-masks (same style as Part (b))
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
 * ============================================================ */
#define GPIO_LED_BASE   0x40000000U   // AXI GPIO: LED output
#define GPIO_SW_BASE    0x40010000U   // AXI GPIO: Switch input
#define GPIO_BTN_BASE   0x40020000U   // AXI GPIO: Button input
#define GPIO_TRI_OFFSET 0x4U          // TRI: 1 = input, 0 = output
#define INTC_BASE       0x41200000U   // AXI Interrupt Controller

/* ============================================================
 *  AXI INTC REGISTERS
 * ============================================================ */
#define IISR (*(volatile unsigned int*)(INTC_BASE + 0x00U))  // Status
#define IER  (*(volatile unsigned int*)(INTC_BASE + 0x08U))  // Enable mask
#define IIAR (*(volatile unsigned int*)(INTC_BASE + 0x0CU))  // Ack source
#define MER  (*(volatile unsigned int*)(INTC_BASE + 0x1CU))  // Master enable

/* ============================================================
 *  GPIO INTERRUPT REGISTERS (local control)
 * ============================================================ */
#define GIER_SW   (*(volatile unsigned int*)(GPIO_SW_BASE  + 0x011CU))
#define IPIER_SW  (*(volatile unsigned int*)(GPIO_SW_BASE  + 0x0128U))
#define IPISR_SW  (*(volatile unsigned int*)(GPIO_SW_BASE  + 0x0120U))
#define GIER_BTN  (*(volatile unsigned int*)(GPIO_BTN_BASE + 0x011CU))
#define IPIER_BTN (*(volatile unsigned int*)(GPIO_BTN_BASE + 0x0128U))
#define IPISR_BTN (*(volatile unsigned int*)(GPIO_BTN_BASE + 0x0120U))

/* Last digit of Student ID: 70/90/000369 → 9 */
#define STUDENT_ID_DIGIT 9U

/* ============================================================
 *  FUNCTION PROTOTYPES
 * ============================================================ */
static void SystemInit(void);
static void GpioInit(void);
static void to_bin4(unsigned int value, char out[5]);
void myISR(void) __attribute__((interrupt_handler));

/***************************************************************
 *  MAIN PROGRAM
 ***************************************************************/
int main(void)
{
    init_platform();
    xil_printf("\r\n[Lab1 - Interrupt v2] Add(+9) + Invert logic\r\n");

    /* Configure interrupt controller and clear any stale flags */
    SystemInit();

    /* Interrupt mapping:
     *   IRQ0 → Switches GPIO
     *   IRQ1 → Push button GPIO
     */
    GpioInit();

    /* Register our ISR in the MicroBlaze exception system */
    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(
        XIL_EXCEPTION_ID_INT,
        (Xil_ExceptionHandler)myISR,
        NULL
    );
    Xil_ExceptionEnable();

    /* Enable interrupts globally in the CPU (MSR[IE] = 1) */
    microblaze_enable_interrupts();

    xil_printf("[System] Ready. Waiting for SW/BTN interrupts...\r\n");

    /* Idle loop – all logic is handled inside the ISR */
    while (1);

    /* Not reached, but provided for completeness */
    cleanup_platform();
    return 0;
}

/***************************************************************
 *  SystemInit
 *  ------------------------------------------------------------
 *  Brings the interrupt system (INTC + GPIO) to a clean state
 *  and then enables the two interrupt sources.
 ***************************************************************/
static void SystemInit(void)
{
    /* 1) Clear any pending interrupt flags (GPIO + INTC) */
    IPISR_SW  = 0x1;   // Clear SW GPIO interrupt (write-1-to-clear)
    IPISR_BTN = 0x1;   // Clear BTN GPIO interrupt
    IISR      = 0x3;   // Clear both IRQ0 and IRQ1 in INTC

    /* 2) Disable outputs while reconfiguring */
    GIER_SW = GIER_BTN = 0x0;
    IPIER_SW = IPIER_BTN = 0x0;
    IER = MER = 0x0;

    /* 3) Re-enable GPIO interrupt logic */
    GIER_SW  |= 0x80000000U;    // Global enable for SW GPIO
    GIER_BTN |= 0x80000000U;    // Global enable for BTN GPIO
    IPIER_SW  |= 0x1U;          // Enable channel 1 interrupt (SW)
    IPIER_BTN |= 0x1U;          // Enable channel 1 interrupt (BTN)

    /* 4) Configure and enable AXI INTC (using OR-mask as in Part b) */
    IER |= (0x1U | 0x2U);       // Enable IRQ0 (SW) ORed with IRQ1 (BTN)
    MER |= 0x3U;                // Master enable + hardware mode

    xil_printf("[SystemInit] INTC and GPIO interrupt lines configured.\r\n");
}

/***************************************************************
 *  GpioInit
 *  ------------------------------------------------------------
 *  Sets GPIO directions and initializes LED outputs.
 ***************************************************************/
static void GpioInit(void)
{
    *(volatile unsigned int*)(GPIO_SW_BASE  + GPIO_TRI_OFFSET)  = 0xF; // SW  = input (4 bits)
    *(volatile unsigned int*)(GPIO_BTN_BASE + GPIO_TRI_OFFSET)  = 0xF; // BTN = input
    *(volatile unsigned int*)(GPIO_LED_BASE + GPIO_TRI_OFFSET)  = 0x0; // LED = output
    *(volatile unsigned int*)(GPIO_LED_BASE) = 0x0;                    // Clear LEDs
}

/***************************************************************
 *  myISR
 *  ------------------------------------------------------------
 *  Central interrupt handler for both sources:
 *   - IRQ0: Switches  → MSB index on LEDs
 *   - IRQ1: Button    → (LED + 9) then bitwise invert
 ***************************************************************/
void myISR(void)
{
    volatile unsigned int pending = IISR;  // Snapshot of pending IRQ lines

    /* ---------------- SWITCH INTERRUPT (IRQ0) ---------------- */
    if (pending & 0x1U) {
        /* Read 4-bit switch value */
        unsigned int sw = *((volatile unsigned int*)(GPIO_SW_BASE)) & 0xF;

        /* Find index of most significant '1' (3..0), or -1 if none */
        int msb_index = -1;
        for (int i = 3; i >= 0; --i) {
            if (sw & (1U << i)) {
                msb_index = i;
                break;
            }
        }

        /* LED pattern = index of MSB, or 0x0 if no switch ON */
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
        /* Simple debounce (~50 ms).
         * BTN interrupt is level/edge-sensitive and may trigger
         * on both press and release (depending on HW config). */
        usleep(50000);

        /* Read current 4-bit LED value */
        unsigned int led = *((volatile unsigned int*)(GPIO_LED_BASE)) & 0xF;

        /* Part (c) behaviour:
         *   result = ~(LED + STUDENT_ID_DIGIT) & 0xF
         * so we keep only the lower 4 bits after inversion.
         */
        unsigned int result = (~(led + STUDENT_ID_DIGIT)) & 0xF;
        *((volatile unsigned int*)(GPIO_LED_BASE)) = result;

        /* UART trace with both input and result */
        char led_str[5], res_str[5];
        to_bin4(led, led_str);
        to_bin4(result, res_str);
        xil_printf("[BTN IRQ] (LED=%s + %d) inverted -> %s\r\n",
                   led_str, STUDENT_ID_DIGIT, res_str);

        /* Acknowledge BTN interrupt (GPIO + INTC) */
        IPISR_BTN = 0x1;
        IIAR      = 0x2;
    }
}

/***************************************************************
 *  to_bin4
 *  ------------------------------------------------------------
 *  Converts a 4-bit integer (0..15) into a binary string
 *  representation useful for UART logging.
 ***************************************************************/
static void to_bin4(unsigned int value, char out[5])
{
    for (int bit = 3; bit >= 0; --bit)
        out[3 - bit] = (value & (1U << bit)) ? '1' : '0';
    out[4] = '\0';
}
