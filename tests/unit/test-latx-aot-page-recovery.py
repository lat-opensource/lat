#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Check legacy AOT page recovery with ordinary, differently flagged TUs.

Compile the production page loader and TU recovery with in-memory records.
The next page's record is valid, so a read past the requested slice is
visible as a spurious code copy without relying on a crash or sanitizer.
"""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

PRELUDE = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CONFIG_LATX_TU
#define MAX_TB_IN_CACHE 8
#define CF_PARALLEL 0x100
#define CODE_GEN_ALIGN 16
#define TARGET_PAGE_SIZE 4096
#define TARGET_PAGE_MASK (~(uintptr_t)(TARGET_PAGE_SIZE - 1))
#define ROUND_UP(a, n) (((a) + (n) - 1) & ~((uintptr_t)(n) - 1))
#define unlikely(x) (x)
#define qatomic_set(p, v) (*(p) = (v))
#define R_CS 1
#define TU_TBM_TABLE_MAGIC 1
#define qemu_icache_linesize 64

enum { PAGE_UNLOAD, PAGE_FLUSH, PAGE_SMC, PAGE_LOADED };
enum { ELF_AOT_FILE = 1, PE_AOT_FILE = 2, CACHE_AOT_FILE = 4,
       CROSS_PAGE_TU_AOT = 128 };
enum { IR1_TYPE_CALL, AOT_TB_RECOVERED };
typedef uintptr_t target_ulong;
typedef intptr_t abi_long;
typedef struct { struct { target_ulong base; } segs[2]; } CPUArchState;
static CPUArchState env;
static struct { CPUArchState *cpu_state; } ls = { &env }, *lsenv = &ls;
static struct { uint32_t tb_cflags; void *code_gen_ptr; } ctx, *tcg_ctx = &ctx;
typedef struct TBMini { uintptr_t unused; } TBMini;
typedef struct aot_tb {
    uint32_t cflags, offset_in_segment, size, tu_id, is_first_tb,
             tb_cache_offset, tu_size, offset_in_tu, last_ir1_type;
    int next_tb_pc_offset, target_tb_pc_offset;
} aot_tb;
typedef struct { uintptr_t page_tbs_offset, page_tbs_offset_parallel;
                 int tb_num_in_page, tb_num_in_page_parallel; } page_table_info;
typedef struct { int aot_file_type; } aot_header;
typedef struct { struct { target_ulong seg_begin, seg_end; } details;
                 uintptr_t page_table_offset, segment_tbs_offset;
                 int aot_file_type, segment_tbs_num; } aot_segment;
typedef struct { target_ulong seg_begin, seg_end; void *buffer;
                 aot_segment *p_segment; } seg_info;
typedef struct { target_ulong pc; uint32_t cflags;
                 struct { void *ptr; int size; } tc; } TranslationBlock;

static _Alignas(64) unsigned char code[512];
static void *aot_buffer;
static int published, copies, state;
static int page_get_page_state(target_ulong page) { return state; }
static void page_set_page_state(target_ulong page, int value) { state = value; }
static bool check_ir1(target_ulong start, aot_tb *records, int n, void *buffer)
{
    return true;
}
static void observe_copy(void *dst, const void *src, size_t size)
{
    copies++;
    memcpy(dst, src, size);
}
static TranslationBlock *creat_tb(aot_tb *t, abi_long start, target_ulong base,
                                  void *code_ptr)
{
    static TranslationBlock tb;
    tb = (TranslationBlock){ start + t->offset_in_segment, t->cflags,
                             { code_ptr, 4 } };
    return &tb;
}
static void aot_tb_register(TranslationBlock *tb, int origin)
{
    assert(origin == AOT_TB_RECOVERED && tb->cflags == tcg_ctx->tb_cflags);
    published++;
}
static void aot_do_tb_reloc(TranslationBlock *tb, aot_tb *t,
                          abi_long start, abi_long end) {}
static void aot_link_tree_insert(TranslationBlock *tb, target_ulong a,
                                 target_ulong b) {}
static bool use_tu_jmp(TranslationBlock *tb) { return false; }
static bool use_indirect_jmp(TranslationBlock *tb) { return false; }
static bool hras_callin_has_cold_exit(TranslationBlock *tb) { return false; }
static void tcg_tb_insert_tu(TranslationBlock **tbs, size_t count)
{
    assert(!"legacy TU without an HRAS cold exit should not be indexed");
}
#ifdef CONFIG_LATX_TBMINI_ENABLE
static void aot_tbmini_set_pointer(uintptr_t at, uint64_t tb, uint64_t tag) {}
#endif
#define memcpy observe_copy
#define recover_tb_range(page, records, n, all, count, start, end) \
    recover_tb_range_page(page, records, n, start, end)
'''

TEST = r'''
int main(void)
{
    enum { BASE = 0x10000 };
    struct Cache { aot_header header; page_table_info pages[2];
                   aot_tb records[3]; uint32_t code[3]; } cache = {
        .header = { ELF_AOT_FILE },
    };
    aot_segment segment = {
        .details = {BASE, BASE + 2 * TARGET_PAGE_SIZE},
        .page_table_offset = (char *)cache.pages - (char *)&cache,
        .segment_tbs_offset = (char *)cache.records - (char *)&cache,
        .aot_file_type = ELF_AOT_FILE,
        .segment_tbs_num = 3,
    };
    seg_info info = {BASE, BASE + 2 * TARGET_PAGE_SIZE, &cache, &segment};
    cache.pages[0].page_tbs_offset_parallel = segment.segment_tbs_offset;
    cache.pages[0].tb_num_in_page_parallel = 2;
    cache.pages[1].page_tbs_offset_parallel = segment.segment_tbs_offset +
                                            2 * sizeof(aot_tb);
    cache.pages[1].tb_num_in_page_parallel = 1;
    for (int i = 0; i < 3; i++) {
        unsigned offset = i < 2 ? 16 * i : TARGET_PAGE_SIZE;
        cache.records[i] = (aot_tb){
            .cflags = CF_PARALLEL | (i == 1),
            .offset_in_segment = offset, .size = 4,
            .is_first_tb = 1, .tu_id = offset, .tu_size = 4,
            .tb_cache_offset = (char *)&cache.code[i] - (char *)&cache,
            .next_tb_pc_offset = -1, .target_tb_pc_offset = -1,
        };
    }
    ctx.code_gen_ptr = code;
    state = PAGE_FLUSH;
    int loaded = load_page(BASE, CF_PARALLEL | 2, &info);
    if (copies || published || ctx.code_gen_ptr != code) {
        fputs("cache miss changed code or published a TB\n", stderr);
        return 1;
    }
    assert(!loaded && state == PAGE_FLUSH);

    /* The next request matches one TU, followed by a nonmatching suffix. */
    assert(load_page(BASE, CF_PARALLEL, &info));
    assert(copies == 1 && published == 1 && state == PAGE_LOADED);
    assert(!load_page(BASE, CF_PARALLEL, &info));
    assert(copies == 1 && published == 1);
    puts("PASS: mismatched AOT page leaves code and page state unchanged");
}
'''


def extract(source, name):
    start = source.rindex('\n', 0, source.index(name)) + 1
    return source[start:source.index('\n}', start) + 2]


def main():
    repo = Path(sys.argv[1])
    source = (repo / 'target/i386/latx/sbt/aot_recover_tb.c').read_text()
    loader = extract(source, 'recover_tb_range_page(') + '\n' + extract(
        source, 'int load_page(target_ulong pc').replace('inline int', 'int', 1)
    with tempfile.TemporaryDirectory(prefix='latx-aot-page-') as tmp:
        test = Path(tmp) / 'test.c'
        binary = Path(tmp) / 'test'
        test.write_text(PRELUDE + loader + TEST)
        cc = shlex.split(os.environ.get('CC', 'cc'))
        for flags in ([], ['-DCONFIG_LATX_TBMINI_ENABLE']):
            subprocess.run(cc + ['-std=gnu11', '-O2'] + flags +
                           [str(test), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
