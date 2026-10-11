/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "aot_recover_tu.h"

static void test_page_count_boundary(void)
{
    assert(!aot_recover_page_count_usable(-1));
    assert(!aot_recover_page_count_usable(0));
    assert(aot_recover_page_count_usable(1));
}

static void test_standalone_tbs_are_not_grouped(void)
{
    const aot_recover_tb_member members[] = {
        { .tu_id = 0, .cflags = 3, .is_first_tb = 64, .offset_in_tu = 0 },
        { .tu_id = 0, .cflags = 3, .is_first_tb = 96, .offset_in_tu = 0 },
    };
    int indices[2];
    int count;
    int first_count;

    assert(aot_recover_collect_tu_members(members, 2, 0, 2, indices,
                                          &count, &first_count));
    assert(count == 1);
    assert(first_count == 1);
    assert(indices[0] == 0);

    assert(aot_recover_collect_tu_members(members, 2, 1, 2, indices,
                                          &count, &first_count));
    assert(count == 1);
    assert(first_count == 1);
    assert(indices[0] == 1);

    assert(!aot_recover_collect_tu_members(members, -1, 0, 2, indices,
                                           &count, &first_count));
}

static void test_non_contiguous_tu_members_are_collected(void)
{
    const aot_recover_tb_member members[] = {
        { .tu_id = 0x401000, .cflags = 3, .is_first_tb = 128,
          .offset_in_tu = 0 },
        { .tu_id = 0, .cflags = 3, .is_first_tb = 64, .offset_in_tu = 0 },
        { .tu_id = 0x402000, .cflags = 3, .is_first_tb = 64,
          .offset_in_tu = 0 },
        { .tu_id = 0x401000, .cflags = 3, .is_first_tb = 0,
          .offset_in_tu = 8 },
        { .tu_id = 0x401000, .cflags = 3, .is_first_tb = 0,
          .offset_in_tu = 4 },
    };
    const int expected[] = { 0, 4, 3 };
    int indices[5];
    int count;
    int first_count;

    assert(aot_recover_collect_tu_members(members, 5, 0, 5, indices,
                                          &count, &first_count));
    assert(count == 3);
    assert(first_count == 1);
    assert(memcmp(indices, expected, sizeof(expected)) == 0);
}

static void test_duplicate_first_and_overflow_are_visible(void)
{
    const aot_recover_tb_member duplicate_first[] = {
        { .tu_id = 0x401000, .cflags = 3, .is_first_tb = 128,
          .offset_in_tu = 0 },
        { .tu_id = 0x401000, .cflags = 3, .is_first_tb = 64,
          .offset_in_tu = 4 },
    };
    const aot_recover_tb_member too_many[] = {
        { .tu_id = 0x401000, .cflags = 3, .is_first_tb = 128,
          .offset_in_tu = 0 },
        { .tu_id = 0x401000, .cflags = 3, .is_first_tb = 0,
          .offset_in_tu = 4 },
        { .tu_id = 0x401000, .cflags = 3, .is_first_tb = 0,
          .offset_in_tu = 8 },
    };
    int indices[3];
    int count;
    int first_count;

    assert(aot_recover_collect_tu_members(duplicate_first, 2, 0, 2,
                                          indices, &count, &first_count));
    assert(count == 2);
    assert(first_count == 2);

    assert(!aot_recover_collect_tu_members(too_many, 3, 0, 2, indices,
                                           &count, &first_count));
}

static void test_large_page_of_standalone_tbs(void)
{
    enum { STANDALONE_COUNT = 512 };
    aot_recover_tb_member *members;
    int index;
    int count;
    int first_count;
    int tu_indices[1];

    members = calloc(STANDALONE_COUNT, sizeof(*members));
    assert(members != NULL);
    for (index = 0; index < STANDALONE_COUNT; index++) {
        members[index].tu_id = 0;
        members[index].cflags = 3;
        members[index].is_first_tb = 64 + index;
        members[index].offset_in_tu = 0;
    }

    for (index = 0; index < STANDALONE_COUNT; index++) {
        assert(aot_recover_collect_tu_members(members, STANDALONE_COUNT, index,
                                              1, tu_indices, &count,
                                              &first_count));
        assert(count == 1);
        assert(first_count == 1);
        assert(tu_indices[0] == index);
    }
    free(members);
}

static void test_single_tu_member_limit(void)
{
    enum {
        TU_MEMBER_LIMIT = 510,
        TU_MEMBER_COUNT = TU_MEMBER_LIMIT + 1,
    };
    aot_recover_tb_member *members;
    int tu_indices[TU_MEMBER_COUNT];
    int count;
    int first_count;

    members = calloc(TU_MEMBER_COUNT, sizeof(*members));
    assert(members != NULL);
    for (int index = 0; index < TU_MEMBER_COUNT; index++) {
        members[index].tu_id = 0x401000;
        members[index].cflags = 3;
        members[index].is_first_tb = index == 0 ? 128 : 0;
        members[index].offset_in_tu = index * 4;
    }

    assert(aot_recover_collect_tu_members(members, TU_MEMBER_LIMIT,
                                          0, TU_MEMBER_LIMIT, tu_indices,
                                          &count, &first_count));
    assert(count == TU_MEMBER_LIMIT);
    assert(first_count == 1);

    assert(!aot_recover_collect_tu_members(members, TU_MEMBER_COUNT,
                                           0, TU_MEMBER_LIMIT, tu_indices,
                                           &count, &first_count));
    free(members);
}

int main(void)
{
    test_page_count_boundary();
    test_standalone_tbs_are_not_grouped();
    test_non_contiguous_tu_members_are_collected();
    test_duplicate_first_and_overflow_are_visible();
    test_large_page_of_standalone_tbs();
    test_single_tu_member_limit();
    return 0;
}
