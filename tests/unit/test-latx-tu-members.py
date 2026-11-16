#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Check production TU member publication and invalidation with the real TB union.

Only QHT/cache residency and individual TB invalidation are modelled.
This checks the metadata lifetime boundary and ring traversal;
full AOT generation/load and SMC tests cover the LoongArch runtime separately.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


def declaration(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n};', start) + 3] + '\n'


PRELUDE = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CONFIG_LATX 1
#define CONFIG_LATX_TU 1
#define CONFIG_LATX_AOT 1
#define CONFIG_USER_ONLY 1
#define TARGET_PAGE_SIZE 4096
#define TARGET_PAGE_MASK (~(uintptr_t)4095)
#define TARGET_PAGE_ALIGN(x) (((x) + 4095) & TARGET_PAGE_MASK)
#define TU_TB_INDEX_NEXT 0
#define TU_TB_INDEX_TARGET 1
#define TU_TB_MODE_NONE 0
#define TU_TB_MODE_SWITCH_TO_TB 1
#define TU_TB_MODE_BROKEN 2
#define TU_TB_MODE_STATIC 3
#define TB_GEN_CODE 4
#define BAD_TB 5
typedef GUEST_PC_TYPE target_ulong;
typedef unsigned QemuSpin;
typedef struct { uintptr_t start, last; } IntervalTreeNode;
typedef struct TranslationBlock TranslationBlock;
struct tb_tc { void *ptr; size_t size; };
struct tu_unlink { uint32_t ins; uint32_t stub_offset; };
typedef int tu_tb_mode_type;
'''

SUPPORT = r'''
_Static_assert(offsetof(TranslationBlock, s_data) ==
               offsetof(TranslationBlock, tu_next), "reuse the metadata slot");
