/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef KZT_ADDRESS_POLICY_H
#define KZT_ADDRESS_POLICY_H

#include <stdbool.h>
#include <stdint.h>

static inline bool kzt_guest_addr_is_valid(uint64_t addr,
                                            uint64_t guest_addr_max)
{
    return addr <= guest_addr_max;
}

static inline bool kzt_guest_range_is_valid(uint64_t start, uint64_t len,
                                             uint64_t guest_addr_max)
{
    return len - 1 <= guest_addr_max &&
           start <= guest_addr_max - len + 1;
}

static inline bool kzt_data_addr_is_valid(bool kzt_enabled, uint64_t addr,
                                           uint64_t guest_addr_max)
{
    return kzt_guest_addr_is_valid(addr, guest_addr_max) || kzt_enabled;
}

static inline bool kzt_data_range_is_valid(bool kzt_enabled, uint64_t start,
                                            uint64_t len,
                                            uint64_t guest_addr_max)
{
    if (kzt_guest_range_is_valid(start, len, guest_addr_max)) {
        return true;
    }

    /* A host-pointer exception must never make an overflowing range valid. */
    return kzt_enabled && start > guest_addr_max && len != 0 &&
           start <= UINT64_MAX - len + 1;
}

#endif
