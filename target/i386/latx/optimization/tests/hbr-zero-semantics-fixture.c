/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Exercises the SHBR self-zeroing and zero-preserving state transitions:
 *   - vpxor xmm, xmm, xmm zeroes the whole register (self-zeroing).
 *   - vaddps of two zero sources stays zero (zero-preserving).
 *   - vdivps  of two zero sources yields NaN (must NOT be tracked as zero).
 *   - vpcmpgtd of two zero sources yields zero (0 > 0 is false).
 *   - vpcmpeqd of two zero sources yields all-ones (must NOT be zero).
 *   - packed floating self-subtraction can yield NaN, unlike integer SUB.
 *   - legacy divps 0/0 must not hide a later PAND source dependency.
 *   - legacy PCMPEQ of distinct zero registers produces all-ones.
 *   - unpack and shuffle operations can move untracked low lanes high.
 *   - legacy and VEX packed subtraction under round-down produce negative zero.
 *   - memory sources prevent both legacy and VEX false-zero propagation.
 */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Vector128 {
    uint32_t word[4];
} Vector128 __attribute__((aligned(16)));

static __attribute__((noinline)) void run_self_zeroing(Vector128 *output)
{
    static const uint32_t one = 0x3f800000;
    __asm__ volatile(
        "vxorps %%xmm0, %%xmm0, %%xmm0\n\t"
        "movss (%[one]), %%xmm1\n\t"
        "vaddss %%xmm1, %%xmm0, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        :
        : [one] "r" (&one), [output] "r" (output)
        : "xmm0", "xmm1", "memory"
    );
}

static __attribute__((noinline)) void run_zero_add(Vector128 *output)
{
    __asm__ volatile(
        "vxorps %%xmm1, %%xmm1, %%xmm1\n\t"
        "vaddps %%xmm1, %%xmm1, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        :
        : [output] "r" (output)
        : "xmm0", "xmm1", "memory"
    );
}

static __attribute__((noinline)) void run_zero_div(Vector128 *output)
{
    __asm__ volatile(
        "vxorps %%xmm1, %%xmm1, %%xmm1\n\t"
        "vdivps %%xmm1, %%xmm1, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        :
        : [output] "r" (output)
        : "xmm0", "xmm1", "memory"
    );
}

static __attribute__((noinline)) void run_legacy_zero_div_dependency(
        Vector128 *output)
{
    static const Vector128 data = { {
        0x3f000000, 0x3f800000, 0x3f000000, 0x40000000,
    } };
    static const uint32_t one = 0x3f800000;
    __asm__ volatile(
        "pxor %%xmm0, %%xmm0\n\t"
        "divps %%xmm0, %%xmm0\n\t"
        "movdqa (%[data]), %%xmm1\n\t"
        "addss (%[one]), %%xmm1\n\t"
        "pand %%xmm1, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        "pxor %%xmm0, %%xmm0\n\t"
        "pxor %%xmm1, %%xmm1\n\t"
        :
        : [data] "r" (&data), [one] "r" (&one), [output] "r" (output)
        : "xmm0", "xmm1", "memory"
    );
}

static __attribute__((noinline)) void run_legacy_zero_cmpeq_dependency(
        Vector128 *output)
{
    static const Vector128 data = { {
        0x3f000000, 0x3f800000, 0x3f000000, 0x40000000,
    } };
    static const uint32_t one = 0x3f800000;
    __asm__ volatile(
        "pxor %%xmm0, %%xmm0\n\t"
        "pxor %%xmm2, %%xmm2\n\t"
        "pcmpeqd %%xmm2, %%xmm0\n\t"
        "movdqa (%[data]), %%xmm1\n\t"
        "addss (%[one]), %%xmm1\n\t"
        "pand %%xmm1, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        "pxor %%xmm0, %%xmm0\n\t"
        "pxor %%xmm1, %%xmm1\n\t"
        "pxor %%xmm2, %%xmm2\n\t"
        :
        : [data] "r" (&data), [one] "r" (&one), [output] "r" (output)
        : "xmm0", "xmm1", "xmm2", "memory"
    );
}

