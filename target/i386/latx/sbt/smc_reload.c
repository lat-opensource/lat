#include "qemu/osdep.h"

#include "latx-options.h"
#include "smc_reload.h"

static GTree *smc_reload_tree;

static gint compare_page_addr(gconstpointer a, gconstpointer b,
                              gpointer user_data G_GNUC_UNUSED)
{
    const target_ulong *addr_a = a;
    const target_ulong *addr_b = b;

    if (*addr_a < *addr_b) {
        return -1;
    }
    if (*addr_a > *addr_b) {
        return 1;
    }
    return 0;
}

static void free_reload_info(gpointer data)
{
    SMCReloadInfo *info = data;

    if (info) {
        g_free(info->tb_vector);
        g_free(info->ir1_code_buffer);
        g_free(info);
    }
}

void smc_reload_tree_init(void)
{
    if (smc_reload_tree) {
        smc_reload_tree_clear();
    }
    smc_reload_tree = g_tree_new_full(compare_page_addr, NULL, NULL,
                                      free_reload_info);
}

void smc_reload_tree_insert(SMCReloadInfo *reload_node)
{
    if (smc_reload_tree == NULL) {
        smc_reload_tree_init();
    }

    if (reload_node == NULL) {
        return;
    }

    g_tree_insert(smc_reload_tree, &reload_node->page_addr, reload_node);
}

SMCReloadInfo *smc_reload_tree_lookup(target_ulong page_addr)
{
    if (smc_reload_tree == NULL) {
        return NULL;
    }

    return g_tree_lookup(smc_reload_tree, &page_addr);
}

void smc_reload_tree_remove(SMCReloadInfo *reload_node)
{
    if (smc_reload_tree == NULL || reload_node == NULL) {
        return;
    }

    g_tree_remove(smc_reload_tree, &reload_node->page_addr);
}

unsigned int smc_reload_tree_get_node_count(void)
{
    if (smc_reload_tree == NULL) {
        return 0;
    }

    return g_tree_nnodes(smc_reload_tree);
}

static gboolean smc_reload_tree_print(gpointer key G_GNUC_UNUSED,
                                      gpointer value,
                                      gpointer data G_GNUC_UNUSED)
{
    const SMCReloadInfo *info = value;

    g_printerr("Page addr: 0x%" PRIx64 ", TB num: %u\n",
               (uint64_t)info->page_addr, info->tb_count);
    return false;
}

void smc_reload_tree_foreach(void)
{
    if (smc_reload_tree == NULL) {
        return;
    }

    g_tree_foreach(smc_reload_tree, smc_reload_tree_print, NULL);
}

void smc_reload_tree_clear(void)
{
    if (smc_reload_tree) {
        g_tree_destroy(smc_reload_tree);
        smc_reload_tree = NULL;
    }
}
