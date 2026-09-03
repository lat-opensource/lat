#include <immintrin.h>
#include <stdint.h>

static __attribute__((noreturn)) void exit_group(int status)
{
    __asm__ volatile("syscall\n1:\n\tjmp 1b"
                     : : "a"(60), "D"(status) : "rcx", "r11");
    __builtin_unreachable();
}

static uint8_t sat_u8(int16_t value)
{
    if (value < 0) {
        return 0;
    }
    if (value > UINT8_MAX) {
        return UINT8_MAX;
    }
    return value;
}

static uint16_t sat_u16(int32_t value)
{
    if (value < 0) {
        return 0;
    }
    if (value > UINT16_MAX) {
        return UINT16_MAX;
    }
    return value;
}

void _start(void)
{
    uint32_t state = 1;
    int16_t h1[16] __attribute__((aligned(32)));
    int16_t h2[16] __attribute__((aligned(32)));
    int32_t w1[8] __attribute__((aligned(32)));
    int32_t w2[8] __attribute__((aligned(32)));
    uint8_t got_b[32] __attribute__((aligned(32)));
    uint16_t got_h[16] __attribute__((aligned(32)));

    for (int round = 0; round < 100000; ++round) {
        for (int i = 0; i < 16; ++i) {
            state = state * 1664525u + 1013904223u;
            h1[i] = state;
            state = state * 1664525u + 1013904223u;
            h2[i] = state;
        }
        for (int i = 0; i < 8; ++i) {
            state = state * 1664525u + 1013904223u;
            w1[i] = state;
            state = state * 1664525u + 1013904223u;
            w2[i] = state;
        }
        if (round == 0) {
            h1[0] = INT16_MIN;
            h1[1] = -1;
            h1[2] = 0;
            h1[3] = UINT8_MAX;
            h1[4] = UINT8_MAX + 1;
            h1[5] = INT16_MAX;
            w1[0] = INT32_MIN;
            w1[1] = -1;
            w1[2] = 0;
            w1[3] = UINT16_MAX;
            w1[4] = UINT16_MAX + 1;
            w1[5] = INT32_MAX;
        }

        __m256i bh1 = _mm256_load_si256((const __m256i *)h1);
        __m256i bh2 = _mm256_load_si256((const __m256i *)h2);
        __m256i bw1 = _mm256_load_si256((const __m256i *)w1);
        __m256i bw2 = _mm256_load_si256((const __m256i *)w2);

        _mm256_store_si256((__m256i *)got_b,
                           _mm256_packus_epi16(bh1, bh2));
        _mm256_store_si256((__m256i *)got_h,
                           _mm256_packus_epi32(bw1, bw2));

        for (int lane = 0; lane < 2; ++lane) {
            for (int i = 0; i < 8; ++i) {
                if (got_b[lane * 16 + i] != sat_u8(h1[lane * 8 + i]) ||
                    got_b[lane * 16 + 8 + i] != sat_u8(h2[lane * 8 + i])) {
                    exit_group(1);
                }
            }
            for (int i = 0; i < 4; ++i) {
                if (got_h[lane * 8 + i] != sat_u16(w1[lane * 4 + i]) ||
                    got_h[lane * 8 + 4 + i] != sat_u16(w2[lane * 4 + i])) {
                    exit_group(2);
                }
            }
        }
    }
    exit_group(0);
}
