/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "target/i386/latx/include/callback-fpr.h"
#include "test-kzt-callback-fpr.h"

static const uint64_t native_values[8] = {
    UINT64_C(0x3ff0000000000000), UINT64_C(0x4000000000000000),
    UINT64_C(0x4008000000000000), UINT64_C(0x4010000000000000),
    UINT64_C(0x4014000000000000), UINT64_C(0x4018000000000000),
    UINT64_C(0x401c000000000000), UINT64_C(0x4020000000000000),
};

static const uint64_t guest_values[8] = {
    UINT64_C(0xdeadbeef00000018), UINT64_C(0xdeadbeef00000019),
    UINT64_C(0xdeadbeef0000001a), UINT64_C(0xdeadbeef0000001b),
    UINT64_C(0xdeadbeef0000001c), UINT64_C(0xdeadbeef0000001d),
    UINT64_C(0xdeadbeef0000001e), UINT64_C(0xdeadbeef0000001f),
};

static bool callback_ran;

static void load_f24_f31(const uint64_t values[8])
{
    __asm__ volatile(
        "fld.d $f24, %0\n\t"
        "fld.d $f25, %1\n\t"
        "fld.d $f26, %2\n\t"
        "fld.d $f27, %3\n\t"
        "fld.d $f28, %4\n\t"
        "fld.d $f29, %5\n\t"
        "fld.d $f30, %6\n\t"
        "fld.d $f31, %7\n\t"
        :
        : "m"(values[0]), "m"(values[1]), "m"(values[2]), "m"(values[3]),
          "m"(values[4]), "m"(values[5]), "m"(values[6]), "m"(values[7])
        : "memory");
}

static void store_f24_f31(uint64_t values[8])
{
    __asm__ volatile(
        "fst.d $f24, %0\n\t"
        "fst.d $f25, %1\n\t"
        "fst.d $f26, %2\n\t"
        "fst.d $f27, %3\n\t"
        "fst.d $f28, %4\n\t"
        "fst.d $f29, %5\n\t"
        "fst.d $f30, %6\n\t"
        "fst.d $f31, %7\n\t"
        : "=m"(values[0]), "=m"(values[1]), "=m"(values[2]), "=m"(values[3]),
          "=m"(values[4]), "=m"(values[5]), "=m"(values[6]), "=m"(values[7])
        :
        : "memory");
}

void cpu_loop(void *cpu)
{
    g_assert_null(cpu);
    callback_ran = true;
    load_f24_f31(guest_values);
}

int main(void)
{
    uint64_t actual[8];

    load_f24_f31(native_values);
    latx_kzt_callback_cpu_loop(NULL);
    store_f24_f31(actual);

    g_assert_true(callback_ran);
    g_assert_cmpmem(actual, sizeof(actual), native_values,
                    sizeof(native_values));
    return 0;
}
