/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Build on x86-64: cc -O2 -mavx tests/latx/mxcsr-exceptions.c -o mxcsr-exceptions
 * Run natively, then under LATX on LoongArch with LATX_ENABLE_FCSR_EXC=0/1.
 * Requires AVX. A SIGFPE with all exceptions masked is itself a test failure.
 */
#include <stdint.h>
#include <stdio.h>

#define CHECK_SEQUENCE(LOAD, STORE, DIV, ONE, ZERO) do {                  \
    const uint32_t clean = 0x1f80, one = 0x3f800000, zero = 0;            \
    uint32_t saved, raised, cleared;                                    \
    __asm__ volatile(                                                  \
        "stmxcsr %[saved]\n\t"                                         \
        LOAD " %[clean]\n\t"                                           \
        "movss %[one], %%xmm0\n\t"                                     \
        "movss %[zero], %%xmm1\n\t"                                    \
        DIV " " ZERO ", " ONE "\n\t"                                   \
        STORE " %[raised]\n\t"                                         \
        LOAD " %[clean]\n\t"                                           \
        STORE " %[cleared]\n\t"                                        \
        "ldmxcsr %[saved]\n\t"                                         \
        : [saved] "=m" (saved), [raised] "=m" (raised),                  \
          [cleared] "=m" (cleared)                                     \
        : [clean] "m" (clean), [one] "m" (one), [zero] "m" (zero)        \
        : "xmm0", "xmm1", "memory");                                   \
    if (raised != (clean | 4) || cleared != clean) {                     \
        fprintf(stderr, LOAD ": raised=%08x cleared=%08x\n",            \
                raised, cleared);                                      \
        ++failures;                                                    \
    }                                                                  \
    /* Exercise replacement of every exception-flag combination. */     \
    for (uint32_t flags = 0; flags < 64; ++flags) {                      \
        uint32_t input = clean | flags, output;                         \
        __asm__ volatile(                                              \
            "stmxcsr %[saved]\n\t"                                     \
            LOAD " %[input]\n\t"                                       \
            STORE " %[output]\n\t"                                     \
            "ldmxcsr %[saved]\n\t"                                     \
            : [saved] "=m" (saved), [output] "=m" (output)              \
            : [input] "m" (input) : "memory");                          \
        if (output != input) {                                         \
            fprintf(stderr, LOAD ": input=%08x output=%08x\n",          \
                    input, output);                                    \
            ++failures;                                                \
        }                                                              \
    }                                                                  \
} while (0)

int main(void)
{
    int failures = 0;
    CHECK_SEQUENCE("ldmxcsr", "stmxcsr", "divss", "%%xmm0", "%%xmm1");
    CHECK_SEQUENCE("vldmxcsr", "vstmxcsr", "vdivss",
                   "%%xmm0, %%xmm0", "%%xmm1");
    printf("MXCSR exception tests: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures != 0;
}
