#include "qemu/osdep.h"

#include "aot.h"
#include "aot_lib.h"
#include "segment.h"
#include "latx-options.h"

int qemu_loglevel;
int option_aot_pe_profile;
int option_aot;
int option_fast_jmpcache_hash;
int option_tu = 1;
int option_load_aot = 1;
const char *aot_process_profile = "browser";

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

static void check_cache_name_ignores_options(seg_info *seg)
{
    char plain[PATH_MAX];
    char configured[PATH_MAX];

    option_tu = 1;
    get_cache_name(seg, plain);
    option_tu = 0;
    get_cache_name(seg, configured);
    g_assert(strcmp(configured, plain) == 0);
    option_tu = 1;
    g_assert(segment_get_aot_file_name(seg, configured, strlen(plain)) ==
             -ENAMETOOLONG);
}

static void check_hash_policy(seg_info *seg)
{
    char names[2][PATH_MAX], configured[PATH_MAX];

    for (int hash = 0; hash <= 1; hash++) {
        option_fast_jmpcache_hash = hash;
        get_cache_name(seg, names[hash]);
        option_load_aot = 0;
        get_cache_name(seg, configured);
        g_assert(strcmp(configured, names[hash]) == 0);
        option_load_aot = 1;
    }
    g_assert(strcmp(names[0], names[1]) != 0);
    option_fast_jmpcache_hash = 0;
}

static void check_cross_page_policy(seg_info *seg)
{
    char page[PATH_MAX], configured[PATH_MAX], cross[PATH_MAX];

    option_tu = 1;
    option_aot = 1;
    get_cache_name(seg, page);
    for (int mode = 0; mode <= 3; mode++) {
        option_aot = mode;
        g_assert(aot_cross_page_tu_enabled() == (mode == 3));
        g_assert(aot_tu_loading_enabled() == (mode == 3));
        get_cache_name(seg, configured);
        g_assert(strcmp(configured, page) == 0);
    }
    option_aot = 3;
    option_load_aot = 0;
    g_assert(aot_cross_page_tu_enabled());
    g_assert(!aot_tu_loading_enabled());
    get_cache_name(seg, cross);
    g_assert(strcmp(cross, page) == 0);
    option_tu = 0;
    g_assert(!aot_cross_page_tu_enabled());
    g_assert(!aot_tu_loading_enabled());
    option_load_aot = 1;
    option_tu = 1;
    option_aot = 1;
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
    g_assert(g_str_has_prefix(elf_cache_name, "v3-01-100-"));
    g_assert(g_str_has_prefix(first_name, "v3-02-100-"));
    g_assert(g_str_has_suffix(first_name, "-140000000"));
    g_assert(strcmp(first_name, colliding_cache_name) != 0);

    check_cache_name_ignores_options(&elf_seg);
    check_cache_name_ignores_options(&pe_seg);
    check_hash_policy(&elf_seg);
    check_hash_policy(&pe_seg);
    check_cross_page_policy(&elf_seg);
    check_cross_page_policy(&pe_seg);

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
