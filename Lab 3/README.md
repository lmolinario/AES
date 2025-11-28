# **Lab 3 – AD/DA Conversion and Audio Processing**

---

## **Overview**

This assignment is related to **Lab 3 – Audio Acquisition, Processing, and Playback** on the Zybo Z7 board.

The goal is to implement a **real-time DSP application** that:

1. **Acquires audio** from the LINE IN jack using the onboard **SSM2603** codec
2. Performs **Analog → Digital** conversion (ADC)
3. Makes samples available to the **ARM Processing System (PS)** through I2S
4. Applies a **software FIR filter** to the audio stream
5. Sends processed samples back to the codec for **Digital → Analog** conversion (DAC)
6. Plays the filtered output through the **HPH OUT** jack

The audio codec is accessible via:

* **I2C** (control interface for configuration)
* **I2S** (audio data interface via AXI I2S peripheral)

---

## **Hardware System**

The Vivado hardware platform provides:

| Peripheral         | Function                                               |
| ------------------ | ------------------------------------------------------ |
| `ps7_uart_1`       | UART console output                                    |
| `ps7_global_timer` | High-resolution timing (used for performance analysis) |
| `ps7_ddr`          | DDR memory interface                                   |
| `axi_gpio`         | Switches, LEDs, Buttons                                |
| `axi_i2s_adi_1`    | I2S controller for SSM2603 codec                       |
| `axi_fifo_mm_s`    | FIFO for RX/TX audio samples                           |

The **SSM2603** handles audio ADC/DAC; its configuration registers are written via **XIicPs**.


---
<p align="center">
  <img src="../img/Zybo_ADDA.jpg" alt="Zybo Z7 board used in the labs" width="650"/>
</p>
*Photo taken during AES Lab*

---

## **Assignment Structure**

You must produce **three progressively more advanced implementations**:

---

# **Step 1 – Loopback (IN → OUT)**

Implement and test the **basic audio loopback**:

* Acquire samples from I2S RX FIFO
* Immediately transmit them to I2S TX FIFO
* No processing
* Codec initialized via I2C
* I2S configured for 48 kHz, 24-bit frame format
* Blocking read/write via FIFO

### **Additional Requirement**

Use the **Global Timer** to measure the sampling period:

```c
u32 t = Xil_In32(GLOBAL_TMR_BASEADDR + GTIMER_COUNTER_LOWER_OFFSET);
```
Given that:

CPU clock ≈ 667 MHz

Global Timer clock = CPU / 2 ≈ 333 MHz

The sampling frequency is:
```
Δticks = t[n+1] − t[n]
Fs     = GLOBAL_TMR_FREQ / Δticks   ≈ 333e6 / Δticks
```

Important:

* UART printing must be done only after capturing t0 and t1, so that serial I/O does not perturb the measurement.
* The Global Timer runs at **half the ARM clock** (≈333 MHz)
* The Global Timer by writing to the CONTROL register (e.g. 0x03 for enable + auto-increment).

---

# **Step 2 – FIR Audio Filtering**
Extend the loopback to apply a software FIR filter in real time.

Implement a **software FIR filter**:

```
y[n] = h0·x[n] + h1·x[n-1] + … + hN·x[n-N]
```
---
<p align="center">
  <img src="img/filter.png" alt="Conceptual functionality of the convolution represented like this" width="650"/>
</p>
*Pic taken from https://sites.google.com/view/lab-instructions-aes/lab3-adda-conversion-and-audio-processing

---
### Requirements:

* Maintain a sliding buffer of past samples
* Use a **parameterizable number of taps**
* Use FIR coefficients provided in main_step2_fir.c:
  * Base Low-Pass (LP) and High-Pass (HP) kernels
  * More aggressive LP / HP versions with more taps
* Implement a FIR function:

```c
int FIR_filter(int *buffer, float *h, int N);
```
where:
* buffer[0] holds the newest sample x[n]
* buffer[k] holds x[n−k]
* h[0..N−1] are the FIR coefficients

**Per-Channel Processing**
* Apply filtering **per-channel** (Left and Right independently)
* Two sliding windows are maintained: bufferL[] and bufferR[].

**Filter Selection via Switches**
* Use switches to select:

