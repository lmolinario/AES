# Lab 1 – Polling & Interrupts
This repository contains the full implementation of Lab 1 for the
Advanced Embedded Systems course (University of Cagliari).
The project demonstrates how to interface GPIO peripherals on the Zybo Z7 and how to manage events using both polling and interrupt-driven techniques.
The lab progresses from a  switch-reading application to a dual-interrupt system using the AXI GPIO and the AXI Interrupt Controller (INTC), 
concluding with an extended ISR that performs arithmetic and bitwise logic.

---

## Instructions

Students must prepare the following versions:

**(a)** *Polling Basic version* – Check the switches and show on LEDs the same binary pattern.

**(b)** *Interrupt version (Part 1)* – Two interrupt sources (Switch + Button) using the AXI Interrupt Controller.  
Constraint: use at least one **OR / AND mask**.

* **SW interrupt:** On a switch change, display on the LEDs the binary index of the most-significant switch that is ON (3..0).  
  If none are ON, display `0x00`.
* **BTN interrupt:** On button press / release, invert (`~`) the current LED pattern.

***Example Cycle:***


    Switch = 0b0101 → MSB = 2 → LED = 0010
    Button pressed → invert → 1101
    Final LEDs = 1101

**(c)** *Interrupt version (Part 2)* – Modify the Button ISR:
On button press, **add the last digit of your Student ID** to the current LED pattern and then **invert (~)** the result.

---

## Hardware Mapping

| Peripheral | Base Address | Description              |
| ---------- | ------------ | ------------------------ |
| `GPIO_LED` | `0x40000000` | 4-bit output LEDs        |
| `GPIO_SW`  | `0x40010000` | 4-bit input switches     |
| `GPIO_BTN` | `0x40020000` | Push buttons (input)     |
| `INTC`     | `0x41200000` | AXI Interrupt Controller |

**Registers used:**

* `DATA` (offset `0x0`)
* `TRI`  (offset `0x4`) — direction control (`1` = input, `0` = output)
* `IER`, `MER`, `IISR`, `IIAR` — interrupt control registers (AXI INTC)

---

## Project Structure

```
Lab 1/
│
├── AES/                                → Full Xilinx SDK 2019.1 workspace
│   ├── a_lab1_polling_basic/           → SDK project (Part a)
│   ├── b_lab1_interrupt_part1/         → SDK project (Part b)
│   ├── c_lab1_interrupt_part2/         → SDK project (Part c)
│   ├── *_bsp/                          → Auto-generated Board Support Packages
│   ├── design_1_wrapper_hw_platform_0/ → Exported Vivado hardware platform (.hdf)
│   └── SDK.log                         → Build/debug session log
│
├── a_lab1_polling_basic.c           → Clean and documented source (Part a)
├── b_lab1_interrupt_part1.c         → Clean and documented source (Part b)
├── c_lab1_interrupt_part2.c         → Clean and documented source (Part c)
└── README.md                        → Lab documentation
```

**Notes:**

* Folder `AES/` contains the **complete SDK workspace** used for compilation and on-board testing.
* The `.c` files in the root are **clean standalone versions** for review and grading.
* All three programs share the same **AXI GPIO** and **AXI Interrupt Controller** configuration.

---


## Implementations Summary

### 🔹 (a) `a_lab1_polling_basic.c`

**Polling-based version.**
Reads the 4 switches and mirrors their binary value directly on the LEDs.

**Features**

* Continuous polling with change detection.
* UART logging for each state change.

**Output Example:**

```
SW = 0101 → LED = 0101
```

---

### 🔹 (b) `b_lab1_interrupt_part1.c`

**Dual-interrupt version.**
Handles both switch and button interrupts via AXI INTC.

**ISR Logic**

* SW interrupt → show MSB index of active switch.
* BTN interrupt → invert the LED pattern.
* Uses **OR-masks** in interrupt configuration (constraint requirement).

**Example:**

```
SW = 0101 → LED = 0010
BTN pressed → invert → LED = 1101
```

---

### 🔹 (c) `c_lab1_interrupt_part2.c`

**Extended interrupt version.**
Enhances the BTN ISR with arithmetic logic.

**ISR Logic**

* On button press:

    1. Read current LED pattern.
    2. Add **last Student ID digit (9 in my case)**.
    3. Invert the result (~) and mask to 4 bits.

**Example:**

```
LED = 0010 (2)
BTN pressed → (2 + 9) = 11 = 1011 → ~1011 = 0100
Final LED = 0100
```


---


*Developed by Lello Molinario*
