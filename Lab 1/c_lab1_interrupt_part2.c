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
 *         3) inverts (~) the result and masks to 4 bits.
 *
 *  Example behaviour:
 *      SW=0100 → LED=0010 (MSB=2)
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
#define GPIO_TRI_OFFSET 0x4U          // Direction (1=input, 0=output)
#define INTC_BASE       0x41200000U   // AXI Interrupt Controller

/* ============================================================
 *  INTERRUPT CONTROLLER REGISTERS (AXI INTC)
 * ============================================================ */
#define IISR (*(volatile unsigned int*)(INTC_BASE + 0x00U))  // Status
#define IER  (*(volatile unsigned int*)(INTC_BASE + 0x08U))  // Enable mask
#define IAR  (*(volatile unsigned int*)(INTC_BASE + 0x0CU))  // Acknowledge
#define MER  (*(volatile unsigned int*)(INTC_BASE + 0x1CU))  // Master Enable

/* ============================================================
 *  GPIO INTERRUPT REGISTERS
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
void myISR_partC(void) __attribute__((interrupt_handler));

/***************************************************************
 *  MAIN PROGRAM
 ***************************************************************/
int main(void)
{
    init_platform();
    xil_printf("\r\n[Lab1 - Interrupt v2] Add(+9) + Invert logic\r\n");


    /* UART Debug Insight:
    * Output visible through RealTerm (Windows) or any serial terminal
    * at 115200 baud, 8 data bits, no parity, 1 stop bit (8N1).
    * Each switch change triggers a UART message showing the binary LED mapping.
    */

    SystemInit();   // Configure INTC + GPIO interrupt logic
    GpioInit();     // Set GPIO directions

    /* Register interrupt handler via Xilinx exception API */
    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(
        XIL_EXCEPTION_ID_INT,
        (Xil_ExceptionHandler)myISR_partC,
        NULL
    );
    Xil_ExceptionEnable();

    /* Enable interrupts globally on MicroBlaze (MSR[IE] = 1) */
    microblaze_enable_interrupts();
    xil_printf("[System] Ready. Waiting for SW/BTN interrupts...\r\n");

    while (1);  // Idle loop – logic handled in ISR

    /* Not reached: the program runs indefinitely inside the main loop.
    * Included for completeness and good practice when using Xilinx SDK.
    * cleanup_platform() would de-initialize UART/caches if ever reached. */
    cleanup_platform();
    return 0;
}

/***************************************************************
 *  SystemInit
 *  ------------------------------------------------------------
 *  Initializes the interrupt controller and GPIO interrupt logic.
 ***************************************************************/
static void SystemInit(void)
{

    /* NOTE ON 'volatile':
    * 'volatile' ensures that each read/write actually accesses
    * the physical hardware registers instead of cached values.
    */

    /* Clear pending interrupts */
    IPISR_SW  = 0x1;
    IPISR_BTN = 0x1;
    IISR      = 0x3;

    /* Disable all before configuration */
    GIER_SW = GIER_BTN = 0x0;
    IPIER_SW = IPIER_BTN = 0x0;
    IER = MER = 0x0;

    /* Re-enable GPIO interrupt logic (OR-mask constraint) */
    GIER_SW  |= 0x80000000U;  // Global enable
    GIER_BTN |= 0x80000000U;
    IPIER_SW  |= 0x1U; // Enable channel interrupt
    IPIER_BTN |= 0x1U;

    /* Configure and enable AXI INTC
    * OR-mask logic is used to preserve previous bits when enabling
    * multiple interrupt lines simultaneously.
    */
    IER |= (0x1U | 0x2U);   // Enable IRQ0 and IRQ1 ← OR-mask
    MER |= 0x3U;            // Master + Hardware enable ← OR-mask

    xil_printf("[SystemInit] INTC and GPIO interrupt lines configured.\r\n");
}

/***************************************************************
 *  GpioInit
 *  ------------------------------------------------------------
 *  Sets direction of GPIO channels and clears LED outputs.
 ***************************************************************/
