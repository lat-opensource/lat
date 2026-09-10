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
    uint32_t state = 1;
    uint8_t source[32] __attribute__((aligned(32)));
    uint8_t control[32] __attribute__((aligned(32)));
    uint8_t got[32] __attribute__((aligned(32)));

    for (int round = 0; round < 100000; ++round) {
        for (int i = 0; i < 32; ++i) {
            state = state * 1664525u + 1013904223u;
            source[i] = state;
            state = state * 1664525u + 1013904223u;
            control[i] = state;
        }
        if (round == 0) {
            for (int i = 0; i < 32; ++i) {
                source[i] = i;
                control[i] = i * 17;
            }
        }

        __m256i data = _mm256_load_si256((const __m256i *)source);
        __m256i index = _mm256_load_si256((const __m256i *)control);
        _mm256_store_si256((__m256i *)got, _mm256_shuffle_epi8(data, index));

        for (int i = 0; i < 32; ++i) {
            int lane = i & ~15;
            uint8_t want = (control[i] & 0x80) ? 0 :
                           source[lane + (control[i] & 0x0f)];

            if (got[i] != want) {
                exit_group(1);
            }
        }
    }
    exit_group(0);
}
