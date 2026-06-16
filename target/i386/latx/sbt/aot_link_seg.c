/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file aot_link_seg.c
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT optimization
 */
#include "aot_link_seg.h"
#include "latx-options.h"
#include "reg-map.h"
#include "accel/tcg/internal.h"

#ifdef CONFIG_LATX_AOT
static aot_link_info *aot_global_info;
static int aot_global_info_total;
static int aot_global_info_index;

static void patch_jrra(aot_link_info *info)
{
    TranslationBlock *tb = info->curr;
    TranslationBlock *next = tb_htable_lookup(thread_cpu, tb->next_86_pc,
            0, tb->flags, tb->cflags);
    target_ulong curr_page_addr = tb_page_addr1(info->curr) != -1 ?
        tb_page_addr1(info->curr) : tb_page_addr0(info->curr);
    uint64_t *addr = (uint64_t *)((uintptr_t)tb->tc.ptr
            + (uintptr_t)tb->return_target_ptr);

    if (next && ((curr_page_addr & TARGET_PAGE_MASK) == (tb_page_addr0(next)
                & TARGET_PAGE_MASK))) {
        int temp0 = reg_itemp_map[ITEMP0];
        uint64_t patch_pcalau12i, patch_ori;

        ptrdiff_t offset_high = (((uint64_t)next->tc.ptr >> 12)
            - ((uintptr_t)addr >> 12)) & 0xfffff;
        ptrdiff_t offset_low = (uint64_t)next->tc.ptr & 0xfff;

        /* pcalau12i itemp0, offset_high */
        patch_pcalau12i = 0x1a000000 | temp0 | (offset_high << 5);
        /* ori itemp0, itemp0, offset_low */
        patch_ori = 0x03800000 | (offset_low << 10) | (temp0 << 5) | temp0;
        *addr = (patch_pcalau12i | (patch_ori << 32));
        return;
    }

    uint32_t *patch = (uint32_t *)addr;
    patch[0] = 0x800;
    patch[1] = 0x50000000 | (3 << 10);
}

__inline static void link_aot_tb(aot_link_info *info)
{
    TranslationBlock *tb = info->curr;
    TranslationBlock *aim_tb = NULL;
    if (info->aim1_pc) {
        aim_tb = tb_htable_lookup(thread_cpu, info->aim1_pc,
                0, tb->flags, tb->cflags);
        if (aim_tb) {
            tb_add_jump(tb, 0, aim_tb);
        }
    }
    if (info->aim2_pc) {
        aim_tb = tb_htable_lookup(thread_cpu, info->aim2_pc,
                0, tb->flags, tb->cflags);
        if (aim_tb) {
            tb_add_jump(tb, 1, aim_tb);
        }
    }
}

void try_aot_link(void)
{
    for (int i = 0; i < aot_global_info_index; i++) {
        aot_link_info *info = aot_global_info + i;
        link_aot_tb(info);
#if defined(CONFIG_LATX_JRRA) || defined(CONFIG_LATX_JRRA_STACK)
        if (info->curr->next_86_pc) {
            patch_jrra(info);
        }
#endif
    }
    aot_global_info_index = 0;
}

void aot_link_tree_init(void)
{
    aot_global_info_total = 100000;
    aot_global_info = malloc(aot_global_info_total * sizeof(aot_link_info));
    aot_global_info_index = 0;
}

void aot_link_tree_insert(TranslationBlock *curr,
        target_ulong aim1_pc, target_ulong aim2_pc)
{
    if (aot_global_info_index >= aot_global_info_total) {
        aot_global_info_total += 1000;
        aot_global_info = realloc(aot_global_info,
            aot_global_info_total * sizeof(aot_link_info));
    }
    aot_link_info *info = aot_global_info + aot_global_info_index;
    info->curr = curr;
    info->aim1_pc = aim1_pc;
    info->aim2_pc = aim2_pc;
    aot_global_info_index++;
}
#endif
