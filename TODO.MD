LAB 3
---
testare step 3 ... il resto ok

LAB 4
---



- RIVEDERE IL CODICE DEL LAB 4 :
  - step 2 Funzione vec_add_serial() con restrict.
  - step 3 Compilazione con NEON / auto-vectorization (SIMD)
  - vec_add_serial() con restrict
  - uso della funzione al posto del loop inline
  - compilazione con -O2 / -O3 + NEON
  - evidenza di auto-vectorization SIMD
  - test con ARRAY_SIZE > 10
  - preparare il file di testo 

1. **Array size reale + sperimentazione**

* In `shared.h` hai `#define ARRAY_SIZE 10`: è troppo piccolo per mostrare miglioramenti misurabili.
* Devi provare **più taglie** (es. 1e5, 1e6, 5e6 elementi) e riportare i tempi.

2. **Baseline seriale**

* Dá  integrare e usare la funzione richiesta:

  ```c
  void vec_add_serial(u32 * restrict c, const u32 * restrict a, const u32 * restrict b, int n) {
      for (int i = 0; i < n; ++i) c[i] = a[i] + b[i];
  }
  ```
* e  misurare **tempo seriale (1 core)** vs **tempo parallelo (2 core)**.

3. **SIMD / auto-vectorization (NEON)**

* La consegna richiede di compilare con le opzioni specifiche (vectorize + neon) e **verificare l’assembly** con istruzioni vettoriali.
* Quindi serve:

  * una versione che chiama `vec_add_serial(...)` (su CPU0) compilata con quelle flag,
  * controllare il `.s`/disassembly che compaiano istruzioni NEON (tipicamente `vld1`, `vadd`, `vst1`, ecc.).

4. **File di risultati (obbligatorio per il form)**

* Dá creare un **breve file di testo** con: array size considerata, tempi, speedup parallel, speedup SIMD.


---

## File richiesto dal form: “brief text file”

Crea un file tipo `lab4_results.txt` e incolla questo (poi sostituisci i numeri con i tuoi):

```text
Lab 4 – Results (Parallel + SIMD)

Board: Zybo Z7 (Zynq-7000), bare-metal
Timer: Global Timer @ ~333 MHz (CPU/2)
Caches: Disabled (I-Cache, D-Cache) for coherency in shared DDR test

Array size (n): <INSERISCI N>  (u32 vectors A,B,C in shared DDR @ 0x2F100000)

1) Serial (single-core) – vec_add_serial() on CPU0
- duration_ticks: <...>
- duration_us: <...>

2) Parallel (dual-core) – CPU0 first half + CPU1 second half
- duration_ticks: <...>
- duration_us: <...>

Parallel speedup:
S_par = T_serial / T_parallel = <...>

3) SIMD / Vectorized (single-core, NEON auto-vectorization)
Build flags:
- Compiler: -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -mvectorize-with-neon-quad -ftree-vectorize -fopt-info-vec-optimized ...
- Linker:   -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -mvectorize-with-neon-quad -ftree-vectorize ...

Evidence:
- Disassembly contains NEON vector ops (e.g., vld1/vadd/vst1): <SI/NO + NOTE>

- duration_ticks: <...>
- duration_us: <...>

SIMD speedup:
S_simd = T_serial / T_simd = <...>

Notes:
- Button used as start trigger.
- Correctness check: errors = <0 se OK>.
```
Chissa se devo aggiungiere anche compilatore/versione, e il comando/voce dove ho  preso il disassembly.

---

LAB 5
---

Report in Latex sulle ottimizzazioni ( direi su tutto il lab 5)
verificare nel codice se fare un loop per testare le immagini presenti come esempio
