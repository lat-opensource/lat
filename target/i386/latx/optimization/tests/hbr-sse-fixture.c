/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdint.h>
#include <stdio.h>
#include <xmmintrin.h>

static __attribute__((noinline)) __m128 scalar_chain(__m128 value)
{
    /* Force a memory-backed input for the translated scalar operation. */
    volatile float addend = 1.25f;
    return _mm_add_ss(value, _mm_set_ss(addend));
}

int main(void)
{
    union {
        __m128 vector;
        uint32_t word[4];
    } result;

    result.vector = _mm_set_ps(8.0f, 4.0f, 2.0f, 1.0f);
    result.vector = scalar_chain(result.vector);
    printf("%08x %08x %08x %08x\n", result.word[0], result.word[1],
           result.word[2], result.word[3]);
    return 0;
}
