#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Exercise production TU restore preparation and its two metadata consumers.

Decode/IR optimization are stubbed to write their normal metadata outputs.
The production caller must pass a private clone to both recovery consumers.
The full signal-fault fixture checks generated guest execution separately.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

PRELUDE = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define TB_GEN_CODE 4
#define IS_TU_TB 0x20
#define IS_TU_JMP 0x40
#define CF_PARALLEL 1
#define TCG_MAX_INSNS 255
struct separated_data { unsigned tu_tb_mode, last_ir1_type; void *ir1;
                        uintptr_t next_pc; void *next_tb[2]; };
typedef struct TranslationBlock TranslationBlock;
struct TranslationBlock { uintptr_t pc; unsigned bool_flags, size;
    struct { void *ptr; } tc;
    union { struct separated_data *s_data; TranslationBlock *tu_next; };
};
typedef struct { unsigned tcg_cflags; } CPUState;
static TranslationBlock *live;
static unsigned decodes, optimizations, push_fixes, pattern_fixes;
static int ir_token;
static int is_tu_tb(const TranslationBlock *tb) {
    return (tb->bool_flags & IS_TU_TB) != 0;
}
static void tr_disasm(TranslationBlock *tb, int max) {
    assert(tb != live && !is_tu_tb(tb));
    assert(!(tb->bool_flags & IS_TU_JMP));
    assert(tb->s_data->tu_tb_mode == TB_GEN_CODE);
    assert(tb->s_data->ir1 == NULL && tb->s_data->next_tb[0] == NULL);
    assert(max == TCG_MAX_INSNS);
    tb->s_data->ir1 = &ir_token;
    tb->s_data->next_pc = tb->pc + tb->size;
    decodes++;
}
static void ir1_optimization(TranslationBlock *tb) {
    assert(tb->s_data->ir1 == &ir_token);
    tb->s_data->last_ir1_type = 77;
    optimizations++;
}
static void consumer(TranslationBlock *tb) {
    if (is_tu_tb(live)) {
        assert(tb != live && tb->s_data != (void *)live->tu_next);
        assert(tb->s_data->ir1 == &ir_token);
        assert(tb->s_data->last_ir1_type == 77);
        assert(tb->s_data->next_pc == live->pc + live->size);
    } else {
        assert(tb == live);
    }
}
static void ir2_opt_push_pop_fix(TranslationBlock *tb, CPUState *cpu, int i) {
    consumer(tb); assert(i == 2); push_fixes++;
}
static void opt_instptn_fix(CPUState *cpu, TranslationBlock *tb, int i) {
    consumer(tb); assert(i == 1); pattern_fixes++;
}
'''
TEST = r'''
int main(void) {
    for (unsigned members = 1; members <= 3; members += 2) {
        for (unsigned tu = 0; tu < 2; tu++) {
            for (unsigned parallel = 0; parallel < 2; parallel++) {
                TranslationBlock ring[3] = {{0}}, before[3];
                struct separated_data ordinary = {0};
                for (unsigned k = 0; k < members; k++) {
                    ring[k].pc = 0x1000 + k * 16;
                    ring[k].tc.ptr = (void *)(uintptr_t)(0x8000 + k * 32);
                    ring[k].size = 9;
                    ring[k].bool_flags = 0x100 | (tu ? IS_TU_TB | IS_TU_JMP : 0);
                    ring[k].tu_next = &ring[(k + 1) % members];
                }
                live = &ring[members - 1];
                if (!tu) live->s_data = &ordinary;
                memcpy(before, ring, sizeof(ring));
                decodes = optimizations = push_fixes = pattern_fixes = 0;
                CPUState cpu = {parallel ? CF_PARALLEL : 0};
                restore(&cpu, live, (uintptr_t)live->tc.ptr + 8, 1);
                assert(memcmp(before, ring, sizeof(ring)) == 0);
                assert(decodes == (tu && (PUSH || PATTERN)));
                assert(optimizations == decodes);
                assert(push_fixes == (PUSH && !parallel));
                assert(pattern_fixes == PATTERN);
            }
        }
    }
    puts("PASS: private TU restore metadata, membership and both consumers");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('repo', type=Path)
    args = parser.parse_args()
    tu = (args.repo / 'target/i386/latx/optimization/tu.c').read_text()
    name = 'void tu_prepare_restore_tb('
    helper = ''
    if name in tu:
        start = tu.index(name)
        helper = tu[start:tu.index('\n}', start) + 2]
    caller = (args.repo / 'accel/tcg/translate-all.c').read_text()
    start = caller.index('    restore_state_to_opc(env, tb, data);')
    start = caller.index('\n', start) + 1
    tail = caller[start:caller.index('#ifdef CONFIG_PROFILER', start)]
    signature = ('static void restore(CPUState *cpu, TranslationBlock *tb, '
                 'uintptr_t searched_pc, int i) {\n')
    with tempfile.TemporaryDirectory(prefix='latx-tu-restore-') as tmp:
        path = Path(tmp)
        cc = shlex.split(os.environ.get('CC', 'cc'))
        for push, pattern in ((0, 0), (1, 0), (0, 1), (1, 1)):
            defines = ['-DCONFIG_LATX_TU', '-DPUSH=%d' % push,
                       '-DPATTERN=%d' % pattern]
            if push:
                defines += ['-DCONFIG_LATX_OPT_PUSH_POP']
            if pattern:
                defines += ['-DCONFIG_LATX_INSTS_PATTERN']
            source = path / 'restore.c'
            source.write_text(PRELUDE + helper + '\n' + signature + tail +
                              '\n}\n' + TEST)
            binary = path / 'restore'
            subprocess.run(cc + ['-std=gnu11', '-O2'] + defines +
                           [str(source), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
