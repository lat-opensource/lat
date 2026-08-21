/* SPDX-License-Identifier: GPL-2.0-only */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct Vector128 {
    uint32_t word[4];
} Vector128 __attribute__((aligned(16)));

static __attribute__((noinline)) uint32_t run_vpsrlvq_data(void)
{
    static const uint64_t data[2] __attribute__((aligned(16))) = {
        UINT64_C(0x112233443f800000), UINT64_C(0xa1b2c3d4e5f60718),
    };
    static const uint64_t counts[2] __attribute__((aligned(16))) = { 40, 0 };
    static const uint32_t one = 0x3f800000;
    uint32_t output;

    __asm__ volatile(
        "vmovdqa (%[data]), %%xmm1\n\t"
        "vaddss (%[one]), %%xmm1, %%xmm1\n\t"
        "vmovdqa (%[counts]), %%xmm2\n\t"
        "vpsrlvq %%xmm2, %%xmm1, %%xmm3\n\t"
        "vmovd %%xmm3, %[output]\n\t"
        : [output] "=r" (output)
        : [data] "r" (data), [one] "r" (&one), [counts] "r" (counts)
        : "xmm1", "xmm2", "xmm3", "memory"
    );
    return output;
}

static __attribute__((noinline)) uint32_t run_vpsrlvq_count(void)
{
    static const uint64_t data[2] __attribute__((aligned(16))) = {
        UINT64_C(0x1122334455667788), UINT64_C(0xa1b2c3d4e5f60718),
    };
    static const uint64_t counts[2] __attribute__((aligned(16))) = {
        UINT64_C(0x0000000100000000), 0,
    };
    static const uint32_t zero;
    uint32_t output;

    __asm__ volatile(
        "vmovdqa (%[data]), %%xmm1\n\t"
        "vmovdqa (%[counts]), %%xmm2\n\t"
        "vaddss (%[zero]), %%xmm2, %%xmm2\n\t"
        "vpsrlvq %%xmm2, %%xmm1, %%xmm3\n\t"
        "vmovd %%xmm3, %[output]\n\t"
        : [output] "=r" (output)
        : [data] "r" (data), [counts] "r" (counts), [zero] "r" (&zero)
        : "xmm1", "xmm2", "xmm3", "memory"
    );
    return output;
}

static __attribute__((noinline)) uint32_t run_vpsllvq_count(void)
{
    static const uint64_t data[2] __attribute__((aligned(16))) = {
        UINT64_C(0x1122334455667788), UINT64_C(0xa1b2c3d4e5f60718),
    };
    static const uint64_t counts[2] __attribute__((aligned(16))) = {
        UINT64_C(0x0000000100000000), 0,
    };
    static const uint32_t zero;
    uint32_t output;

    __asm__ volatile(
        "vmovdqa (%[data]), %%xmm1\n\t"
        "vmovdqa (%[counts]), %%xmm2\n\t"
        "vaddss (%[zero]), %%xmm2, %%xmm2\n\t"
        "vpsllvq %%xmm2, %%xmm1, %%xmm3\n\t"
        "vmovd %%xmm3, %[output]\n\t"
        : [output] "=r" (output)
        : [data] "r" (data), [counts] "r" (counts), [zero] "r" (&zero)
        : "xmm1", "xmm2", "xmm3", "memory"
    );
    return output;
}

static __attribute__((noinline)) void run_vpsllvq_def(Vector128 *output)
{
    static const Vector128 data = { {
        0x3f800000, 0xffffffff, 0x13579bdf, 0xfedcba98,
    } };
    static const uint32_t one = 0x3f800000;
    uint32_t value = 1;
    uint32_t count = 32;

    __asm__ volatile(
        "movd %[value], %%xmm1\n\t"
        "movd %[count], %%xmm2\n\t"
        "vpsllvq %%xmm2, %%xmm1, %%xmm3\n\t"
        "vmovdqa (%[data]), %%xmm4\n\t"
        "vaddss (%[one]), %%xmm4, %%xmm4\n\t"
        "pand %%xmm3, %%xmm4\n\t"
        "vmovdqa %%xmm4, (%[output])\n\t"
        :
        : [value] "r" (value), [count] "r" (count), [data] "r" (&data),
          [one] "r" (&one), [output] "r" (output)
        : "xmm1", "xmm2", "xmm3", "xmm4", "memory"
    );
}

int main(void)
{
    static const Vector128 expected_def = { { 0, 1, 0, 0 } };
    Vector128 def = { {0} };
    uint32_t right_data = run_vpsrlvq_data();
    uint32_t right_count = run_vpsrlvq_count();
    uint32_t left_count = run_vpsllvq_count();

    run_vpsllvq_def(&def);
    if (right_data != 0x00112233 || right_count != 0 || left_count != 0 ||
        memcmp(&def, &expected_def, sizeof(def))) {
        fprintf(stderr,
                "AVX shift mismatch: right-data=%08" PRIx32
                " right-count=%08" PRIx32 " left-count=%08" PRIx32
                " left-def=%08" PRIx32 ":%08" PRIx32 ":%08" PRIx32
                ":%08" PRIx32 "\n",
                right_data, right_count, left_count, def.word[0], def.word[1],
                def.word[2], def.word[3]);
        return 1;
    }

    printf("right-data=%08" PRIx32 " right-count=%08" PRIx32
           " left-count=%08" PRIx32 " left-def=%08" PRIx32 ":%08"
           PRIx32 ":%08" PRIx32 ":%08" PRIx32 "\n",
           right_data, right_count, left_count, def.word[0], def.word[1],
           def.word[2], def.word[3]);
    return 0;
}
