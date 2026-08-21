/* SPDX-License-Identifier: GPL-2.0-only */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Vector128 {
    uint64_t qword[2];
} Vector128 __attribute__((aligned(16)));

static __attribute__((noinline)) void roundss_live(Vector128 *output)
{
    static const Vector128 input = { {
        UINT64_C(0x402000003fe00000), UINT64_C(0x8877665544332211),
    } };

    __asm__ volatile(
        "movdqa (%[input]), %%xmm0\n\t"
        "roundss $0, %%xmm0, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        :
        : [input] "r" (&input), [output] "r" (output)
        : "xmm0", "memory"
    );
}

static __attribute__((noinline)) uint32_t roundss_dead(void)
{
    static const Vector128 input = { {
        UINT64_C(0x402000003fe00000), UINT64_C(0x8877665544332211),
    } };
    uint32_t output;

    __asm__ volatile(
        "movdqa (%[input]), %%xmm0\n\t"
        "roundss $0, %%xmm0, %%xmm0\n\t"
        "movss %%xmm0, %[output]\n\t"
        "pxor %%xmm0, %%xmm0\n\t"
        : [output] "=m" (output)
        : [input] "r" (&input)
        : "xmm0", "memory"
    );
    return output;
}

static __attribute__((noinline)) void roundsd_live(Vector128 *output)
{
    static const Vector128 input = { {
        UINT64_C(0x3ffc000000000000), UINT64_C(0x8877665544332211),
    } };

    __asm__ volatile(
        "movdqa (%[input]), %%xmm0\n\t"
        "roundsd $0, %%xmm0, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        :
        : [input] "r" (&input), [output] "r" (output)
        : "xmm0", "memory"
    );
}

static __attribute__((noinline)) uint64_t roundsd_dead(void)
{
    static const Vector128 input = { {
        UINT64_C(0x3ffc000000000000), UINT64_C(0x8877665544332211),
    } };
    uint64_t output;

    __asm__ volatile(
        "movdqa (%[input]), %%xmm0\n\t"
        "roundsd $0, %%xmm0, %%xmm0\n\t"
        "movsd %%xmm0, %[output]\n\t"
        "pxor %%xmm0, %%xmm0\n\t"
        : [output] "=m" (output)
        : [input] "r" (&input)
        : "xmm0", "memory"
    );
    return output;
}

int main(void)
{
    Vector128 ss_live;
    Vector128 sd_live;

    roundss_live(&ss_live);
    roundsd_live(&sd_live);
    printf("ss-live=%016" PRIx64 ":%016" PRIx64 " ss-dead=%08" PRIx32
           "\n", ss_live.qword[1], ss_live.qword[0], roundss_dead());
    printf("sd-live=%016" PRIx64 ":%016" PRIx64 " sd-dead=%016" PRIx64
           "\n", sd_live.qword[1], sd_live.qword[0], roundsd_dead());
    return 0;
}