static __attribute__((noinline)) void run_punpck_low_dependency(
        Vector128 *output)
{
    static const Vector128 data = { {
        0x3f000000, 0x12345678, 0x89abcdef, 0xfedcba98,
    } };
    static const uint32_t all_ones = UINT32_MAX;
    static const uint32_t one = 0x3f800000;
    __asm__ volatile(
        "pxor %%xmm0, %%xmm0\n\t"
        "movd (%[all_ones]), %%xmm2\n\t"
        "punpcklbw %%xmm2, %%xmm0\n\t"
        "movdqa (%[data]), %%xmm1\n\t"
        "addss (%[one]), %%xmm1\n\t"
        "pand %%xmm1, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        "pxor %%xmm0, %%xmm0\n\t"
        "pxor %%xmm1, %%xmm1\n\t"
        "pxor %%xmm2, %%xmm2\n\t"
        :
        : [data] "r" (&data), [all_ones] "r" (&all_ones),
          [one] "r" (&one), [output] "r" (output)
        : "xmm0", "xmm1", "xmm2", "memory"
    );
}

static __attribute__((noinline)) void run_pshufd_low_dependency(
        Vector128 *output)
{
    static const Vector128 data = { {
        0x3f000000, 0x12345678, 0x89abcdef, 0xfedcba98,
    } };
    static const uint32_t one = 0x3f800000;
    __asm__ volatile(
        "movd (%[one]), %%xmm0\n\t"
        "pshufd $1, %%xmm0, %%xmm0\n\t"
        "movdqa (%[data]), %%xmm1\n\t"
        "addss (%[one]), %%xmm1\n\t"
        "pand %%xmm1, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        "pxor %%xmm0, %%xmm0\n\t"
        "pxor %%xmm1, %%xmm1\n\t"
        :
        : [data] "r" (&data), [one] "r" (&one), [output] "r" (output)
        : "xmm0", "xmm1", "memory"
    );
}

static __attribute__((noinline)) void run_shufps_low_dependency(
        Vector128 *output)
{
    static const Vector128 data = { {
        0x3f000000, 0x12345678, 0x89abcdef, 0xfedcba98,
    } };
    static const uint32_t one = 0x3f800000;
    __asm__ volatile(
        "movd (%[one]), %%xmm0\n\t"
        "shufps $0x10, %%xmm0, %%xmm0\n\t"
        "movdqa (%[data]), %%xmm1\n\t"
        "addss (%[one]), %%xmm1\n\t"
        "pand %%xmm1, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        "pxor %%xmm0, %%xmm0\n\t"
        "pxor %%xmm1, %%xmm1\n\t"
        :
        : [data] "r" (&data), [one] "r" (&one), [output] "r" (output)
        : "xmm0", "xmm1", "memory"
    );
}

static __attribute__((noinline)) void run_round_down_zero_dependency(
        Vector128 *output)
{
    static const Vector128 data = { {
        0x3f000000, 0x3f800000, 0xbf800000, 0x40000000,
    } };
    static const uint32_t one = 0x3f800000;
    uint32_t old_mxcsr;
    uint32_t down_mxcsr;

    __asm__ volatile("stmxcsr %0" : "=m" (old_mxcsr));
    down_mxcsr = (old_mxcsr & ~UINT32_C(0x6000)) | UINT32_C(0x2000);
    __asm__ volatile("ldmxcsr %0" : : "m" (down_mxcsr));
    __asm__ volatile(
        "pxor %%xmm0, %%xmm0\n\t"
        "subps %%xmm0, %%xmm0\n\t"
        "movdqa (%[data]), %%xmm1\n\t"
        "addss (%[one]), %%xmm1\n\t"
        "pand %%xmm1, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        "pxor %%xmm0, %%xmm0\n\t"
        "pxor %%xmm1, %%xmm1\n\t"
        :
        : [data] "r" (&data), [one] "r" (&one), [output] "r" (output)
        : "xmm0", "xmm1", "memory"
    );
    __asm__ volatile("ldmxcsr %0" : : "m" (old_mxcsr));
}

