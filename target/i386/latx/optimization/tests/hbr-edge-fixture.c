/* SPDX-License-Identifier: GPL-2.0-only */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct Vector128 {
    uint32_t word[4];
} Vector128 __attribute__((aligned(16)));

static __attribute__((noinline)) void run_psignd(Vector128 *output)
{
    static const Vector128 data = { {
        0x3f800000, 0xfffffff9, 0x00000009, 0x0000000b,
    } };
    static const Vector128 signs = { {
        0x00000001, 0xffffffff, 0x00000000, 0xfffffffe,
    } };
    static const uint32_t one = 0x3f800000;

    __asm__ volatile(
        "movdqa (%[data]), %%xmm1\n\t"
        "addss (%[one]), %%xmm1\n\t"
        "movdqa (%[signs]), %%xmm2\n\t"
        "psignd %%xmm2, %%xmm1\n\t"
        "movdqa %%xmm1, (%[output])\n\t"
        :
        : [data] "r" (&data), [one] "r" (&one), [signs] "r" (&signs),
          [output] "r" (output)
        : "xmm1", "xmm2", "memory"
    );
}

static __attribute__((noinline)) void run_psrlq(Vector128 *output)
{
    static const uint64_t data[2] __attribute__((aligned(16))) = {
        UINT64_C(0x112233443f800000), UINT64_C(0xa1b2c3d4e5f60718),
    };
    static const uint32_t one = 0x3f800000;

    __asm__ volatile(
        "movdqa (%[data]), %%xmm1\n\t"
        "addss (%[one]), %%xmm1\n\t"
        "psrlq $40, %%xmm1\n\t"
        "movdqa %%xmm1, (%[output])\n\t"
        :
        : [data] "r" (data), [one] "r" (&one), [output] "r" (output)
        : "xmm1", "memory"
    );
}

static __attribute__((noinline)) void run_psrldq(Vector128 *output)
{
    static const uint8_t data[16] __attribute__((aligned(16))) = {
        0x00, 0x00, 0x80, 0x3f, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    };
    static const uint32_t one = 0x3f800000;

    __asm__ volatile(
        "movdqa (%[data]), %%xmm1\n\t"
        "addss (%[one]), %%xmm1\n\t"
        "psrldq $12, %%xmm1\n\t"
        "movdqa %%xmm1, (%[output])\n\t"
        :
        : [data] "r" (data), [one] "r" (&one), [output] "r" (output)
        : "xmm1", "memory"
    );
}

static __attribute__((noinline)) void run_pcmpestrm(Vector128 *output)
{
    static const uint8_t left[16] __attribute__((aligned(16))) = {
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    };
    static const uint8_t right[16] __attribute__((aligned(16))) = {
        0x11, 0x42, 0x13, 0x14, 0x45, 0x16, 0x17, 0x18,
        0x49, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x50,
    };
    static const uint8_t sentinel[16] __attribute__((aligned(16))) = {
        0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
        0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
    };

    __asm__ volatile(
        "movdqa (%[left]), %%xmm1\n\t"
        "movdqa (%[right]), %%xmm2\n\t"
        "movdqa (%[sentinel]), %%xmm0\n\t"
        "movl $16, %%eax\n\t"
        "movl $16, %%edx\n\t"
        "pcmpestrm $0x08, %%xmm2, %%xmm1\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        :
        : [left] "r" (left), [right] "r" (right),
          [sentinel] "r" (sentinel), [output] "r" (output)
        : "eax", "edx", "xmm0", "xmm1", "xmm2", "cc", "memory"
    );
}

static __attribute__((noinline)) void run_pcmpistrm(Vector128 *output)
{
    static const uint8_t left[16] __attribute__((aligned(16))) = {
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    };
    static const uint8_t right[16] __attribute__((aligned(16))) = {
        0x11, 0x42, 0x13, 0x14, 0x45, 0x16, 0x17, 0x18,
        0x49, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x50,
    };
    static const uint8_t sentinel[16] __attribute__((aligned(16))) = {
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    };

    __asm__ volatile(
        "movdqa (%[left]), %%xmm1\n\t"
        "movdqa (%[right]), %%xmm2\n\t"
        "movdqa (%[sentinel]), %%xmm0\n\t"
        "pcmpistrm $0x08, %%xmm2, %%xmm1\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        :
        : [left] "r" (left), [right] "r" (right),
          [sentinel] "r" (sentinel), [output] "r" (output)
        : "xmm0", "xmm1", "xmm2", "cc", "memory"
    );
}

