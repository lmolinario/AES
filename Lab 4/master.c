/***************************************************************
 *  Lab 4 – Parallel Processing on Dual-Core ARM Cortex-A9
 *  ------------------------------------------------------------
 *  Author: Lello Molinario
 *  University of Cagliari – Advanced Embedded Systems (AES)
 *  Academic Year: 2025–2026
 *
 *  Description:
 *  ------------------------------------------------------------
 *  This application evaluates the performance benefits of
 *  data-level parallelism on a dual-core ARM Cortex-A9 system.
 *
 *  A simple vector addition kernel (C = A + B) is implemented
 *  and executed in two configurations:
 *
 *    1) SERIAL version:
 *       - Executed entirely on Core 0
 *       - Used as performance baseline
 *
 *    2) PARALLEL version:
 *       - Core 0 processes the first half of the vector
 *       - Core 1 (worker) processes the second half
 *       - Synchronization via shared memory flags
 *
 *  Execution time is measured using the ARM Global Timer
 *  to compute the achieved speedup.
 *
 *
 ***************************************************************/
#include "xparameters.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xtime_l.h"
#include "shared.h"
#include "xil_io.h"

/* Global timer timestamps */
u32 t_start, t_end;

/* ============================================================
 * SERIAL BASELINE
 * ------------------------------------------------------------
 * Simple vector addition executed sequentially.
 * The 'restrict' qualifier allows the compiler to
 * safely optimize memory accesses.
 * ============================================================ */
void vec_add_serial(u32 * restrict c,
                    const u32 * restrict a,
                    const u32 * restrict b,
                    int n)
{
    for (int i = 0; i < n; ++i) {
        c[i] = a[i] + b[i];
    }
}


/* ============================================================
 * MAIN – MASTER CORE (Core 0)
 * ============================================================ */
int main(void)
{
    int i;
    static u32 C_serial[ARRAY_SIZE];// Reference output vector

    /* Disable caches for deterministic timing measurements */
    Xil_ICacheDisable();
    Xil_DCacheDisable();
	print("Started!\n\r");
    xil_printf("=== LAB 4 === Parallel Vector Add (SERIAL vs PARALLEL)\n\r");
       xil_printf("MASTER: ARRAY_SIZE=%d\r\n", ARRAY_SIZE);

    /* --------------------------------------------------------
     * Start synchronization via switch
     * (used to align experiments manually)
     * -------------------------------------------------------- */
    while (Xil_In32(XPAR_AXI_GPIO_2_BASEADDR) == 0);

    /* --------------------------------------------------------
     * Shared-memory flag initialization
     * --------------------------------------------------------
     * Shared flags act as a simple software barrier
     * (cache disabled ⇒ no explicit memory fences required) */

    SHARED->start = 0;
    SHARED->done0 = 0;
    SHARED->done1 = 0;

    /* --------------------------------------------------------
     * Vector initialization
     * -------------------------------------------------------- */
    for (i = 0; i < ARRAY_SIZE; ++i) {
        SHARED->A[i] = i;
        SHARED->B[i] = 2 * i;
        SHARED->C[i] = 0;
    }

    /* ========================================================
     * SERIAL EXECUTION (Baseline)
     * ======================================================== */
    xil_printf("Core 0: Starting SERIAL version...\r\n");

    t_start = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
    vec_add_serial(C_serial, SHARED->A, SHARED->B, ARRAY_SIZE);
    t_end   = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);

    u32 serial_cycles = t_end - t_start;

    xil_printf("Core 0: Serial done, cycles = %lu\r\n",
               (unsigned long)serial_cycles);

    /* Clear output vector before parallel run */
      for (i = 0; i < ARRAY_SIZE; ++i) {
          SHARED->C[i] = 0;
      }



    /* ========================================================
     * PARALLEL EXECUTION
     * ======================================================== */
    xil_printf("Core 0: Starting PARALLEL version...\r\n");

    /* Start timing just before releasing worker core */
    t_start = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);

    /* Notify worker core to start */
    SHARED->start = 1;

    /* Core 0 processes first half of the vector */
    for (i = 0; i < ARRAY_SIZE / 2; ++i) {
        SHARED->C[i] = SHARED->A[i] + SHARED->B[i];
    }

    /* Wait for worker core (Core 1) to complete */
    SHARED->done0 = 1;

    /* wait worker */
    while (SHARED->done1 == 0);

    t_end = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);

    u32 parallel_cycles = t_end - t_start;

    /* ========================================================
     * RESULTS AND SPEEDUP
     * ======================================================== */
    u32 speedup_x100 = (serial_cycles * 100) / parallel_cycles;

    xil_printf("\r\n=== TIMING SUMMARY ===\r\n");
    xil_printf("Serial cycles   = %lu\r\n", (unsigned long)serial_cycles);
    xil_printf("Parallel cycles = %lu\r\n", (unsigned long)parallel_cycles);
    xil_printf("Speedup         = %lu.%02lu x\r\n",
               speedup_x100 / 100, speedup_x100 % 100);

    /* ========================================================
     * RESULT VALIDATION
     * --------------------------------------------------------
     * Ensures correctness of parallel computation
     * ======================================================== */
    int errors = 0;
    for (i = 0; i < ARRAY_SIZE; ++i) {
        if (SHARED->C[i] != C_serial[i]) {
            errors++;
        }
    }
    xil_printf("Errors          = %d\r\n\n", errors);

    while (1) { }

    return 0;
}