static __attribute__((noinline)) void run_vex_round_down_zero_dependency(
        Vector128 *output)
{
    static const Vector128 data = { {
        0x3f000000, 0x3f800000, 0xbf800000, 0x40000000,
    } };
    static const uint32_t one = 0x3f800000;
    uint32_t old_mxcsr;
    uint32_t down_mxcsr;

    __asm__ volatile("stmxcsr %0" : "=m" (old_mxcsr));
    down_mxcsr = (old_mxcsr & ~UINT32_C(0x6000)) | UINT32_C(0x2000);
    __asm__ volatile("ldmxcsr %0" : : "m" (down_mxcsr));
    __asm__ volatile(
        "vxorps %%xmm0, %%xmm0, %%xmm0\n\t"
        "vsubps %%xmm0, %%xmm0, %%xmm0\n\t"
        "movdqa (%[data]), %%xmm1\n\t"
        "addss (%[one]), %%xmm1\n\t"
        "pand %%xmm1, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        "vpxor %%xmm0, %%xmm0, %%xmm0\n\t"
        "vpxor %%xmm1, %%xmm1, %%xmm1\n\t"
        :
        : [data] "r" (&data), [one] "r" (&one), [output] "r" (output)
        : "xmm0", "xmm1", "memory"
    );
    __asm__ volatile("ldmxcsr %0" : : "m" (old_mxcsr));
}

static __attribute__((noinline)) void run_zero_cmpgt(Vector128 *output)
{
    __asm__ volatile(
        "vxorps %%xmm1, %%xmm1, %%xmm1\n\t"
        "vpcmpgtd %%xmm1, %%xmm1, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        :
        : [output] "r" (output)
        : "xmm0", "xmm1", "memory"
    );
}

static __attribute__((noinline)) void run_zero_cmpeq(Vector128 *output)
{
    __asm__ volatile(
        "vxorps %%xmm1, %%xmm1, %%xmm1\n\t"
        "vpcmpeqd %%xmm1, %%xmm1, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        :
        : [output] "r" (output)
        : "xmm0", "xmm1", "memory"
    );
}

static __attribute__((noinline)) void run_float_self_sub(Vector128 *output)
{
    static const Vector128 data = { {
        0x3f800000, 0x7f800000, 0x7f800000, 0x7f800000,
    } };
    static const uint32_t one = 0x3f800000;
    __asm__ volatile(
        "movdqa (%[data]), %%xmm1\n\t"
        "addss (%[one]), %%xmm1\n\t"
        "subps %%xmm1, %%xmm1\n\t"
        "movdqa %%xmm1, (%[output])\n\t"
        :
        : [data] "r" (&data), [one] "r" (&one), [output] "r" (output)
        : "xmm1", "memory"
    );
}

static __attribute__((noinline)) void run_legacy_memory_source(
        Vector128 *output)
{
    static const Vector128 data = { {
        0x3f800000, 0x89abcdef, 0x13579bdf, 0xfedcba98,
    } };
    static const Vector128 all_ones = { {
        0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    } };
    static const uint32_t one = 0x3f800000;
    __asm__ volatile(
        "movdqa (%[data]), %%xmm0\n\t"
        "addss (%[one]), %%xmm0\n\t"
        "pxor %%xmm1, %%xmm1\n\t"
        "paddb (%[all_ones]), %%xmm1\n\t"
        "pand %%xmm1, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        :
        : [data] "r" (&data), [one] "r" (&one),
          [all_ones] "r" (&all_ones), [output] "r" (output)
        : "xmm0", "xmm1", "memory"
    );
}

static __attribute__((noinline)) void run_vex_memory_source(Vector128 *output)
{
    static const Vector128 data = { {
        0x3f800000, 0x89abcdef, 0x13579bdf, 0xfedcba98,
    } };
    static const Vector128 all_ones = { {
        0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    } };
    static const uint32_t one = 0x3f800000;
    __asm__ volatile(
        "movdqa (%[data]), %%xmm0\n\t"
        "addss (%[one]), %%xmm0\n\t"
        "vxorps %%xmm1, %%xmm1, %%xmm1\n\t"
        "vpxor (%[all_ones]), %%xmm1, %%xmm2\n\t"
        "pand %%xmm2, %%xmm0\n\t"
        "movdqa %%xmm0, (%[output])\n\t"
        :
        : [data] "r" (&data), [one] "r" (&one),
          [all_ones] "r" (&all_ones), [output] "r" (output)
        : "xmm0", "xmm1", "xmm2", "memory"
    );
}

