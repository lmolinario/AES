/***************************************************************
 *  Lab 1 – Polling and Interrupts (Part a: Polling Basic)
 *  Author: Lello Molinario
 *  University of Cagliari – Embedded Systems Lab
 *  Student ID: 70/90/000369
 *  Date: November 2025
 *
 *  Description:
 *  ------------------------------------------------------------
 *  Simple polling-based program for Zybo Z7.
 *  Continuously reads the 4 input switches (GPIO_SW)
 *  and mirrors their binary value directly on the LEDs (GPIO_LED).
 *  UART messages are printed each time a state change is detected,
 *  for easier debugging and traceability.
 ***************************************************************/

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"

/* ============================================================
 *  BASE ADDRESSES (from hardware design)
 * ============================================================ */
#define GPIO_LED_BASE     0x40000000U   // AXI GPIO base for LEDs
#define GPIO_SW_BASE      0x40010000U   // AXI GPIO base for switches
#define GPIO_TRI_OFFSET   0x4U          // Offset of TRI register (direction)

/* ============================================================
 *  MAIN PROGRAM
 * ============================================================ */
int main(void)
{
    /* --- Platform initialization --- */
    init_platform();   // Initialize BSP (UART, caches, etc.)
    xil_printf("\r\n[Lab1 - Polling Basic] LED mirrors switch state\r\n");

    /* --- Configure GPIO directions ---
     * TRI = 1 → input
     * TRI = 0 → output
     * Only lower 4 bits are used (SW[3:0], LED[3:0]).
     */
    *(volatile unsigned int*)(GPIO_SW_BASE  + GPIO_TRI_OFFSET) = 0xF; // 4-bit input
    *(volatile unsigned int*)(GPIO_LED_BASE + GPIO_TRI_OFFSET) = 0x0; // 4-bit output

    /* --- Initialize LEDs to OFF --- */
    *(volatile unsigned int*)(GPIO_LED_BASE) = 0x0;

    unsigned int last_value = 0x0;   // Last switch state

    /* --- Main polling loop --- */
    while (1) {
        /* Read 4-bit switch state */
        unsigned int sw_value = *((volatile unsigned int*)(GPIO_SW_BASE)) & 0xF;

        /* Update LEDs only when state changes */
        if (sw_value != last_value) {

            /* Write to LED output */
            *((volatile unsigned int*)(GPIO_LED_BASE)) = sw_value;

            /* Prepare human-readable binary strings */
            char sw_str[5] = {0};
            char led_str[5] = {0};
            for (int bit = 3; bit >= 0; --bit) {
                sw_str[3 - bit]  = (sw_value & (1U << bit)) ? '1' : '0';
                led_str[3 - bit] = (sw_value & (1U << bit)) ? '1' : '0';
            }

            /* Print state change over UART */
            xil_printf("SW = %s -> LED = %s\r\n", sw_str, led_str);

            /* Store new value */
            last_value = sw_value;
        }
    }

    /* --- Never reached, but good practice --- */
    cleanup_platform();
    return 0;
}
