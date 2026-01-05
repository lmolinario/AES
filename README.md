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

| Lab       | Title                                     | Description                                                                                                                                                                                                                                                                      |
|-----------|-------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Lab 1** | **Polling & Interrupts**                  | (a) `a_lab1_polling_basic.c` – polling version (read switches and mirror on LEDs) <br> (b) `b_lab1_interrupt_part1.c` – dual interrupt (SW + BTN) via AXI Interrupt Controller <br> (c) `c_lab1_interrupt_part2.c` – extended ISR: add Student ID digit (9) + invert pattern     |
| **Lab 2** | **The PS - UART Microserver**             | (1) `main_negative.c` – receive PPM P6 image over UART → compute negative → send back processed image <br> (2) `main_hist_stretch.c` – linear histogram stretching with arbitrary image size <br> (3) `main_hist_equalization.c` – optional bonus: global histogram equalization |
| **Lab 3** | **AD/DA conversion and audio processing** |   (1) `lab3_step1_loopback.c` – audio loopback using I2S and SSM2603 codec <br> (2) `lab3_step2_fir.c` – real-time software FIR filtering with selectable LPF/HPF variants <br> (3) `lab3_step3_timing.c` – FIR performance analysis using the Global Timer (cycles, taps, cache ON/OFF) |                                                                                                                                                                                                                                                                               |
| **Lab 4** | **Parallel Processing**                   |                                                                                                                                                                                                                                                                                  |
| **Lab 5** | **DNN on MNIST**                   |                                                                                                                                                                                                                                                                                  |

---

##  Repository Structure

```
AES Labs/
│
│── img/    → images
│ 
├── Lab 1/
│   ├── a_lab1_polling_basic.c
│   ├── b_lab1_interrupt_part1.c
│   ├── c_lab1_interrupt_part2.c
│   ├── AES/                     → Full Xilinx SDK 2019.1 workspace
│   └── README.md                → Lab 1 documentation
│ 
├── Lab 2/
│   ├── main_negative.c
│   ├── main_hist_stretch.c
│   ├── main_hist_equalization.c
│   ├── AES/                     → Full Xilinx SDK 2019.1 workspace
│   └── README.md                → Lab 2 documentation
│
├── Lab 3/
│   ├── main_XXXX.c
│   ├── main_XXXX.c
│   ├── main_XXXX.c
│   ├── AES/                     → Full Xilinx SDK 2019.1 workspace
│   └── README.md                → Lab 3 documentation
│
├── Lab 4/
│   ├── main_XXXX.c
│   ├── main_XXXX.c
│   ├── main_XXXX.c
│   ├── AES/                     → Full Xilinx SDK 2019.1 workspace
│   └── README.md                → Lab 4 documentation
│
├── Lab 5/
│   ├── main_XXXX.c
│   ├── main_XXXX.c
│   ├── main_XXXX.c
│   ├── AES/                     → Full Xilinx SDK 2019.1 workspace
│   └── README.md                → Lab 5 documentation
│
└── README.md                    → Root documentation (this file)
```


---

*Developed and maintained by Lello Molinario*

