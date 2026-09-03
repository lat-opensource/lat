/* SPDX-License-Identifier: GPL-2.0-only */
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>

static __attribute__((noinline)) __m256 avx_chain(__m256 left, __m256 right)
{
    __m256 scale = _mm256_set1_ps(2.0f);
    return _mm256_add_ps(_mm256_mul_ps(left, scale), right);
}

int main(void)
{
    union { __m256 vector; uint32_t word[8]; } result;
    __m128 low;

    result.vector = avx_chain(_mm256_set1_ps(1.25f), _mm256_set1_ps(0.5f));
    low = _mm256_castps256_ps128(result.vector);
    low = _mm_add_ss(low, _mm_set_ss(1.0f));
    result.vector = _mm256_insertf128_ps(result.vector, low, 0);
    _mm256_zeroupper();
    printf("%08x %08x %08x %08x %08x %08x %08x %08x\n",
           result.word[0], result.word[1], result.word[2], result.word[3],
           result.word[4], result.word[5], result.word[6], result.word[7]);
    return 0;
}
