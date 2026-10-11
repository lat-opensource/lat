/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_HBR_SEMANTIC_H
#define LATX_HBR_SEMANTIC_H

#include <stdbool.h>
#include <stdint.h>

static inline uint16_t shbr_live_before(uint16_t live_after,
        uint16_t definitions, uint16_t dependencies, uint16_t reads)
{
    if (live_after & definitions) {
        live_after = (live_after & ~definitions) | dependencies;
    }
    return live_after | reads;
}

static inline bool shbr_dest_is_dead(uint16_t live_after,
        uint16_t destination, uint16_t reads)
{
    return !(live_after & destination) && !(reads & destination);
}

#endif
