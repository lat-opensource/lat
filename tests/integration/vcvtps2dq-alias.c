#include <immintrin.h>
#include <stdint.h>

static __attribute__((noreturn)) void exit_group(int status)
{
    __asm__ volatile("syscall\n1:\n\tjmp 1b"
                     : : "a"(60), "D"(status) : "rcx", "r11");
    __builtin_unreachable();
}

void _start(void)
{
    static const uint32_t bits[8] __attribute__((aligned(32))) = {
        0x00000000, 0x3f000000, 0x3fc00000, 0x40200000,
        0xbf000000, 0xbfc00000, 0x4f000000, 0x7fc12345,
    };
    static const int32_t rounded_expected[4][8] = {
        { 0, 0, 2, 2,  0, -2, INT32_MIN, INT32_MIN },
        { 0, 0, 1, 2, -1, -2, INT32_MIN, INT32_MIN },
        { 0, 1, 2, 3,  0, -1, INT32_MIN, INT32_MIN },
        { 0, 0, 1, 2,  0, -1, INT32_MIN, INT32_MIN },
    };
    static const int32_t truncated_expected[8] = {
        0, 0, 1, 2, 0, -1, INT32_MIN, INT32_MIN,
    };
    int32_t output[4][4][8] __attribute__((aligned(32)));
    __m256 input = _mm256_castsi256_ps(
        _mm256_load_si256((const __m256i *)bits));

    for (unsigned int rounding = 0; rounding < 4; ++rounding) {
        unsigned int mxcsr = _mm_getcsr();
        _mm_setcsr((mxcsr & ~(3u << 13)) | (rounding << 13));

        __m256i rounded_alias = _mm256_castps_si256(input);
        __asm__ volatile("vcvtps2dq %0, %0" : "+x"(rounded_alias));
        _mm256_store_si256((__m256i *)output[rounding][0], rounded_alias);

        __m256i rounded_distinct;
        __asm__ volatile("vcvtps2dq %1, %0"
                         : "=&x"(rounded_distinct) : "x"(input));
        _mm256_store_si256((__m256i *)output[rounding][1], rounded_distinct);

        __m256i truncated_alias = _mm256_castps_si256(input);
        __asm__ volatile("vcvttps2dq %0, %0" : "+x"(truncated_alias));
        _mm256_store_si256((__m256i *)output[rounding][2], truncated_alias);

        __m256i truncated_distinct;
        __asm__ volatile("vcvttps2dq %1, %0"
                         : "=&x"(truncated_distinct) : "x"(input));
        _mm256_store_si256((__m256i *)output[rounding][3], truncated_distinct);

        for (int i = 0; i < 8; ++i) {
            if (output[rounding][0][i] != rounded_expected[rounding][i] ||
                output[rounding][1][i] != rounded_expected[rounding][i] ||
                output[rounding][2][i] != truncated_expected[i] ||
                output[rounding][3][i] != truncated_expected[i]) {
                exit_group(1);
            }
        }
    }
    exit_group(0);
}
