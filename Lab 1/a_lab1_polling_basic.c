/***************************************************************
 *  Lab 1 – Polling and Interrupts (Part a: Polling Basic)
 *  Author: Lello Molinario
 *  University of Cagliari – Embedded Systems Lab
 *  Student ID: 70/90/000369
 *  Date: November 2025
 *
 *  Description:
 *  ------------------------------------------------------------
 *  Basic polling implementation for the Zybo Z7 board.
 *  The program continuously reads the 4 input switches (GPIO_SW)
 *  and mirrors their binary pattern on the 4 LEDs (GPIO_LED).
 *  A UART message is printed only when a state change is detected.
 *
 *  Functional behavior:
 *   SW = 0101 → LED = 0101
 *   SW = 1111 → LED = 1111
 *   SW = 0000 → LED = 0000
 ***************************************************************/

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "sleep.h"   // optional delay for readability


/* ============================================================
 *  MEMORY-MAPPED BASE ADDRESSES (from Vivado system design)
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
 *  MAIN FUNCTION
 * ============================================================ */
int main(void)
{
    /* --- Initialize the hardware platform (UART, caches, etc.) --- */
    init_platform();
    xil_printf("\r\n[Lab1 – Polling Basic] Starting switch-to-LED mirroring...\r\n");

    /* --- Configure GPIO directions and initialize LEDs --- */
    GpioInit();

    unsigned int last_value = 0x0;   // stores last switch state

    /* --- Main polling loop ---
     * The board continuously reads the 4 switches.
     * The LEDs are updated only when a change is detected.
     */
    while (1)
    {
        /* Read the 4-bit switch value (mask lower nibble) */
        volatile unsigned int sw_value = *((volatile unsigned int*)(GPIO_SW_BASE)) & 0xF;

        /* Update LEDs only on state change */
        if (sw_value != last_value)
        {
            UpdateLeds(sw_value);

            /* Convert binary values to string for UART output */
            char sw_str[5]  = {0};
            char led_str[5] = {0};

            for (int bit = 3; bit >= 0; --bit)
            {
                sw_str[3 - bit]  = (sw_value & (1U << bit)) ? '1' : '0';
                led_str[3 - bit] = (sw_value & (1U << bit)) ? '1' : '0';
            }

            /* Debug output via UART */
            xil_printf("SW = %s  ->  LED = %s\r\n", sw_str, led_str);

            last_value = sw_value;
        }

        /* Optional debounce or pacing delay (for UART readability) */
        usleep(5000);  // 5 ms delay (optional)
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
 *        Switches are inputs (TRI = 1), LEDs are outputs (TRI = 0).
 */
static void GpioInit(void)
{
    /* Configure direction registers */
    *(volatile unsigned int*)(GPIO_SW_BASE  + GPIO_TRI_OFFSET) = 0xF; // 4-bit input
    *(volatile unsigned int*)(GPIO_LED_BASE + GPIO_TRI_OFFSET) = 0x0; // 4-bit output

    /* Initialize LEDs to OFF (all zeros) */
    *(volatile unsigned int*)(GPIO_LED_BASE) = 0x0;

    xil_printf("[Init] GPIO configured: SW=input, LED=output\r\n");
}


/**
 * @brief Update LEDs according to the 4-bit switch value.
 * @param sw_value  4-bit value read from the switch GPIO
 */
static void UpdateLeds(unsigned int sw_value)
{
    *(volatile unsigned int*)(GPIO_LED_BASE) = sw_value & 0xF;
}
