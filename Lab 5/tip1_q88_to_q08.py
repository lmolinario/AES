import os

SRC_DIR = "test/mnist_digits_bin"
DST_DIR = "test/mnist_digits_q08"

os.makedirs(DST_DIR, exist_ok=True)

for fname in sorted(os.listdir(SRC_DIR)):
    if not fname.endswith(".bin"):
        continue

    src_path = os.path.join(SRC_DIR, fname)
    base, _ = os.path.splitext(fname)
    dst_path = os.path.join(DST_DIR, base + "_q08.bin")

    with open(src_path, "rb") as f:
        data = f.read()

    if len(data) % 2 != 0:
        raise ValueError(f"{fname}: dimensione non valida")

    q08 = bytearray()

    for i in range(0, len(data), 2):
        q08.append(data[i])   # SOLO LSB

    with open(dst_path, "wb") as f:
        f.write(q08)

    print(f"{fname}: {len(data)} → {len(q08)}")

print("Conversione completata.")
