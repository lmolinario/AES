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
 * NOTE:
 * Only the lower 32 bits of the ARM Global Timer are used.
 * This is sufficient for the short execution window of the
 * experiment and avoids handling 64-bit overflows.
 *
 ***************************************************************/

#include "xparameters.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "shared.h"
#include "xil_io.h"
#include "xtime_l.h"



static inline u32 read_global_timer(void)
{
    return Xil_In32(XPAR_GLOBAL_TMR_BASEADDR + 0x00U);
}



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


    xil_printf("Started!\r\n");
    xil_printf("=== LAB 4 === Parallel Vector Add (SERIAL vs PARALLEL)\r\n");
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

    /* Initialize shared flags */
    SHARED->start  = FLAG_RESET;
    SHARED->done0  = FLAG_RESET;
    SHARED->done1  = FLAG_RESET;
    SHARED->ready1 = FLAG_RESET;


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

    /* SERIAL BASELINE */
    t_start = read_global_timer();
    vec_add_serial(C_serial,
               (const u32 *)SHARED->A,
               (const u32 *)SHARED->B,
               ARRAY_SIZE);

    t_end   = read_global_timer();


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

    /* Wait until worker core is ready */
    while (SHARED->ready1 != FLAG_SET);

    xil_printf("Master: Worker ready, starting parallel section.\r\n");


    /* Timing via ARM Global Timer (32-bit low counter is sufficient
     * for the measured execution window of this experiment) */
    t_start = read_global_timer();

    /* Notify worker core to start */
    SHARED->start = FLAG_SET;

    /* Core 0 processes first half of the vector */
    for (i = 0; i < ARRAY_SIZE / 2; ++i) {
        SHARED->C[i] = SHARED->A[i] + SHARED->B[i];
    }

    SHARED->done0 = FLAG_SET;


    /* wait worker */
    while (SHARED->done1 != FLAG_SET);
    t_end = read_global_timer();

    u32 parallel_cycles = t_end - t_start;

    /* ========================================================
     * RESULTS AND SPEEDUP
     * ======================================================== */
    if (parallel_cycles == 0) parallel_cycles = 1;
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
