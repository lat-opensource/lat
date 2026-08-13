/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#include "wrappertbbridge.h"

static GTree *tree;
static GMutex tree_lock;
static gint pc_cmp(gconstpointer ap, gconstpointer bp, gpointer user_data)
{
    const struct kzt_tbbridge *a = ap;
    const struct kzt_tbbridge *b = bp;

    (void)user_data;

    if (a->pc > b->pc) {
        return 1;
    } else if (a->pc < b->pc){
        return -1;
    }

    return 0;
}
void* kzt_tbbridge_init(void)
{
    g_mutex_lock(&tree_lock);
    if (!tree) {
        tree = g_tree_new_full(pc_cmp, NULL, free, NULL);
    }
    g_mutex_unlock(&tree_lock);
    lsassert(tree);
    return tree;
}
struct kzt_tbbridge* kzt_tbbridge_lookup(target_ulong pc)
{
    lsassert(tree&&pc);
    struct kzt_tbbridge key = {.pc = pc};
    g_mutex_lock(&tree_lock);
    struct kzt_tbbridge *bridge =
        (struct kzt_tbbridge *)g_tree_lookup(tree, &key);
    g_mutex_unlock(&tree_lock);
    return bridge;
}

int kzt_tbbridge_insert(target_ulong pc, ADDR func, void * wrapper)
{
    lsassert(tree&&pc&&wrapper);
    struct kzt_tbbridge key = {.pc = pc};

    g_mutex_lock(&tree_lock);
    struct kzt_tbbridge *bridge =
        (struct kzt_tbbridge *)g_tree_lookup(tree, &key);
    if (bridge) {
        int ret = (bridge->func == func && bridge->wrapper == wrapper) ? 1 : -1;
        g_mutex_unlock(&tree_lock);
        return ret;
    }
    struct kzt_tbbridge* new_tbbridge = malloc(sizeof(struct kzt_tbbridge));
    lsassert(new_tbbridge);
    new_tbbridge->pc = pc;
    new_tbbridge->func = func;
    new_tbbridge->wrapper = wrapper;
    g_tree_insert(tree, new_tbbridge, new_tbbridge);
    g_mutex_unlock(&tree_lock);
    return 0;
}
