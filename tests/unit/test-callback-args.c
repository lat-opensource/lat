/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "target/i386/latx/context/callback-args.h"

#define TEST_ASSERT(condition)                                             \
    do {                                                                   \
        if (!(condition)) {                                                \
            fprintf(stderr, "%s:%d: assertion failed: %s\n",             \
                    __FILE__, __LINE__, #condition);                       \
            abort();                                                       \
        }                                                                  \
    } while (0)

static void collect_args(LatxCallbackArgs *args, uint64_t *stack,
                         const char *fmt, ...)
{
    va_list ap;

    memset(args, 0, sizeof(*args));
    args->stack = stack;
    va_start(ap, fmt);
    latx_callback_collect_args(args, fmt, &ap);
    va_end(ap);
}

static uint64_t double_bits(double value)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t float_bits(float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void test_integer_widths_and_stack(void)
{
    LatxCallbackArgs args;
    uint64_t stack[5] = { 0 };
    size_t stack_count;
    int marker;

    TEST_ASSERT(latx_callback_stack_args("piuIULlwWcC", &stack_count));
    TEST_ASSERT(stack_count == 5);
    collect_args(&args, stack, "piuIULlwWcC", &marker, -1,
                 UINT32_MAX, INT64_MIN, UINT64_MAX,
                 ULONG_MAX, -2L, -3, 0xfffe, -4, 0xfe);

    TEST_ASSERT(args.gpr_count == 6);
    TEST_ASSERT(args.xmm_count == 0);
    TEST_ASSERT(args.stack_count == 5);
    TEST_ASSERT(args.gpr[0] == (uintptr_t)&marker);
    TEST_ASSERT(args.gpr[1] == UINT64_MAX);
    TEST_ASSERT(args.gpr[2] == UINT32_MAX);
    TEST_ASSERT(args.gpr[3] == (uint64_t)INT64_MIN);
    TEST_ASSERT(args.gpr[4] == UINT64_MAX);
    TEST_ASSERT(args.gpr[5] == ULONG_MAX);
    TEST_ASSERT(stack[0] == UINT64_C(0xfffffffffffffffe));
    TEST_ASSERT(stack[1] == UINT64_C(0xfffffffffffffffd));
    TEST_ASSERT(stack[2] == UINT64_C(0xfffe));
    TEST_ASSERT(stack[3] == UINT64_C(0xfffffffffffffffc));
    TEST_ASSERT(stack[4] == UINT64_C(0xfe));
}

static void test_gpr_and_xmm_are_independent(void)
{
    LatxCallbackArgs args;
    size_t stack_count;

    TEST_ASSERT(latx_callback_stack_args("idididididid", &stack_count));
    TEST_ASSERT(stack_count == 0);
    collect_args(&args, NULL, "idididididid",
                 1, 1.25, 2, 2.25, 3, 3.25,
                 4, 4.25, 5, 5.25, 6, 6.25);

    TEST_ASSERT(args.gpr_count == 6);
    TEST_ASSERT(args.xmm_count == 6);
    TEST_ASSERT(args.stack_count == 0);
    for (size_t i = 0; i < 6; i++) {
        TEST_ASSERT(args.gpr[i] == i + 1);
        TEST_ASSERT(args.xmm[i] == double_bits(i + 1.25));
    }
}

static void test_overflow_keeps_source_order(void)
{
    LatxCallbackArgs args;
    uint64_t stack[2] = { 0 };
    size_t stack_count;

    TEST_ASSERT(latx_callback_stack_args("iiiiiiddddddddid",
                                         &stack_count));
    TEST_ASSERT(stack_count == 2);
    collect_args(&args, stack, "iiiiiiddddddddid",
                 1, 2, 3, 4, 5, 6,
                 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5,
                 7, 9.5);

    TEST_ASSERT(args.gpr_count == 6);
    TEST_ASSERT(args.xmm_count == 8);
    TEST_ASSERT(args.stack_count == 2);
    TEST_ASSERT(stack[0] == 7);
    TEST_ASSERT(stack[1] == double_bits(9.5));
}

static void test_float_promotion_and_overflow(void)
{
    LatxCallbackArgs args;
    uint64_t stack[1] = { 0 };
    size_t stack_count;

    TEST_ASSERT(latx_callback_stack_args("fffffffff", &stack_count));
    TEST_ASSERT(stack_count == 1);
    collect_args(&args, stack, "fffffffff",
                 1.125, 2.125, 3.125, 4.125, 5.125,
                 6.125, 7.125, 8.125, 9.125);

    TEST_ASSERT(args.gpr_count == 0);
    TEST_ASSERT(args.xmm_count == 8);
    TEST_ASSERT(args.stack_count == 1);
    for (size_t i = 0; i < 8; i++) {
        TEST_ASSERT(args.xmm[i] == float_bits(i + 1.125f));
    }
    TEST_ASSERT(stack[0] == float_bits(9.125f));
}

static void test_invalid_and_empty_format(void)
{
    size_t stack_count = 99;

    TEST_ASSERT(latx_callback_stack_args("", &stack_count));
    TEST_ASSERT(stack_count == 0);
    stack_count = 99;
    TEST_ASSERT(!latx_callback_stack_args("px", &stack_count));
    TEST_ASSERT(stack_count == 99);
    TEST_ASSERT(!latx_callback_stack_args(NULL, &stack_count));
}

static void test_stack_alignment(void)
{
    const uintptr_t stack_pointers[] = { 0x1000, 0x1008 };

    for (size_t i = 0; i < sizeof(stack_pointers) /
                              sizeof(stack_pointers[0]); i++) {
        for (size_t stack_args = 0; stack_args < 4; stack_args++) {
            size_t stack_words = latx_callback_stack_words(
                stack_pointers[i], stack_args);
            uintptr_t before_sentinel = stack_pointers[i] -
                stack_words * sizeof(uint64_t);
            uintptr_t guest_entry = before_sentinel - sizeof(uint64_t);

            TEST_ASSERT(stack_words >= stack_args);
            TEST_ASSERT(stack_words <= stack_args + 1);
            TEST_ASSERT((before_sentinel & 0xf) == 0);
            TEST_ASSERT((guest_entry & 0xf) == 8);
        }
    }
}

int main(void)
{
    test_integer_widths_and_stack();
    test_gpr_and_xmm_are_independent();
    test_overflow_keeps_source_order();
    test_float_promotion_and_overflow();
    test_invalid_and_empty_format();
    test_stack_alignment();
    return 0;
}
