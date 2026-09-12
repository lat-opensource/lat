/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu-def.h"

#ifdef AOT_MERGE_TEST_NO_TU
#undef CONFIG_LATX_TU
#endif

/* Include the implementation to cover the private merge-tree lifecycle too. */
#include "../aot_merge.c"

__thread CPUState *thread_cpu;
int qemu_loglevel;
int option_debug_aot;
time_t aot_st_ctime;
aot_file_info *aot_buffer_all;
int aot_buffer_all_num;
seg_info **seg_info_vector;
static char path_buffer[PATH_MAX];
char *aot_file_path = path_buffer;
static char lock_buffer[PATH_MAX];
char *aot_file_lock = lock_buffer;

static char *test_dir;
static bool track_allocations;
static size_t live_bytes;
static size_t allocation_count;
static unsigned int translate_count;
static unsigned int generate_count;
static bool generate_success;
static bool fail_opendir;
static struct {
    void *ptr;
    size_t size;
} allocations[128];

void *__real_malloc(size_t size);
void __real_free(void *ptr);
void *__wrap_malloc(size_t size);
void __wrap_free(void *ptr);
int __wrap_get_aot_path(const char *name, char *path, size_t size);
DIR *__real_opendir(const char *name);
DIR *__wrap_opendir(const char *name);

void *__wrap_malloc(size_t size)
{
    void *ptr = __real_malloc(size);

    if (track_allocations && ptr) {
        size_t i;

        for (i = 0; i < G_N_ELEMENTS(allocations); i++) {
            if (!allocations[i].ptr) {
                allocations[i].ptr = ptr;
                allocations[i].size = size;
                live_bytes += size;
                allocation_count++;
                break;
            }
        }
        g_assert_cmpuint(i, <, G_N_ELEMENTS(allocations));
    }
    return ptr;
}

void __wrap_free(void *ptr)
{
    if (ptr) {
        for (size_t i = 0; i < G_N_ELEMENTS(allocations); i++) {
            if (allocations[i].ptr == ptr) {
                live_bytes -= allocations[i].size;
                allocations[i].ptr = NULL;
                break;
            }
        }
    }
    __real_free(ptr);
}

int __wrap_get_aot_path(const char *name, char *path, size_t size)
{
    int len = snprintf(path, size, "%s/%s.aot", test_dir, name);

    return len < 0 || (size_t)len >= size ? -ENAMETOOLONG : 0;
}

DIR *__wrap_opendir(const char *name)
{
    if (fail_opendir) {
        errno = ENOENT;
        return NULL;
    }
    return __real_opendir(name);
}

int aot_get_file_name(char *file G_GNUC_UNUSED, char *buf G_GNUC_UNUSED,
                       int index G_GNUC_UNUSED)
{
    g_assert_not_reached();
}

int qemu_log(const char *fmt G_GNUC_UNUSED, ...)
{
    return 0;
}

void print_stack_trace(void)
{
}

void pstrcpy(char *buf, int size, const char *str)
{
    g_strlcpy(buf, str, size);
}

uint8_t get_file_type(const char *name G_GNUC_UNUSED)
{
    return ELF_AOT_FILE;
}

void pre_translate(int begin, int end, CPUState *cpu G_GNUC_UNUSED,
                   tb_tmp_message *messages)
{
    g_assert_cmpint(begin, ==, 0);
    g_assert_cmpint(end, ==, 1);
    g_assert(messages != NULL);
    g_assert(aot_buffer_all != NULL);
    translate_count++;
}

int do_generate_aot(int begin, int end)
{
    g_assert_cmpint(begin, ==, 0);
    g_assert_cmpint(end, ==, 1);
    g_assert(aot_buffer_all != NULL);
    generate_count++;
    return generate_success;
}

static void write_cache(const char *path, const char *library,
                         uint32_t tb_offset, bool footer)
{
    size_t size = MiB;
    g_autofree uint8_t *contents = g_malloc0(size);
    aot_header *header = (aot_header *)contents;
    aot_segment *segment = (aot_segment *)(header + 1);
    aot_tb *tb = (aot_tb *)(segment + 1);
    uint32_t *code = (uint32_t *)(tb + 1);
    char *name = (char *)(code + 1);

    header->aot_file_type = ELF_AOT_FILE;
    header->segment_table_offset = (uint8_t *)segment - contents;
    header->segments_num = 1;
    segment->details.seg_begin = 0x10000;
    segment->details.seg_end = 0x11000;
    segment->details.file_offset = 0;
    segment->segment_tbs_offset = (uint8_t *)tb - contents;
    segment->segment_tbs_num = 1;
    segment->lib_name_offset = (uint8_t *)name - contents;
    tb->offset_in_segment = tb_offset;
    tb->tb_cache_offset = (uint8_t *)code - contents;
    tb->tb_cache_size = sizeof(*code);
    tb->tu_size = sizeof(*code);
    tb->rel_start_index = -1;
    tb->rel_end_index = -1;
    *code = 0x03400000; /* LoongArch nop, copied but never executed. */
    strcpy(name, library);
    if (footer) {
        memcpy(contents + size - strlen(AOT_VERSION), AOT_VERSION,
               strlen(AOT_VERSION));
    }
    g_assert(g_file_set_contents(path, (char *)contents, size, NULL));
}

