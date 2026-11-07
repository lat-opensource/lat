/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file aot_link_seg.h
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT header
 */
#ifndef _AOT_LINK_SEG_H_
#define _AOT_LINK_SEG_H_

#include "qemu-def.h"
#include "qemu.h"

typedef struct aot_link_info {
    TranslationBlock *curr;
    target_ulong aim1_pc;
    target_ulong aim2_pc;
} aot_link_info;

void aot_link_tree_init(void);
void aot_link_tree_insert(TranslationBlock *curr,
        target_ulong aim1_pc, target_ulong aim2_pc);
bool aot_link_tree_reserve(size_t extra);
void try_aot_link(void);
void aot_link_direct_target(CPUState *cpu, TranslationBlock *curr,
    target_ulong pc, uint32_t flags, uint32_t cflags, int exit_index);
void aot_resolve_pending_links(CPUState *cpu, TranslationBlock *target);
void aot_link_pending_reset(void);
#endif
