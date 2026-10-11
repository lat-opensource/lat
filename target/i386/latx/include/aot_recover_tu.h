/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef AOT_RECOVER_TU_H
#define AOT_RECOVER_TU_H

#include <stdbool.h>
#include <stdint.h>

typedef struct aot_recover_tb_member {
    uint32_t tu_id;
    uint32_t cflags;
    uint32_t is_first_tb;
    uint32_t offset_in_tu;
} aot_recover_tb_member;

static inline bool aot_recover_page_count_usable(int num)
{
    return num > 0;
}

/*
 * Collect the members of one TU in code-cache order.
 *
 * A zero tu_id denotes a standalone TB.  Its tu_size/is_first_tb union is
 * non-zero for every standalone TB, so grouping zero-id entries would make
 * several independent TBs look like duplicate TU leaders.
 */
static inline bool aot_recover_collect_tu_members(
        const aot_recover_tb_member *members, int num, int first,
        int max_members, int *tu_indices, int *tu_count, int *first_count)
{
    uint32_t tu_id;
    uint32_t cflags;

    if (members == NULL || tu_indices == NULL || tu_count == NULL ||
        first_count == NULL || num <= 0 || first < 0 || first >= num ||
        max_members <= 0) {
        return false;
    }

    *tu_count = 0;
    *first_count = 0;
    tu_id = members[first].tu_id;
    cflags = members[first].cflags;

    if (tu_id == 0) {
        *tu_count = 1;
        *first_count = members[first].is_first_tb != 0;
        tu_indices[0] = first;
        return true;
    }

    for (int j = 0; j < num; j++) {
        int pos;

        if (members[j].tu_id != tu_id || members[j].cflags != cflags) {
            continue;
        }
        if (members[j].is_first_tb) {
            (*first_count)++;
        }
        if (*tu_count >= max_members) {
            return false;
        }
        pos = (*tu_count)++;
        while (pos > 0 &&
               members[tu_indices[pos - 1]].offset_in_tu >
               members[j].offset_in_tu) {
            tu_indices[pos] = tu_indices[pos - 1];
            pos--;
        }
        tu_indices[pos] = j;
    }

    return true;
}

#endif
