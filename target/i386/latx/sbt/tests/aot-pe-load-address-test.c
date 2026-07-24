#include "qemu/osdep.h"

#include "aot.h"
#include "aot_lib.h"
#include "segment.h"

int qemu_loglevel;

int qemu_log(const char *fmt G_GNUC_UNUSED, ...)
{
    return 0;
}

void print_stack_trace(void)
{
}

static void get_cache_name(seg_info *seg, char *name)
{
    g_assert(segment_get_aot_file_name(seg, name, PATH_MAX) == 0);
}

int main(void)
{
    char elf_name[] = "/tmp/libexample.so";
    char pe_name[] = "/tmp/example.dll";
    char colliding_name[] = "/tmp/example.dll-140000000";
    seg_info elf_seg = {
        .file_name = elf_name,
        .seg_begin = 0x400000,
        .aot_file_type = ELF_AOT_FILE,
    };
    seg_info pe_seg = {
        .file_name = pe_name,
        .seg_begin = 0x140000000,
        .aot_file_type = PE_AOT_FILE,
    };
    seg_info relocated_pe_seg = pe_seg;
    seg_info same_page_pe_seg = pe_seg;
    seg_info colliding_elf_seg = {
        .file_name = colliding_name,
        .seg_begin = 0x400000,
        .aot_file_type = ELF_AOT_FILE,
    };
    char elf_cache_name[PATH_MAX];
    char colliding_cache_name[PATH_MAX];
    char first_name[PATH_MAX];
    char second_name[PATH_MAX];
    size_t map_len = TARGET_PAGE_SIZE;
    void *mapping;

    get_cache_name(&elf_seg, elf_cache_name);
    get_cache_name(&pe_seg, first_name);
    get_cache_name(&colliding_elf_seg, colliding_cache_name);
    g_assert(g_str_has_prefix(elf_cache_name, "v2-01-"));
    g_assert(g_str_has_prefix(first_name, "v2-02-"));
    g_assert(g_str_has_suffix(first_name, "-140000000"));
    g_assert(strcmp(first_name, colliding_cache_name) != 0);

    same_page_pe_seg.seg_begin += 0x80;
    get_cache_name(&same_page_pe_seg, second_name);
    g_assert(g_str_has_suffix(second_name, "-140000080"));
    g_assert(strcmp(first_name, second_name) != 0);

    relocated_pe_seg.seg_begin = 0x180000000;
    get_cache_name(&relocated_pe_seg, second_name);
    g_assert(g_str_has_suffix(second_name, "-180000000"));
    g_assert(strcmp(first_name, second_name) != 0);

    pe_seg.seg_flag = SEG_RUNNING;
    g_assert(pe_seg.seg_flag & SEG_RUNNING);
    g_assert(pe_seg.aot_file_type == PE_AOT_FILE);

    lib_tree_init();
    mapping = mmap(NULL, map_len, PROT_READ,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    g_assert(mapping != MAP_FAILED);
    g_assert(lib_tree_insert(first_name, mapping, map_len) != NULL);
    g_assert(lib_tree_insert(second_name, NULL, 0) != NULL);
    g_assert(get_lib_num() == 2);
    g_assert(lib_tree_lookup(first_name) != lib_tree_lookup(second_name));
    g_assert(lib_tree_remove(first_name));
    g_assert(lib_tree_remove(second_name));
    g_assert(get_lib_num() == 0);

    return 0;
}
