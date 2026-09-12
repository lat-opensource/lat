/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdint.h>
#include <stdio.h>

struct xsave_area {
    unsigned char bytes[4096];
} __attribute__((aligned(64)));

int main(void)
{
    static struct xsave_area area;
    static const float initial[4] __attribute__((aligned(16))) = {
        1.0f, 2.0f, 4.0f, 8.0f,
    };
    static const float addend[4] __attribute__((aligned(16))) = {
        2.0f, 0.0f, 0.0f, 0.0f,
    };
    uint32_t output[4] __attribute__((aligned(16)));

    __asm__ volatile(
        "movaps %[initial], %%xmm0\n\t"
        "addss %[addend], %%xmm0\n\t"
        "xrstor %[area]\n\t"
        "movaps %%xmm0, %[output]\n\t"
        : [output] "=m" (output), [area] "+m" (area)
        : [initial] "m" (initial), [addend] "m" (addend),
          "a" (0U), "d" (0U)
        : "xmm0", "memory"
    );

    printf("%08x %08x %08x %08x\n",
           output[0], output[1], output[2], output[3]);
    return 0;
}