static __attribute__((noinline)) void run_pcmpistrm_pand(Vector128 *output)
{
    static const Vector128 data = { {
        0x3f800000, 0x89abcdef, 0x13579bdf, 0xfedcba98,
    } };
    static const uint8_t left[16] __attribute__((aligned(16))) = {
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    };
    static const uint8_t right[16] __attribute__((aligned(16))) = {
        0x11, 0x42, 0x13, 0x14, 0x45, 0x16, 0x17, 0x18,
        0x49, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x50,
    };
    static const uint32_t one = 0x3f800000;

    __asm__ volatile(
        "movdqa (%[data]), %%xmm3\n\t"
        "addss (%[one]), %%xmm3\n\t"
        "pxor %%xmm0, %%xmm0\n\t"
        "movdqa (%[left]), %%xmm1\n\t"
        "movdqa (%[right]), %%xmm2\n\t"
        "pcmpistrm $0x48, %%xmm2, %%xmm1\n\t"
        "pand %%xmm0, %%xmm3\n\t"
        "movdqa %%xmm3, (%[output])\n\t"
        :
        : [data] "r" (&data), [one] "r" (&one), [left] "r" (left),
          [right] "r" (right), [output] "r" (output)
        : "xmm0", "xmm1", "xmm2", "xmm3", "cc", "memory"
    );
}

static __attribute__((noinline)) void run_psignd_zero_sign(Vector128 *output)
{
    static const Vector128 data = { {
        0x3f800000, 0x89abcdef, 0x13579bdf, 0xfedcba98,
    } };
    static const uint32_t one = 0x3f800000;

    __asm__ volatile(
        "movdqa (%[data]), %%xmm1\n\t"
        "addss (%[one]), %%xmm1\n\t"
        "pxor %%xmm2, %%xmm2\n\t"
        "psignd %%xmm2, %%xmm1\n\t"
        "movdqa %%xmm1, (%[output])\n\t"
        :
        : [data] "r" (&data), [one] "r" (&one), [output] "r" (output)
        : "xmm1", "xmm2", "memory"
    );
}

static __attribute__((noinline)) void run_zero_compare_pand(Vector128 *output)
{
    static const Vector128 data = { {
        0x3f800000, 0x89abcdef, 0x13579bdf, 0xfedcba98,
    } };
    static const uint32_t one = 0x3f800000;

    __asm__ volatile(
        "movdqa (%[data]), %%xmm3\n\t"
        "addss (%[one]), %%xmm3\n\t"
        "pxor %%xmm1, %%xmm1\n\t"
        "cmpeqps %%xmm1, %%xmm1\n\t"
        "pand %%xmm1, %%xmm3\n\t"
        "movdqa %%xmm3, (%[output])\n\t"
        :
        : [data] "r" (&data), [one] "r" (&one), [output] "r" (output)
        : "xmm1", "xmm3", "memory"
    );
}

static __attribute__((noinline)) void run_zero_rcpps_pand(Vector128 *output)
{
    static const Vector128 data = { {
        0x3f800000, 0x7fffffff, 0xffffffff, 0x12345678,
    } };
    static const uint32_t one = 0x3f800000;

    __asm__ volatile(
        "movdqa (%[data]), %%xmm3\n\t"
        "addss (%[one]), %%xmm3\n\t"
        "pxor %%xmm1, %%xmm1\n\t"
        "rcpps %%xmm1, %%xmm1\n\t"
        "pand %%xmm1, %%xmm3\n\t"
        "movdqa %%xmm3, (%[output])\n\t"
        :
        : [data] "r" (&data), [one] "r" (&one), [output] "r" (output)
        : "xmm1", "xmm3", "memory"
    );
}

static int check_vector(const char *name, const Vector128 *actual,
        const Vector128 *expected)
{
    if (!memcmp(actual, expected, sizeof(*actual))) {
        return 0;
    }
    fprintf(stderr,
            "%s mismatch: got=%08" PRIx32 ":%08" PRIx32 ":%08" PRIx32
            ":%08" PRIx32 " expected=%08" PRIx32 ":%08" PRIx32
            ":%08" PRIx32 ":%08" PRIx32 "\n",
            name, actual->word[0], actual->word[1], actual->word[2],
            actual->word[3], expected->word[0], expected->word[1],
            expected->word[2], expected->word[3]);
    return 1;
}

