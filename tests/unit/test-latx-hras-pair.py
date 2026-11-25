#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Replay an executor that crossed the first CALL word before unlinking."""
import os
from pathlib import Path
import subprocess
import shlex
import sys
import tempfile


def function(source, name):
    pos = source.index(name + '(')
    start = source.rfind('\n', 0, pos) + 1
    brace = source.index('{', pos)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
header = (root / 'target/i386/latx/include/hardware-ras.h').read_text()
hras = (root / 'target/i386/latx/optimization/hardware-ras.c').read_text()
target = (root / 'tcg/loongarch/tcg-target.c.inc').read_text()
defines = '\n'.join(line for line in header.splitlines()
                    if line.startswith('#define HRAS_'))
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#define CONFIG_LATX_TU
#define CONFIG_LATX_FAST_JMPCACHE
#define CONFIG_LATX_LAZYLINK
#define CF_PARALLEL 0x80000
#define CF_INVALID 0x40000
#define HF_TF_MASK 0x100
#define CPU_LOG_TB_NOCHAIN 1
#define TB_JMP_RESET_OFFSET_INVALID 0xffff
#define OPC_B 0x14
typedef uint64_t target_ulong;
typedef uint32_t tcg_insn_unit;
typedef struct TranslationBlock {
    struct { void *ptr; size_t size; } tc;
    target_ulong pc;
    uint32_t flags, cflags;
    uintptr_t jmp_target_arg[2];
    int lazylink[2];
    uint64_t lazylinkinst[2];
    int64_t lazypc[2];
} TranslationBlock;
static TranslationBlock source, continuation, callee;
static int option_hw_cache_ras = 1;
static void *thread_cpu;
static intptr_t tcg_splitwx_diff;
#define qatomic_read(p) __atomic_load_n(p, __ATOMIC_SEQ_CST)
#define qatomic_set(p, v) __atomic_store_n(p, v, __ATOMIC_SEQ_CST)
#define tb_cflags(tb) ((tb)->cflags)
static void flush_idcache_range(uintptr_t rx, uintptr_t rw, size_t len)
{
    __builtin___clear_cache((char *)rx, (char *)rx + len);
}
static bool qemu_loglevel_mask(int mask) { return false; }
static bool hras_tb_in_tu(const TranslationBlock *tb) { return true; }
static bool hras_call_is_single(const TranslationBlock *tb) { return false; }
static bool hras_set_call_target(TranslationBlock *tb, int n, uintptr_t addr)
{ abort(); }
static bool hras_unpack_call_offset(int64_t value, target_ulong pc,
                                   target_ulong *next)
{ *next = continuation.pc; return true; }
static TranslationBlock *tb_htable_lookup(void *cpu, target_ulong pc,
                                         int cs, uint32_t flags, int cflags)
{ return pc == source.pc ? &source : &continuation; }
'''
code += defines + '\n'
for name in ('hras_encode_bl', 'hras_encode_linked_call_pair'):
    code += function(header, name) + '\n'
code += function(target, 'tb_target_set_jmp_pair') + '\n'
code += function(hras, 'hras_link_call') + '\n'
code += r'''
#ifdef __loongarch__
extern void enter_at(void *pc, uintptr_t scratch);
__asm__(".text\n.globl enter_at\nenter_at:\n"
        "move $t0, $a1\njirl $zero, $a0, 0\n");
static int execute(void *pc, uintptr_t scratch)
{
    pid_t child = fork();
    assert(child >= 0);
    if (!child) { enter_at(pc, scratch); _exit(0); }
    int status;
    assert(waitpid(child, &status, 0) == child);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}
#endif
int main(void)
{
    uint32_t *slot = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(slot != MAP_FAILED);
    /* A real reset pair: pcaddu18i t0,0; jirl a0,t0,64. */
    uint64_t reset = ((uint64_t)(0x4c000000 | (16 << 10) | (12 << 5) | 4)
                      << 32) | (0x1e000000 | 12);
    slot[0] = reset;
    slot[1] = reset >> 32;
    slot[16] = 0x4c000020; /* jirl zero,ra,0: return to the harness. */
    source.tc.ptr = slot;
    source.pc = 0x1000;
    source.cflags = CF_PARALLEL;
    source.lazylink[0] = 2;
    source.lazylinkinst[0] = reset;
    continuation.pc = 0x1005;
    continuation.tc.ptr = slot + 2;
    callee.tc.ptr = slot + 32;
    hras_link_call(&source, &callee);
    assert((slot[1] & HRAS_LA_OPCODE_MASK) == HRAS_LA_BL_OPCODE);
    /* Executor A has consumed this first word; executor B now unlinks. */
    uint32_t consumed = slot[0];
    uintptr_t scratch = consumed == (uint32_t)reset ? (uintptr_t)slot : 0;
    tb_target_set_jmp_pair((uintptr_t)slot, (uintptr_t)slot, reset);
    assert(slot[0] == (uint32_t)reset && slot[1] == reset >> 32);
    uintptr_t resumed_target = scratch + 64;
    printf("consumed=%08x restored=%08x target=%p expected=%p\n",
           consumed, slot[1], (void *)resumed_target, (void *)(slot + 16));
#ifdef __loongarch__
    int control = execute(slot, 0);
    int resumed = execute(slot + 1, scratch);
    printf("LoongArch reset-pair control=%d in-flight executor=%d\n",
           control, resumed);
    assert(control == 0 && resumed == 0);
#endif
    assert(resumed_target == (uintptr_t)(slot + 16));
    assert(munmap(slot, 4096) == 0);
    puts("PASS: in-flight parallel HRAS CALL survives unlink");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='latx-hras-pair-') as tmp:
    src = Path(tmp) / 'pair.c'
    binary = Path(tmp) / 'pair'
    src.write_text(code)
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + ['-std=gnu99', '-O2',
                    '-UNDEBUG', str(src), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
