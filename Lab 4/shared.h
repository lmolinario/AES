/***************************************************************
 *  Lab 4 – Parallel Processing on Dual-Core ARM Cortex-A9
 *  ------------------------------------------------------------
 *  File: shared.h
 *
 *  Author: Lello Molinario
 *  University of Cagliari – Advanced Embedded Systems (AES)
 *  Academic Year: 2025–2026
 *
 *  Description:
 *  ------------------------------------------------------------
 *  Definition of the shared memory region used for
 *  inter-core communication between the two ARM Cortex-A9 cores.
 *
 *  The shared region is allocated in DDR memory and contains:
 *    • Input vectors A and B
 *    • Output vector C
 *    • Software-based synchronization flags
 *
 *  The memory layout is explicitly shared and accessed
 *  through a fixed physical base address.
 ***************************************************************/
#ifndef SHARED_H
#define SHARED_H

#include "xil_types.h"

/* ============================================================
 * VECTOR SIZE
 * ------------------------------------------------------------
 * Compile-time configuration parameter.
 * During evaluation, ARRAY_SIZE is increased to analyze
 * scalability and parallel speedup.
 * ============================================================ */
#define ARRAY_SIZE 10   /* Default: functional test
                         * Typical values: 1000, 10000 */

/* ============================================================
 * SHARED MEMORY BASE ADDRESS
 * ------------------------------------------------------------
 * Must reside in DDR memory and must not overlap with
 * code, data, heap, or stack regions.
 * The address must be identical for both cores.
 * ============================================================ */
#define SHARED_BASE 0x2F100000U

/* ============================================================
 * SYNCHRONIZATION FLAG VALUES
 * ============================================================ */
#define FLAG_RESET 0U
#define FLAG_SET   1U

/* ============================================================
 * SHARED MEMORY STRUCTURE
 * ------------------------------------------------------------
 * Memory layout:
 *   [ A | B | C | synchronization flags ]
 *
 * All synchronization variables are declared volatile to
 * prevent compiler reordering or caching.
 *
 * The structure is naturally aligned to 32-bit boundaries,
 * as required by the ARM Cortex-A9 architecture.
 * ============================================================ */
typedef struct __attribute__((aligned(4))) {

    u32 A[ARRAY_SIZE];   /* Input vector A */
    u32 B[ARRAY_SIZE];   /* Input vector B */
    u32 C[ARRAY_SIZE];   /* Output vector C */

    /* Synchronization flags */
    volatile u32 ready1; /* Core 1 signals readiness */
    volatile u32 start;  /* Barrier: 0 = wait, 1 = start */
    volatile u32 done0;  /* Core 0 completion flag */
    volatile u32 done1;  /* Core 1 completion flag */

} shared_mem_t;

/* ============================================================
 * SHARED MEMORY ACCESS MACRO
 * ============================================================ */
#define SHARED ((volatile shared_mem_t *) SHARED_BASE)

#endif /* SHARED_H */
