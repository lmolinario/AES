#!/usr/bin/env python3
# ================================================================
#  tip1_q88_to_q08.py
#
#  Lab 5 – DNN on MNIST (UART Microserver – Tip 1)
#  Course: Advanced Embedded Systems (AES)
#  Board: Zybo Z7 – Zynq-7000 (ARM Cortex-A9)
#
#  Description:
#  ---------------------------------------------------------------
#  Tip 1 – UART Bandwidth Optimization
#
#  This script converts MNIST test images stored in Q8.8 fixed-point
#  format (signed 16-bit, little-endian: 2 bytes/pixel) into a compact
#  Q0.8 representation (unsigned 8-bit: 1 byte/pixel).
#
#  Rationale:
#    - MNIST pixels are typically normalized in the range [0, 1).
#    - In Q8.8 this corresponds to integer values in [0x0000, 0x00FF].
#    - Therefore, the Most Significant Byte (MSB) is expected to be 0x00.
#    - By transmitting only the Least Significant Byte (LSB), UART payload
#      is reduced by 50% with no loss of information under the assumption.
#
#  Defensive handling:
#    - If a pixel value exceeds 0x00FF (i.e., MSB != 0), it is clipped to 0x00FF.
#    - This keeps the conversion valid even if the dataset contains out-of-range
#      values due to preprocessing differences.
#
#  Input/Output:
#    - Input:  .bin files, 784 pixels, 2 bytes/pixel (Q8.8)
#    - Output: .bin files, 784 pixels, 1 byte/pixel (Q0.8)
#
#  Author: Lello Molinario
#  Academic Year: 2025–2026
# ================================================================

import os


# ----------------------------------------------------------------
# Configuration
# ----------------------------------------------------------------
# SRC_DIR: directory containing input MNIST images in Q8.8
# DST_DIR: directory where converted images in Q0.8 will be stored
SRC_DIR = "../test/mnist_digits_bin"    # Input images (Q8.8, 2 bytes/pixel)
DST_DIR = "../test/mnist_digits_q08"    # Output images (Q0.8, 1 byte/pixel)

# Ensure output directory exists (idempotent)
os.makedirs(DST_DIR, exist_ok=True)


# ----------------------------------------------------------------
# Batch conversion
# ----------------------------------------------------------------
# Files are processed in deterministic order to ensure reproducibility
# in testing and debugging.
for fname in sorted(os.listdir(SRC_DIR)):

    # Process only binary image files
    if not fname.endswith(".bin"):
        continue

    src_path = os.path.join(SRC_DIR, fname)
    base, _ = os.path.splitext(fname)
    dst_path = os.path.join(DST_DIR, base + "_q08.bin")

    # ------------------------------------------------------------
    # Read source file (Q8.8 format)
    # ------------------------------------------------------------
    # The input is expected to be a raw binary dump of int16 values:
    #   [LSB0, MSB0, LSB1, MSB1, ...]
    with open(src_path, "rb") as f:
        data = f.read()

    # Each pixel must occupy exactly 2 bytes (int16)
    if len(data) % 2 != 0:
        raise ValueError(f"{fname}: invalid file size (odd number of bytes)")

    # Output container: 1 byte per pixel (Q0.8)
    q08 = bytearray()

    # ------------------------------------------------------------
    # Pixel-by-pixel conversion (Q8.8 → Q0.8)
    # ------------------------------------------------------------
    for i in range(0, len(data), 2):

        # Little-endian layout:
        #   data[i]     -> LSB
        #   data[i + 1] -> MSB
        lsb = data[i]
        msb = data[i + 1]

        # Reconstruct integer representation of Q8.8 pixel:
        # value = (MSB << 8) | LSB
        value = (msb << 8) | lsb

        # --------------------------------------------------------
        # Defensive clipping
        # --------------------------------------------------------
        # Tip 1 is correct only if:
        #   0x0000 <= value <= 0x00FF   (MSB == 0)
        #
        # If out-of-range data is found, clip to 0x00FF, which
        # corresponds to the maximum representable value < 1.0 in Q0.8.
        if value > 0x00FF:
            value = 0x00FF

        # --------------------------------------------------------
        # UART optimization: keep only LSB
        # --------------------------------------------------------
        # Q8.8 real value: value / 256
        # Q0.8 real value: (value & 0xFF) / 256
        #
        # Under MSB==0, the numerical value is preserved exactly.
        q08.append(value & 0xFF)

    # ------------------------------------------------------------
    # Write converted image to output file
    # ------------------------------------------------------------
    with open(dst_path, "wb") as f:
        f.write(q08)

    # Log compression result (useful for lab report evidence)
    print(f"{fname}: {len(data)} bytes → {len(q08)} bytes")

print("Conversion completed successfully.")
