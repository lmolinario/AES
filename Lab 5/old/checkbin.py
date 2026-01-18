import os

files = {
  "Gemm0_biases.bin": 64,
  "Gemm1_biases.bin": 32,
  "Gemm2_biases.bin": 16,
  "Gemm3_biases.bin": 10,
  "Gemm0_weights.bin": 784*64,
  "Gemm1_weights.bin": 64*32,
  "Gemm2_weights.bin": 32*16,
  "Gemm3_weights.bin": 16*10,
}

base = "group_4/weights"

for fn, n in files.items():
    path = os.path.join(base, fn)
    size = os.path.getsize(path)
    print(f"{fn:18} size={size:6} bytes | expected int16={n*2:6} | float32={n*4:6}")


import numpy as np

path = "group_4/weights/Gemm3_biases.bin"

raw = open(path, "rb").read()
print("bytes:", len(raw))

i16 = np.frombuffer(raw, dtype="<i2")
f32 = np.frombuffer(raw, dtype="<f4")

print("int16 count:", i16.size, "min/max:", i16.min(), i16.max(), "sample:", i16[:10])
print("float32 count:", f32.size, "min/max:", f32.min(), f32.max(), "sample:", f32[:10])
