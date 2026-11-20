#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Check production TU interval lookup, publication and allocation bounds.

The protocol run verifies acquire/release at public pointers; it is not a
weak-memory crash reproducer. The concurrent run uses actual QEMU atomics.
Tables are reclaimed only after readers join, as required by region reset.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

PRELUDE = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <glib.h>
#define QEMU_BUILD_BUG_ON(x) _Static_assert(!(x), "atomic size")
#include "qemu/atomic.h"
#define CONFIG_LATX_TU 1
#define CONFIG_LATX_AOT 1
#define CODE_GEN_ALIGN 32
#define TB_MAGIC 0xbeefUL
#define TU_TBM_TABLE_MAGIC 0xbeeeUL
#define HOST_VIRT_ADDR_SPACE_BITS 48
#define MAKE_64BIT_MASK(s, n) (((1ULL << (n)) - 1) << (s))
#define ROUND_DOWN(n, d) ((n) & ~((__typeof__(n))(d) - 1))
#define ROUND_UP(n, d) ROUND_DOWN((n) + (d) - 1, d)
static const size_t qemu_icache_linesize = 64;
typedef struct TBMini {
    union {
        struct { unsigned short pointer[3], magic; } mtbp_struct;
        uint64_t mtbp_uint64;
    };
} TBMini;
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
typedef struct QemuMutex QemuMutex;
#include "qemu/thread-posix.h"
static void check_pthread(int status) { assert(status == 0); }
/* Keep the production mutex initialization checks; omit only tracing hooks. */
#define qemu_mutex_pre_lock(m, f, l) ((void)0)
#define qemu_mutex_post_lock(m, f, l) ((void)0)
#define qemu_mutex_pre_unlock(m, f, l) ((void)0)
MUTEX_IMPLEMENTATION
#define qemu_mutex_lock(p) qemu_mutex_lock_impl(p, __FILE__, __LINE__)
#define qemu_mutex_unlock(p) qemu_mutex_unlock_impl(p, __FILE__, __LINE__)
struct tb_tc { void *ptr; size_t size; };
typedef struct TranslationBlock {
    struct tb_tc tc;
    uint32_t tu_cold_offset, tu_cold_size;
    unsigned serial;
} TranslationBlock;
static struct { void *start, *end; } region;
static uintptr_t tcg_splitwx_diff;
static bool enabled = true;
static bool aot_cross_page_tu_enabled(void) { return enabled; }
static bool in_code_gen_buffer(void *p) {
    return (uintptr_t)p >= (uintptr_t)region.start &&
           (uintptr_t)p < (uintptr_t)region.end;
}
static size_t allocated;
static void *tracked_alloc(size_t bytes) {
    size_t *p = g_malloc0(bytes + 16);
    p[0] = bytes;
    allocated += bytes;
    return (char *)p + 16;
}
static void tracked_free(void *ptr) {
    if (ptr) {
        size_t *p = (void *)((char *)ptr - 16);
        allocated -= p[0];
        g_free(p);
    }
}
#undef g_new0
#define g_new0(type, n) ((type *)tracked_alloc(sizeof(type) * (n)))
#define g_malloc0(n) tracked_alloc(n)
#define g_free(p) tracked_free(p)
#ifdef CHECK_PROTOCOL
static bool checking_read;
static void check_access(const void *ptr, uintptr_t value,
                         int order, bool write);
#undef qatomic_read
#undef qatomic_set
#undef qatomic_load_acquire
#undef qatomic_store_release
#define INDEX_LOAD(ptr, order) ({ \
    __typeof__(*(ptr)) val = __atomic_load_n(ptr, order); \
    check_access(ptr, (uintptr_t)val, order, false); val; })
