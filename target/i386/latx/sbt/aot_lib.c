/**
 * @file aot_lib.c
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT optimization
 */
#include "aot_lib.h"
#include "latx-options.h"

#ifdef CONFIG_LATX_AOT
static GTree *lib_tree;
/* static GTree *umaplib_tree; */
static void lib_delete(gconstpointer a) 
{
    lib_info *oldkey = (lib_info *)a;
    lsassert(oldkey);
    lsassert(oldkey->name);
    free(oldkey->name);
    free(oldkey->buffer);
    free(oldkey);
}

static gint lib_cmp(gconstpointer a, gconstpointer b)
{
    lib_info *pa = (lib_info *)a;
    lib_info *pb = (lib_info *)b;
    assert(pa && pb && pa != pb);
    return strcmp(pa->name, pb->name);
}

void lib_tree_init(void)
{
    lib_tree = g_tree_new_full((GCompareDataFunc)lib_cmp,
        NULL, NULL, (GDestroyNotify)lib_delete);
    lsassert(lib_tree);
}

typedef struct lib_dump_ctx {
    lib_info **vec;
    int index;
} lib_dump_ctx;

static gboolean dump_lib_tree_node(gpointer key, gpointer val,
                                       gpointer data)
{
    lib_dump_ctx *ctx = (lib_dump_ctx *)data;
    lib_info *lib = (lib_info *)val;
    ctx->vec[ctx->index++] = lib;
    return 0;
}


lib_info *lib_tree_lookup(char *name) {
    lib_info key = {.name = name, .buffer = NULL};
    return (lib_info *)g_tree_lookup(lib_tree, &key);

}

lib_info *lib_tree_insert(char *name, void *buffer)
{
    lib_info *lib = (lib_info *)malloc(sizeof(lib_info));
    if (lib == NULL) {
        qemu_log_mask(LAT_LOG_AOT, "Error! No memory for lib_tree_insert alloc!\n");
        _exit(-1);
    }

    lib->name = (char *)malloc(strlen(name) + 1);
    if (lib->name == NULL) {
        qemu_log_mask(LAT_LOG_AOT, "Error! No memory for lib_tree_insert alloc!\n");
        _exit(-1);
    }
    strncpy(lib->name, name, strlen(name) + 1);

    lib->buffer = buffer;
    lib->is_unmapped = 0;
    /* Now insert this new lib into lib_tree */
    g_tree_replace(lib_tree, lib, lib);
    return lib;
}

gint get_lib_num(void)
{
    return g_tree_nnodes(lib_tree);
}

void do_lib_record(lib_info **lib_info_vector)
{
    lib_dump_ctx ctx = {
        .vec = lib_info_vector,
        .index = 0,
    };
    g_tree_foreach(lib_tree, dump_lib_tree_node, &ctx);
}

#endif

