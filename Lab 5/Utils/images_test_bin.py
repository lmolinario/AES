#!/usr/bin/env python3
# ================================================================
#  images_test_bin.py
#
#  Lab 5 – DNN on MNIST (Verification & Debug Utility)
#  Course: Advanced Embedded Systems (AES)
#  Board: Zybo Z7 – Zynq-7000 (ARM Cortex-A9)
#
#  Description:
#  ---------------------------------------------------------------
#  This script performs a consistency check between:
#
#    1) MNIST test images embedded in C code via macros
#       (test_images.h, imm_test_X)
#    2) MNIST images stored as raw binary files (.bin) used for
#       UART-based inference testing
#
#  The goal is to verify that:
#    - The numerical content of the images is identical
#    - No unintended scaling, truncation, or endianness issues
#      were introduced during format conversion or transmission
#
#  The script:
#    - Parses imm_test_X macros from test_images.h
#    - Loads corresponding .bin images (Q8.8 or Q0.8)
#    - Automatically detects the fixed-point scale
#    - Computes pixel-wise differences
#    - Produces diagnostic prints and visual comparison images
#
#  Output:
#    - Console report (max diff, non-zero diff count)
#    - PNG images showing:
#        • C image
#        • BIN image
#        • Absolute difference
#
#  Author: Lello Molinario
#  Academic Year: 2025–2026
# ================================================================

import re
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt


# ================================================================
# PATH CONFIGURATION
# ================================================================
# NOTE:
# Adjust TEST_IMAGES_H to your local workspace.
# BIN_DIR points to the directory containing MNIST .bin images
# received or prepared for UART testing.

TEST_IMAGES_H = r"D:\DATI LELLO\ingegneria_informatica\UNICA LM Cyber\AES\Lab 5\test_images.h"
BIN_DIR = r"D:\DATI LELLO\ingegneria_informatica\UNICA LM Cyber\AES\Lab 5\mnist_from_uart-main\mnist_digits_bin"

OUT_IMG_DIR = Path("../comparison_images")
OUT_IMG_DIR.mkdir(exist_ok=True)


# ================================================================
# PARSE test_images.h (C MACROS)
# ================================================================
def parse_test_images_h(path):
    """
    Parse MNIST test images embedded in test_images.h as C macros.

    Expected format:
        #define imm_test_0 v0, v1, v2, ..., v783
        ...
        #define imm_test_9 ...

    Returns:
        dict[int, np.ndarray]:
            Mapping digit -> image array (int16, length 784)
    """
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    images = {}

    # Match everything after "#define imm_test_X" until end of line
    pattern = re.compile(
        r"#define\s+imm_test_(\d+)\s+([^\n\r]+)"
    )

    matches = pattern.findall(content)

    if not matches:
        raise RuntimeError("Nessuna macro imm_test_X trovata in test_images.h")

    for idx, data in matches:
        # Robust split: remove empty tokens and whitespace
        tokens = [t.strip() for t in data.split(",") if t.strip() != ""]
        values = [int(v) for v in tokens]

        if len(values) != 784:
            raise ValueError(
                f"imm_test_{idx}: {len(values)} valori (attesi 784)"
            )

        images[int(idx)] = np.array(values, dtype=np.int16)

    return images


# ================================================================
# LOAD BIN IMAGE
# ================================================================
def load_bin_image(path):
    """
    Load a raw MNIST image from a .bin file.

    Assumptions:
        - 784 pixels
        - Stored as int16 (Q8.8 or Q0.8 promoted)

    Returns:
        np.ndarray (int16, length 784)
    """
    img = np.fromfile(path, dtype=np.int16)

    if img.size != 784:
        raise ValueError(f"{path} has {img.size} samples (expected 784)")

    return img


# ================================================================
# MAIN COMPARISON LOGIC
# ================================================================
c_images = parse_test_images_h(TEST_IMAGES_H)

print("=== MNIST IMAGE CONSISTENCY CHECK ===\n")

for digit in range(10):
    bin_path = Path(BIN_DIR) / f"{digit}.bin"

    c_img = c_images[digit]
    bin_img = load_bin_image(bin_path)

    # ------------------------------------------------------------
    # Automatic fixed-point scale detection
    # ------------------------------------------------------------
    # If values exceed 8-bit range, assume Q8.8 (divide by 256)
    if np.max(np.abs(bin_img)) > 255:
        scale = 256  # Q8.8
    else:
        scale = 1    # Q0.8

    bin_img_scaled = bin_img // scale

    # Pixel-wise difference
    diff = bin_img_scaled - c_img

    # ------------------------------------------------------------
    # Console report
    # ------------------------------------------------------------
    print(f"Digit {digit}:")
    print(f"  Scale detected : {'Q8.8 (/256)' if scale == 256 else 'Q0.8'}")
    print(f"  Max abs diff  : {np.max(np.abs(diff))}")
    print(f"  Non-zero diff : {np.count_nonzero(diff)}")

    if np.count_nonzero(diff) == 0:
        print("   PERFECT MATCH\n")
    else:
        print("   DIFFERENCES FOUND\n")

    # ============================================================
    # SAVE VISUAL COMPARISON
    # ============================================================
    fig, axs = plt.subplots(1, 3, figsize=(9, 3))

    axs[0].imshow(c_img.reshape(28, 28), cmap="gray")
    axs[0].set_title("C image")

    axs[1].imshow(bin_img_scaled.reshape(28, 28), cmap="gray")
    axs[1].set_title("BIN image")

    axs[2].imshow(np.abs(diff).reshape(28, 28), cmap="hot")
    axs[2].set_title("ABS diff")

    for ax in axs:
        ax.axis("off")

    plt.tight_layout()
    plt.savefig(OUT_IMG_DIR / f"digit_{digit}_compare.png", dpi=150)
    plt.close()

print("Comparison images saved in:", OUT_IMG_DIR.resolve())
