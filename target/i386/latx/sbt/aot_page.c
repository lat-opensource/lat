/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file aot_page.c
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT optimization
 */
#include "aot_page.h"
#include "latx-options.h"

#ifdef CONFIG_LATX_AOT
static GTree *page_tree;
static void page_delete(gconstpointer a) 
{
    page_info *oldkey = (page_info *)a;
    lsassert(oldkey);
    free(oldkey);
}

static gint page_cmp(gconstpointer a, gconstpointer b)
{
    page_info *pa = (page_info *)a;
    page_info *pb = (page_info *)b;
    assert(pa && pb);
    if (pa->end && pb->end) {
        if ((pa->start < pb->end)
            && (pa->end > pb->start)) {
                return 0;
        }
        return pa->start > pb->start ? 1 : -1;
    }
    if (pa->end == 0) {
        if (pa->start >= pb->start &&
                pa->start < pb->end) {
            return 0;
        }
    }

    if (pb->end == 0) {
        if (pb->start >= pa->start && 
                pb->start < pa->end) {
            return 0;
        }
    }
    return pa->start > pb->start ? 1 : -1;
}

void page_tree_init(void)
{
    page_tree = g_tree_new_full((GCompareDataFunc)page_cmp,
        NULL, NULL, (GDestroyNotify)page_delete);
    lsassert(page_tree);
}

page_info *page_tree_lookup(target_ulong page) {
    page_info key = {.start = page, .end = 0, .buffer = NULL};
    return (page_info *)g_tree_lookup(page_tree, &key);

}

void page_tree_insert(target_ulong start, target_ulong end,
        void *buffer, aot_segment *p_segment, char *seg_name)
{
    page_info *page = (page_info *)malloc(sizeof(page_info));
    if (page == NULL) {
        qemu_log_mask(LAT_LOG_AOT, "Error! No memory for page_tree_insert alloc!\n");
        _exit(-1);
    }
    page->start = start;
    page->end = end;
    page->buffer = buffer;
    page->p_segment = p_segment;
    page->is_running = false;
    g_tree_replace(page_tree, page, page);
}

void page_tree_remove(target_ulong start, target_ulong end)
{
    page_info key = {.start = start, .end = end, .buffer = NULL};
    page_info *val = g_tree_lookup(page_tree, &key);
    if (val) {
        g_tree_remove(page_tree, val);
    }
}

gint get_page_num(void)
{
    return g_tree_nnodes(page_tree);
}
#endif
