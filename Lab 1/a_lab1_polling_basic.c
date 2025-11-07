/***************************************************************
 *  Lab 1 – Polling and Interrupts (Part a: Polling Basic)
 *  Author: Lello Molinario
 *  University of Cagliari – Embedded Systems Lab
 *  Student ID: 70/90/000369
 *  Date: November 2025
 *
 *  Description:
 *  ------------------------------------------------------------
 *  Simple polling-based program for the Zybo Z7 board.
 *  The program continuously reads the 4 input switches (GPIO_SW)
 *  and mirrors their binary value directly on the LEDs (GPIO_LED).
 *  A UART message is printed each time a state change is detected,
 *  for debugging and traceability.
 *
 ***************************************************************/

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"

/* ============================================================
 *  BASE ADDRESSES (from Vivado system design)
 * ============================================================ */
#define GPIO_LED_BASE     0x40000000U   // AXI GPIO base address for LEDs
#define GPIO_SW_BASE      0x40010000U   // AXI GPIO base address for switches
#define GPIO_TRI_OFFSET   0x4U          // Offset for TRI (direction) register

/* ============================================================
 *  FUNCTION PROTOTYPES
 * ============================================================ */
static void GpioInit(void);
static void UpdateLeds(unsigned int sw_value);

/* ============================================================
 *  MAIN PROGRAM
 * ============================================================ */
int main(void)
{
    /* --- Initialize platform (UART, caches, etc.) --- */
    init_platform();
    xil_printf("\r\n[Lab1 - Polling Basic] LED mirrors switch state\r\n");

    /* --- Configure GPIO directions and clear LEDs --- */
    GpioInit();

    unsigned int last_value = 0x0;   // store previous switch state

    /* --- Main polling loop --- */
    while (1) {
        /* Read the 4-bit switch value (mask lower nibble) */
        unsigned int sw_value = *((volatile unsigned int*)(GPIO_SW_BASE)) & 0xF;

        /* Update only when a change is detected */
        if (sw_value != last_value) {
            UpdateLeds(sw_value);

            /* Prepare binary representation for UART output */
            char sw_str[5]  = {0};
            char led_str[5] = {0};

            for (int bit = 3; bit >= 0; --bit) {
                sw_str[3 - bit]  = (sw_value & (1U << bit)) ? '1' : '0';
                led_str[3 - bit] = (sw_value & (1U << bit)) ? '1' : '0';
            }

            /* UART debug message */
            xil_printf("SW = %s -> LED = %s\r\n", sw_str, led_str);

            last_value = sw_value;
        }
    }

    /* Not reached (infinite loop), but included for completeness */
    cleanup_platform();
    return 0;
}

/* ============================================================
 *  FUNCTION DEFINITIONS
 * ============================================================ */

/**
 * @brief Configure GPIO direction registers.
 *        SW as input (TRI=1), LED as output (TRI=0).
 */
static void GpioInit(void)
{
    /* Set GPIO directions */
    *(volatile unsigned int*)(GPIO_SW_BASE  + GPIO_TRI_OFFSET) = 0xF; // 4-bit input
    *(volatile unsigned int*)(GPIO_LED_BASE + GPIO_TRI_OFFSET) = 0x0; // 4-bit output

    /* Initialize LEDs to OFF */
    *(volatile unsigned int*)(GPIO_LED_BASE) = 0x0;
}

/**
 * @brief Write the switch value to LEDs (lower 4 bits).
 * @param sw_value  4-bit value read from GPIO switches
 */
static void UpdateLeds(unsigned int sw_value)
{
    *(volatile unsigned int*)(GPIO_LED_BASE) = sw_value & 0xF;
}
