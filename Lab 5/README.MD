# **Lab 5 – Deep Neural Network Inference on MNIST (UART Microserver)**

---

## **Overview**

This assignment is related to **Lab 5 – Implementing a DNN-based classification microserver on the Zybo Z7**.

The goal is to implement a **fully working inference pipeline** for handwritten digit recognition using a **Deep Neural Network (DNN)** executed on the **ARM Cortex-A9 Processing System**, with input images received through **UART**.

The microserver must:

1. Receive a **28×28 grayscale image** (784 signed 16-bit integers) from UART
2. Process it using a **fixed-point DNN** implemented in C
3. Output the recognized digit (0–9) back to UART
4. Measure execution times using the **Global Timer**
5. Repeat the entire design for a **custom network topology**, assigned by the student’s group

This lab introduces **model inference on embedded systems**, **UART-based microservers**, **layer-by-layer DNN composition**, and **low-level performance profiling**.

---

## **Hardware System**

The hardware platform (from previous labs) includes:

| Peripheral         | Function                                                  |
| ------------------ | --------------------------------------------------------- |
| `ps7_cortexa9_0`   | ARM CPU executing the DNN classification                  |
| `ps7_ddr`          | Storage for weights, buffers, and intermediate tensors    |
| `ps7_uart_1`       | UART interface for receiving images and sending results   |
| `ps7_global_timer` | High-resolution timing for layer-wise and total inference |

The system runs **bare-metal**, without OS support, so all memory management and I/O handling must be implemented explicitly.

---

## **Assignment Structure**

You must implement a complete MNIST inference pipeline, following the steps below.

---

# **Step 1 – Analyze the DNN Topology**

Open the provided ONNX model using:

👉 **[https://netron.app](https://netron.app)**

Verify the sequence of layers and tensor dimensions.

For the base network, the architecture typically consists of:

```
Fully Connected (784 → 64)
ReLU
Fully Connected (64 → 32)
ReLU
Fully Connected (32 → 10)
```

Weights and biases are provided in:

* `weights.h`
* `test_images.h`

---

# **Step 2 – Implement Layer Functions**

The template `app_template.c` provides:

* `FC_forward()` → Fully Connected layer
* `relu_forward()`
* `resultsProcessing()`

You must **compose the network** by chaining layers exactly as displayed in Netron.

Example flow:

```c
FC_forward(image, out0, 784, 64, weights0, bias0, 8);
relu_forward(out0, out0_relu, 64);

FC_forward(out0_relu, out1, 64, 32, weights1, bias1, 8);
relu_forward(out1, out1_relu, 32);

FC_forward(out1_relu, out2, 32, 10, weights2, bias2, 8);
```


---

# **Step 3 – Test on Pre-Loaded Sample Images**

Before handling UART input, test the network locally using:

```c
immagine[i]  // from test_images.h
```

Print the classification result for all provided samples to verify correctness.

This step ensures:

* correct DNN composition
* correct fixed-point arithmetic
* correct weight/bias loading

---

# **Step 4 – Implement UART Input (Microserver Mode)**

The microserver must receive the 784 image pixels via UART as **signed 16-bit integers**.

Reconstruct each pixel using:

```c
byte1 = inbyte();      // LSB
byte2 = inbyte();      // MSB
pixel = (byte2 << 8) | byte1;
```

Implement a helper:

```c
DATA readfromUART();
```

Then fill the input buffer:

```c
for (int i = 0; i < 784; i++)
    image[i] = readfromUART();
```

You may use **RealTerm** (Windows) or the Linux script in the lab package to send image files over UART.

---

# **Step 5 – Implement the Classification Loop**

Create a cyclic loop performing:

1. **Wait for incoming image**
2. **Receive 784 pixels**
3. **Run DNN forward pass**
4. **Print the predicted digit**
5. **Repeat indefinitely**

Pseudo-structure:

```c
while (1) {
    xil_printf("Waiting for the image...\n");

    receive_image_from_uart(image);
    run_dnn(image, output);
    int digit = resultsProcessing(output, 10);

    xil_printf("RESULT: %d\n", digit);
}
```

---

# **Step 6 – Measure Execution Time**

Use the Global Timer to measure:

* Total inference latency
* Layer-by-layer latency (optional)

Example:

```c
XTime t0, t1;
XTime_GetTime(&t0);

FC_forward(...);

XTime_GetTime(&t1);
double seconds = (double)(t1 - t0) / COUNTS_PER_SECOND;
```

The Global Timer runs at **CPU_clock/2 ≈ 333 MHz**.

---

# **Step 7 – Repeat for Your Custom Network**

Based on your matricola:

️**group_number = matricola mod 5 = 4** --->
I belong to **Group 4**

Inside `aes_mnist_assigment_groups.zip`, open folder:

```
Group_4/
```

This folder contains:

* a new ONNX model
* new weights and biases
* new test images

Repeat:

1. DNN topology analysis
2. Code adaptation to new layer sizes
3. Test with new sample images
4. Measure performance

---

## **Project Structure**

```
Lab5_MNIST_DNN/
│
├── AES/                               → SDK workspace
│   ├── mnist_app/                     → Application project
│   ├── *_bsp/                         → Board support package
│   ├── design_1_wrapper_hw_platform   → Hardware platform
│
├── app_template.c                     → Starting template
├── weights.h                          → Base DNN weights and biases
├── test_images.h                      → Sample images
├── uart_images/                       → Images to send to UART
├── README.md                          → Lab documentation (this file)
```

---

## **Implementation Summary**

### 🔹 DNN Inference

* Fixed-point arithmetic (Q8 format)
* Three FC layers, two ReLU
* Manual memory buffers

### 🔹 UART Microserver

* Receive 784 × int16 pixels
* Send classification result
* Blocking UART I/O

### 🔹 Performance Measurement

* Use Global Timer
* Evaluate layer-wise and end-to-end latency

### 🔹 Custom Network

* Replace topology with Group-4 architecture
* Update buffer sizes
* Reload weights and biases

---

## **Output Example**

```
Started
Testing sample images…
Image 0 → Predicted digit: 9
Image 1 → Predicted digit: 3
Image 2 → Predicted digit: 1
--------------------------------
Waiting for the image…
RESULT: 7
Waiting for the image…
RESULT: 2
```

---

## **Notes**

* Add library **-lm** to linker (required for `math.h`)
* Increase stack size in linker script if intermediate buffers overflow
* RealTerm must send raw binary, *not* ASCII
* UART delays do not affect DNN timing measurements
* All code must be clean, documented, and organized according to AES grading guidelines

---

*Developed by Lello Molinario*