static bool enabled = true;
static bool in_pre_translate;
static bool option_tu = true;
static unsigned invalidations;
static TranslationBlock *resident[8], *cached[8];
static bool expected_remove;
static TranslationBlock *visited[8];
static bool aot_cross_page_tu_enabled(void) { return enabled; }
static bool is_bad_tb(TranslationBlock *tb) {
    return !tb->icount || tb->s_data->tu_tb_mode == TU_TB_MODE_BROKEN ||
           tb->s_data->tu_tb_mode == BAD_TB;
}
static bool is_tu_tb(TranslationBlock *tb) { return tb->bool_flags & IS_TU_TB; }
static uint32_t tb_cflags(TranslationBlock *tb) { return tb->cflags; }
static void do_tb_phys_invalidate(TranslationBlock *tb, bool remove) {
    assert(remove == expected_remove);
    assert(!(tb->cflags & CF_INVALID));
    assert(invalidations < 8);
    visited[invalidations++] = tb;
    tb->cflags |= CF_INVALID;
    for (unsigned i = 0; i < 8; i++) {
        if (resident[i] == tb) {
            resident[i] = NULL;
        }
        if (cached[i] == tb) {
            cached[i] = NULL;
        }
    }
}
static TranslationBlock *lookup(target_ulong pc) {
    for (unsigned i = 0; i < 8; i++) {
        if (resident[i] && resident[i]->pc == pc) {
            return resident[i];
        }
    }
    return NULL;
}
'''

TEST = r'''
int main(void) {
    TranslationBlock page_tbs[5] = {0};
    struct separated_data page_data[5] = {0};
    TranslationBlock *page_list[] = {
        &page_tbs[0], &page_tbs[1], &page_tbs[2], &page_tbs[3],
        &page_tbs[4]
    };
    TranslationBlock tbs[5] = {0};
    struct separated_data data[5] = {0};
    TranslationBlock reloaded[3] = {0};
    TranslationBlock *first[] = {&tbs[0], &tbs[1], &tbs[2]};
    TranslationBlock *second[] = {&tbs[3], &tbs[4]};
    for (unsigned i = 0; i < 5; i++) {
        page_tbs[i].icount = 1;
        page_tbs[i].size = 16;
        page_tbs[i].s_data = &page_data[i];
        page_data[i].tu_tb_mode = TU_TB_MODE_NONE;
    }
    page_tbs[0].pc = 0x1000;
    page_tbs[1].pc = 0x2000;
    page_tbs[2].pc = 0x2010;
    page_tbs[3].pc = 0x2ff8;
    page_tbs[3].size = 32;
    page_tbs[4].pc = 0x5000;
    page_data[2].tu_tb_mode = TU_TB_MODE_STATIC;
    page_data[3].tu_tb_mode = TU_TB_MODE_STATIC;
    page_data[4].tu_tb_mode = TU_TB_MODE_STATIC;
    page_data[0].next_tb[TU_TB_INDEX_NEXT] = &page_tbs[2];
    page_data[0].next_tb[TU_TB_INDEX_TARGET] = &page_tbs[3];
    page_data[0].next_tb[1] = &page_tbs[4];

    ts_ignore_unbacked_static_successors(page_list, 5);
    /* A dynamic member on the same page preserves the current rule. */
    assert(page_data[0].next_tb[TU_TB_INDEX_NEXT] == &page_tbs[2]);
    /* A spanning static block is unbacked when any page lacks a dynamic TB. */
    assert(!page_data[0].next_tb[TU_TB_INDEX_TARGET]);
    /* A page with only a static block is treated as an unknown successor. */
    assert(!page_data[0].next_tb[1]);

    for (unsigned i = 0; i < 5; i++) {
        tbs[i].pc = 0x1000 + i * 0x2000;
        tbs[i].size = 32;
        tbs[i].bool_flags = IS_TU_TB;
        data[i].tu_id = 0x1234 + i;
        tbs[i].s_data = &data[i];
        resident[i] = cached[i] = &tbs[i];
    }

    /* Offline AOT serialization still owns these distinct metadata objects. */
    in_pre_translate = true;
    tu_link_members(first, 3);
    for (unsigned i = 0; i < 3; i++) {
        assert(tbs[i].s_data == &data[i]);
        assert(tbs[i].s_data->tu_id == 0x1234 + i);
    }
    in_pre_translate = false;
    enabled = false;
    tu_link_members(first, 3);
    assert(tbs[0].s_data == &data[0]);
    enabled = true;

    /* Publication consumes the metadata slot after its final reader. */
    tu_link_members(first, 3);
    tu_link_members(second, 2);
    tu_link_members(NULL, 0);
    assert(tbs[0].tu_next == &tbs[1]);
    assert(tbs[1].tu_next == &tbs[2]);
    assert(tbs[2].tu_next == &tbs[0]);
    for (unsigned i = 0; i < 3; i++) {
        for (unsigned j = 0; j < 3; j++) {
            assert(tb_same_tu(&tbs[i], &tbs[j]));
        }
        assert(!tb_same_tu(&tbs[i], &tbs[3]));
    }
    assert(!tb_same_tu(NULL, &tbs[0]));
    assert(!tb_same_tu(&tbs[0], NULL));
    enabled = false;
    assert(!tb_same_tu(&tbs[0], &tbs[1]));
    enabled = true;

    /* Invalidate from a middle member, independent of the guest CFG. */
    expected_remove = true;
    do_tb_group_invalidate(&tbs[1], true);
    assert(invalidations == 3);
    assert(visited[0] == &tbs[1]);
    assert(visited[1] == &tbs[2]);
    assert(visited[2] == &tbs[0]);
    assert(!(tbs[3].cflags & CF_INVALID));
    assert(!(tbs[4].cflags & CF_INVALID));
    do_tb_group_invalidate(&tbs[2], true);
    assert(invalidations == 3);
    assert(tb_same_tu(&tbs[0], &tbs[2]));
    for (unsigned i = 0; i < 3; i++) {
        assert(resident[i] == NULL && cached[i] == NULL);
        assert(lookup(tbs[i].pc) != &tbs[i]);
        reloaded[i].pc = tbs[i].pc;
        reloaded[i].bool_flags = IS_TU_TB;
        first[i] = &reloaded[i];
    }
    assert(resident[3] == &tbs[3] && cached[3] == &tbs[3]);
    assert(resident[4] == &tbs[4] && cached[4] == &tbs[4]);

    /* The same guest PCs can publish a fresh complete group after eviction. */
    tu_link_members(first, 3);
    for (unsigned i = 0; i < 3; i++) {
        resident[i] = cached[i] = &reloaded[i];
        assert(lookup(tbs[i].pc) == &reloaded[i]);
        assert(!(reloaded[i].cflags & CF_INVALID));
        assert(tb_same_tu(&reloaded[i], &reloaded[(i + 1) % 3]));
        assert(!tb_same_tu(&reloaded[i], &tbs[i]));
    }

    /* A singleton closes on itself and forwards page-removal policy. */
    tu_link_members(second, 1);
    assert(tbs[3].tu_next == &tbs[3]);
    expected_remove = false;
    do_tb_group_invalidate(&tbs[3], false);
    assert(invalidations == 4 && visited[3] == &tbs[3]);
    do_tb_group_invalidate(&tbs[3], false);
    assert(invalidations == 4);

    puts("PASS: TU metadata lifetime, membership and invalidation");
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('repo', type=Path)
    args = parser.parse_args()
    repo = args.repo
    header = (repo / 'include/exec/exec-all.h').read_text()
    tu = (repo / 'target/i386/latx/optimization/tu.c').read_text()
    invalidate = (repo / 'accel/tcg/translate-all.c').read_text()
    ts = (repo / 'target/i386/latx/optimization/ts.c').read_text()
    source = PRELUDE
    source += declaration(header, 'struct separated_data{')
    source += declaration(header, 'struct TranslationBlock {')
    source += SUPPORT
    source += function(ts, 'static bool ts_page_has_dynamic_tb(')
    source += function(ts, 'static void ts_ignore_unbacked_static_successors(')
    source += function(tu, 'void tu_link_members(')
    source += function(invalidate, 'static bool tb_same_tu(')
    source += function(invalidate, 'static void do_tb_group_invalidate(')
    source += TEST
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    with tempfile.TemporaryDirectory(prefix='latx-tu-members-') as tmp:
        path = Path(tmp)
        for width in (32, 64):
            unit = path / f'members-{width}.c'
            binary = path / f'members-{width}'
            unit.write_text(source.replace('GUEST_PC_TYPE', f'uint{width}_t'))
            subprocess.run(compiler + ['-std=gnu11', '-O2', '-g', str(unit),
                                      '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
