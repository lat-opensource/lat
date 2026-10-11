/* SPDX-License-Identifier: GPL-2.0-only */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Vector128 {
    uint64_t qword[2];
} Vector128 __attribute__((aligned(16)));

static __attribute__((noinline, noclone)) void run_mem64_pairs(
        const double *input, uintptr_t index, Vector128 output[2])
{
    __asm__ volatile(
        "movsd 0(%[input],%[index]), %%xmm0\n\t"
        "movsd 8(%[input],%[index]), %%xmm1\n\t"
        "mulsd 16(%[input],%[index]), %%xmm0\n\t"
        "mulsd 24(%[input],%[index]), %%xmm1\n\t"
        "movdqa %%xmm0, 0(%[output])\n\t"
        "movdqa %%xmm1, 16(%[output])\n\t"
        :
        : [input] "r" (input), [index] "r" (index),
          [output] "r" (output)
        : "xmm0", "xmm1", "memory"
    );
}

int main(void)
{
    static const double input[4] __attribute__((aligned(16))) = {
        1.5, -4.0, 2.0, 0.5,
    };
    Vector128 output[2] = { 0 };
    const uint64_t expected_low[2] = {
        UINT64_C(0x4008000000000000),
        UINT64_C(0xc000000000000000),
    };

    run_mem64_pairs(input, 0, output);
    for (int i = 0; i < 2; i++) {
        if (output[i].qword[0] != expected_low[i] ||
            output[i].qword[1] != 0) {
            fprintf(stderr,
                    "mem64-pair[%d] mismatch: got=%016" PRIx64
                    ":%016" PRIx64 " expected=%016" PRIx64 ":0\n",
                    i, output[i].qword[0], output[i].qword[1],
                    expected_low[i]);
            return 1;
        }
    }

    printf("mem64-pair=%016" PRIx64 ":%016" PRIx64
           " %016" PRIx64 ":%016" PRIx64 "\n",
           output[0].qword[0], output[0].qword[1],
           output[1].qword[0], output[1].qword[1]);
    return 0;
}
