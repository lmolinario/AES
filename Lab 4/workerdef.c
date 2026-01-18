/***************************************************************
 *  Lab 4 – Parallel Processing on Dual-Core ARM Cortex-A9
 *  ------------------------------------------------------------
 *
 *  Author: Lello Molinario
 *  University of Cagliari – Advanced Embedded Systems (AES)
 *  Academic Year: 2025–2026
 *
 *
 *  Description:
 *  ------------------------------------------------------------
 *  This application implements the worker task executed on
 *  Core 1 of the dual-core ARM Cortex-A9 processor.
 *
 *  Core 1 participates in a shared-memory parallel vector
 *  addition by computing the second half of the output vector:
 *
 *      C[i] = A[i] + B[i],  for i ∈ [ARRAY_SIZE/2, ARRAY_SIZE)
 *
 *  Synchronization with Core 0 is achieved using software
 *  flags located in a shared DDR memory region.
 *
 *  Execution model:
 *    • Busy-wait until start signal is asserted by Core 0
 *    • Compute assigned data partition
 *    • Signal completion via shared flag
 *
 *
 ***************************************************************/
#include "xparameters.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "shared.h"
#include "xil_io.h"



/* ============================================================
 * MAIN – WORKER CORE (Core 1)
 * ============================================================ */
int main(void)
{
    int i;

    /* --------------------------------------------------------
     * Disable caches
     * --------------------------------------------------------
     * Caches are disabled to guarantee deterministic timing
     * and immediate visibility of shared-memory updates
     * between the two cores.
     * -------------------------------------------------------- */
    Xil_ICacheDisable();
    Xil_DCacheDisable();


    /* Ensure completion flag is cleared before synchronization */
    SHARED->ready1 = 0;
    SHARED->start  = 0;   // opzionale, ma consigliato
    SHARED->done1  = 0;

    xil_printf("Core 1: Waiting for start signal...\r\n");

    /* Optional hardware-level synchronization via switch
     * (used to manually align master/worker execution) */
    while (Xil_In32(XPAR_AXI_GPIO_2_BASEADDR) == 0);

       /* Signal readiness to master */
    SHARED->ready1 = 1;

/* Wait for start flag from master (Core 0) */

    while (SHARED->start == 0) {
        // busy-wait
    }

//    xil_printf("Core 1: Starting second half computation\r\n");

    /* --------------------------------------------------------
     * Parallel computation – second half of the vector
     * --------------------------------------------------------
     * Each core works on a disjoint data partition, avoiding
     * data races and the need for additional synchronization.
     * -------------------------------------------------------- */
    for (i = ARRAY_SIZE / 2; i < ARRAY_SIZE; ++i) {
        SHARED->C[i] = SHARED->A[i] + SHARED->B[i];
    }

    /* --------------------------------------------------------
     * Signal completion to Core 0
     * -------------------------------------------------------- */
    SHARED->done1 = 1;

//    xil_printf("Core 1: Done.\r\n");

     /* --------------------------------------------------------
     * Idle loop
     * --------------------------------------------------------
     * No further tasks are scheduled for the worker core.
     * -------------------------------------------------------- */
    while (1) {
        // Idle loop
    }

    return 0;
}
