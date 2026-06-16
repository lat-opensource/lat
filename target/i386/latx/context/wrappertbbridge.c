/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#include "wrappertbbridge.h"

static GTree * tree;
static gint pc_cmp(gconstpointer ap, gconstpointer bp)
{
    const struct kzt_tbbridge *a = ap;
    const struct kzt_tbbridge *b = bp;

    if (a->pc > b->pc) {
        return 1;
    } else if (a->pc < b->pc){
        return -1;
    }

    return 0;
}
void* kzt_tbbridge_init(void)
{
    tree = g_tree_new(pc_cmp);
    lsassert(tree);
    return tree;
}
struct kzt_tbbridge* kzt_tbbridge_lookup(target_ulong pc)
{
    lsassert(tree&&pc);
    struct kzt_tbbridge key = {.pc = pc};
    return (struct kzt_tbbridge *)g_tree_lookup(tree, &key);
}

int kzt_tbbridge_insert(target_ulong pc, ADDR func, void * wrapper)
{
    lsassert(tree&&pc&&wrapper);
    if (kzt_tbbridge_lookup(pc)) {
        return 1;
    }
    struct kzt_tbbridge* new_tbbridge = malloc(sizeof(struct kzt_tbbridge));
    new_tbbridge->pc = pc;
    new_tbbridge->func = func;
    new_tbbridge->wrapper = wrapper;
    g_tree_insert(tree, new_tbbridge, new_tbbridge);
    return 0;
}
