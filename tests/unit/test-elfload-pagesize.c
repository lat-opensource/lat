#include "qemu/osdep.h"
#include <glib.h>
#include "elf.h"
#include "linux-user/elfload-pagesize.h"

static void test_overlap_falls_back_to_target_pagesize(void)
{
    const ElfLoadMapRange segments[] = {
        { PT_LOAD, 0x24aa400, 0x3c41da0, 0x1000 },
        { PT_LOAD, 0x60ed1a0, 0x8593e60, 0x4000 },
    };

    g_assert_cmphex(elf_load_exec_pagesize(segments, G_N_ELEMENTS(segments), 1,
                                           0, 0x4000, 0x1000),
                    ==, 0x1000);
}

static void test_aligned_segment_keeps_host_pagesize(void)
{
    const ElfLoadMapRange segments[] = {
        { PT_LOAD, 0x20000, 0x1800, 0x4000 },
    };

    g_assert_cmphex(elf_load_exec_pagesize(segments, G_N_ELEMENTS(segments), 0,
                                           0, 0x4000, 0x1000),
                    ==, 0x4000);
}

static void test_small_align_uses_target_pagesize(void)
{
    const ElfLoadMapRange segments[] = {
        { PT_LOAD, 0x20000, 0x1800, 0x1000 },
    };

    g_assert_cmphex(elf_load_exec_pagesize(segments, G_N_ELEMENTS(segments), 0,
                                           0, 0x4000, 0x1000),
                    ==, 0x1000);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/elfload/pagesize/overlap-falls-back-to-target",
                    test_overlap_falls_back_to_target_pagesize);
    g_test_add_func("/elfload/pagesize/aligned-keeps-host",
                    test_aligned_segment_keeps_host_pagesize);
    g_test_add_func("/elfload/pagesize/small-align-uses-target",
                    test_small_align_uses_target_pagesize);

    return g_test_run();
}
