/***************************************************************
 *  Lab 1 – Polling and Interrupts (Part a: Polling Basic)
 *  Author: Lello Molinario
 *  University of Cagliari – Embedded Systems Lab
 *  Student ID: 70/90/000369
 *  Date: November 2025
 *
 *  Description:
 *  Simple polling-based program for Zybo Z7.
 *  Continuously reads the 4 input switches and mirrors
 *  their binary value directly on the LEDs.
 *  Includes UART log output for traceability.
 ***************************************************************/

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"

#define GPIO_LED_BASE   0x40000000
#define GPIO_SW_BASE    0x40010000
#define GPIO_TRI_OFFSET 0x4

int main(void)
{
    init_platform();
    xil_printf("Lab1 – Polling Basic (LED = SW)\r\n");

    // Configure directions
    *(volatile unsigned int*)(GPIO_SW_BASE  + GPIO_TRI_OFFSET) = 0xF; // 4-bit input
    *(volatile unsigned int*)(GPIO_LED_BASE + GPIO_TRI_OFFSET) = 0x0; // 4-bit output

    unsigned int last_value = 0;

    while (1) {
        unsigned int sw_value = *((volatile unsigned int*)(GPIO_SW_BASE));
        if (sw_value != last_value) { // update only on change
            *((volatile unsigned int*)(GPIO_LED_BASE)) = sw_value;
            xil_printf("SW=%04b -> LED=%04b\r\n", sw_value, sw_value);
            last_value = sw_value;
        }
    }
}
