/* SPDX-License-Identifier: GPL-2.0-only */
#include "qemu/osdep.h"
#include <sys/wait.h>

/* Use the production collector and comparator with a fixed traversal order. */
#define qht_iter test_qht_iter
#include "../aot.c"
#undef qht_iter

TBContext tb_ctx;
static TranslationBlock *entries;
static size_t entry_count;
static unsigned iterations;
static int fail_after = -1;
static target_ulong smc_pc = ~(target_ulong)0;

void *__real_realloc(void *ptr, size_t size);
void *__wrap_realloc(void *ptr, size_t size);
void *__wrap_realloc(void *ptr, size_t size)
{
    if (fail_after == 0) {
        return NULL;
    }
    if (fail_after > 0) {
        fail_after--;
    }
    return __real_realloc(ptr, size);
}

int page_get_page_state(target_ulong address)
{
    return address == smc_pc ? PAGE_SMC : 0;
}

void test_qht_iter(struct qht *ht, qht_iter_func_t func, void *userp)
{
    size_t i;

    iterations++;
    for (i = 0; i < entry_count; i++) {
        func(&entries[i], i, userp);
    }
}

static void check_snapshot(size_t count, bool filtered)
{
    TranslationBlock **expected;
    size_t i, used = 0;

    entry_count = count;
    entries = g_new0(TranslationBlock, count);
    expected = g_new(TranslationBlock *, count);
    for (i = 0; i < count; i++) {
        entries[i].pc = (count - i) / 4; /* Include equal sort keys. */
        entries[i].icount = 1;
        entries[i].cflags = i & 1 ? CF_PARALLEL : 0;
        if (filtered) {
            switch (i % 7) {
            case 0: entries[i].icount = 0; break;
            case 1: entries[i].bool_flags = IS_AOT_TB; break;
            case 2: entries[i].bool_flags = IS_TUNNEL_LIB; break;
            case 3: entries[i].cflags |= CF_INVALID; break;
            case 4: entries[i].pc = smc_pc; break;
            default: break;
            }
        }
        if (!not_required_tb(&entries[i])) {
            expected[used++] = &entries[i];
        }
    }
    qsort(expected, used, sizeof(*expected), tb_cmp);
    iterations = 0;
    get_tb();
    g_assert_cmpuint(iterations, ==, 1);
    g_assert_cmpint(tb_num, ==, used);
    for (i = 0; i < used; i++) {
        g_assert(tb_vector[i] == expected[i]);
    }
    if (filtered) {
        g_assert_cmpuint(used, ==, (count / 7) * 2 +
                         (count % 7 > 5 ? count % 7 - 5 : 0));
    }
    free(tb_vector);
    tb_vector = NULL;
    g_free(expected);
    g_free(entries);
}

static void check_failure(int allocation)
{
    pid_t pid;
    int status;

    entry_count = 1024;
    entries = g_new0(TranslationBlock, entry_count);
    for (size_t i = 0; i < entry_count; i++) {
        entries[i].icount = 1;
    }
    pid = fork();
    g_assert_cmpint(pid, >=, 0);
    if (!pid) {
        fail_after = allocation;
        get_tb();
        _exit(99);
    }
    g_assert_cmpint(waitpid(pid, &status, 0), ==, pid);
    g_assert(WIFEXITED(status));
    g_assert_cmpint(WEXITSTATUS(status), ==, EXIT_FAILURE);
    g_free(entries);
}

int main(void)
{
    AOTTBCollector collector = { .capacity = INT_MAX };
    TranslationBlock tb = { .icount = 1 };

    check_snapshot(0, false);
    check_snapshot(1, false);
    check_snapshot(5, true); /* All entries filtered. */
    check_snapshot(256, false);
    check_snapshot(257, false);
    check_snapshot(65537, false);
    check_snapshot(65537, true);
    check_failure(0);
    check_failure(1);
    tb_num = INT_MAX;
    do_tb_collect(&tb, 0, &collector);
    g_assert(collector.failed);
    g_assert_cmpint(tb_num, ==, INT_MAX);
    puts("PASS: one walk, filtering, growth, stable input order, failures");
    return 0;
}