- *SW0 = enable Low-Pass filter (LP)*
- *SW1 = enable High-Pass filter (HP)*
- *SW2 = enable Low-Pass (aggressive) filter*
- *SW3 = enable High-Pass (aggressive) filter*
- *If none active → raw passthrough*


Only one filter should be active at a time; priority is evaluated in this order:
`SW0 → SW1 → SW2 → SW3`.
### Testing:

You may use online tone generators such as:
[https://www.szynalski.com/tone-generator](https://www.szynalski.com/tone-generator)

Typical tests:
* 1 kHz sine for LP/HP behavior
* Swept tones / noise for qualitative spectrum shaping

You can also design additional FIR kernels using online tools, e.g.:
* http://t-filter.engineerjs.com/
---

# **Step 3 – Filtering Execution Time Measurement**

Use the Global Timer to measure:

1. **Cycles required by the FIR function**
2. **Cycles available per sample**:

```
budget_cycles = timer_frequency / Fs
```
For Fs ≈ 48 kHz and GLOBAL_TMR_FREQ ≈ 333 MHz:
```
budget_cycles ≈ 333e6 / 48e3 ≈ 6937 cycles per sample
```

3. **Cycles per tap:**
```
cycles_per_tap = cycles_per_call / N

```
4**Maximum feasible taps in real time:**
```
max_taps ≈ budget_cycles / cycles_per_tap

```
Example procedure:

```c
u32 t0 = Xil_In32(GLOBAL_TMR_BASEADDR);
for (int i = 0; i < iterations; i++) {
    dummy = FIR_filter(buffer, h, N);
}
u32 t1   = Xil_In32(GLOBAL_TMR_BASEADDR);
u32 dt   = t1 - t0;
float cycles_per_call = dt / (float)iterations;
```
Where dummy is a volatile variable to prevent the compiler from optimizing away the FIR call.

**Cache ON vs Cache OFF**

Repeat the timing experiment with:

```c
Xil_ICacheDisable();
Xil_DCacheDisable();
```

---

## **Project Structure**

```
Lab3_Audio/
│
├── AES/                                      → Full Xilinx SDK/Vitis workspace
│   ├── lab3_step1_loopback/                  → Step 1: raw audio loopback + Fs
│   ├── lab3_step2_fir/                       → Step 2: FIR filter implementation
│   ├── lab3_step3_timing/                    → Step 3: FIR timing + tap analysis
│   ├── *_bsp/                                → Board Support Package
│   └── design_1_wrapper_hw_platform_0/       → Exported hardware (.hdf/.xsa)
│
├── main_step1_loopback.c                     → Loopback + Global Timer Fs
├── main_step2_fir.c                          → Real-time FIR filtering (LP/HP)
├── main_step3_timing.c                       → FIR timing + cache ON/OFF study
│
└── README.md                                 → Lab documentation (this file)
```

Each folder contains:

* Clean and documented C source
* FIFO-based I2S read/write
* Codec initialization via I2C
* Real-time audio processing

---

## **Implementation Summary**

### 🔹 **Step 1: Loopback**

* Configure SSM2603 via I2C
* Enable I2S RX/TX
* Read left/right samples
* Write back same samples
* Measure sampling frequency

### 🔹 **Step 2: FIR Filtering**

* Convolution via sliding window
* Adjustable number of taps
* LP / HP filters provided
* Switch-controlled processing

### 🔹 **Step 3: FIR Timing**

Measured:

* FIR cycles per sample
* Cycles per tap
* Maximum sustainable taps
* Cache ON vs cache OFF

Example output:

```
N=20 | FIR = 860 cycles | 43 cyc/tap | MAX taps ≈ 160
```

---

## **Audio Test Workflow**

1. Connect **LINE IN** to a tone generator
2. Connect headphones to **HPH OUT**
3. Run Step 1 → verify loopback
4. Run Step 2 → enable FIR via switch
5. Switch between clean / filtered
6. Run Step 3 → observe timing output

---

## **Notes**

* I2S interface is blocking for simplicity
* FIR is pure software implementation
* Coefficients must match the number of taps
* Audio buffers must be reset at startup
* Filtering must not exceed sample period

---

*Developed by Lello Molinario*