#define INDEX_STORE(ptr, val, order) do { \
    check_access(ptr, (uintptr_t)(val), order, true); \
    __atomic_store_n(ptr, val, order); \
} while (0)
#define qatomic_read(ptr) INDEX_LOAD(ptr, __ATOMIC_RELAXED)
#define qatomic_set(ptr, val) INDEX_STORE(ptr, val, __ATOMIC_RELAXED)
#define qatomic_load_acquire(ptr) INDEX_LOAD(ptr, __ATOMIC_ACQUIRE)
#define qatomic_store_release(ptr, val) INDEX_STORE(ptr, val, __ATOMIC_RELEASE)
#endif
'''

TEST = r'''
#ifdef CHECK_PROTOCOL
static bool public_slot(const void *ptr) {
    uintptr_t p = (uintptr_t)ptr;
    uintptr_t begin = (uintptr_t)latx_tb_index_chunks;
    if (ptr == &latx_tb_index_chunks ||
        (begin && p >= begin && p - begin <
         latx_tb_index_chunk_count * sizeof(*latx_tb_index_chunks))) {
        return true;
    }
    for (size_t i = 0; latx_tb_index_chunks &&
         i < latx_tb_index_chunk_count; i++) {
        LatxTBIndexChunk *chunk = latx_tb_index_chunks[i];
        begin = (uintptr_t)chunk;
        if (chunk && p >= begin && p - begin < sizeof(*chunk)) {
            return true;
        }
    }
    for (LatxTBIndexGroup *g = latx_tb_index_groups; g; g = g->next) {
        for (size_t i = 0; i < g->count; i++) {
            if (ptr == &g->ranges[i].tb) { return true; }
        }
    }
    return false;
}
static void check_access(const void *ptr, uintptr_t value,
                         int order, bool write) {
    if (!value || (!write && !checking_read) || !public_slot(ptr)) {
        return;
    }
    if (write) {
        assert(order == __ATOMIC_RELEASE || order == __ATOMIC_SEQ_CST);
    } else {
        assert(order == __ATOMIC_ACQUIRE || order == __ATOMIC_SEQ_CST);
    }
}
#endif
static TranslationBlock *lookup(uintptr_t pc) {
#ifdef CHECK_PROTOCOL
    checking_read = true;
#endif
    bool covered;
    TranslationBlock *tb = latx_tb_index_get(pc, &covered);
#ifdef CHECK_PROTOCOL
    checking_read = false;
#endif
    return tb;
}
static size_t directory_bytes(void) {
    return latx_tb_index_chunk_count * sizeof(*latx_tb_index_chunks);
}
static void boundaries(void) {
    uintptr_t base = (uintptr_t)region.start;
    TranslationBlock small[8] = {0}, *members[8];
    for (unsigned i = 0; i < 8; i++) {
        small[i].tc.ptr = (void *)(base + 4 * i);
        small[i].tc.size = 4;
        small[i].tu_cold_offset = 8192;
        small[i].tu_cold_size = 4;
        members[7 - i] = &small[i];
    }
    tcg_tb_insert_tu(members, 8);
    for (unsigned i = 0; i < 8; i++) {
        assert(lookup(base + 4 * i) == &small[i]);
        assert(lookup(base + 4 * i + 3) == &small[i]);
        assert(lookup(base + 8192 + 4 * i) == &small[i]);
        assert(lookup(base + 8192 + 4 * i + 3) == &small[i]);
        assert(tcg_tb_lookup(base + 4 * i) == &small[i]);
        assert(tcg_tb_lookup(base + 8192 + 4 * i + 3) == &small[i]);
    }
    assert(!lookup(base + 32));
    assert(!lookup(base + 8192 + 32));
    /* A code word resembling TB_MAGIC inside a TU hole is not a TB. */
    uintptr_t fake = ROUND_UP(base + 512, CODE_GEN_ALIGN);
    TBMini *fake_header = (void *)fake;
    fake_header->mtbp_uint64 = (uintptr_t)&small[0] | (TB_MAGIC << 48);
#ifndef CONFIG_LATX_TBMINI_ENABLE
    TranslationBlock misleading = { .tc = { (void *)(fake + 8), 8 } };
    tcg_tb_insert(&misleading);
#endif
    assert(!tcg_tb_lookup(fake + 8));
#ifndef CONFIG_LATX_TBMINI_ENABLE
    tcg_tb_remove(&misleading);
    for (unsigned i = 0; i < 8; i++) { tcg_tb_insert(&small[i]); }
    assert(g_tree_nnodes(model_tree.tree) == 8);
#endif
    /* An ordinary TB outside every TU still uses its original TBMini. */
    uintptr_t plain = ROUND_UP(base + (1 << 20), CODE_GEN_ALIGN);
    TranslationBlock ordinary = { .tc = { .ptr = (void *)(plain + 8),
                                          .size = 8 } };
    TBMini *plain_header = (void *)plain;
    plain_header->mtbp_uint64 = (uintptr_t)&ordinary | (TB_MAGIC << 48);
#ifndef CONFIG_LATX_TBMINI_ENABLE
    tcg_tb_insert(&ordinary);
#endif
    assert(tcg_tb_lookup(plain + 8) == &ordinary);
#ifndef CONFIG_LATX_TBMINI_ENABLE
    tcg_tb_remove(&ordinary);
#endif
    assert(!tcg_tb_lookup(base - 1));
    assert(!tcg_tb_lookup((uintptr_t)region.end));
    assert(!lookup(base + 4096));
    bool covered;
    assert(!latx_tb_index_get(base + 4096, &covered) && covered);
    assert(!latx_tb_index_get(base + (1 << 23), &covered) && !covered);
    assert(!lookup(base - 1));
    assert(!lookup((uintptr_t)region.end));
    for (unsigned i = 0; i < 8; i++) {
#ifdef CONFIG_LATX_TBMINI_ENABLE
        latx_tb_index_remove(&small[i]);
        latx_tb_index_remove(&small[i]);
#else
        tcg_tb_remove(&small[i]);
        tcg_tb_remove(&small[i]);
#endif
        assert(!lookup(base + 4 * i));
        assert(!lookup(base + 8192 + 4 * i));
        assert(!tcg_tb_lookup(base + 4 * i));
        assert(!tcg_tb_lookup(base + 8192 + 4 * i));
    }
#ifndef CONFIG_LATX_TBMINI_ENABLE
    assert(g_tree_nnodes(model_tree.tree) == 0);
#endif
    /* A later table at the same code addresses must return new identities. */
    TranslationBlock replacement[8];
    memcpy(replacement, small, sizeof(small));
    for (unsigned i = 0; i < 8; i++) { members[i] = &replacement[i]; }
    tcg_tb_insert_tu(members, 8);
    for (unsigned i = 0; i < 8; i++) {
        assert(lookup(base + 4 * i) == &replacement[i]);
    }
    /* Distinct TUs share a directory bucket; one spans a bucket boundary. */
    TranslationBlock cross = { .tc = {
        .ptr = (void *)(base + (1 << LATX_TB_INDEX_CHUNK_SHIFT) - 4),
        .size = 12,
    }};
    TranslationBlock *one[] = { &cross };
    latx_tb_index_insert_group(one, 1);
    assert(lookup((uintptr_t)cross.tc.ptr + 11) == &cross);
    assert(!lookup((uintptr_t)cross.tc.ptr + 12));
    assert(lookup(base) == &replacement[0]);
    TranslationBlock upper = { .tc = {
        .ptr = (char *)region.end - 4, .size = 4,
    }};
    one[0] = &upper;
    latx_tb_index_insert_group(one, 1);
    assert(lookup((uintptr_t)region.end - 1) == &upper);
    assert(!lookup((uintptr_t)region.end));
    enabled = false;
    assert(!lookup(base));
    size_t before = allocated;
    latx_tb_index_insert_group(one, 1);
    assert(allocated == before);
    enabled = true;
    latx_tb_index_reset();
    assert(!lookup(base));
    assert(!lookup((uintptr_t)cross.tc.ptr));
    assert(allocated == directory_bytes());
    /* Directory keys use RW addresses; callers supply the executable alias. */
    tcg_splitwx_diff = 1 << 26;
    TranslationBlock alias = { .tc = {
        .ptr = (void *)(base + tcg_splitwx_diff + 64), .size = 8,
    }, .tu_cold_offset = 8192, .tu_cold_size = 4 };
    one[0] = &alias;
    latx_tb_index_insert_group(one, 1);
    assert(lookup((uintptr_t)alias.tc.ptr + 7) == &alias);
    assert(lookup((uintptr_t)alias.tc.ptr + 8192) == &alias);
    assert(!lookup((uintptr_t)alias.tc.ptr + 8));
    latx_tb_index_remove(&alias);
    assert(!lookup((uintptr_t)alias.tc.ptr));
    latx_tb_index_reset();
    tcg_splitwx_diff = 0;
}
static void memory_bound(void) {
    enum { N = 1024 };
    TranslationBlock *tbs = calloc(N, sizeof(*tbs));
    TranslationBlock **members = calloc(N, sizeof(*members));
    uintptr_t base = (uintptr_t)region.start;
    size_t before = allocated;
    for (unsigned i = 0; i < N; i++) {
        tbs[i].tc.ptr = (void *)(base + i * 512);
        tbs[i].tc.size = 512;
        tbs[i].tu_cold_offset = N * 512 - i * 508;
        tbs[i].tu_cold_size = 4;
        members[i] = &tbs[i];
    }
    latx_tb_index_insert_group(members, N);
    /* Bound the real requested allocations, not a duplicated size formula. */
    printf("TU index: %zu bytes for 1024 hot/cold members\n",
           allocated - before);
    assert(allocated - before < 64 * 1024);
    for (unsigned i = 0; i < N; i++) {
        assert(lookup(base + i * 512 + 511) == &tbs[i]);
        assert(lookup(base + N * 512 + i * 4 + 3) == &tbs[i]);
    }
    latx_tb_index_reset();
    assert(allocated == directory_bytes());
    free(members);
    free(tbs);
}
#ifndef CHECK_PROTOCOL
#define MEMBERS 2048
static TranslationBlock concurrent[MEMBERS];
static pthread_barrier_t start;
static void *reader(void *unused) {
    pthread_barrier_wait(&start);
    for (unsigned n = 0; n < 100000; n++) {
        unsigned i = (n * 257u) % MEMBERS;
        uintptr_t pc = (uintptr_t)region.start + i * 96;
        TranslationBlock *tb = lookup(pc);
        if (tb) {
            assert(tb == &concurrent[i]);
            assert(tb->serial == i + 1 && tb->tc.size == 8);
        }
    }
    return NULL;
}
static void threaded(void) {
    pthread_t threads[3];
    check_pthread(pthread_barrier_init(&start, NULL, 4));
    for (unsigned i = 0; i < 3; i++) {
        check_pthread(pthread_create(&threads[i], NULL, reader, NULL));
    }
    pthread_barrier_wait(&start);
    for (unsigned i = 0; i < MEMBERS; i += 32) {
        TranslationBlock *members[32];
        for (unsigned j = 0; j < 32; j++) {
            TranslationBlock *tb = &concurrent[i + j];
            tb->tc.ptr = (char *)region.start + (i + j) * 96;
            tb->tc.size = 8;
            tb->serial = i + j + 1;
            members[j] = tb;
        }
        latx_tb_index_insert_group(members, 32);
    }
    for (unsigned i = 0; i < 3; i++) {
        check_pthread(pthread_join(threads[i], NULL));
    }
    for (unsigned i = 0; i < MEMBERS; i++) {
        assert(lookup((uintptr_t)concurrent[i].tc.ptr) == &concurrent[i]);
    }
    pthread_barrier_destroy(&start);
    latx_tb_index_reset();
}
#endif
int main(void) {
    /* Non-cross-page modes never initialize this optional index or its lock. */
    enabled = false;
    latx_tb_index_reset();
    latx_tb_index_reset();
    assert(!allocated && !latx_tb_index_lock.initialized);
    enabled = true;
    /* The final directory chunk is only partially backed by code memory. */
    region.start = calloc(1, (1 << 24) + 16);
    region.end = (char *)region.start + (1 << 24) + 16;
    latx_tb_index_init();
    assert(!allocated);
    /* An enabled index can also be flushed before its first TU publication. */
    latx_tb_index_reset();
    assert(!allocated && latx_tb_index_lock.initialized);
#ifndef CONFIG_LATX_TBMINI_ENABLE
    qemu_mutex_init(&model_tree.lock);
    model_tree.tree = g_tree_new(tb_tc_cmp);
#endif
    boundaries();
#ifndef CONFIG_LATX_TBMINI_ENABLE
    g_tree_destroy(model_tree.tree);
    qemu_mutex_destroy(&model_tree.lock);
#endif
    memory_bound();
#ifndef CHECK_PROTOCOL
    threaded();
#endif
    g_free(latx_tb_index_chunks);
    free(region.start);
    assert(!allocated);
    qemu_mutex_destroy(&latx_tb_index_lock);
    puts("PASS: TU index intervals, publication, memory and reset");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('repo', type=Path)
    parser.add_argument('backend', choices=('mini', 'tree'))
    parser.add_argument('mode', choices=('protocol', 'threaded'))
    args = parser.parse_args()
    repo = args.repo.resolve()
    text = (repo / 'tcg/tcg.c').read_text()
    first = text.index('#define LATX_TB_INDEX_CHUNK_SHIFT')
    end_marker = '\n#ifdef CONFIG_LATX_TU\nvoid tcg_tb_insert_tu'
    last = text.index(end_marker, first)
    query_marker = '#ifdef CONFIG_LATX_TU\nstatic void *query_tu_tbm_table'
    fallback = text.index(query_marker, last)
    fallback_end = text.index('/* Ordinary TBs retain', fallback)
    def function(source, signature):
        start = source.index(signature)
        return source[start:source.index('\n}', start) + 2] + '\n'

    mutex = (repo / 'util/qemu-thread-posix.c').read_text()
    common = (repo / 'util/qemu-thread-common.h').read_text()
    mutex_source = function(mutex, 'static void error_exit(')
    mutex_source += function(common, 'static inline void qemu_mutex_post_init(')
    for signature in ('void qemu_mutex_init(', 'void qemu_mutex_destroy(',
                      'void qemu_mutex_lock_impl(',
                      'void qemu_mutex_unlock_impl('):
        mutex_source += function(mutex, signature)
    tree_prelude = r'''
#define likely(x) (x)
struct tcg_region_tree { QemuMutex lock; GTree *tree; };
static struct tcg_region_tree model_tree;
static struct tcg_region_tree *tc_ptr_to_region_tree(const void *p) {
    return in_code_gen_buffer((void *)p) ? &model_tree : NULL;
}
'''
    index_source = text[first:last]
    if index_source.endswith("\n#endif\n"):
        index_source = index_source[:-len("\n#endif\n")]
    tree_source = tree_prelude
    for signature in ('static int ptr_cmp_tb_tc(', 'static gint tb_tc_cmp(',
                      'void tcg_tb_insert(', 'void tcg_tb_remove(',
                      'TranslationBlock *tcg_tb_lookup('):
        tree_source += function(text, signature)
    source = (PRELUDE.replace('MUTEX_IMPLEMENTATION', mutex_source) +
              index_source + '\n' +
              function(text, 'void tcg_tb_insert_tu(') +
              '#ifdef CONFIG_LATX_TBMINI_ENABLE\n' +
              text[fallback:fallback_end] + '\n#else\n' + tree_source +
              '\n#endif\n' + TEST)
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(subprocess.check_output(
        ['pkg-config', '--cflags', '--libs', 'glib-2.0'], text=True))
    with tempfile.TemporaryDirectory(prefix='latx-tb-index-') as tmp:
        path = Path(tmp)
        unit = path / 'index.c'
        unit.write_text(source)
        binary = path / args.backend
        command = cc + ['-std=gnu11', '-O2', '-g', '-UNDEBUG', '-pthread',
                        '-I', str(repo / 'include')]
        if args.backend == 'mini':
            command += ['-DCONFIG_LATX_TBMINI_ENABLE']
        if args.mode == 'protocol':
            command += ['-DCHECK_PROTOCOL']
        subprocess.run(command + [str(unit), '-o', str(binary)] + flags,
                       check=True)
        subprocess.run([str(binary)], check=True, timeout=30)


if __name__ == '__main__':
    main()
