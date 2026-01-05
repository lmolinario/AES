#include "xparameters.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xtime_l.h"
#include "shared.h"
#include "xil_io.h"


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
    // Disable caches to avoid coherency issues for this simple exercise.
    Xil_ICacheDisable();
    Xil_DCacheDisable();
    int i;
    static u32 C_serial[ARRAY_SIZE];
    u32 t_serial_start, t_serial_end;
    u32 t_parallel_start, t_parallel_end;



    while (Xil_In32(XPAR_AXI_GPIO_2_BASEADDR)==0);//wait for the start on button




    // Initialize data in shared memory
    for (i = 0; i < ARRAY_SIZE; ++i) {
        SHARED->A[i] = i;
        SHARED->B[i] = 2 * i;
    }

    /* ================= SERIAL BASELINE ================= */
    
    xil_printf("Core 0: Starting SERIAL version...\r\n");
    
    t_serial_start = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
    
    vec_add_serial(C_serial, SHARED->A, SHARED->B, ARRAY_SIZE);
    
    t_serial_end = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
    
    xil_printf("Core 0: Serial done, duration = %lu\r\n",
               (unsigned long)(t_serial_end - t_serial_start));

    // Init flags
    SHARED->done0 = 0;
    SHARED->done1 = 0;
    SHARED->start = 0;


    xil_printf("Core 0: Data initialized, signaling core 1 to start...\r\n");

    
    xil_printf("Core 0: Parallel vector add demo (fixed shared base)\r\n");

    t_parallel_start = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);


    // Signal core 1 to start
    SHARED->start = 1;





    // Core 0 processes first half
    for (i = 0; i < ARRAY_SIZE /2 ; ++i) {
        SHARED->C[i] = SHARED->A[i] + SHARED->B[i];
    }
    SHARED->done0 = 1;

    // Wait until core 1 finishes
    while (SHARED->done1 == 0) {
        // busy-wait
    }

    t_parallel_end = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
    
    xil_printf("Core 0: Parallel computation completed.\r\n");



    // Check correctness
    int errors = 0;
    for (i = 0; i < ARRAY_SIZE; ++i) {
        u32 expected = SHARED->A[i] + SHARED->B[i];
        if (SHARED->C[i] != expected) {
            errors++;
            if (errors < 10) {
                xil_printf("Mismatch at %d: got %lu, expected %lu\r\n",
                           i,
                           (unsigned long)SHARED->C[i],
                           (unsigned long)expected);
                int idx = i;
                xil_printf("Debug at %d: A=%lu, B=%lu, C=%lu, expected=%lu\r\n",
                           idx,
                           (unsigned long)SHARED->A[idx],
                           (unsigned long)SHARED->B[idx],
                           (unsigned long)SHARED->C[idx],
                           (unsigned long)(SHARED->A[idx] + SHARED->B[idx]));
            }
        }
    }

    for (i = 0; i < ARRAY_SIZE; ++i) {
        if (C_serial[i] != SHARED->C[i]) {
            xil_printf("Serial/Parallel mismatch at %d\r\n", i);
            break;
        }
    }

    xil_printf("\r\n=== TIMING SUMMARY ===\r\n");
    xil_printf("ARRAY_SIZE      = %d\r\n", ARRAY_SIZE);
    xil_printf("Serial cycles   = %lu\r\n",
               (unsigned long)(t_serial_end - t_serial_start));
    xil_printf("Parallel cycles = %lu\r\n",
               (unsigned long)(t_parallel_end - t_parallel_start));
    
    float speedup = (float)(t_serial_end - t_serial_start) /
                    (float)(t_parallel_end - t_parallel_start);
    
    xil_printf("Speedup         = %.2f x\r\n", speedup);
    xil_printf("Errors          = %d\r\n", errors);


    while (1) {
        for (volatile int d = 0; d < 1000000; d++);
    }

    return 0;
}
