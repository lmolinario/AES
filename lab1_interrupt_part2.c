/***************************************************************
 *  Lab 1 – Polling and Interrupts (Part c: Interrupt v2)
 *  Author: Lello Molinario
 *  University of Cagliari – Embedded Systems Lab
 *  Student ID: 70/90/000369
 *  Date: November 2025
 *
 *  Description:
 *  Extended dual-interrupt version.
 *   • SW interrupt: show MSB of active switch.
 *   • BTN interrupt: add last digit of Student ID (9),
 *     then invert (~) the result.
 *  Bonus: UART debug messages + debounce delay.
 ***************************************************************/

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "sleep.h"

#define GPIO_LED_BASE   0x40000000
#define GPIO_SW_BASE    0x40010000
#define GPIO_BTN_BASE   0x40020000
#define GPIO_TRI_OFFSET 0x4
#define INTC_BASE       0x41200000

#define IISR (*(volatile unsigned int*)(INTC_BASE + 0x00))
#define IER  (*(volatile unsigned int*)(INTC_BASE + 0x08))
#define IIAR (*(volatile unsigned int*)(INTC_BASE + 0x0C))
#define MER  (*(volatile unsigned int*)(INTC_BASE + 0x1C))

#define GIER_SW   (*(volatile unsigned int*)(GPIO_SW_BASE  + 0x011C))
#define IPIER_SW  (*(volatile unsigned int*)(GPIO_SW_BASE  + 0x0128))
#define IPISR_SW  (*(volatile unsigned int*)(GPIO_SW_BASE  + 0x0120))
#define GIER_BTN  (*(volatile unsigned int*)(GPIO_BTN_BASE + 0x011C))
#define IPIER_BTN (*(volatile unsigned int*)(GPIO_BTN_BASE + 0x0128))
#define IPISR_BTN (*(volatile unsigned int*)(GPIO_BTN_BASE + 0x0120))

#define MATRICOLA_DIGIT 9

static void to_bin4(unsigned int value, char out[5])
{
    for (int bit = 3; bit >= 0; --bit) {
        out[3 - bit] = (value & (1u << bit)) ? '1' : '0';
    }
    out[4] = '\0';
}

void myISR(void) __attribute__((interrupt_handler));

int main(void)
{
    init_platform();
    xil_printf("Lab1 – Interrupt v2 (add + invert)\r\n");

    *(volatile unsigned int*)(GPIO_SW_BASE  + GPIO_TRI_OFFSET)=0xF;
    *(volatile unsigned int*)(GPIO_BTN_BASE + GPIO_TRI_OFFSET)=0xF;
    *(volatile unsigned int*)(GPIO_LED_BASE + GPIO_TRI_OFFSET)=0x0;

    GIER_SW=0x80000000; GIER_BTN=0x80000000;
    IPIER_SW=0x1; IPIER_BTN=0x1;
    IER=0x3; MER=0x3;
    microblaze_enable_interrupts();

    xil_printf("INTC configured – waiting...\r\n");
    while(1);
}

void myISR(void)
{
    unsigned int pending = IISR;

    // --- SW interrupt
    if(pending & 0x1){
        unsigned int sw=*((volatile unsigned int*)(GPIO_SW_BASE));
        int msb_index = -1;
        for(int i=3;i>=0;i--){
            if(sw & (1u<<i)){ msb_index=i; break; }
        }

        unsigned int pattern = (msb_index < 0) ? 0u : (unsigned int)msb_index;
        *((volatile unsigned int*)(GPIO_LED_BASE))=pattern;

        char sw_str[5];
        char led_str[5];
        to_bin4(sw, sw_str);
        to_bin4(pattern, led_str);
        if (msb_index < 0) {
            xil_printf("[SW IRQ] SW=%s -> LED=%s (index=none)\r\n", sw_str, led_str);
        } else {
            xil_printf("[SW IRQ] SW=%s -> LED=%s (index=%d)\r\n", sw_str, led_str, msb_index);
        }
        IPISR_SW=0x1; IIAR=0x1;
    }

    // --- BTN interrupt (add + invert)
    if(pending & 0x2){
        usleep(50000); // debounce
        unsigned int led=*((volatile unsigned int*)(GPIO_LED_BASE));
        unsigned int result=~(led + MATRICOLA_DIGIT)&0xF;
        *((volatile unsigned int*)(GPIO_LED_BASE))=result;

        char led_str[5];
        char result_str[5];
        to_bin4(led, led_str);
        to_bin4(result, result_str);
        xil_printf("[BTN IRQ] (%s + %d) inverted -> %s\r\n",
                   led_str, MATRICOLA_DIGIT, result_str);
        IPISR_BTN=0x1; IIAR=0x2;
    }
}
