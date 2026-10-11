/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LAT_EFLAGS_LINK_H
#define LAT_EFLAGS_LINK_H

#include <stdint.h>

enum LatEflagsLinkAction {
    LAT_EFLAGS_LINK_NOP = 1,
    LAT_EFLAGS_LINK_BYPASS = 2,
};

/* Offsets are supplied by the translator, not inferred from opcodes. */
static inline unsigned lat_eflags_link_actions(unsigned incoming_use,
                                               int optimized_taken,
                                               uint16_t instruction_offset,
                                               uint16_t stub_offset)
{
    if (incoming_use) {
        return 0;
    }
    return ((!optimized_taken && instruction_offset != UINT16_MAX) ?
            LAT_EFLAGS_LINK_NOP : 0) |
           ((stub_offset != UINT16_MAX) ? LAT_EFLAGS_LINK_BYPASS : 0);
}

#endif
