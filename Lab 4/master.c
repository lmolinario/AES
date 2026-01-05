// master.h
#include "xparameters.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xtime_l.h"
#include "shared.h"
#include "xil_io.h"

u32 t_start, t_end;

/* Serial baseline (auto-vectorizzabile NEON) */
void vec_add_serial(u32 * restrict c,
                    const u32 * restrict a,
                    const u32 * restrict b,
                    int n)
{
    for (int i = 0; i < n; ++i) {
        c[i] = a[i] + b[i];
    }
}

int main(void)
{
    int i;
    static u32 C_serial[ARRAY_SIZE];

    Xil_ICacheDisable();
    Xil_DCacheDisable();
	print("Started!\n\r");
    xil_printf("=== LAB 4 === Parallel Vector Add (SERIAL vs PARALLEL)\n\r");
       xil_printf("MASTER: ARRAY_SIZE=%d\r\n", ARRAY_SIZE);

    /* Start tramite pulsante */
    while (Xil_In32(XPAR_AXI_GPIO_2_BASEADDR) == 0);

    // Init flags
    SHARED->start = 0;
    SHARED->done0 = 0;
    SHARED->done1 = 0;

    /* ================= INIT ================= */
    for (i = 0; i < ARRAY_SIZE; ++i) {
        SHARED->A[i] = i;
        SHARED->B[i] = 2 * i;
        SHARED->C[i] = 0;
    }



    /* ============== SERIAL ================== */
    xil_printf("Core 0: Starting SERIAL version...\r\n");

    t_start = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
    vec_add_serial(C_serial, SHARED->A, SHARED->B, ARRAY_SIZE);
    t_end   = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);

    u32 serial_cycles = t_end - t_start;

    xil_printf("Core 0: Serial done, cycles = %lu\r\n",
               (unsigned long)serial_cycles);

    /* Clear output vector before parallel computation */
      for (i = 0; i < ARRAY_SIZE; ++i) {
          SHARED->C[i] = 0;
      }



    /* ============== PARALLEL ================ */
    xil_printf("Core 0: Starting PARALLEL version...\r\n");

    t_start = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);



    SHARED->start = 1;

    /* Prima metà */
    for (i = 0; i < ARRAY_SIZE / 2; ++i) {
        SHARED->C[i] = SHARED->A[i] + SHARED->B[i];
    }
    SHARED->done0 = 1;

    /* Attesa worker */
    while (SHARED->done1 == 0);

    t_end = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);

    u32 parallel_cycles = t_end - t_start;

    /* ============== RESULTS ================= */
    u32 speedup_x100 = (serial_cycles * 100) / parallel_cycles;

    xil_printf("\r\n=== TIMING SUMMARY ===\r\n");
    xil_printf("Serial cycles   = %lu\r\n", (unsigned long)serial_cycles);
    xil_printf("Parallel cycles = %lu\r\n", (unsigned long)parallel_cycles);
    xil_printf("Speedup         = %lu.%02lu x\r\n",
               speedup_x100 / 100, speedup_x100 % 100);

    /* Check */
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
