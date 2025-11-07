# Lab 1 – Polling & Interrupts

**Course:** Advanced Embedded Systems (University of Cagliari)  
**Instructor:** Prof. Paolo Meloni  
**Author:** Lello Molinario  
**Board:** Zybo Z7 / MicroBlaze  
**Date:** November 2025

---

## 🧠 Overview
This laboratory introduces **polling-based I/O** and **interrupt-driven I/O** on the Zybo Z7 board using the AXI GPIO and AXI Interrupt Controller peripherals.  
The goal is to progressively evolve from a simple polling program to a dual-interrupt handler with logic and arithmetic processing.

---

## ⚙️ Hardware Mapping
| Peripheral | Base Address | Description |
|-------------|---------------|--------------|
| `GPIO_LED`  | `0x40000000`  | 4-bit output LEDs |
| `GPIO_SW`   | `0x40010000`  | 4-bit input switches |
| `GPIO_BTN`  | `0x40020000`  | Push buttons (input) |
| `INTC`      | `0x41200000`  | AXI Interrupt Controller |

Registers used:
- `DATA` (offset `0x0`)
- `TRI`  (offset `0x4`) — direction control (`1` = input, `0` = output)
- `IER`, `MER`, `IISR`, `IIAR` — interrupt control registers (AXI INTC)

---

## 🧩 Implementations

### 🔹 (a) `lab1_polling_basic.c`
**Description:**  
Polling-based version. Continuously reads the 4 switches and mirrors their binary value directly on the LEDs.

**Features:**
- Simple while loop polling GPIO input.
- Updates LEDs only when input changes.
- UART debug output (`xil_printf`) for traceability.

**Expected Output:**
SW = 0101 → LED = 0101


---

### 🔹 (b) `lab1_interrupt_part1.c`
**Description:**  
Interrupt-based version (Part 1) using two sources — Switches and Buttons — through the AXI Interrupt Controller.

**ISR behavior:**
- **SW interrupt:** When a switch changes, display the **index of the most significant switch ON** (3..0) on LEDs.  
  If no switches are ON, display `0000`.
- **BTN interrupt:** When a button is pressed or released, invert (`~`) the current LED pattern.

**Example:**
SW = 0101 → MSB = 2 → LED = 0010
BTN pressed → invert → LED = 1101


---

### 🔹 (c) `lab1_interrupt_part2.c`
**Description:**  
Extended interrupt version. Similar to part (b) but modifies the **button ISR** to include arithmetic logic.

**BTN ISR behavior:**
- On button press, add the **last digit of the Student ID** to the current LED pattern, then **invert** the result.

**Example (Student ID ends with 9):**
SW = 0101 → LED = 0101
BTN pressed → (LED + 9) = 1110 → invert → LED = 0001


---

## 🧰 Build & Run
**Tools:** Xilinx Vitis / SDK  
**Steps:**
1. Import project (`File → New → Application Project`).
2. Copy `.c` files to `src/`.
3. Select *MicroBlaze* processor target.
4. Build and program the Zybo board.
5. Open UART terminal @ 115200 baud to monitor messages.

---


📅 *Academic Year 2025 – University of Cagliari*  
🧑‍💻 *Developed by Lello Molinario*
