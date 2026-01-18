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
 *  This header defines the shared memory region used for
 *  inter-core communication between the two ARM Cortex-A9
 *  processors.
 *
 *  The shared region is allocated in DDR memory and contains:
 *    • Input vectors A and B
 *    • Output vector C
 *    • Synchronization flags for software-based barriers
 *
 *  The memory layout is explicitly shared between cores and
 *  accessed via a fixed physical base address.
 *
 ***************************************************************/
#ifndef SHARED_H
#define SHARED_H

#include "xil_types.h"


/* ============================================================
 * VECTOR SIZE
 * ------------------------------------------------------------
 * Default value is set to a small size for functional testing.
 * Larger values (e.g., 1000 or 10000) are used during lab
 * evaluation to analyze scalability and parallel speedup.
 * ============================================================ */
#define ARRAY_SIZE 10 //default value -  1000 and 10000 for lab evaluation

/* ============================================================
 * SHARED MEMORY BASE ADDRESS
 * ------------------------------------------------------------
 * The base address must:
 *   • Reside in DDR memory
 *   • Not overlap with program sections
 *     (.text, .data, .bss, heap, stack)
 *
 * The chosen address is platform-dependent and must be
 * consistent across both cores.
 * ============================================================ */
#define SHARED_BASE 0x2f100000U


/* ============================================================
 * SHARED MEMORY STRUCTURE
 * ------------------------------------------------------------
 * Memory layout:
 *   [ A | B | C | synchronization flags ]
 *
 * The 'volatile' qualifier prevents compiler optimizations
 * that could break correct inter-core synchronization.
 * ============================================================ */
typedef struct {
    u32 A[ARRAY_SIZE];
    u32 B[ARRAY_SIZE];
    u32 C[ARRAY_SIZE];

    volatile u32 ready1; // Core 1 ready for parallel section
    volatile u32 start;  // 0 = wait, 1 = go
    volatile u32 done0;  // set by core 0
    volatile u32 done1;  // set by core 1

} shared_mem_t;


/* ============================================================
 * SHARED MEMORY ACCESS MACRO
 * ------------------------------------------------------------
 * Provides typed access to the shared memory region starting
 * at SHARED_BASE.
 * ============================================================ */
#define SHARED ((volatile shared_mem_t *)SHARED_BASE)

#endif // SHARED_H
