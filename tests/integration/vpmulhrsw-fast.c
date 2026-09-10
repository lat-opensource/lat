#include <immintrin.h>
#include <stdint.h>

static int16_t reference(int16_t a, int16_t b)
{
    int32_t value = ((int32_t)a * b + 0x4000) >> 15;

    /* PMULHRSW returns 0x8000 for the sole +32768 result. */
    return (int16_t)value;
}

static __attribute__((noreturn)) void exit_group(int status)
{
    __asm__ volatile("syscall\n1:\n\tjmp 1b"
                     : : "a"(60), "D"(status) : "rcx", "r11");
    __builtin_unreachable();
}

void _start(void)
{
    uint32_t state = 1;
    int16_t a[16] __attribute__((aligned(32)));
    int16_t b[16] __attribute__((aligned(32)));
    int16_t got[16] __attribute__((aligned(32)));

    for (int round = 0; round < 100000; ++round) {
        for (int i = 0; i < 16; ++i) {
            state = state * 1664525u + 1013904223u;
            a[i] = state;
            state = state * 1664525u + 1013904223u;
            b[i] = state;
        }
        if (round == 0) {
            a[0] = b[0] = INT16_MIN;
            a[1] = INT16_MIN;
            b[1] = INT16_MAX;
            a[2] = b[2] = INT16_MAX;
            a[3] = 1;
            b[3] = 0x4000;
            a[4] = -1;
            b[4] = 0x4000;
        }
        __m256i va = _mm256_load_si256((const __m256i *)a);
        __m256i vb = _mm256_load_si256((const __m256i *)b);
        _mm256_store_si256((__m256i *)got, _mm256_mulhrs_epi16(va, vb));
        for (int i = 0; i < 16; ++i) {
            int16_t want = reference(a[i], b[i]);

            if (got[i] != want) {
                exit_group(1);
            }
        }
    }
    exit_group(0);
}
