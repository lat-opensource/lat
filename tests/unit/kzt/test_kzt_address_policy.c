/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <stdint.h>
#include <stdlib.h>

#include "kzt-address-policy.h"

static const uint64_t guest_addr_max = 0x3fffffffffULL;

#define CHECK(condition) do {     \
    if (!(condition)) {           \
        abort();                  \
    }                             \
} while (0)

static void test_guest_vm_policy_stays_strict(void)
{
    CHECK(kzt_guest_addr_is_valid(guest_addr_max, guest_addr_max));
    CHECK(!kzt_guest_addr_is_valid(guest_addr_max + 1, guest_addr_max));

    CHECK(kzt_guest_range_is_valid(guest_addr_max - 0xfff, 0x1000,
                                   guest_addr_max));
    CHECK(!kzt_guest_range_is_valid(guest_addr_max, 2, guest_addr_max));
    CHECK(!kzt_guest_range_is_valid(UINT64_MAX - 1, 4, guest_addr_max));
    CHECK(!kzt_guest_range_is_valid(0, 0, guest_addr_max));
}

static void test_kzt_data_policy_keeps_host_pointer_compatibility(void)
{
    uint64_t host_pointer = guest_addr_max + 0x10000;

    CHECK(!kzt_data_addr_is_valid(false, host_pointer, guest_addr_max));
    CHECK(kzt_data_addr_is_valid(true, host_pointer, guest_addr_max));

    CHECK(!kzt_data_range_is_valid(false, host_pointer, 32,
                                   guest_addr_max));
    CHECK(kzt_data_range_is_valid(true, host_pointer, 32,
                                  guest_addr_max));

    CHECK(!kzt_data_range_is_valid(true, guest_addr_max, 2,
                                   guest_addr_max));
    CHECK(!kzt_data_range_is_valid(true, UINT64_MAX - 1, 4,
                                   guest_addr_max));
    CHECK(!kzt_data_range_is_valid(true, host_pointer, 0,
                                   guest_addr_max));
}

int main(void)
{
    test_guest_vm_policy_stays_strict();
    test_kzt_data_policy_keeps_host_pointer_compatibility();
    return 0;
}
