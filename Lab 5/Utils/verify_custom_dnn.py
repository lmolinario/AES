#!/usr/bin/env python3
# ================================================================
#  dnn_python_reference.py
#
#  Lab 5 – DNN on MNIST (Python Golden Model)
#  Course: Advanced Embedded Systems (AES)
#  Board: Zybo Z7 – Zynq-7000 (ARM Cortex-A9)
#
#  Description:
#  ---------------------------------------------------------------
#  This script implements a **bit-accurate Python reference model**
#  of the DNN used in Lab 5, with the explicit goal of validating
#  the correctness of:
#
#    - weights and biases exported from training
#    - fixed-point arithmetic (Q8.8)
#    - layer ordering and dimensions
#    - ReLU activation behavior
#    - final classification results
#
#  The implementation mirrors *exactly* the C code running on the
#  ARM Cortex-A9:
#    - int16 arithmetic
#    - explicit MAC loops
#    - fixed-point scaling via shifts
#
#  This script therefore acts as a **golden model** for debugging
#  and cross-checking the embedded implementation.
#
#  Author: Lello Molinario
#  Academic Year: 2025–2026
# ================================================================

import numpy as np
import os


# ================================================================
# PARAMETERS
# ================================================================
QF = 8                 # Fractional bits for Q8.8 fixed-point format
IMG_SIZE = 784         # MNIST image size (28 × 28)
BASE = "mnist_digits_bin"  # Directory containing test images


# ================================================================
# LOAD BINARIES (INT16, LITTLE-ENDIAN)
# ================================================================
def load_bin(path):
    """
    Load a raw binary file containing int16 values
    encoded in little-endian format.
    """
    return np.fromfile(path, dtype=np.int16)


# ------------------------------------------------
# Bias vectors
# ------------------------------------------------
b0 = load_bin("../group_4/weights/Gemm0_biases.bin")
b1 = load_bin("../group_4/weights/Gemm1_biases.bin")
b2 = load_bin("../group_4/weights/Gemm2_biases.bin")
b3 = load_bin("../group_4/weights/Gemm3_biases.bin")

# ------------------------------------------------
# Weight matrices (row-major layout)
# ------------------------------------------------
# Dimensions follow the custom Group 4 topology:
#   FC0: 784 → 64
#   FC1: 64  → 32
#   FC2: 32  → 16
#   FC3: 16  → 10
w0 = load_bin("../group_4/weights/Gemm0_weights.bin").reshape(64, 784)
w1 = load_bin("../group_4/weights/Gemm1_weights.bin").reshape(32, 64)
w2 = load_bin("../group_4/weights/Gemm2_weights.bin").reshape(16, 32)
w3 = load_bin("../group_4/weights/Gemm3_weights.bin").reshape(10, 16)


# ================================================================
# LOAD TEST IMAGES
# ================================================================
images = []
expected = []

for d in range(10):
    img = load_bin(os.path.join(BASE, f"{d}.bin"))

    # Each MNIST image must contain exactly 784 samples
    assert img.size == IMG_SIZE, f"{d}.bin size error"

    images.append(img)
    expected.append(d)


# ================================================================
# DNN OPERATIONS (BIT-ACCURATE WITH C CODE)
# ================================================================
def fc_forward(x, W, b):
    """
    Fully-connected layer forward pass.

    This function mirrors the C implementation:
      - Accumulator initialized as (bias << QF)
      - MAC in int32 precision
      - Final right shift by QF
      - Output cast to int16

    Args:
        x (np.ndarray): input vector (int16)
        W (np.ndarray): weight matrix (int16), shape (out, in)
        b (np.ndarray): bias vector (int16)

    Returns:
        np.ndarray: output vector (int16)
    """
    out = np.zeros(W.shape[0], dtype=np.int16)

    for o in range(W.shape[0]):
        mac = int(b[o]) << QF
        for i in range(W.shape[1]):
            mac += int(x[i]) * int(W[o, i])
        out[o] = np.int16(mac >> QF)

    return out


def relu(x):
    """
    ReLU activation function.
    Matches the embedded implementation exactly.
    """
    return np.maximum(x, 0).astype(np.int16)


def argmax(x):
    """
    Return index of maximum value (class prediction).
    """
    return int(np.argmax(x))


# ================================================================
# TEST / VERIFICATION
# ================================================================
print("\n===== PYTHON DNN VERIFICATION =====\n")

for i in range(10):
    x = images[i]

    # FC0 + ReLU
    x = fc_forward(x, w0, b0)
    x = relu(x)

    # FC1 + ReLU
    x = fc_forward(x, w1, b1)
    x = relu(x)

    # FC2 + ReLU
    x = fc_forward(x, w2, b2)
    x = relu(x)

    # FC3 (logits)
    logits = fc_forward(x, w3, b3)

    pred = argmax(logits)

    print(f"Digit {i}: expected={i} predicted={pred}")
    print("logits:", logits.tolist())
    print()
