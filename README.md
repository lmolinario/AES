# AES Labs – Advanced Embedded Systems


**Course:** Advanced Embedded Systems (University of Cagliari – A.Y. 2025)

**Instructor:** Prof. Paolo Meloni

**Board:** Digilent Zybo Z7 

**Language:** C

---

## Overview

This repository contains all laboratory assignments developed for the **Advanced Embedded Systems** course at the *University of Cagliari*.
Each lab focuses on hands-on embedded programming for the **MicroBlaze soft-core processor** implemented on the **Zybo Z7 FPGA board**, with emphasis on real hardware control, AXI peripherals, and interrupt handling.

---

### Zybo Z7 Development Board 

<p align="center">
  <img src="img/Zybo.png" alt="Zybo Z7 board used in the labs" width="650"/>
</p>

*Photo taken during AES Lab*

---

## Labs Summary

| Lab       | Title                                     | Description                                                                                                                                                                                                                                                                            |
|-----------|-------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Lab 1** | **Polling & Interrupts**                  | (1) `a_lab1_polling_basic.c` – polling version (read switches and mirror on LEDs) <br> (2) `b_lab1_interrupt_part1.c` – dual interrupt (SW + BTN) via AXI Interrupt Controller <br> (3) `c_lab1_interrupt_part2.c` – extended ISR: add Student ID digit (9) + invert pattern           |
| **Lab 2** | **The PS – UART Microserver**             | (1) `main_negative.c` – receive PPM P6 image over UART → compute negative → send back processed image <br> (2) `main_hist_stretch.c` – linear histogram stretching with arbitrary image size <br> (3) `main_hist_equalization.c` – optional bonus: global histogram equalization       |
| **Lab 3** | **AD/DA Conversion and Audio Processing** | (1) `lab3_step1_loopback.c` – audio loopback using I2S and SSM2603 codec <br> (2) `lab3_step2_fir.c` – real-time software FIR filtering with selectable LPF/HPF variants <br> (3) `lab3_step3_timing.c` – FIR performance analysis using the Global Timer (cycles, taps, cache ON/OFF) |
| **Lab 4** | **Parallel Processing**                   | (1) `master.c` – master core initialization, workload partitioning, timing and synchronization <br> (2) `worker.c` – worker core execution of parallel vector addition <br> (3) `shared.c` – shared-memory data structures and synchronization flags                                 |
| **Lab 5** | **DNN on MNIST** | (1) `lab5_mnist.c` – fixed-point MNIST inference (784→64→32→10, Q8.8) on ARM Cortex-A9 with UART-based image input <br> (2) `lab5_mnist_custom_TIPS.c` – custom Group 4 DNN (784→64→32→16→10) with UART and data-representation optimizations (TIP 1: Q0.8 images, TIP 2: Q1.7 weights) <br> (3) Performance analysis – end-to-end and phase-level timing (RX / DNN / PRINT / ITER) using ARM Global Timer |


                
##  Repository Structure

```
AES Labs/
│
│── img/                            → images
│ 
├── Lab 1/
│   ├── a_lab1_polling_basic.c      → polling version (read switches and mirror on LEDs)
│   ├── b_lab1_interrupt_part1.c    → dual interrupt (SW + BTN) via AXI Interrupt Controller
│   ├── c_lab1_interrupt_part2.c    → extended ISR: add Student ID digit (9) + invert pattern
│   ├── AES/                        → Full Xilinx SDK 2019.1 workspace
│   └── README.md                   → Lab 1 documentation
│ 
├── Lab 2/
│   ├── main_negative.c            → compute negative 
│   ├── main_hist_stretch.c        → linear histogram stretching with arbitrary image size 
│   ├── main_hist_equalization.c   → global histogram equalization
│   ├── AES/                       → Full Xilinx SDK 2019.1 workspace
│   └── README.md                  → Lab 2 documentation
│
├── Lab 3/
│   ├── lab3_step1_loopback.c      → audio loopback using I2S and SSM2603 codec
│   ├── lab3_step2_fir.c           → real-time software FIR filtering with selectable LPF/HPF variants
│   ├── lab3_step3_timing.c        → FIR performance analysis using the Global Timer (cycles, taps, cache ON/OFF) 
│   ├── AES/                       → Full Xilinx SDK 2019.1 workspace
│   └── README.md                  → Lab 3 documentation
│
├── Lab 4/
│   ├── master.c                  → Core 0: serial baseline, parallel execution, timing
│   ├── worker.c                  → Core 1: second-half vector computation
│   ├── shared.h                  → Shared DDR data structures and synchronization flags
│   ├── Lab 4 Report.pdf          → Final experimental report
│   ├── AES/                      → Full Xilinx SDK 2019.1 workspace
│   └── README.md                 → Lab 4 documentation
│
├── Lab 5/
│   ├── lab5_mnist.c                        → Base MNIST inference (784→64→32→10, Q8.8)
│   ├── lab5_mnist_custom_TIPS.c            → Group 4 custom DNN
│   │                                          784→64→32→16→10,
│   │                                          MODE_BASELINE / TIP1 / TIP1+TIP2)
│   ├── Lab 5 Report.pdf                    → Final experimental report
│   │  
│   ├── AES/                                → Full Xilinx SDK 2019.1 workspace
│   └── README.md                           → Lab 5 documentation
│
└── README.md                               → Root documentation (this file)
```


---

*Developed and maintained by Lello Molinario*

