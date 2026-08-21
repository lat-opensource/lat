/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdint.h>
#include <stdio.h>
#include <xmmintrin.h>
#include <smmintrin.h>

struct fxsave_area {
    unsigned char bytes[512];
} __attribute__((aligned(16)));

static __attribute__((noinline)) __m128 branch_loop(__m128 value)
{
    /* Keep the loop and both control-flow successors in the test binary. */
    volatile int count = 5;
    while (count--) {
        if (count & 1) {
            value = _mm_add_ss(value, _mm_set_ss(1.0f));
        } else {
            value = _mm_sub_ss(value, _mm_set_ss(0.5f));
        }
    }
    return value;
}

int main(void)
{
    struct fxsave_area state;
    union { __m128 vector; uint32_t word[4]; } result;
    __m128 mask = _mm_set_ps1(-0.0f);
    __m128 alternate = _mm_set_ps(9.0f, 7.0f, 5.0f, 3.0f);

    result.vector = _mm_set_ps(8.0f, 4.0f, 2.0f, 1.0f);
    result.vector = branch_loop(result.vector);
    result.vector = _mm_blendv_ps(result.vector, alternate, mask);
    __asm__ volatile("fxsave %0" : "=m" (state));
    printf("%08x %08x %08x %08x %02x\n", result.word[0], result.word[1],
           result.word[2], result.word[3], state.bytes[0]);
    return 0;
}
