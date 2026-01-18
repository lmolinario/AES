import re
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt

# ==============================
# PATH CONFIG (TUOI PATH REALI)
# ==============================

TEST_IMAGES_H = r"D:\DATI LELLO\ingegneria_informatica\UNICA LM Cyber\AES\Lab 5\test_images.h"
BIN_DIR = r"/Lab 5/mnist_from_uart-main/mnist_digits_bin"

OUT_IMG_DIR = Path("../comparison_images")
OUT_IMG_DIR.mkdir(exist_ok=True)

# ==============================
# PARSE test_images.h
# ==============================
import re
import numpy as np

def parse_test_images_h(path):
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    images = {}

    # prende TUTTO dopo #define imm_test_X fino a newline
    pattern = re.compile(
        r"#define\s+imm_test_(\d+)\s+([^\n\r]+)"
    )

    matches = pattern.findall(content)

    if not matches:
        raise RuntimeError("Nessuna macro imm_test_X trovata")

    for idx, data in matches:
        # split robusto: elimina token vuoti e whitespace
        tokens = [t.strip() for t in data.split(",") if t.strip() != ""]
        values = [int(v) for v in tokens]

        if len(values) != 784:
            raise ValueError(
                f"imm_test_{idx}: {len(values)} valori (attesi 784)"
            )

        images[int(idx)] = np.array(values, dtype=np.int16)

    return images

# ==============================
# LOAD BIN IMAGE
# ==============================

def load_bin_image(path):
    img = np.fromfile(path, dtype=np.int16)
    if img.size != 784:
        raise ValueError(f"{path} has {img.size} samples (expected 784)")
    return img

# ==============================
# MAIN COMPARISON
# ==============================

c_images = parse_test_images_h(TEST_IMAGES_H)

print("=== MNIST IMAGE CONSISTENCY CHECK ===\n")

for digit in range(10):
    bin_path = Path(BIN_DIR) / f"{digit}.bin"

    c_img = c_images[digit]
    bin_img = load_bin_image(bin_path)

    # Detect scale factor
    if np.max(np.abs(bin_img)) > 255:
        scale = 256  # Q8.8
    else:
        scale = 1    # Q0.8

    bin_img_scaled = bin_img // scale

    diff = bin_img_scaled - c_img

    print(f"Digit {digit}:")
    print(f"  Scale detected: {'Q8.8 (/256)' if scale == 256 else 'Q0.8'}")
    print(f"  Max abs diff : {np.max(np.abs(diff))}")
    print(f"  Non-zero diff: {np.count_nonzero(diff)}")

    if np.count_nonzero(diff) == 0:
        print("  ✔ PERFECT MATCH\n")
    else:
        print("  ⚠ DIFFERENCES FOUND\n")

    # ==============================
    # SAVE VISUAL COMPARISON
    # ==============================

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
