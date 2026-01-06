import os
import struct
from collections import Counter

IMG_PIXELS = 28 * 28
SRC_DIR = "test/mnist_digits_bin"   # cartella immagini originali

def analyze_image(path):
    with open(path, "rb") as f:
        data = f.read()

    print("\n" + "="*70)
    print(f"FILE: {path}")
    print(f"Dimensione file: {len(data)} bytes")

    # ---- Caso A: int16 (Q?.?) ----
    if len(data) == IMG_PIXELS * 2:
        values = struct.unpack("<" + "h"*IMG_PIXELS, data)

        msb = [(v >> 8) & 0xFF for v in values]
        lsb = [v & 0xFF for v in values]

        print("Formato candidato: int16 little-endian")

        print(f"Valori min/max: {min(values)} / {max(values)}")

        print(f"MSB unici: {sorted(set(msb))[:10]} ...")
        print(f"LSB unici: {sorted(set(lsb))[:10]} ...")

        print(f"MSB = 0 count: {msb.count(0)} / {IMG_PIXELS}")
        print(f"LSB = 0 count: {lsb.count(0)} / {IMG_PIXELS}")

        # Heuristics
        if all(m == 0 for m in msb):
            print(">>> IMMAGINE È Q0.8 (byte alto sempre zero)")
        elif all(l == 0 for l in lsb):
            print(">>> IMMAGINE È Q8.0 (byte basso sempre zero)")
        elif all(v >= 0 for v in values):
            print(">>> IMMAGINE POSITIVA, POSSIBILE Q8.8")
        else:
            print(">>> FORMATO NON STANDARD")

        # Campione valori
        print("Primi 10 pixel (int16):", values[:10])

    # ---- Caso B: uint8 ----
    elif len(data) == IMG_PIXELS:
        pixels = list(data)
        print("Formato candidato: uint8 (Q0.8 puro)")

        print(f"Valori min/max: {min(pixels)} / {max(pixels)}")
        print("Primi 10 pixel:", pixels[:10])

    else:
        print(">>> DIMENSIONE NON COMPATIBILE CON MNIST")

def main():
    files = sorted(f for f in os.listdir(SRC_DIR) if f.endswith(".bin"))
    if not files:
        print("Nessun file trovato")
        return

    for f in files:
        analyze_image(os.path.join(SRC_DIR, f))

if __name__ == "__main__":
    main()
