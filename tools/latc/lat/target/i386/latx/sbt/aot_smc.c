/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file aot_smc.c
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT optimization
 */
#include "aot_smc.h"
#include "latx-options.h"

#ifdef CONFIG_LATX_AOT
static GTree *smc_tree;
static gint smc_cmp(gconstpointer a, gconstpointer b)
{
    smc_info *pa = (smc_info *)a;
    smc_info *pb = (smc_info *)b;
    assert(pa && pb);
    if ((pa->begin == pb->begin) || (pa->begin < pb->end && pb->begin < pa->end)) {
        return 0;
    } else if (pa->begin < pb->begin) {
        return -1;
    } else {
        return 1;
    }
}

void smc_tree_init(void)
{
    smc_tree = g_tree_new_full((GCompareDataFunc)smc_cmp,
        NULL, NULL, NULL);
    lsassert(smc_tree);
}

static gboolean dump_smc_tree_node(gpointer key, gpointer val,
                                       gpointer data)
{
    static int index;
    smc_info **vec = (smc_info **)data;
    smc_info *smc = (smc_info *)val;
    vec[index++] = smc;
    return 0;
}

#include "aot_page.h"
void smc_tree_insert(abi_ulong begin, abi_ulong end)
{
    lsassert(end >= begin);
    smc_info *old_info = 
        smc_tree_lookup(begin << TARGET_PAGE_BITS, end << TARGET_PAGE_BITS);

    if (old_info && old_info->begin <= begin && old_info->end >= end) {
        return;
    }

    while (old_info) {
        if (old_info->begin < begin) {
            begin = old_info->begin;
        }
        if (old_info->end > end) {
            end = old_info->end;
        }
        g_tree_remove(smc_tree, old_info);
        free(old_info);
        old_info =
            smc_tree_lookup(begin << TARGET_PAGE_BITS, end << TARGET_PAGE_BITS);
    }

    smc_info *smc = (smc_info *)malloc(sizeof(smc_info));
    smc->begin = begin;
    smc->end = end;
    g_tree_replace(smc_tree, smc, smc);
}

smc_info *smc_tree_lookup(target_ulong begin, target_ulong end)
{
    if (option_aot) {
        smc_info key = {.begin = (begin >> TARGET_PAGE_BITS), 
            .end = (end >> TARGET_PAGE_BITS)};
        return (smc_info *)g_tree_lookup(smc_tree, &key);
    }
    return NULL;
}

gint get_smc_num(void)
{
    return g_tree_nnodes(smc_tree);
}

void do_smc_record(smc_info **smc_info_vector)
{
    g_tree_foreach(smc_tree, dump_smc_tree_node, smc_info_vector);
}
#endif
