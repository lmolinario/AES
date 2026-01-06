import numpy as np
import os
# ===============================
# PARAMETRI
# ===============================
QF = 8
IMG_SIZE = 784
BASE = "mnist_digits_bin"
# ===============================
# LOAD BIN (INT16 LITTLE-ENDIAN)
# ===============================
def load_bin(path):
    return np.fromfile(path, dtype=np.int16)

# Bias
b0 = load_bin("../group_4/weights/Gemm0_biases.bin")
b1 = load_bin("../group_4/weights/Gemm1_biases.bin")
b2 = load_bin("../group_4/weights/Gemm2_biases.bin")
b3 = load_bin("../group_4/weights/Gemm3_biases.bin")

# Weights (row-major!)
w0 = load_bin("../group_4/weights/Gemm0_weights.bin").reshape(64, 784)
w1 = load_bin("../group_4/weights/Gemm1_weights.bin").reshape(32, 64)
w2 = load_bin("../group_4/weights/Gemm2_weights.bin").reshape(16, 32)
w3 = load_bin("../group_4/weights/Gemm3_weights.bin").reshape(10, 16)

# ===============================
# LOAD TEST IMAGES
# ===============================
images = []
expected = []

for d in range(10):
    img = load_bin(os.path.join(BASE, f"{d}.bin"))
    assert img.size == IMG_SIZE, f"{d}.bin size error"
    images.append(img)
    expected.append(d)

# ===============================
# DNN OPS (IDENTICHE AL C)
# ===============================
def fc_forward(x, W, b):
    out = np.zeros(W.shape[0], dtype=np.int16)
    for o in range(W.shape[0]):
        mac = int(b[o]) << QF
        for i in range(W.shape[1]):
            mac += int(x[i]) * int(W[o, i])
        out[o] = np.int16(mac >> QF)
    return out

def relu(x):
    return np.maximum(x, 0).astype(np.int16)

def argmax(x):
    return int(np.argmax(x))

# ===============================
# TEST
# ===============================
print("\n===== PYTHON DNN VERIFICATION =====\n")

for i in range(10):
    x = images[i]

    x = fc_forward(x, w0, b0)
    x = relu(x)

    x = fc_forward(x, w1, b1)
    x = relu(x)

    x = fc_forward(x, w2, b2)
    x = relu(x)

    logits = fc_forward(x, w3, b3)

    pred = argmax(logits)

    print(f"Digit {i}: expected={i} predicted={pred}")
    print("logits:", logits.tolist())
    print()
