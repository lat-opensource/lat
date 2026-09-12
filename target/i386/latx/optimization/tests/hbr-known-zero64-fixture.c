/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Exercise a known-zero SHBR64 run that starts with MOVSD xmm, m64 and ends
 * at a self-aliasing MAXSD.  The architectural high 64 bits must be restored
 * to zero before the final packed read.
 */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Vector128 {
    uint64_t qword[2];
} Vector128 __attribute__((aligned(16)));

static __attribute__((noinline)) void run_known_zero64(Vector128 *output)
{
    static const Vector128 initial = {
        .qword = {
            UINT64_C(0x3ff0000000000000),
            UINT64_C(0x0123456789abcdef),
        },
    };
    static const double value = 1.5;
    static const double addend = 2.0;

    __asm__ volatile(
        "movdqa (%[initial]), %%xmm0\n\t"
        "movsd (%[value]), %%xmm0\n\t"
        "addsd (%[addend]), %%xmm0\n\t"
        "maxsd %%xmm0, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        :
        : [initial] "r" (&initial), [value] "r" (&value),
          [addend] "r" (&addend), [output] "r" (output)
        : "xmm0", "memory"
    );
}

int main(void)
{
    static const Vector128 expected = {
        .qword = {
            UINT64_C(0x400c000000000000),
            UINT64_C(0x0000000000000000),
        },
    };
    Vector128 output = { 0 };

    run_known_zero64(&output);
    if (output.qword[0] != expected.qword[0] ||
        output.qword[1] != expected.qword[1]) {
        fprintf(stderr,
                "known-zero64 mismatch: got=%016" PRIx64 ":%016" PRIx64
                " expected=%016" PRIx64 ":%016" PRIx64 "\n",
                output.qword[0], output.qword[1],
                expected.qword[0], expected.qword[1]);
        return 1;
    }

    printf("known-zero64=%016" PRIx64 ":%016" PRIx64 "\n",
           output.qword[0], output.qword[1]);
    return 0;
}
