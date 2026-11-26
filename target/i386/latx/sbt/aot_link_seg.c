/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file aot_link_seg.c
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT optimization
 */
#include "aot_link_seg.h"
#include "qemu/error-report.h"
#include "latx-options.h"
#include "reg-map.h"
#include "accel/tcg/internal.h"

#ifdef CONFIG_LATX_AOT
enum {
    AOT_MODE_LOAD_ALL = 2,
    AOT_LINK_PAGE_INITIAL_CAPACITY = 512,
    AOT_LINK_SEGMENT_INITIAL_CAPACITY = 100000,
};

static aot_link_info *aot_global_info;
static size_t aot_global_info_total;
static size_t aot_global_info_index;

typedef struct AOTPendingLinkKey {
    target_ulong pc;
    uint32_t flags;
    uint32_t cflags;
    CPUState *cpu;
} AOTPendingLinkKey;

typedef struct AOTPendingLink {
    TranslationBlock *source;
    unsigned int exit_index;
} AOTPendingLink;

/* Runtime accesses hold mmap_lock; flush clears it before TB recycling. */
static GHashTable *aot_pending_links;

static guint aot_pending_link_hash(gconstpointer opaque)
{
    const AOTPendingLinkKey *key = opaque;
    uint64_t hash = key->pc ^ ((uint64_t)(uintptr_t)key->cpu >> 4) ^
                    ((uint64_t)key->flags << 32) ^ key->cflags;

    hash ^= hash >> 33;
    hash *= UINT64_C(0xff51afd7ed558ccd);
    hash ^= hash >> 33;
    return hash ^ (hash >> 32);
}

static gboolean aot_pending_link_equal(gconstpointer aptr, gconstpointer bptr)
{
    const AOTPendingLinkKey *a = aptr;
    const AOTPendingLinkKey *b = bptr;

    return a->pc == b->pc && a->flags == b->flags &&
           a->cflags == b->cflags && a->cpu == b->cpu;
}

static void aot_pending_queue_free(gpointer opaque)
{
    g_queue_free_full(opaque, g_free);
}

void aot_link_pending_reset(void)
{
    if (aot_pending_links) {
        g_hash_table_remove_all(aot_pending_links);
    }
    aot_global_info_index = 0;
}

void aot_link_direct_target(CPUState *cpu, TranslationBlock *curr,
    target_ulong pc, uint32_t flags, uint32_t cflags, int exit_index)
{
    AOTPendingLinkKey key = { pc, flags, cflags, cpu };
    TranslationBlock *next;
    GQueue *queue;

    tcg_debug_assert(have_mmap_lock());
    if (exit_index < 0 || exit_index > 1 ||
        (tb_cflags(curr) & CF_INVALID) || use_tu_jmp(curr) ||
        curr->jmp_target_arg[exit_index] == TB_JMP_RESET_OFFSET_INVALID ||
        curr->jmp_reset_offset[exit_index] == TB_JMP_RESET_OFFSET_INVALID) {
        return;
    }
    next = tb_htable_lookup(cpu, pc, 0, flags, cflags);
    if (next) {
        tb_add_jump_aot(curr, exit_index, next);
        return;
    }
    tb_reset_jump(curr, exit_index);
    if (!aot_pending_links) {
        return;
    }
    queue = g_hash_table_lookup(aot_pending_links, &key);
    if (!queue) {
        AOTPendingLinkKey *stored = g_new(AOTPendingLinkKey, 1);
        *stored = key;
        queue = g_queue_new();
        g_hash_table_insert(aot_pending_links, stored, queue);
    }
    AOTPendingLink *link = g_new(AOTPendingLink, 1);
    link->source = curr;
    link->exit_index = exit_index;
    g_queue_push_tail(queue, link);
}

void aot_resolve_pending_links(CPUState *cpu, TranslationBlock *target)
{
    AOTPendingLinkKey key;
    GQueue *queue;

    if (!aot_cross_page_tu_enabled() || !aot_pending_links || !target ||
        (tb_cflags(target) & CF_INVALID)) {
        return;
    }
    tcg_debug_assert(have_mmap_lock());
    key = (AOTPendingLinkKey) {
        target->pc, target->flags, tb_cflags(target), cpu
    };
    queue = g_hash_table_lookup(aot_pending_links, &key);
    if (!queue) {
        return;
    }
    for (GList *it = queue->head; it; it = it->next) {
        AOTPendingLink *link = it->data;
        if (!(tb_cflags(link->source) & CF_INVALID)) {
            tb_add_jump_aot(link->source, link->exit_index, target);
        }
    }
    g_hash_table_remove(aot_pending_links, &key);
}

bool aot_link_tree_reserve(size_t extra)
{
    size_t needed;
    aot_link_info *info;

    if (extra > SIZE_MAX / sizeof(*info) - aot_global_info_index) {
        return false;
    }
    needed = aot_global_info_index + extra;
    if (needed <= aot_global_info_total) {
        return true;
    }
    info = g_try_renew(aot_link_info, aot_global_info, needed);
    if (!info) {
        return false;
    }
    aot_global_info = info;
    aot_global_info_total = needed;
    return true;
}

#if defined(CONFIG_LATX_JRRA) || defined(CONFIG_LATX_JRRA_STACK)
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
#endif

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
    for (size_t i = 0; i < aot_global_info_index; i++) {
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
    if (option_aot == AOT_MODE_LOAD_ALL) {
        aot_global_info_total = AOT_LINK_SEGMENT_INITIAL_CAPACITY;
    } else {
        aot_global_info_total = AOT_LINK_PAGE_INITIAL_CAPACITY;
    }
    aot_global_info = g_new(aot_link_info, aot_global_info_total);
    aot_global_info_index = 0;
    if (aot_cross_page_tu_enabled()) {
        aot_pending_links = g_hash_table_new_full(aot_pending_link_hash,
                aot_pending_link_equal, g_free, aot_pending_queue_free);
    }
}

void aot_link_tree_insert(TranslationBlock *curr,
        target_ulong aim1_pc, target_ulong aim2_pc)
{
    /* Cross-page recovery registers direct exits after publishing the TU. */
    tcg_debug_assert(!aot_cross_page_tu_enabled() || (!aim1_pc && !aim2_pc));
    if (aot_global_info_index >= aot_global_info_total) {
        size_t new_total;
        aot_link_info *new_info;

        if (aot_global_info_total >
            SIZE_MAX / 2 / sizeof(*aot_global_info)) {
            warn_report_once("AOT link table capacity overflow; "
                             "skipping additional AOT links");
            return;
        }
        new_total = aot_global_info_total << 1;
        new_info = g_try_renew(aot_link_info, aot_global_info, new_total);
        if (!new_info) {
            warn_report_once("Could not grow AOT link table; "
                             "skipping additional AOT links");
            return;
        }
        aot_global_info = new_info;
        aot_global_info_total = new_total;
    }
    aot_link_info *info = aot_global_info + aot_global_info_index;
    info->curr = curr;
    info->aim1_pc = aim1_pc;
    info->aim2_pc = aim2_pc;
    aot_global_info_index++;
}
#endif
