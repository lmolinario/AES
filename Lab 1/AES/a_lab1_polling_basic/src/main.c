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
#define GPIO_LED_BASE     0x40000000U   // AXI GPIO base address for LEDs - for all 'U' = unsigned constant → ensures positive address
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


    /* UART Debug Insight:
    * Output visible through RealTerm (Windows) or any serial terminal
    * at 115200 baud, 8 data bits, no parity, 1 stop bit (8N1).
    * Each switch change triggers a UART message showing the binary LED mapping.
    */



    /* --- Configure GPIO directions and initialize LEDs --- */
    GpioInit();

    unsigned int last_value = 0x0;   // stores last switch state

    /* --- Main polling loop ---
     * The board continuously reads the 4 switches.
     * The LEDs are updated only when a change is detected.
     */
    while (1)
    {
        /* NOTE ON 'volatile':
         * The 'volatile' qualifier tells the compiler that the memory content
         * may change independently from program control,
         * preventing optimization that would cache register values.
         */


        /* ------------------------------------------------------------
         * Read the 4-bit switch value (mask lower nibble)
         * ------------------------------------------------------------
         * The switches are memory-mapped to an AXI GPIO device.
         * Each bit of the GPIO_DATA register represents one switch.
         * The 'volatile' keyword ensures that the compiler performs
         * an actual read from the hardware register at every iteration
         * (no caching or optimization).
         * ------------------------------------------------------------ */
        volatile unsigned int sw_value = *((volatile unsigned int*)(GPIO_SW_BASE)) & 0xF;// 'volatile' → always read actual HW register (no compiler caching)

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

    /* Not reached: the program runs indefinitely inside the main loop.
    * Included for completeness and good practice when using Xilinx SDK.
    * cleanup_platform() would de-initialize UART/caches if ever reached. */
    cleanup_platform();
    return 0;
}


/* ============================================================
 *  FUNCTION DEFINITIONS
 * ============================================================ */

/**
 * @brief Configure GPIO direction registers.
 *
 * Each AXI GPIO has two main registers:
 *   - DATA  (offset 0x0): read/write actual I/O state
 *   - TRI   (offset 0x4): direction control bitmask
 *       bit = 1 → input (Hi-Z)
 *       bit = 0 → output (driven)
 *
 * The switches GPIO is configured as input (TRI=0xF)
 * and the LED GPIO as output (TRI=0x0).
 * All LEDs are initially turned off.
 */

static void GpioInit(void)
{
    /* Configure direction registers */
    *(volatile unsigned int*)(GPIO_SW_BASE  + GPIO_TRI_OFFSET) = 0xF; // 4-bit input (volatile → ensures actual HW write)
    *(volatile unsigned int*)(GPIO_LED_BASE + GPIO_TRI_OFFSET) = 0x0; // 4-bit output (volatile → ensures actual HW write)

    /* Initialize LEDs to OFF (all zeros) */
    *(volatile unsigned int*)(GPIO_LED_BASE) = 0x0; // volatile → force real write to LED register

    xil_printf("[Init] GPIO configured: SW=input, LED=output\r\n");
}


/**
 * @brief Update LEDs according to the 4-bit switch value.
 *
 * The LED controller exposes a 32-bit DATA register,
 * but only the 4 least significant bits are connected
 * to the board LEDs. Masking (sw_value & 0xF)
 * prevents unintended writes to unused bits.
 *
 * Writing to this memory address causes immediate
 * hardware-level change on the LED pins.
 */

static void UpdateLeds(unsigned int sw_value)
{
    *(volatile unsigned int*)(GPIO_LED_BASE) = sw_value & 0xF; // write 4 LSBs to LED register (volatile → ensure real HW write)
}


/***************************************************************
 *  SYSTEM OPERATION SUMMARY
 *  ------------------------------------------------------------
 *  - The CPU polls GPIO_SW (input) continuously.
 *  - A state change triggers UART feedback and LED update.
 *  - Accesses are fully blocking and sequential.
 *  - No interrupt controller (INTC) is used at this stage.
 ***************************************************************/

/***************************************************************
 *  TECHNICAL NOTES
 *  ------------------------------------------------------------
 *  This polling-based version does not use interrupts, but it
 *  maintains the same memory map and structure as the interrupt
 *  versions (Part b and Part c). When compiling multiple labs in
 *  the same SDK workspace, I use distinct function names for ISR
 *  handlers in later parts  to avoid linker symbol conflicts.
 ***************************************************************/
