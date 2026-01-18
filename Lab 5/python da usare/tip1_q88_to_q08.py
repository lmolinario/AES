import os

# ============================================================
# Tip 1 – UART Bandwidth Optimization
# ------------------------------------------------------------
# This script converts MNIST images stored in Q8.8 fixed-point
# format (int16, 2 bytes per pixel) into Q0.8 format (uint8,
# 1 byte per pixel).
#
# The goal is to reduce UART transmission time by exploiting
# the fact that, for normalized images in the range [0, 1),
# the most significant byte (MSB) of Q8.8 pixels is always 0x00.
#
# By transmitting only the least significant byte (LSB),
# the required UART bandwidth is reduced by 50%, while
# preserving the numerical value of the pixels.
#
# Assumptions:
#  - Pixels are non-negative
#  - Input images are stored in little-endian Q8.8 format
#  - Image size is 28×28 = 784 pixels
# ============================================================

SRC_DIR = "../test/mnist_digits_bin"    # Input images (Q8.8, 2 bytes/pixel)
DST_DIR = "../test/mnist_digits_q08"    # Output images (Q0.8, 1 byte/pixel)

os.makedirs(DST_DIR, exist_ok=True)

for fname in sorted(os.listdir(SRC_DIR)):

    # Process only binary image files
    if not fname.endswith(".bin"):
        continue

    src_path = os.path.join(SRC_DIR, fname)
    base, _ = os.path.splitext(fname)
    dst_path = os.path.join(DST_DIR, base + "_q08.bin")

    # --------------------------------------------------------
    # Read source file (Q8.8 format)
    # --------------------------------------------------------
    with open(src_path, "rb") as f:
        data = f.read()

    # Each pixel must occupy exactly 2 bytes (int16)
    if len(data) % 2 != 0:
        raise ValueError(f"{fname}: invalid file size (odd number of bytes)")

    q08 = bytearray()

    # --------------------------------------------------------
    # Pixel-by-pixel conversion
    # --------------------------------------------------------
    for i in range(0, len(data), 2):

        # Little-endian format:
        #   data[i]     -> LSB
        #   data[i + 1] -> MSB
        lsb = data[i]
        msb = data[i + 1]

        # Reconstruct the Q8.8 fixed-point value
        value = (msb << 8) | lsb

        # ----------------------------------------------------
        # Defensive normalization
        # ----------------------------------------------------
        # Tip 1 is only valid if the pixel value lies in:
        #   [0x0000, 0x00FF]
        # i.e., if the MSB is zero.
        #
        # Since real datasets may not strictly satisfy this
        # condition, values are clipped to the valid range
        # [0, 1) before applying the UART optimization.
        if value > 0x00FF:
            value = 0x00FF

        # ----------------------------------------------------
        # Q8.8 → Q0.8 conversion
        # ----------------------------------------------------
        # Only the least significant byte is kept.
        # The real numerical value is preserved:
        #
        #   Q8.8: value / 256
        #   Q0.8: (value & 0xFF) / 256
        #
        q08.append(value & 0xFF)

    # --------------------------------------------------------
    # Write converted image to output file
    # --------------------------------------------------------
    with open(dst_path, "wb") as f:
        f.write(q08)

    print(f"{fname}: {len(data)} bytes → {len(q08)} bytes")

print("Conversion completed successfully.")
