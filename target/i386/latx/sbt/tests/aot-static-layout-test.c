/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"

#include "aot.h"
#include "aot_static_layout.h"
#include "exec/exec-all.h"

static void test_static_components(void)
{
    AOTStaticLayoutNode nodes[] = {
        { .guest_pc = 0x1000, .code_size = 64 },
        { .guest_pc = 0x1100, .code_size = 64 },
        { .guest_pc = 0x1200, .code_size = 64 },
        { .guest_pc = 0x1300, .code_size = 64 },
        { .guest_pc = 0x2000, .code_size = 64 },
    };
    AOTStaticLayoutEdge edges[] = {
        { .from = 0, .to = 1, .kind = AOT_LAYOUT_EDGE_FLOW },
        { .from = 1, .to = 2, .kind = AOT_LAYOUT_EDGE_FLOW },
        { .from = 2, .to = 0, .kind = AOT_LAYOUT_EDGE_FLOW },
        { .from = 1, .to = 4, .kind = AOT_LAYOUT_EDGE_CALL },
    };
    uint32_t count = aot_static_layout_find_components(nodes,
                                                        G_N_ELEMENTS(nodes),
                                                        edges,
                                                        G_N_ELEMENTS(edges));

    g_assert(count == 1);
    g_assert(nodes[0].component == 0);
    g_assert(nodes[1].component == 0);
    g_assert(nodes[2].component == 0);
    g_assert(nodes[3].component == AOT_STATIC_LAYOUT_NO_COMPONENT);
    g_assert(nodes[4].component == 0);
}

static void test_static_set_placement(void)
{
    AOTStaticLayoutNode nodes[] = {
        { .code_size = 64, .component = 0 },
        { .code_size = 64, .component = 0 },
        { .code_size = 64, .component = 0 },
        { .code_size = 64, .component = 0 },
        { .code_size = 64, .component = 0, .can_pad = true },
        { .code_size = 64000,
          .component = AOT_STATIC_LAYOUT_NO_COMPONENT },
    };
    AOTStaticLayout *layout = aot_static_layout_new(nodes,
                                                     G_N_ELEMENTS(nodes),
                                                     1, 64, 8, 4);

    g_assert(layout);
    for (uint32_t i = 0; i < 4; i++) {
        g_assert(aot_static_layout_place(layout, i, 0, SIZE_MAX) == 0);
    }
    g_assert(aot_static_layout_place(layout, 4, 0, SIZE_MAX) == 64);
    g_assert(aot_static_layout_cost(layout, 0) == 0);
    g_assert(aot_static_layout_padding_used(layout) == 64);
    g_assert(aot_static_layout_place(layout, 4, 0, SIZE_MAX) == 0);
    aot_static_layout_free(layout);
}

static void test_static_budget(void)
{
    AOTStaticLayoutNode nodes[9] = { 0 };
    AOTStaticLayout *layout;

    for (uint32_t i = 0; i < 8; i++) {
        nodes[i].code_size = 64;
        nodes[i].component = 0;
        nodes[i].can_pad = true;
    }
    nodes[8].code_size = 64000;
    nodes[8].component = AOT_STATIC_LAYOUT_NO_COMPONENT;
    layout = aot_static_layout_new(nodes, G_N_ELEMENTS(nodes), 1, 64, 8, 4);

    for (uint32_t i = 0; i < 8; i++) {
        aot_static_layout_place(layout, i, 0, SIZE_MAX);
    }
    g_assert(aot_static_layout_padding_used(layout) <= 640);
    aot_static_layout_free(layout);
}

static void test_static_unavailable_padding(void)
{
    AOTStaticLayoutNode nodes[] = {
        { .code_size = 64, .component = 0 },
        { .code_size = 64, .component = 0 },
        { .code_size = 64, .component = 0 },
        { .code_size = 64, .component = 0 },
        { .code_size = 64, .component = 0, .can_pad = true },
        { .code_size = 64000,
          .component = AOT_STATIC_LAYOUT_NO_COMPONENT },
    };
    AOTStaticLayout *layout = aot_static_layout_new(nodes,
                                                     G_N_ELEMENTS(nodes),
                                                     1, 64, 8, 4);

    for (uint32_t i = 0; i < 4; i++) {
        aot_static_layout_place(layout, i, 0, SIZE_MAX);
    }
    g_assert(aot_static_layout_place(layout, 4, 0, 0) == 0);
    g_assert(aot_static_layout_cost(layout, 0) == 1);
    g_assert(aot_static_layout_padding_used(layout) == 0);
    aot_static_layout_free(layout);
}

