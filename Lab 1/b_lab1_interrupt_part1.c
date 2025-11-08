/***************************************************************
 *  Lab 1 – Polling and Interrupts (Part b: Interrupt v1)
 *  Author: Lello Molinario
 *  University of Cagliari – Embedded Systems Lab
 *  Student ID: 70/90/000369
 *  Date: November 2025
 *
 *  Description
 *  ------------------------------------------------------------
 *  Dual-interrupt program for the Zybo Z7 board (MicroBlaze).
 *  The system handles two interrupt sources using the AXI
 *  Interrupt Controller (INTC):
 *
 *   • Switch interrupt (SW):
 *       On switch change, reads the 4 input switches and sets
 *       the LEDs to display the binary index (0–3) of the most
 *       significant switch that is ON. If no switches are ON,
 *       LEDs show 0x0.
 *
 *   • Button interrupt (BTN):
 *       On button press or release, negates (bitwise invert)
 *       the current LED pattern (4 LSBs).
 *
 *  Example cycle:
 *       SW  = 0b0101 → MSB = 2 → LED = 0010
 *       BTN pressed  → invert  → LED = 1101
 *
 *  Features
 *  ------------------------------------------------------------
 *   • Two interrupt sources: SW (IRQ0) and BTN (IRQ1)
 *   • AXI INTC fully configured (IER, MER, IISR, IIAR)
 *   • OR-mask usage (assignment constraint)
 *   • Software debounce (~50 ms)
 *   • UART logging for traceability
 ***************************************************************/

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_exception.h"
#include "sleep.h"

/* ============================================================
 *  BASE ADDRESSES (from Vivado system design)
 * ============================================================ */
#define GPIO_LED_BASE   0x40000000U   // AXI GPIO: LEDs (output)
#define GPIO_SW_BASE    0x40010000U   // AXI GPIO: Switches (input)
#define GPIO_BTN_BASE   0x40020000U   // AXI GPIO: Buttons (input)
#define GPIO_TRI_OFFSET 0x4U          // TRI register offset (direction)
#define INTC_BASE       0x41200000U   // AXI Interrupt Controller base

/* ============================================================
 *  INTERRUPT CONTROLLER (AXI INTC) REGISTERS
 * ============================================================ */
#define IISR (*(volatile unsigned int*)(INTC_BASE + 0x00U))  // Interrupt Status
#define IER  (*(volatile unsigned int*)(INTC_BASE + 0x08U))  // Interrupt Enable
#define IAR  (*(volatile unsigned int*)(INTC_BASE + 0x0CU))  // Interrupt Acknowledge
#define MER  (*(volatile unsigned int*)(INTC_BASE + 0x1CU))  // Master Enable

/* ============================================================
 *  GPIO INTERRUPT REGISTERS (peripheral-level control)
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
static void to_bin4(unsigned int value, char out[5]);
void myISR_partB(void) __attribute__((interrupt_handler));

/***************************************************************
 *  MAIN FUNCTION
 ***************************************************************/
int main(void)
{
    init_platform();  // Initialize UART, caches, and peripherals
    xil_printf("\r\n[Lab1 - Interrupt v1] Starting program...\r\n");

    /* --- Configure system and GPIOs --- */
    SystemInit();
    GpioInit();

    /* --- Register interrupt handler --- */
    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(
        XIL_EXCEPTION_ID_INT,
        (Xil_ExceptionHandler)myISR_partB,
        NULL
    );
    Xil_ExceptionEnable();

    /* --- Enable MicroBlaze global interrupt (MSR[IE]=1) --- */
    microblaze_enable_interrupts();
    xil_printf("[System] Ready. Waiting for SW/BTN interrupts...\r\n");

    /* Idle loop – interrupts will handle everything */
    while (1);

    /* Not reached (infinite loop), but included for completeness */
    cleanup_platform();
    return 0;
}

/***************************************************************
 *  SystemInit
 *  ------------------------------------------------------------
 *  Initializes the interrupt controller and clears all flags.
 ***************************************************************/
