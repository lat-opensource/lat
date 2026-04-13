#include "smc_reload.h"
#include "latx-options.h"
#include <glib.h>

static GTree *reload_tree = NULL;

static gint compare_page_addr(gconstpointer a, gconstpointer b, gpointer user_data)
{
    target_ulong addr_a = *((target_ulong *)a);
    target_ulong addr_b = *((target_ulong *)b);

    if (addr_a < addr_b) return -1;
    if (addr_a > addr_b) return 1;
    return 0;
}

static void free_reload_info(gpointer data)
{
    reload_info *info = (reload_info *)data;
    if (info) {
        if (info->tb_vector) {
            g_free(info->tb_vector);
        }
        if (info->ir1_code_buffer) {
            g_free(info->ir1_code_buffer);
        }
        g_free(info); }
}

void reload_tree_init(void)
{
    if (reload_tree != NULL) {
        reload_tree_clear();
    }
    reload_tree = g_tree_new_full(compare_page_addr, NULL, NULL, free_reload_info);
}

void reload_tree_insert(reload_info *reload_node)
{
    if (reload_tree == NULL) {
        reload_tree_init();
    }

    if (reload_node == NULL) {
        return;
    }

    g_tree_insert(reload_tree, &reload_node->page_addr, reload_node);
}

reload_info *reload_tree_lookup(target_ulong page_addr)
{
    if (reload_tree == NULL) {
        return NULL;
    }

    return (reload_info *)g_tree_lookup(reload_tree, &page_addr);
}

void reload_tree_remove(reload_info *reload_node)
{
    if (reload_tree == NULL || reload_node == NULL) {
        return;
    }

    g_tree_remove(reload_tree, &reload_node->page_addr);
}

gint get_reload_node_num(void)
{
    if (reload_tree == NULL) {
        return 0;
    }

    return g_tree_nnodes(reload_tree);
}

static gboolean foreach_print_callback(gpointer key, gpointer value, gpointer data)
{
    reload_info *info = (reload_info *)value;
    fprintf(stderr, "Page addr: 0x%lx, TB num: %d\n", (uint64_t)info->page_addr, info->tb_num);
    return false;
}

void reload_tree_foreach(void)
{
    if (reload_tree == NULL) {
        return;
    }

    g_tree_foreach(reload_tree, foreach_print_callback, NULL);
}

void reload_tree_clear(void)
{
    if (reload_tree != NULL) {
        g_tree_destroy(reload_tree);
        reload_tree = NULL;
    }
}