static void init_aot_tb(aot_tb *tb, target_ulong guest, uint32_t tu_id,
                        uint32_t code_size)
{
    memset(tb, 0, sizeof(*tb));
    tb->offset_in_segment = guest;
    tb->tu_id = tu_id;
    tb->size = 16;
    tb->next_tb_pc_offset = -1;
    tb->target_tb_pc_offset = -1;
    tb->tu_size = code_size;
}

static void test_static_aot_store(void)
{
    const target_ulong base = 0x400000;
    const uint32_t line_size = 64;
    const uint32_t cycle_size = 64 * 256;
    const uint32_t tb_alloc_size = ROUND_UP(sizeof(TranslationBlock),
                                            line_size);
    const uint32_t code_size = cycle_size - tb_alloc_size - line_size;
    aot_segment segment = {
        .details.seg_begin = base,
        .details.seg_end = base + 0x1000,
        .segment_tbs_num = 6,
    };
    aot_tb tbs[6];

    init_aot_tb(&tbs[0], 0x000, base + 0x000, code_size);
    init_aot_tb(&tbs[1], 0x010, base + 0x000, 0);
    init_aot_tb(&tbs[2], 0x100, base + 0x100, code_size);
    init_aot_tb(&tbs[3], 0x200, base + 0x200, code_size);
    init_aot_tb(&tbs[4], 0x300, base + 0x300, code_size);
    init_aot_tb(&tbs[5], 0x400, base + 0x400, code_size);

    tbs[0].last_ir1_type = IR1_TYPE_BRANCH;
    tbs[0].target_tb_pc_offset = 0x100;
    tbs[1].last_ir1_type = IR1_TYPE_CALL;
    tbs[1].target_tb_pc_offset = 0x400;
    tbs[2].last_ir1_type = IR1_TYPE_BRANCH;
    tbs[2].target_tb_pc_offset = 0x200;
    tbs[3].last_ir1_type = IR1_TYPE_BRANCH;
    tbs[3].target_tb_pc_offset = 0x300;
    tbs[4].last_ir1_type = IR1_TYPE_BRANCH;
    tbs[4].target_tb_pc_offset = 0x400;
    tbs[5].last_ir1_type = IR1_TYPE_BRANCH;
    tbs[5].target_tb_pc_offset = 0x000;

    g_assert(aot_static_layout_store(&segment, tbs, 0, base,
                                     line_size, 256, 4));
    g_assert(tbs[0].layout_padding_lines == 0);
    g_assert(tbs[2].layout_padding_lines == 0);
    g_assert(tbs[3].layout_padding_lines == 0);
    g_assert(tbs[4].layout_padding_lines == 0);
    g_assert(tbs[5].layout_padding_lines > 0);
    g_assert(tbs[5].layout_padding_lines <= 4);
}

static void test_static_stored_padding(void)
{
    aot_header header = {
        .layout_line_log2 = 6,
        .layout_set_log2 = 8,
        .layout_ways = 4,
        .layout_magic = AOT_STATIC_LAYOUT_MAGIC,
    };
    aot_tb tb = { .layout_padding_lines = 2 };

    g_assert(aot_static_layout_header_matches(&header, 64, 256, 4));
    g_assert(!aot_static_layout_header_matches(&header, 64, 256, 8));
    g_assert(aot_static_layout_stored_padding(&header, &tb, 64, 256, 4,
                                              128) == 128);
    g_assert(aot_static_layout_stored_padding(&header, &tb, 64, 256, 4,
                                              127) == 0);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/latx/aot-static-layout/components",
                    test_static_components);
    g_test_add_func("/latx/aot-static-layout/set-placement",
                    test_static_set_placement);
    g_test_add_func("/latx/aot-static-layout/budget",
                    test_static_budget);
    g_test_add_func("/latx/aot-static-layout/unavailable-padding",
                    test_static_unavailable_padding);
    g_test_add_func("/latx/aot-static-layout/aot-store",
                    test_static_aot_store);
    g_test_add_func("/latx/aot-static-layout/stored-padding",
                    test_static_stored_padding);
    return g_test_run();
}