static void GpioInit(void)
{
    *(volatile unsigned int*)(GPIO_SW_BASE  + GPIO_TRI_OFFSET)  = 0xF; // SW=input
    *(volatile unsigned int*)(GPIO_BTN_BASE + GPIO_TRI_OFFSET)  = 0xF; // BTN=input
    *(volatile unsigned int*)(GPIO_LED_BASE + GPIO_TRI_OFFSET)  = 0x0; // LED=output
    *(volatile unsigned int*)(GPIO_LED_BASE) = 0x0;                    // Clear LEDs
}

/***************************************************************
 *  myISR_partC
 *  ------------------------------------------------------------
 *  Central ISR handling both sources:
 *   - IRQ0: Switches → LED = index of MSB switch ON
 *   - IRQ1: Button   → (LED + 9), invert (~), mask 4 bits
 ***************************************************************/
void myISR_partC(void)
{
    volatile unsigned int pending = IISR;

    /* ---------------- SWITCH INTERRUPT (IRQ0) ---------------- */
    if (pending & 0x1U)
    {
        unsigned int sw = *((volatile unsigned int*)(GPIO_SW_BASE)) & 0xF;
        int msb_index = -1;
        for (int i = 3; i >= 0; --i)
        {
            if (sw & (1U << i))
            {
                msb_index = i;
                break;
            }
        }

        unsigned int led_pattern = (msb_index < 0) ? 0x0U : (unsigned int)msb_index;
        *((volatile unsigned int*)(GPIO_LED_BASE)) = led_pattern;

        char sw_str[5], led_str[5];
        to_bin4(sw, sw_str);
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
        /* Simple debounce (~50 ms) */
        usleep(50000);

        /* Read current 4-bit LED value */
        unsigned int led = *((volatile unsigned int*)(GPIO_LED_BASE)) & 0xF;

        /* Part (c) behavior:
         * result = ~(LED + STUDENT_ID_DIGIT) & 0xF
         * (inversion of the sum, masked to 4 bits)
         */
        unsigned int result = (~(led + STUDENT_ID_DIGIT)) & 0xF;
        *((volatile unsigned int*)(GPIO_LED_BASE)) = result;

        /* UART trace */
        char led_str[5], res_str[5];
        to_bin4(led, led_str);
        to_bin4(result, res_str);
        xil_printf("[BTN IRQ] (LED=%s + %d) inverted -> %s\r\n",
                   led_str, STUDENT_ID_DIGIT, res_str);

        /* Acknowledge BTN interrupt */
        IPISR_BTN = 0x1;
        IAR       = 0x2;
    }
}

/***************************************************************
 *  to_bin4
 *  ------------------------------------------------------------
 *  Converts a 4-bit integer into a binary string (e.g. 0x5 → "0101").
 ***************************************************************/
static void to_bin4(unsigned int value, char out[5])
{
    for (int bit = 3; bit >= 0; --bit)
        out[3 - bit] = (value & (1U << bit)) ? '1' : '0';
    out[4] = '\0';
}



/***************************************************************
 *  SYSTEM OPERATION SUMMARY
 *  ------------------------------------------------------------
 *  - INTC aggregates two IRQ sources (SW, BTN)
 *  - SW IRQ updates LEDs with index of MSB active switch
 *  - BTN IRQ computes ~(LED + 9) & 0xF and updates LEDs
 *  - UART provides trace logs for both ISR events
 ***************************************************************/

/***************************************************************
 *  TECHNICAL NOTES
 *  ------------------------------------------------------------
 *  • Proper interrupt servicing order: clear peripheral → clear INTC
 *  • OR-mask enables multiple lines without overwriting bits
 *  • 'volatile' ensures reliable hardware access
 *  • Debounce delay avoids spurious BTN triggers
 *  • The 4-bit mask confines LED results to valid range (0–15)
 ***************************************************************/