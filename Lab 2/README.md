# Lab 2 – UART Microserver

---

## Instructions

This assignment is related to Lab 2 on the UART microserver.  
Please prepare the following code versions:

- **Negative**
- **Histogram stretching** (with arbitrary image size)
- **Any optional code** you have implemented (bonus)

The microserver must:

1. Receive a **PPM P6** (binary) image over UART, including:
   - Magic number (`P6`)
   - Dimensions (`<width> <height>`)
   - Max value (`255`)
   - Raw RGB pixel data (`width × height × 3` bytes)

2. Process the pixel data according to the selected version:
   - **Negative**
   - **Histogram Stretching**
   - **Histogram Equalization** (optional bonus)

3. Send back a **valid PPM P6 image**, with:
   - Same header (3 lines)
   - Processed RGB bytes

---

## Supported Image Format

| Field       | Description                        |
|-------------|------------------------------------|
| Magic       | `P6` (binary RGB)                  |
| Dimensions  | Arbitrary `<width> <height>`       |
| Max value   | Must be `255`                      |
| Pixel data  | `width × height × 3` bytes (RGB)   |

Example header:

```

P6
128 128
255

```

---

## UART Protocol

| Step | Description                                      |
|------|--------------------------------------------------|
| 1    | Client sends 3 header lines (`P6`, size, maxval) |
| 2    | Client sends RGB data                            |
| 3    | Microserver processes the image                  |
| 4    | Microserver sends back header + new RGB data     |

Baud rate: **115200 8N1**

Compatible with:

- **RealTerm** (Windows)
- **gtkterm** / **minicom** (Linux)
- **ImageMagick** (`display out.ppm`)
- **Netpbm viewer** (http://paulcuth.me.uk/netpbm-viewer)

---

## Project Structure

```

Lab2_UART/
│
├── AES/                                → Full Xilinx SDK 2019.1 workspace
│   ├── lab2_negative/                  → SDK project (Negative transformation)
│   ├── lab2_hist_stretch/              → SDK project (Histogram stretching)
│   ├── lab2_hist_equalization/         → SDK project (Histogram equalization)
│   ├── *_bsp/                          → Auto-generated Board Support Packages
│   ├── design_1_wrapper_hw_platform_0/ → Exported Vivado hardware platform (.hdf)
│   └── SDK.log                         → Build/debug session log
│
├── main_negative.c                     → Negative transformation
├── main_hist_stretch.c                 → Linear histogram stretching
├── main_hist_equalization.c            → Global histogram equalization (bonus)
│
└── README.md                           → Lab documentation

```

Each folder contains:

- A **clean and documented** C source file.
- Ready-to-compile code for Xilinx SDK / Vitis.
- UART I/O and PPM parsing implemented directly on PS UART1.

---

## Implementations Summary

### 🔹 (1) Negative Version — `main_negative.c`

Computes the **negative** of each RGB channel:

```

p_out = 255 - p_in

```

**Features**

- Supports arbitrary image sizes  
- Works on all RGB channels  
- Header preserved exactly as received  
- Minimal and efficient transformation  

**Example**

Input pixel:  (20, 120, 200)  
Output:       (235, 135, 55)

---

### 🔹 (2) Histogram Stretching — `main_hist_stretch.c`

Performs **linear contrast stretching**:

```

I_min = min(pixel values)
I_max = max(pixel values)

p_out = (p - I_min) * 255 / (I_max - I_min)

```

**Features**

- Automatic dynamic-range detection  
- Enhances low-contrast images  
- Fully compatible with arbitrary resolution  
- Handles flat images safely  

Example: values in `[40..190]` → mapped into `[0..255]`.

---

### 🔹 (3) Histogram Equalization (Bonus) — `main_hist_equalization.c`

Implements **global histogram equalization** applied to all RGB bytes:

Steps:

1. Build histogram  
2. Compute CDF  
3. Compute mapping:

```

map[i] = round((CDF[i] - CDF_min) / (N - CDF_min) * 255)

```

4. Apply mapping  

**Features**

- Strong contrast enhancement  
- CDF-based dynamic remapping  
- Efficient on the Zybo  

---

## Example Workflow

1. Send image from PC:
```

cat image.ppm > COM4

```
(or RealTerm binary send)

2. Zybo processes image.

3. Receive output:
```

COM4 > out.ppm

```

4. View:
```

display out.ppm

```

---

## Notes

- UART communication is blocking for reliability  
- Pixel buffer is allocated dynamically  
- Processing is performed *in-place*  
- Header is retransmitted **unchanged**  

---

*Developed by Lello Molinario*
