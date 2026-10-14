#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Exercise the KZT fixed-callback decoder, bridge, and patch lifecycle."""

import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile


def source_type(source, name):
    start = source.index(f'typedef struct {name} ')
    end = source.index(f'}} {name};', start) + len(f'}} {name};')
    return source[start:end] + '\n'


def source_function(source, signature):
    start = 0
    while True:
        start = source.index(signature, start)
        brace = source.find('{', start)
        semicolon = source.find(';', start)
        if brace != -1 and (semicolon == -1 or brace < semicolon):
            break
        start += len(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


COMMON_PRELUDE = r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", \
    __LINE__, #x); exit(1); } } while (0)
#define R_CS 1
#define LOG_DEBUG 0
#define qatomic_read(p) (*(p))
#define qatomic_mb_set(p, v) (*(p) = (v))

typedef struct {
    uintptr_t base;
} Segment;

typedef struct CPUArchState {
    uint64_t eip;
    Segment segs[2];
} CPUArchState;

typedef struct CPUState {
    CPUArchState *env_ptr;
    uint32_t tcg_cflags;
} CPUState;

typedef struct CPUX86State {
    uint64_t eip;
} CPUX86State;

typedef uintptr_t ADDR;

typedef struct {
    unsigned int _reg_num;
} IR2_OPND;

static IR2_OPND zero_ir2_opnd;

typedef struct {
    unsigned int tb_flush_count;
} TBContext;

typedef struct {
    void *code_gen_ptr;
} TCGContext;

static TBContext tb_ctx;
static TCGContext tcg_context;
static TCGContext *tcg_ctx = &tcg_context;

typedef struct {
    void *ptr;
} TBTC;

typedef struct TranslationBlock {
    TBTC tc;
} TranslationBlock;

static void flush_idcache_range(uintptr_t rx, uintptr_t rw, size_t len);
static bool have_mmap_lock(void);
static void mmap_lock(void);
static void mmap_unlock(void);
static TranslationBlock *kzt_tb_find_exp(CPUState *cpu, TranslationBlock *tb,
                                         int tb_exit, uint32_t cflags);
static void light_tb_target_set_jmp_target(uintptr_t tc_ptr,
                                           uintptr_t jmp_rx,
                                           uintptr_t jmp_rw, uintptr_t addr);
static int target_latx_emit_callback_bridge(void *code_buf_addr,
                                            void (*callback)(CPUX86State *));
static int target_latx_emit_fixed_callback_bridge(
    void *code_buf_addr, void (*callback)(CPUX86State *),
    unsigned int base_reg, uintptr_t base_addr, unsigned int link_reg,
    uintptr_t return_addr, uintptr_t target);
static void printf_log(int level, const char *fmt, ...);
static IR2_OPND get_la_ir2_opnd(unsigned int reg);
static IR2_OPND ra_alloc_itemp(void);
static void ra_free_temp(IR2_OPND opnd);
static void li_d(IR2_OPND opnd, uintptr_t value);
static void la_jirl(IR2_OPND rd, IR2_OPND rj, int offset);
static void kzt_native_to_wrapper(void);
static void kzt_helper1(ADDR callback);
static void kzt_wrapper_to_native(void);
'''


INSTALLER_TEST = r'''
static TranslationBlock *mock_tb;
static int mmap_depth;
static int fixed_emit_count;
static int normal_emit_count;
static int patch_count;
static uintptr_t emitted_target;
static uintptr_t emitted_base;
static uintptr_t emitted_link;
static unsigned int emitted_base_reg;
static unsigned int emitted_link_reg;
static void (*emitted_callback)(CPUX86State *);

static bool have_mmap_lock(void)
{
    return mmap_depth != 0;
}

static void mmap_lock(void)
{
    mmap_depth++;
}

static void mmap_unlock(void)
{
    CHECK(mmap_depth > 0);
    mmap_depth--;
}

static TranslationBlock *kzt_tb_find_exp(CPUState *cpu, TranslationBlock *tb,
                                         int tb_exit, uint32_t cflags)
{
    return mock_tb;
}

static void light_tb_target_set_jmp_target(uintptr_t tc_ptr,
                                           uintptr_t jmp_rx,
                                           uintptr_t jmp_rw, uintptr_t addr)
{
    uint32_t *insn = (uint32_t *)jmp_rw;

    insn[0] = 0x60000000;
    insn[1] = 0x03400000;
}

static int target_latx_emit_callback_bridge(void *code_buf_addr,
                                            void (*callback)(CPUX86State *))
{
    normal_emit_count++;
    return 8;
}

static int target_latx_emit_fixed_callback_bridge(
    void *code_buf_addr, void (*callback)(CPUX86State *),
    unsigned int base_reg, uintptr_t base_addr, unsigned int link_reg,
    uintptr_t return_addr, uintptr_t target)
{
    fixed_emit_count++;
    emitted_callback = callback;
    emitted_base_reg = base_reg;
    emitted_base = base_addr;
    emitted_link_reg = link_reg;
    emitted_link = return_addr;
    emitted_target = target;
    return 8;
}

static void flush_idcache_range(uintptr_t rx, uintptr_t rw, size_t len)
{
    const uint32_t *insn = (const uint32_t *)rw;

    CHECK(len == 4 || len == 8);
    CHECK(insn[0] == 0x60000000);
    if (len == 8) {
        CHECK(insn[1] == 0x03400000);
    }
    patch_count++;
}

static void printf_log(int level, const char *fmt, ...)
{
    (void)level;
    (void)fmt;
}

static void encode_b(uint32_t *insn, uintptr_t address, uintptr_t target,
                     bool link)
{
    int64_t delta = (int64_t)target - (int64_t)address;
    uint32_t word;

    CHECK(delta % 4 == 0);
    CHECK(delta / 4 >= -(1 << 25));
    CHECK(delta / 4 < (1 << 25));
    word = (uint32_t)(delta / 4);
    insn[0] = (link ? 0x54000000u : 0x50000000u) |
              ((word & 0xffffu) << 10) | ((word >> 16) & 0x3ffu);
}

static void encode_pair(uint32_t *insn, uintptr_t address, uintptr_t base,
                        uintptr_t target, unsigned int base_reg,
                        unsigned int link_reg)
{
    int64_t upper = ((int64_t)base - (int64_t)address) >> 18;
    int64_t lower = ((int64_t)target - (int64_t)base) >> 2;

    CHECK(((int64_t)base - (int64_t)address) == (upper << 18));
    CHECK(((int64_t)target - (int64_t)base) == (lower << 2));
    insn[0] = 0x1e000000u | (((uint32_t)upper & 0xfffffu) << 5) | base_reg;
    insn[1] = 0x4c000000u | (((uint32_t)lower & 0xffffu) << 10) |
              (base_reg << 5) | link_reg;
}

static void callback_a(CPUX86State *env)
{
}

static void callback_b(CPUX86State *env)
{
}

static void test_decoder(void)
{
    uint32_t insn[2] = { 0 };
    KztFixedBranch branch;

    encode_b(insn, 0x1000, 0xff0, false);
    CHECK(kzt_fixed_branch_target(insn, 0x1000, &branch));
    CHECK(branch.target == 0xff0);
    CHECK(branch.insn_count == 1);
    CHECK(branch.base_reg == 0 && branch.link_reg == 0);

    encode_b(insn, 0x1000, 0x1010, true);
    CHECK(kzt_fixed_branch_target(insn, 0x1000, &branch));
    CHECK(branch.target == 0x1010);
    CHECK(branch.insn_count == 1);
    CHECK(branch.link_reg == 1);

    encode_pair(insn, 0x1000, 0x41000, 0x41800, 5, 6);
    CHECK(kzt_fixed_branch_target(insn, 0x1000, &branch));
    CHECK(branch.base == 0x41000);
    CHECK(branch.target == 0x41800);
    CHECK(branch.insn_count == 2);
    CHECK(branch.base_reg == 5);
    CHECK(branch.link_reg == 6);

    insn[1] &= ~(0x1fu << 5);
    insn[1] |= 4u << 5;
    CHECK(!kzt_fixed_branch_target(insn, 0x1000, &branch));
}

static void test_installer(void)
{
    CPUArchState arch = { 0 };
    CPUState cpu = { .env_ptr = &arch, .tcg_cflags = 0 };
    TranslationBlock tb = { 0 };
    KztGuestPcCallbackState state = { 0 };
    uint32_t code_a[32] = { 0 };
    uint32_t code_b[32] = { 0 };
    uint32_t bridge[64];
    uintptr_t target_a = (uintptr_t)&code_a[16];
    uintptr_t target_b = (uintptr_t)&code_b[20];
    uintptr_t target_a2 = (uintptr_t)&code_a[24];
    int count;

    mock_tb = &tb;
    tb.tc.ptr = code_a;
    encode_b(code_a, (uintptr_t)code_a, target_a, false);
    code_a[1] = 0x03400000;
    tb_ctx.tb_flush_count = 3;
    tcg_ctx->code_gen_ptr = bridge;

    CHECK(kzt_install_guest_pc_callback(&state, &cpu, 0x1000,
                                        callback_a) == &tb);
    CHECK(fixed_emit_count == 1);
    CHECK(emitted_callback == callback_a);
    CHECK(emitted_target == target_a);
    CHECK(patch_count == 1);
    CHECK(mmap_depth == 0);

    count = fixed_emit_count;
    CHECK(kzt_install_guest_pc_callback(&state, &cpu, 0x1000,
                                        callback_a) == &tb);
    CHECK(fixed_emit_count == count);
    CHECK(mmap_depth == 0);

    code_a[0] = 0x60000000;
    code_a[1] = 0x03400000;
    CHECK(kzt_install_guest_pc_callback(&state, &cpu, 0x1000,
                                        callback_b) == &tb);
    CHECK(fixed_emit_count == count + 1);
    CHECK(emitted_callback == callback_b);
    CHECK(emitted_target == target_a);
    CHECK(mmap_depth == 0);

    encode_b(code_a, (uintptr_t)code_a, target_a2, false);
    tb_ctx.tb_flush_count++;
    CHECK(kzt_install_guest_pc_callback(&state, &cpu, 0x1000,
                                        callback_b) == &tb);
    CHECK(emitted_target == target_a2);
    CHECK(mmap_depth == 0);

    tb.tc.ptr = code_b;
    encode_b(code_b, (uintptr_t)code_b, target_b, false);
    CHECK(kzt_install_guest_pc_callback(&state, &cpu, 0x1000,
                                        callback_b) == &tb);
    CHECK(emitted_target == target_b);
    CHECK(mmap_depth == 0);

    code_a[0] = 0x12345678;
    tb.tc.ptr = code_a;
    tb_ctx.tb_flush_count++;
    tcg_ctx->code_gen_ptr = bridge;
    CHECK(kzt_install_guest_pc_callback(&state, &cpu, 0x1000,
                                        callback_b) == &tb);
    CHECK(normal_emit_count == 1);
    CHECK(mmap_depth == 0);

    encode_b(code_a, (uintptr_t)code_a, target_a2, true);
    tb.tc.ptr = code_a;
    tb_ctx.tb_flush_count++;
    tcg_ctx->code_gen_ptr = bridge;
    CHECK(kzt_install_guest_pc_callback(&state, &cpu, 0x1000,
                                        callback_a) == &tb);
    CHECK(emitted_link_reg == 1);
    CHECK(emitted_link == (uintptr_t)(code_a + 1));
    CHECK(mmap_depth == 0);

    mock_tb = NULL;
    CHECK(kzt_install_guest_pc_callback(&state, &cpu, 0x1000,
                                        callback_b) == NULL);
    CHECK(mmap_depth == 0);
}

int main(void)
{
    test_decoder();
    test_installer();
    puts("KZT callback branch lifecycle: decoder, relocation, callback identity passed");
    return 0;
}
'''


BRIDGE_TEST = r'''
static IR2_OPND get_la_ir2_opnd(unsigned int reg)
{
    return (IR2_OPND) { ._reg_num = reg };
}

typedef struct {
    unsigned int reg;
    uintptr_t value;
} RegisterWrite;

static RegisterWrite writes[8];
static unsigned int write_count;
static unsigned int temp_sequence[4];
static unsigned int temp_count;
static unsigned int temp_index;
static unsigned int jump_reg;
static uintptr_t jump_target;

static IR2_OPND ra_alloc_itemp(void)
{
    CHECK(temp_index < temp_count);
    return (IR2_OPND) { ._reg_num = temp_sequence[temp_index++] };
}

static void ra_free_temp(IR2_OPND opnd)
{
    (void)opnd;
}

static void li_d(IR2_OPND opnd, uintptr_t value)
{
    CHECK(write_count < sizeof(writes) / sizeof(writes[0]));
    writes[write_count++] = (RegisterWrite) {
        .reg = opnd._reg_num,
        .value = value,
    };
    jump_target = value;
}

static void la_jirl(IR2_OPND rd, IR2_OPND rj, int offset)
{
    CHECK(rd._reg_num == 0);
    CHECK(offset == 0);
    jump_reg = rj._reg_num;
}

static void kzt_native_to_wrapper(void)
{
}

static void kzt_helper1(ADDR callback)
{
}

static void kzt_wrapper_to_native(void)
{
}

static void reset_bridge_test(unsigned int first_temp,
                              unsigned int second_temp)
{
    memset(writes, 0, sizeof(writes));
    write_count = 0;
    temp_sequence[0] = first_temp;
    temp_sequence[1] = second_temp;
    temp_count = 2;
    temp_index = 0;
    jump_reg = 0;
    jump_target = 0;
}

static void apply_writes(uintptr_t regs[32])
{
    for (unsigned int i = 0; i < write_count; i++) {
        regs[writes[i].reg] = writes[i].value;
    }
}

static void callback(CPUX86State *env)
{
}

static void test_separate_registers(void)
{
    uintptr_t regs[32] = { 0 };

    reset_bridge_test(31, 30);
    generate_kzt_fixed_callback_context(NULL, callback, 5, 0x1234, 6,
                                        0x5678, 0x9abc);
    CHECK(write_count == 3);
    CHECK(writes[0].reg == 5 && writes[0].value == 0x1234);
    CHECK(writes[1].reg == 6 && writes[1].value == 0x5678);
    CHECK(writes[2].reg == 31 && writes[2].value == 0x9abc);
    CHECK(jump_reg == 31);
    apply_writes(regs);
    CHECK(regs[5] == 0x1234 && regs[6] == 0x5678);
}

static void test_alias_registers(void)
{
    uintptr_t regs[32] = { 0 };

    reset_bridge_test(31, 30);
    generate_kzt_fixed_callback_context(NULL, callback, 5, 0x1234, 5,
                                        0x5678, 0x9abc);
    CHECK(write_count == 2);
    CHECK(writes[0].reg == 5 && writes[0].value == 0x5678);
    CHECK(writes[1].reg == 31 && writes[1].value == 0x9abc);
    CHECK(jump_reg == 31);
    apply_writes(regs);
    CHECK(regs[5] == 0x5678);
}

static void test_temp_collision(void)
{
    uintptr_t regs[32] = { 0 };

    reset_bridge_test(5, 7);
    generate_kzt_fixed_callback_context(NULL, callback, 5, 0x1234, 6,
                                        0x5678, 0x9abc);
    CHECK(write_count == 3);
    CHECK(writes[0].reg == 5 && writes[0].value == 0x1234);
    CHECK(writes[1].reg == 6 && writes[1].value == 0x5678);
    CHECK(writes[2].reg == 7 && writes[2].value == 0x9abc);
    CHECK(jump_reg == 7);
    apply_writes(regs);
    CHECK(regs[5] == 0x1234 && regs[6] == 0x5678);
}

int main(void)
{
    test_separate_registers();
    test_alias_registers();
    test_temp_collision();
    puts("KZT fixed callback bridge: base, link, and temporary registers passed");
    return 0;
}
'''


def compile_and_run(cc, source, output):
    output.write_text(source)
    command = shlex.split(cc) + [
        '-std=gnu11', '-Wall', '-Wextra', '-O2',
        '-Wno-unused-function', '-Wno-unused-parameter',
        '-Wno-unused-variable',
        '-fsanitize=undefined', '-fno-sanitize-recover=all',
        str(output), '-o', str(output.with_suffix('')),
    ]
    subprocess.run(command, check=True)
    subprocess.run([str(output.with_suffix(''))], check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('repo', type=Path)
    parser.add_argument('--cc', default='cc')
    args = parser.parse_args()

    myalign = (args.repo / 'target/i386/latx/context/myalign.c').read_text()
    config = (args.repo / 'target/i386/latx/latx-config.c').read_text()

    installer = COMMON_PRELUDE
    installer += source_type(myalign, 'KztFixedBranch')
    installer += source_type(myalign, 'KztGuestPcCallbackState')
    for signature in (
        'static int64_t kzt_sign_extend(',
        'static bool kzt_fixed_branch_target(',
        'static void kzt_patch_callback_branch(',
        'static TranslationBlock *kzt_install_guest_pc_callback(',
    ):
        installer += source_function(myalign, signature)
    installer += INSTALLER_TEST

    bridge = COMMON_PRELUDE
    bridge += source_function(config,
                              'static void generate_kzt_fixed_callback_context(')
    bridge += BRIDGE_TEST

    with tempfile.TemporaryDirectory(prefix='latx-kzt-callback-branch-') as temp:
        tempdir = Path(temp)
        compile_and_run(args.cc, installer, tempdir / 'installer.c')
        compile_and_run(args.cc, bridge, tempdir / 'bridge.c')


if __name__ == '__main__':
    main()