static int is_nan_word(uint32_t value)
{
    return (value & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000) &&
           (value & UINT32_C(0x007fffff));
}

static int check_vector(const char *name, const Vector128 *actual,
        const Vector128 *expected)
{
    if (actual->word[0] == expected->word[0] &&
        actual->word[1] == expected->word[1] &&
        actual->word[2] == expected->word[2] &&
        actual->word[3] == expected->word[3]) {
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
    static const Vector128 expected_self_zeroing = { {
        0x3f800000, 0x00000000, 0x00000000, 0x00000000,
    } };
    static const Vector128 expected_zero = { {0, 0, 0, 0} };
    /* 0 > 0 is false, so vpcmpgtd yields zero. */
    static const Vector128 expected_zero_cmpgt = { {0, 0, 0, 0} };
    /* 0 == 0 is true, so vpcmpeqd yields all-ones. */
    static const Vector128 expected_zero_cmpeq = { {
        0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    } };
    static const Vector128 expected_legacy_zero_div_dependency = { {
        0x3fc00000, 0x3f800000, 0x3f000000, 0x40000000,
    } };
    static const Vector128 expected_round_down_zero_dependency = { {
        0x00000000, 0x00000000, 0x80000000, 0x00000000,
    } };
    static const Vector128 expected_punpck_low_dependency = { {
        0x3f000000, 0x12005600, 0x00000000, 0x00000000,
    } };
    static const Vector128 expected_pshufd_low_dependency = { {
        0x00000000, 0x12000000, 0x09800000, 0x3e800000,
    } };
    static const Vector128 expected_shufps_low_dependency = { {
        0x3f800000, 0x12000000, 0x00000000, 0x3e800000,
    } };
    static const Vector128 expected_memory_source = { {
        0x40000000, 0x89abcdef, 0x13579bdf, 0xfedcba98,
    } };
    Vector128 self_zeroing = { {0} };
    Vector128 zero_add = { {0} };
    Vector128 zero_div = { {0} };
    Vector128 zero_cmpgt = { {0} };
    Vector128 zero_cmpeq = { {0} };
    Vector128 legacy_zero_div_dependency = { {0} };
    Vector128 legacy_zero_cmpeq_dependency = { {0} };
    Vector128 punpck_low_dependency = { {0} };
    Vector128 pshufd_low_dependency = { {0} };
    Vector128 shufps_low_dependency = { {0} };
    Vector128 round_down_zero_dependency = { {0} };
    Vector128 vex_round_down_zero_dependency = { {0} };
    Vector128 float_self_sub = { {0} };
    Vector128 legacy_memory_source = { {0} };
    Vector128 vex_memory_source = { {0} };
    int failed = 0;

    run_self_zeroing(&self_zeroing);
    run_zero_add(&zero_add);
    run_zero_div(&zero_div);
    run_zero_cmpgt(&zero_cmpgt);
    run_zero_cmpeq(&zero_cmpeq);
    run_legacy_zero_div_dependency(&legacy_zero_div_dependency);
    run_legacy_zero_cmpeq_dependency(&legacy_zero_cmpeq_dependency);
    run_punpck_low_dependency(&punpck_low_dependency);
    run_pshufd_low_dependency(&pshufd_low_dependency);
    run_shufps_low_dependency(&shufps_low_dependency);
    run_round_down_zero_dependency(&round_down_zero_dependency);
    run_vex_round_down_zero_dependency(&vex_round_down_zero_dependency);
    run_float_self_sub(&float_self_sub);
    run_legacy_memory_source(&legacy_memory_source);
    run_vex_memory_source(&vex_memory_source);

    failed |= check_vector("self-zeroing", &self_zeroing,
                           &expected_self_zeroing);
    failed |= check_vector("zero-add", &zero_add, &expected_zero);
    if (!is_nan_word(zero_div.word[0]) || !is_nan_word(zero_div.word[1]) ||
        !is_nan_word(zero_div.word[2]) || !is_nan_word(zero_div.word[3])) {
        fprintf(stderr,
                "zero-div mismatch: got=%08" PRIx32 ":%08" PRIx32
                ":%08" PRIx32 ":%08" PRIx32 "\n",
                zero_div.word[0], zero_div.word[1], zero_div.word[2],
                zero_div.word[3]);
        failed = 1;
    }
    failed |= check_vector("zero-cmpgt", &zero_cmpgt, &expected_zero_cmpgt);
    failed |= check_vector("zero-cmpeq", &zero_cmpeq, &expected_zero_cmpeq);
    failed |= check_vector("legacy-zero-div-dependency",
                           &legacy_zero_div_dependency,
                           &expected_legacy_zero_div_dependency);
    failed |= check_vector("legacy-zero-cmpeq-dependency",
                           &legacy_zero_cmpeq_dependency,
                           &expected_legacy_zero_div_dependency);
    failed |= check_vector("punpck-low-dependency", &punpck_low_dependency,
                           &expected_punpck_low_dependency);
    failed |= check_vector("pshufd-low-dependency", &pshufd_low_dependency,
                           &expected_pshufd_low_dependency);
    failed |= check_vector("shufps-low-dependency", &shufps_low_dependency,
                           &expected_shufps_low_dependency);
    failed |= check_vector("round-down-zero-dependency",
                           &round_down_zero_dependency,
                           &expected_round_down_zero_dependency);
    failed |= check_vector("vex-round-down-zero-dependency",
                           &vex_round_down_zero_dependency,
                           &expected_round_down_zero_dependency);
    if (float_self_sub.word[0] != 0 ||
        !is_nan_word(float_self_sub.word[1]) ||
        !is_nan_word(float_self_sub.word[2]) ||
        !is_nan_word(float_self_sub.word[3])) {
        fprintf(stderr,
                "float-self-sub mismatch: got=%08" PRIx32 ":%08" PRIx32
                ":%08" PRIx32 ":%08" PRIx32 "\n",
                float_self_sub.word[0], float_self_sub.word[1],
                float_self_sub.word[2], float_self_sub.word[3]);
        failed = 1;
    }
    failed |= check_vector("legacy-memory-source", &legacy_memory_source,
                           &expected_memory_source);
    failed |= check_vector("vex-memory-source", &vex_memory_source,
                           &expected_memory_source);
    if (failed) {
        return 1;
    }

    printf("self-zeroing=%08" PRIx32 " zero-add=%08" PRIx32
           " zero-div=%08" PRIx32 " zero-cmpgt=%08" PRIx32
           " zero-cmpeq=%08" PRIx32 " legacy-zero-div=%08" PRIx32
           " legacy-zero-cmpeq=%08" PRIx32
           " punpck-low=%08" PRIx32 " pshufd-low=%08" PRIx32
           " shufps-low=%08" PRIx32
           " round-down-zero=%08" PRIx32 " vex-round-down-zero=%08" PRIx32
           " float-self-sub=%08" PRIx32
           ":%08" PRIx32 " legacy-memory-source=%08" PRIx32
           " vex-memory-source=%08" PRIx32 "\n",
           self_zeroing.word[0], zero_add.word[0], zero_div.word[0],
           zero_cmpgt.word[0], zero_cmpeq.word[0],
           legacy_zero_div_dependency.word[0],
           legacy_zero_cmpeq_dependency.word[0],
           punpck_low_dependency.word[1], pshufd_low_dependency.word[1],
           shufps_low_dependency.word[1],
           round_down_zero_dependency.word[2],
           vex_round_down_zero_dependency.word[2],
           float_self_sub.word[0], float_self_sub.word[1],
           legacy_memory_source.word[0], vex_memory_source.word[0]);
    return 0;
}