int main(void)
{
    static const Vector128 expected_psignd = { {
        0x40000000, 0x00000007, 0x00000000, 0xfffffff5,
    } };
    static const Vector128 expected_psrlq = { {
        0x00112233, 0x00000000, 0x00a1b2c3, 0x00000000,
    } };
    static const Vector128 expected_psrldq = { {
        0x0f0e0d0c, 0x00000000, 0x00000000, 0x00000000,
    } };
    static const Vector128 expected_pcmp = { {
        0x00007eed, 0x00000000, 0x00000000, 0x00000000,
    } };
    static const Vector128 expected_pcmp_pand = { {
        0x40000000, 0x89abcd00, 0x13579b00, 0x00dcba98,
    } };
    static const Vector128 expected_zero = { {0, 0, 0, 0} };
    static const Vector128 expected_compare_pand = { {
        0x40000000, 0x89abcdef, 0x13579bdf, 0xfedcba98,
    } };
    static const Vector128 expected_rcpps_pand = { {
        0x40000000, 0x7f800000, 0x7f800000, 0x12000000,
    } };
    Vector128 psignd = { {0} };
    Vector128 psrlq = { {0} };
    Vector128 psrldq = { {0} };
    Vector128 pcmpestrm = { {0} };
    Vector128 pcmpistrm = { {0} };
    Vector128 pcmpistrm_pand = { {0} };
    Vector128 psignd_zero_sign = { {0} };
    Vector128 compare_pand = { {0} };
    Vector128 rcpps_pand = { {0} };
    int failed = 0;

    run_psignd(&psignd);
    run_psrlq(&psrlq);
    run_psrldq(&psrldq);
    run_pcmpestrm(&pcmpestrm);
    run_pcmpistrm(&pcmpistrm);
    run_pcmpistrm_pand(&pcmpistrm_pand);
    run_psignd_zero_sign(&psignd_zero_sign);
    run_zero_compare_pand(&compare_pand);
    run_zero_rcpps_pand(&rcpps_pand);

    failed |= check_vector("psignd", &psignd, &expected_psignd);
    failed |= check_vector("psrlq", &psrlq, &expected_psrlq);
    failed |= check_vector("psrldq", &psrldq, &expected_psrldq);
    failed |= check_vector("pcmpestrm", &pcmpestrm, &expected_pcmp);
    failed |= check_vector("pcmpistrm", &pcmpistrm, &expected_pcmp);
    failed |= check_vector("pcmpistrm-pand", &pcmpistrm_pand,
                           &expected_pcmp_pand);
    failed |= check_vector("psignd-zero-sign", &psignd_zero_sign,
                           &expected_zero);
    failed |= check_vector("zero-compare-pand", &compare_pand,
                           &expected_compare_pand);
    failed |= check_vector("zero-rcpps-pand", &rcpps_pand,
                           &expected_rcpps_pand);
    if (failed) {
        return 1;
    }

    printf("psignd=%08" PRIx32 ":%08" PRIx32 ":%08" PRIx32 ":%08"
           PRIx32 " psrlq=%08" PRIx32 ":%08" PRIx32 ":%08" PRIx32
           ":%08" PRIx32 " psrldq=%08" PRIx32 " pcmpestrm=%04" PRIx32
           " pcmpistrm=%04" PRIx32 " pcmpistrm-pand=%08" PRIx32 ":%08"
           PRIx32 ":%08" PRIx32 ":%08" PRIx32
           " psignd-zero-sign=%08" PRIx32 " compare-pand=%08" PRIx32 ":%08"
           PRIx32 ":%08" PRIx32 ":%08" PRIx32 " rcpps-pand=%08"
           PRIx32 ":%08" PRIx32 ":%08" PRIx32 ":%08" PRIx32 "\n",
           psignd.word[0], psignd.word[1], psignd.word[2], psignd.word[3],
           psrlq.word[0], psrlq.word[1], psrlq.word[2], psrlq.word[3],
           psrldq.word[0], pcmpestrm.word[0], pcmpistrm.word[0],
           pcmpistrm_pand.word[0], pcmpistrm_pand.word[1],
           pcmpistrm_pand.word[2], pcmpistrm_pand.word[3],
           psignd_zero_sign.word[0],
           compare_pand.word[0], compare_pand.word[1], compare_pand.word[2],
           compare_pand.word[3], rcpps_pand.word[0], rcpps_pand.word[1],
           rcpps_pand.word[2], rcpps_pand.word[3]);
    return 0;
}