static void SystemInit(void)
{
    /* Step 1: Clear any pending interrupts */
    IPISR_SW  = 0x1;
    IPISR_BTN = 0x1;
    IISR      = 0x3;

    /* Step 2: Disable all interrupt sources before configuration */
    GIER_SW = GIER_BTN = 0x0;
    IPIER_SW = IPIER_BTN = 0x0;
    IER = MER = 0x0;

    /* Step 3: Enable GPIO-level interrupts (using OR-masks) */
    GIER_SW  |= 0x80000000U;   // Global enable
    GIER_BTN |= 0x80000000U;
    IPIER_SW  |= 0x1U;         // Enable channel interrupt
    IPIER_BTN |= 0x1U;

    /* Step 4: Configure and enable INTC */
    IER |= (0x1U | 0x2U);      // Enable IRQ0 and IRQ1 ← OR-mask
    MER |= 0x3U;               // Master enable (ME | HIE) ← OR-mask

    xil_printf("[Init] INTC configured: IRQ0=SW, IRQ1=BTN\r\n");
}

/***************************************************************
 *  GpioInit
 *  ------------------------------------------------------------
 *  Sets direction of GPIO channels and clears LED output.
 ***************************************************************/
static void GpioInit(void)
{
    /* SW and BTN as input (TRI=1), LED as output (TRI=0) */
    *(volatile unsigned int*)(GPIO_SW_BASE  + GPIO_TRI_OFFSET)  = 0xF;
    *(volatile unsigned int*)(GPIO_BTN_BASE + GPIO_TRI_OFFSET)  = 0xF;
    *(volatile unsigned int*)(GPIO_LED_BASE + GPIO_TRI_OFFSET)  = 0x0;

    /* Clear LEDs at startup */
    *(volatile unsigned int*)(GPIO_LED_BASE) = 0x0;

    xil_printf("[Init] GPIOs configured (SW/BTN=IN, LED=OUT)\r\n");
}

/***************************************************************
 *  myISR_partB
 *  ------------------------------------------------------------
 *  Interrupt Service Routine (ISR) handling both sources:
 *   - IRQ0: Switch event
 *   - IRQ1: Button press/release
 ***************************************************************/
void myISR_partB(void)
{
    volatile unsigned int pending = IISR;  // Read pending flags

    /* ---------------- SWITCH INTERRUPT (IRQ0) ---------------- */
    if (pending & 0x1U)
    {
        unsigned int sw_value = *((volatile unsigned int*)(GPIO_SW_BASE)) & 0xF;

        /* Find index of most significant 1 bit (3..0) */
        int msb_index = -1;
        for (int i = 3; i >= 0; --i)
        {
            if (sw_value & (1U << i))
            {
                msb_index = i;
                break;
            }
        }

        /* LED pattern = MSB index or 0x0 if none active */
        unsigned int led_pattern = (msb_index < 0) ? 0x0U : (unsigned int)msb_index;
        *((volatile unsigned int*)(GPIO_LED_BASE)) = led_pattern;

        /* UART trace */
        char sw_str[5], led_str[5];
        to_bin4(sw_value, sw_str);
        to_bin4(led_pattern, led_str);
        xil_printf("[SW IRQ] SW=%s → LED=%s (MSB=%d)\r\n",
                   sw_str, led_str, (msb_index < 0 ? -1 : msb_index));

        /* Acknowledge SW interrupt */
        IPISR_SW = 0x1;
        IAR      = 0x1;
    }

    /* ---------------- BUTTON INTERRUPT (IRQ1) ---------------- */
    if (pending & 0x2U)
    {
        usleep(50000);  // Debounce delay (50 ms)

        /* Read and invert current LED pattern */
        unsigned int led = *((volatile unsigned int*)(GPIO_LED_BASE)) & 0xF;
        unsigned int inv = (~led) & 0xF;
        *((volatile unsigned int*)(GPIO_LED_BASE)) = inv;

        /* UART trace */
        char inv_str[5];
        to_bin4(inv, inv_str);
        xil_printf("[BTN IRQ] Inverted LED -> %s\r\n", inv_str);

        /* Acknowledge BTN interrupt */
        IPISR_BTN = 0x1;
        IAR       = 0x2;
    }
}

/***************************************************************
 *  to_bin4
 *  ------------------------------------------------------------
 *  Converts a 4-bit integer into a binary string (e.g. 0x5→"0101").
 ***************************************************************/
static void to_bin4(unsigned int value, char out[5])
{
    for (int bit = 3; bit >= 0; --bit)
        out[3 - bit] = (value & (1U << bit)) ? '1' : '0';
    out[4] = '\0';
}
