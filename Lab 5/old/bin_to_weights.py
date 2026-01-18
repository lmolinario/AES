import struct

def bin_to_define_q88(binfile, name, outfile, comment=None):
    with open(binfile, "rb") as f:
        data = f.read()

    values = struct.unpack("<" + "h" * (len(data)//2), data)

    if comment:
        outfile.write(f"// {comment}\n")

    outfile.write(f"#define {name} ")

    for i, v in enumerate(values):
        outfile.write(str(v))
        if i != len(values) - 1:
            outfile.write(",")

    outfile.write("\n\n")

def bin_to_define_q17(binfile, name, outfile, comment=None):
    with open(binfile, "rb") as f:
        data = f.read()

    values_q88 = struct.unpack("<" + "h" * (len(data)//2), data)

    values_q17 = []
    for v in values_q88:
        x = v >> 1                  # Q8.8 → Q1.7
        x = max(-128, min(127, x))  # saturazione int8
        values_q17.append(x)

    if comment:
        outfile.write(f"// {comment} (Q1.7 Tip 2)\n")

    outfile.write(f"#define {name}_q17 ")


    for i, v in enumerate(values_q17):
        outfile.write(str(v))
        if i != len(values_q17) - 1:
            outfile.write(",")

    outfile.write("\n\n")



with open("weights_group4.h", "w") as out:

    out.write("/*\n")
    out.write(" * weights_group4.h\n")
    out.write(" *\n")
    out.write(" * Generated automatically\n")
    out.write(" * Baseline Q8.8 + Tip 2 Q1.7\n")
    out.write(" * Author: Lello Molinario\n")
    out.write(" */\n\n")

    out.write("#ifndef SRC_WEIGHTS_GROUP4_H_\n")
    out.write("#define SRC_WEIGHTS_GROUP4_H_\n\n")

    out.write("#include <stdint.h>\n\n")

    # ===== FC0 =====
    bin_to_define_q88("group_4/weights/Gemm0_biases.bin", "bias0", out,
                      "FC0 bias: 64")
    bin_to_define_q17("group_4/weights/Gemm0_biases.bin", "bias0", out,
                      "FC0 bias: 64")

    bin_to_define_q88("group_4/weights/Gemm0_weights.bin", "weights0", out,
                      "FC0 weights: 784 x 64")
    bin_to_define_q17("group_4/weights/Gemm0_weights.bin", "weights0", out,
                      "FC0 weights: 784 x 64")

    # ===== FC1 =====
    bin_to_define_q88("group_4/weights/Gemm1_biases.bin", "bias1", out,
                      "FC1 bias: 32")
    bin_to_define_q17("group_4/weights/Gemm1_biases.bin", "bias1", out,
                      "FC1 bias: 32")

    bin_to_define_q88("group_4/weights/Gemm1_weights.bin", "weights1", out,
                      "FC1 weights: 64 x 32")
    bin_to_define_q17("group_4/weights/Gemm1_weights.bin", "weights1", out,
                      "FC1 weights: 64 x 32")

    # ===== FC2 =====
    bin_to_define_q88("group_4/weights/Gemm2_biases.bin", "bias2", out,
                      "FC2 bias: 16")
    bin_to_define_q17("group_4/weights/Gemm2_biases.bin", "bias2", out,
                      "FC2 bias: 16")

    bin_to_define_q88("group_4/weights/Gemm2_weights.bin", "weights2", out,
                      "FC2 weights: 32 x 16")
    bin_to_define_q17("group_4/weights/Gemm2_weights.bin", "weights2", out,
                      "FC2 weights: 32 x 16")

    # ===== FC3 =====
    bin_to_define_q88("group_4/weights/Gemm3_biases.bin", "bias3", out,
                      "FC3 bias: 10")
    bin_to_define_q17("group_4/weights/Gemm3_biases.bin", "bias3", out,
                      "FC3 bias: 10")

    bin_to_define_q88("group_4/weights/Gemm3_weights.bin", "weights3", out,
                      "FC3 weights: 16 x 10")
    bin_to_define_q17("group_4/weights/Gemm3_weights.bin", "weights3", out,
                      "FC3 weights: 16 x 10")

    out.write("#endif /* SRC_WEIGHTS_GROUP4_H_ */\n")