static void assert_released(void)
{
    g_test_message("live merge allocation bytes: %zu", live_bytes);
    g_assert_cmpuint(live_bytes, ==, 0);
    g_assert(aot_buffer_all == NULL);
    g_assert_cmpint(aot_buffer_all_num, ==, 0);
}

static void test_merge_lifetime(void)
{
    g_autofree char *library = g_build_filename(test_dir, "library", NULL);
    char base_path[PATH_MAX];
    char tmp_path[PATH_MAX];
    char name[] = "merge";
    CPUState cpu = { 0 };
    TaskState task = { 0 };
    seg_info seg = {
        .seg_begin = 0x10000,
        .seg_end = 0x11000,
        .first_tb_id = -1,
        .last_tb_id = -2,
    };
    seg_info *segments[] = { &seg };

    cpu.opaque = &task;
    thread_cpu = &cpu;
    seg_info_vector = segments;
    seg.file_name = library;
    g_assert(g_file_set_contents(library, "library", -1, NULL));
    g_assert_cmpint(__wrap_get_aot_path(name, base_path, sizeof(base_path)),
                    ==, 0);
    g_assert_cmpint(aot_file_get_tmp_path(base_path, tmp_path,
                                         sizeof(tmp_path)), ==, 0);
    merge_segment_tree_init();

    /* Every iteration reads both real artifacts, then traverses the merger. */
    for (int i = 0; i < 16; i++) {
        AOTMergeResult result;

        write_cache(base_path, library, 0, true);
        write_cache(tmp_path, library, 4, true);
        translate_count = generate_count = 0;
        generate_success = (i % 2) == 0;
        track_allocations = true;
        result = aot2_merge(name, 0, 1, &cpu);
        track_allocations = false;
#ifdef CONFIG_LATX_TU
        g_assert_cmpint(result, ==, generate_success ? AOT_MERGE_READY :
                                                       AOT_MERGE_ERROR);
        g_assert_cmpuint(translate_count, ==, 1);
        g_assert_cmpuint(generate_count, ==, 1);
#else
        g_assert_cmpint(result, ==, AOT_MERGE_READY);
#endif
        g_assert_cmpuint(allocation_count, >=, 2);
        assert_released();
    }

#ifdef CONFIG_LATX_TU
    write_cache(base_path, library, 0, true);
    write_cache(tmp_path, library, 0, true);
    translate_count = generate_count = 0;
    track_allocations = true;
    AOTMergeResult result = aot2_merge(name, 0, 1, &cpu);
    track_allocations = false;
    g_assert_cmpint(result, ==, AOT_MERGE_DUPLICATE);
    g_assert_cmpuint(translate_count, ==, 0);
    g_assert_cmpuint(generate_count, ==, 0);
    assert_released();
#endif

    /* A successful first read followed by an invalid second file. */
    write_cache(base_path, library, 0, true);
    write_cache(tmp_path, library, 0, false);
    track_allocations = true;
    AOTMergeResult invalid_result = aot2_merge(name, 0, 1, &cpu);
    track_allocations = false;
    g_assert_cmpint(invalid_result, ==, AOT_MERGE_ERROR);
    assert_released();

    g_tree_destroy(merge_segment_tree);
    merge_segment_tree = NULL;
    g_assert_cmpint(g_remove(base_path), ==, 0);
    g_assert_cmpint(g_remove(tmp_path), ==, 0);
    g_assert_cmpint(g_remove(library), ==, 0);
    thread_cpu = NULL;
    seg_info_vector = NULL;
}

static void test_merge_tree_lifetime(void)
{
    GTree *tree;
    aot_tb first = { 0 };
    aot_tb replacement = { 0 };

    merge_tb_tree_init(&tree);
    track_allocations = true;
    merge_tb_tree_insert(tree, 0, &first, NULL, 0);
    merge_tb_tree_insert(tree, 0, &replacement, NULL, 0);
    track_allocations = false;
    g_assert_cmpuint(g_tree_nnodes(tree), ==, 1);
    g_assert_cmpuint(live_bytes, ==, sizeof(merge_tb_info));
    g_tree_destroy(tree);
    g_assert_cmpuint(live_bytes, ==, 0);
}

static void test_cache_directory_failure(void)
{
    fail_opendir = true;
    for (int i = 0; i < 16; i++) {
        int result;

        track_allocations = true;
        result = aot_file_ctx(12000, 500);
        track_allocations = false;
        g_assert_cmpint(result, ==, -1);
        g_assert_cmpuint(live_bytes, ==, 0);
    }
    fail_opendir = false;
}

int main(int argc, char **argv)
{
    int result;

    g_test_init(&argc, &argv, NULL);
    test_dir = g_dir_make_tmp("latx-aot-merge-memory-XXXXXX", NULL);
    g_assert(test_dir != NULL);
    g_test_add_func("/aot-merge/buffer-lifetime", test_merge_lifetime);
    g_test_add_func("/aot-merge/tree-lifetime", test_merge_tree_lifetime);
    g_test_add_func("/aot-merge/cache-directory-failure",
                    test_cache_directory_failure);
    result = g_test_run();
    g_assert_cmpint(g_rmdir(test_dir), ==, 0);
    g_free(test_dir);
    return result;
}
