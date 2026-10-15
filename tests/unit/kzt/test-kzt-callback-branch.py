#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Exercise the KZT fixed-callback decoder, bridge, and patch lifecycle."""

import argparse
import platform
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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", \
    __LINE__, #x); exit(1); } } while (0)
#define lsassert(x) CHECK(x)
#define R_CS 1
#define LOG_DEBUG 0
#define LOG_INFO 1
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0
#endif
#define qemu_host_page_size ((size_t)sysconf(_SC_PAGESIZE))
#define g_new0(type, n) ((type *)calloc(n, sizeof(type)))
#define g_free free
static unsigned int mmap_calls;
/* The portable model needs writable memory, not executable memory. */
static void *test_mmap(void *p, size_t n, int prot, int flags, int fd, off_t off)
{
    mmap_calls++;
    return mmap(p, n, prot & ~PROT_EXEC, flags, fd, off);
}
#define mmap test_mmap
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
typedef uintptr_t target_ulong;
typedef uintptr_t tb_page_addr_t;
#define TARGET_PAGE_MASK (~(uintptr_t)4095)
#define CF_KZT_PRIVATE 0x00100000
#define TB_JMP_RESET_OFFSET_INVALID UINT16_MAX

typedef struct {
    unsigned int _reg_num;
} IR2_OPND;

static IR2_OPND zero_ir2_opnd;

typedef struct {
    unsigned int tb_flush_count;
} TBContext;

typedef struct {
    void *code_gen_ptr;
    void *code_gen_buffer;
    size_t code_gen_buffer_size;
} TCGContext;

static TBContext tb_ctx;
static TCGContext tcg_context;
static TCGContext *tcg_ctx = &tcg_context;

typedef struct {
    void *ptr;
} TBTC;

typedef struct TranslationBlock {
    TBTC tc;
    target_ulong pc;
    size_t size;
    uintptr_t jmp_target_arg[2];
    uint16_t jmp_reset_offset[2];
    int8_t canlink[2];
    bool published;
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
static void b_to_addr(uintptr_t jmp_rx, uintptr_t jmp_rw, uintptr_t addr);
static void cpu_get_tb_cpu_state(CPUArchState *, target_ulong *, target_ulong *,
                                uint32_t *);
static void tb_phys_invalidate(TranslationBlock *, uintptr_t);
static TranslationBlock *tb_gen_code(CPUState *, target_ulong, target_ulong,
                                     uint32_t, int);
static tb_page_addr_t get_page_addr_code(CPUArchState *, target_ulong);
static void tcg_tb_insert(TranslationBlock *);
static void tcg_tb_remove(TranslationBlock *);
static void tb_destroy(TranslationBlock *);
static TranslationBlock *tb_link_page(TranslationBlock *, tb_page_addr_t,
                                      tb_page_addr_t);
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
static TranslationBlock generated_tbs[128];
static unsigned int generated_count, invalidated_count;
static bool mock_generation_fail, mock_emit_fail, mock_publish_fail;
static unsigned int destroyed;
static uint32_t guest_recipe[64];
static unsigned int mock_link_offset = TB_JMP_RESET_OFFSET_INVALID;
static int mmap_depth;
static int fixed_emit_count;
static int normal_emit_count;
static int patch_count;
static int one_word_patch_count;
static int far_patch_count;
static uintptr_t emitted_target;
static uintptr_t emitted_base;
static uintptr_t emitted_link;
static unsigned int emitted_base_reg;
static unsigned int emitted_link_reg;
static void *emitted_bridge;
static void (*emitted_callback)(CPUX86State *);
static uintptr_t jump_targets[256];
static uintptr_t jump_sources[256];
static unsigned int jump_target_count;

#define REPLAY_INSN UINT32_C(0x02800000)
#define NEAR_B      UINT32_C(0x60000000)
#define NOP_WORD    UINT32_C(0x03400000)
#define PCADDU18I   UINT32_C(0x1e000000)
#define JIRL_WORD   UINT32_C(0x4c000000)
#define FAR_OFFSET  (160 << 20)
#define FAR_MAP_SIZE (FAR_OFFSET + (1 << 20))

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

static bool entry_patch_in_range(uintptr_t from, uintptr_t to)
{
    ptrdiff_t offset = to - from;

    return offset >= -(1 << 27) && offset <= (1 << 27) - 4;
}

static void light_tb_target_set_jmp_target(uintptr_t tc_ptr,
                                           uintptr_t jmp_rx,
                                           uintptr_t jmp_rw, uintptr_t addr)
{
    uint32_t *insn = (uint32_t *)jmp_rw;

    CHECK(jump_target_count < sizeof(jump_targets) / sizeof(jump_targets[0]));
    jump_sources[jump_target_count] = jmp_rx;
    jump_targets[jump_target_count++] = addr;
    if (entry_patch_in_range(jmp_rx, addr)) {
        insn[0] = NEAR_B;
        insn[1] = NOP_WORD;
    } else {
        insn[0] = PCADDU18I;
        insn[1] = JIRL_WORD;
        far_patch_count++;
    }
}

static void b_to_addr(uintptr_t jmp_rx, uintptr_t jmp_rw, uintptr_t addr)
{
    CHECK(jmp_rx == jmp_rw);
    CHECK(entry_patch_in_range(jmp_rx, addr));
    CHECK(jump_target_count < 256);
    jump_sources[jump_target_count] = jmp_rx;
    jump_targets[jump_target_count++] = addr;
    *(uint32_t *)jmp_rw = NEAR_B;
    one_word_patch_count++;
}

static int target_latx_emit_callback_bridge(void *code_buf_addr,
                                            void (*callback)(CPUX86State *))
{
    normal_emit_count++;
    emitted_bridge = code_buf_addr;
    emitted_target = (uintptr_t)((uint32_t *)code_buf_addr + 4);
    emitted_link_reg = 0;
    return mock_emit_fail ? -1 : 8;
}

static int target_latx_emit_fixed_callback_bridge(
    void *code_buf_addr, void (*callback)(CPUX86State *),
    unsigned int base_reg, uintptr_t base_addr, unsigned int link_reg,
    uintptr_t return_addr, uintptr_t target)
{
    if (mock_emit_fail) {
        return -1;
    }
    fixed_emit_count++;
    emitted_bridge = code_buf_addr;
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
    CHECK(insn[0] == NEAR_B || insn[0] == PCADDU18I);
    CHECK(len != 8 || insn[1] != 0);
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

static void cpu_get_tb_cpu_state(CPUArchState *env, target_ulong *pc,
                                 target_ulong *cs_base, uint32_t *flags)
{
    *pc = env->eip + env->segs[R_CS].base;
    *cs_base = env->segs[R_CS].base;
    *flags = 0;
}

static void tb_phys_invalidate(TranslationBlock *tb, uintptr_t page)
{
    CHECK(mmap_depth > 0);
    tb->published = false;
    invalidated_count++;
    mock_tb = NULL;
}

static TranslationBlock *tb_gen_code(
    CPUState *cpu, target_ulong pc, target_ulong cs_base, uint32_t flags,
    int cflags)
{
    CHECK(mmap_depth > 0);
    CHECK(cflags & CF_KZT_PRIVATE);
    if (mock_generation_fail) {
        return NULL;
    }
    CHECK(generated_count < 128);
    TranslationBlock *tb = &generated_tbs[generated_count++];
    uint32_t *code = tcg_ctx->code_gen_ptr;
    memset(tb, 0, sizeof(*tb));
    memcpy(code, guest_recipe, sizeof(guest_recipe));
    tb->tc.ptr = code;
    tb->pc = pc;
    tb->size = 2;
    tb->jmp_reset_offset[0] = mock_link_offset == TB_JMP_RESET_OFFSET_INVALID
                                 ? TB_JMP_RESET_OFFSET_INVALID
                                 : mock_link_offset + 8;
    tb->jmp_target_arg[0] = mock_link_offset;
    tb->jmp_reset_offset[1] = TB_JMP_RESET_OFFSET_INVALID;
    tb->canlink[0] = tb->canlink[1] = 1;
    tcg_ctx->code_gen_ptr = code + 64;
    CHECK(!tb->published);
    return tb;
}

static tb_page_addr_t get_page_addr_code(CPUArchState *env, target_ulong pc)
{
    return mock_publish_fail ? (tb_page_addr_t)-1 : pc;
}

static void tcg_tb_insert(TranslationBlock *tb)
{
    CHECK(!tb->published);
    CHECK(mmap_depth > 0);
}

static void tcg_tb_remove(TranslationBlock *tb) {}

static void tb_destroy(TranslationBlock *tb)
{
    CHECK(!tb->published);
    destroyed++;
}

static TranslationBlock *tb_link_page(TranslationBlock *tb,
                                      tb_page_addr_t first, tb_page_addr_t last)
{
    CHECK(!tb->published && mmap_depth > 0);
    tb->published = true;
    mock_tb = tb;
    return tb;
}

static TranslationBlock *install_one(CPUState *cpu, TranslationBlock *tb,
                                     KztGuestPcCallbackState *state,
                                     uint32_t *bridge, uint32_t *code,
                                     void (*callback)(CPUX86State *))
{
    memcpy(guest_recipe, code, sizeof(guest_recipe));
    tb->tc.ptr = code;
    tb->published = true;
    mock_tb = tb;
    tcg_ctx->code_gen_ptr = bridge;
    tb_ctx.tb_flush_count++;
    memset(state, 0, sizeof(*state));
    return kzt_install_guest_pc_callback(state, cpu, 0x1000, callback);
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
static uintptr_t run_continuation(uint32_t *code, unsigned int result_reg)
{
    uintptr_t regs[32] = { 0 };
    uintptr_t pc = (uintptr_t)code;
    unsigned int callbacks = 0;

    for (unsigned int step = 0; step < 32; step++) {
        uint32_t insn;
        uintptr_t next;
        if (pc == (uintptr_t)emitted_bridge) {
            callbacks++;
            if (emitted_link_reg) {
                regs[emitted_link_reg] = emitted_link;
            }
            pc = emitted_target;
            continue;
        }
        if (pc == (uintptr_t)(code + 3)) {
            CHECK(callbacks == 1);
            return regs[result_reg];
        }
        insn = *(uint32_t *)pc;
        next = pc + 4;
        if (insn == NEAR_B || insn == PCADDU18I) {
            bool found = false;
            for (unsigned int i = jump_target_count; i-- > 0;) {
                if (jump_sources[i] == pc) {
                    next = jump_targets[i];
                    if (insn == PCADDU18I) {
                        regs[12] = next; /* long jump destroys t0 */
                    }
                    found = true;
                    break;
                }
            }
            CHECK(found);
        } else if ((insn & 0xfe000000) == 0x18000000) {
            regs[insn & 31] = pc +
                kzt_sign_extend((insn >> 5) & 0xfffff, 20) * 4;
        } else if ((insn & 0xffc00000) == 0x02c00000) {
            regs[insn & 31] = regs[(insn >> 5) & 31] +
                kzt_sign_extend((insn >> 10) & 0xfff, 12);
        } else if ((insn & 0xfc000000) == 0x50000000) {
            next = pc + kzt_sign_extend(((insn & 0x3ff) << 16) |
                                        ((insn >> 10) & 0xffff), 26) * 4;
        } else if (insn == 0x4c000020) {
            next = regs[1]; /* jirl zero, ra, 0 */
        } else {
            CHECK(insn == NOP_WORD);
        }
        regs[0] = 0;
        pc = next;
    }
    CHECK(false);
    return 0;
}

static void test_installer(void)
{
    CPUArchState arch = { 0 };
    CPUState cpu = { .env_ptr = &arch };
    TranslationBlock old = { 0 };
    KztGuestPcCallbackState state = { 0 };
    size_t size = 512u << 20;
    char *area = mmap(NULL, size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(area != MAP_FAILED);
    uint32_t *source = (void *)(area + (200u << 20));
    uint32_t *next = (void *)(area + (400u << 20));
    unsigned int maps = mmap_calls;

    /* Interior source: no holes exist within b range of the old TB. */
    for (unsigned int link = 0; link < 2; link++) {
        for (unsigned int kind = 0; kind < 3; kind++) {
            memset(source, 0, 256);
            encode_b(source, (uintptr_t)source,
                      (uintptr_t)(source + (link ? 16 : 1)), link);
            source[16] = 0x4c000020;
            source[2] = NOP_WORD;
            if (kind == 0) {
                source[1] = 0x1800000f;
            } else if (kind == 1) {
                encode_b(source + 1, (uintptr_t)(source + 1),
                          (uintptr_t)(source + 3), false);
                source[2] = 0x02c0040f;
            } else {
                source[1] = 0x02c01c0c;
                source[2] = 0x02c0018f;
            }
            uint32_t original = source[0];
            TranslationBlock *tb = install_one(&cpu, &old, &state, next,
                                                source, callback_a);
            CHECK(tb && tb != &old && tb->published && !old.published);
            uint32_t *code = tb->tc.ptr;
            CHECK(source[0] == original);
            CHECK(code[1] == source[1]);
            uintptr_t result = run_continuation(code, 15);
            CHECK(result == (kind == 0 ? (uintptr_t)(code + 1) :
                             kind == 1 ? 0 : 7));
            CHECK(mmap_calls == maps);
        }
    }

    /* A normal first instruction replays once and returns without a scratch. */
    memset(source, 0, 256);
    source[0] = 0x02c01c0c;
    source[1] = 0x02c0018f;
    source[2] = NOP_WORD;
    TranslationBlock *plain = install_one(&cpu, &old, &state, next,
                                           source, callback_a);
    CHECK(plain && !state.fixed_branch);
    CHECK(run_continuation(plain->tc.ptr, 15) == 7);

    encode_pair(source, (uintptr_t)source, (uintptr_t)source + (1 << 18),
                (uintptr_t)source + (1 << 18) + 4, 5, 6);
    TranslationBlock *pair = install_one(&cpu, &old, &state, next,
                                          source, callback_a);
    CHECK(pair && state.fixed_branch);
    CHECK(emitted_base == (uintptr_t)pair->tc.ptr + (1 << 18));
    CHECK(emitted_target == emitted_base + 4);
    CHECK(emitted_link == (uintptr_t)pair->tc.ptr + 8);
    CHECK(emitted_base_reg == 5 && emitted_link_reg == 6);

    unsigned int count = generated_count;
    TranslationBlock *a = mock_tb;
    CHECK(kzt_install_guest_pc_callback(&state, &cpu, 0x1000,
                                        callback_a) == a);
    CHECK(generated_count == count);

    /* The slot at offset zero cannot link past or reset the hook. */
    memset(source, 0, 256);
    encode_b(source, (uintptr_t)source, (uintptr_t)(source + 2), false);
    source[1] = NOP_WORD;
    mock_link_offset = 0;
    a = install_one(&cpu, &old, &state, next, source, callback_a);
    CHECK(a && a->canlink[0] == 0);
    CHECK(a->jmp_reset_offset[0] == TB_JMP_RESET_OFFSET_INVALID);
    CHECK(emitted_target == (uintptr_t)a->tc.ptr + 8);
    mock_link_offset = 16;
    a = install_one(&cpu, &old, &state, next, source, callback_a);
    CHECK(a && a->canlink[0] == 1 && a->jmp_reset_offset[0] == 24);
    mock_link_offset = TB_JMP_RESET_OFFSET_INVALID;

    /* A->B->A must retranslate guest bytes, not decode the old entry patch. */
    a = mock_tb;
    uint32_t original_guest = guest_recipe[0];
    TranslationBlock *b = kzt_install_guest_pc_callback(&state, &cpu, 0x2000,
                                                       callback_b);
    CHECK(b && b != a);
    mock_tb = a;
    TranslationBlock *again = kzt_install_guest_pc_callback(&state, &cpu,
                                                           0x1000, callback_a);
    CHECK(again && again != a && again != b);
    CHECK(state.inst_old[0] == original_guest);
    CHECK(emitted_target == (uintptr_t)again->tc.ptr + 8);

    /* A matching identity with overwritten entry bytes is not a success. */
    count = generated_count;
    *(uint32_t *)again->tc.ptr = original_guest;
    CHECK(kzt_install_guest_pc_callback(&state, &cpu, 0x1000,
                                        callback_a) != again);
    CHECK(generated_count == count + 1);

    mock_generation_fail = true;
    CHECK(kzt_install_guest_pc_callback(&state, &cpu, 0x1000,
                                        callback_b) == NULL);
    CHECK(state.tb == NULL && state.callback == NULL && mmap_depth == 0);
    mock_generation_fail = false;
    mock_emit_fail = true;
    CHECK(kzt_install_guest_pc_callback(&state, &cpu, 0x1000,
                                        callback_b) == NULL);
    CHECK(mock_tb == NULL && mmap_depth == 0);
    mock_emit_fail = false;
    unsigned int before_destroy = destroyed;
    mock_publish_fail = true;
    CHECK(kzt_install_guest_pc_callback(&state, &cpu, 0x1000,
                                        callback_b) == NULL);
    CHECK(mock_tb == NULL && state.tb == NULL && mmap_depth == 0);
    CHECK(destroyed == before_destroy + 1);
    mock_publish_fail = false;
    CHECK(kzt_install_guest_pc_callback(&state, &cpu, 0x1000,
                                        callback_b) != NULL);
    CHECK(munmap(area, size) == 0);
    mock_tb = NULL;
}

int main(void)
{
    test_decoder();
    test_installer();
    puts("KZT callback: interior cache, original continuation, private publication, "
         "entry link exclusion, reinstall and failure retry passed");
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

static void reset_bridge_test(const unsigned int *temps, unsigned int count)
{
    memset(writes, 0, sizeof(writes));
    write_count = 0;
    memcpy(temp_sequence, temps, count * sizeof(temp_sequence[0]));
    temp_count = count;
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

    reset_bridge_test((const unsigned int[]){ 31, 30 }, 2);
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

    reset_bridge_test((const unsigned int[]){ 31, 30 }, 2);
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

    reset_bridge_test((const unsigned int[]){ 5, 7 }, 2);
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

static void test_two_temp_collisions(void)
{
    uintptr_t regs[32] = { 0 };

    reset_bridge_test((const unsigned int[]){ 5, 6, 7 }, 3);
    generate_kzt_fixed_callback_context(NULL, callback, 5, 0x1234, 6,
                                        0x5678, 0x9abc);
    CHECK(write_count == 3);
    CHECK(writes[0].reg == 5 && writes[0].value == 0x1234);
    CHECK(writes[1].reg == 6 && writes[1].value == 0x5678);
    CHECK(writes[2].reg == 7 && writes[2].value == 0x9abc);
    CHECK(jump_reg == 7);
    CHECK(temp_index == 3);
    apply_writes(regs);
    CHECK(regs[5] == 0x1234 && regs[6] == 0x5678);
}

int main(void)
{
    test_separate_registers();
    test_alias_registers();
    test_temp_collision();
    test_two_temp_collisions();
    puts("KZT fixed callback bridge: base, link, and temporary registers passed");
    return 0;
}
'''


EMITTER_TEST = r'''
#define qemu_log(...) ((void)0)
static bool option_dump;
typedef struct { void *first_ir2; } TRANSLATION_DATA;
static TRANSLATION_DATA translation;
static struct { TRANSLATION_DATA *tr_data; } lsenv_storage = { &translation };
#define lsenv (&lsenv_storage)
static unsigned int planned_words, assembled, finished;
static void tr_init(void *tb) {}
static void tr_fini(bool extensions) { finished++; }
static void generate_kzt_callback_context(void *p, void (*cb)(CPUX86State *)) {}
static void generate_kzt_fixed_callback_context(
    void *p, void (*cb)(CPUX86State *), unsigned int base_reg, uintptr_t base,
    unsigned int link_reg, uintptr_t link, uintptr_t target) {}
static int label_dispose(void *tb, TRANSLATION_DATA *data)
{
    return planned_words;
}
static int tr_ir2_assemble(void *buffer, void *first)
{
    assembled++;
    for (unsigned int i = 0; i < planned_words; i++) {
        ((uint32_t *)buffer)[i] = 0x12345678;
    }
    return planned_words;
}
'''

EMITTER_MAIN = r'''
int main(void)
{
    uint32_t code[33] = { 0 };
    tcg_ctx->code_gen_buffer = code;
    tcg_ctx->code_gen_buffer_size = 32 * 4;
    planned_words = 8;
    CHECK(target_latx_emit_callback_bridge(code + 25, NULL) == -1);
    CHECK(target_latx_emit_fixed_callback_bridge(code + 25, NULL,
                                                 0, 0, 0, 0, 0) == -1);
    CHECK(assembled == 0 && finished == 2);
    for (unsigned int i = 0; i < 33; i++) {
        CHECK(code[i] == 0);
    }
    CHECK(target_latx_emit_callback_bridge(code + 24, NULL) == 8);
    CHECK(target_latx_emit_fixed_callback_bridge(code + 24, NULL,
                                                 0, 0, 0, 0, 0) == 8);
    CHECK(assembled == 2 && code[32] == 0 && finished == 4);
    puts("KZT callback bridge capacity: checked before assembly");
    return 0;
}
'''


CALLER_TEST = r'''
#define LIB_EMULATED 0
#define KZT_PUBLIC_LOADER_OK 0
#define KZT_PUBLIC_LOADER_BUSY 2
#define KZT_PUBLIC_LOADER_INVALID_INPUT 1
typedef int kzt_public_loader_result_t;
typedef struct { bool active; uintptr_t r_debug_addr, r_brk_addr;
                 size_t live_map_count; } Observer;
static Observer kzt_public_loader_observer;
static int kzt_public_loader_reader;
#define kzt_try_bind_observed_object NULL
static KztGuestPcCallbackState kzt_public_r_brk_inst;
static int kzt_main_relocated_before_relro, kzt_main_fallback_reported;
static int kzt_observer_failure_reported, x64free_fini;
typedef struct { const char *name; } elfheader_t;
static elfheader_t header = { "fixture" }, *elf_header = &header;
struct image_info { uintptr_t exec_entry, start_code, end_code; };
static struct image_info info1 = { .exec_entry = 0x1000 };
static CPUArchState arch;
static CPUState cpu = { .env_ptr = &arch };
static TranslationBlock installed;
static bool install_ok;
static unsigned int active_logs, failed_logs, fallback_reports, finished;
static int depth;
static CPUState *env_cpu(CPUX86State *env) { return &cpu; }
static void mmap_lock(void) { depth++; }
static void mmap_unlock(void) { CHECK(depth > 0); depth--; }
static bool latx_kzt_runtime_enabled(void) { return true; }
static int kzt_find_main_dynamic_table(elfheader_t *h, uintptr_t *a, size_t *n)
{ *a = 1; *n = 1; return 0; }
static int kzt_public_loader_observer_activate(Observer *o, uintptr_t a,
    size_t n, void *r, void *visit, void *arg)
{ o->active = true; o->r_brk_addr = 0x2000; return KZT_PUBLIC_LOADER_OK; }
static void kzt_public_loader_observer_reset(Observer *o)
{ CHECK(depth > 0 || !o->active); memset(o, 0, sizeof(*o)); }
static int KZTRelocationTargetsAreWritable(elfheader_t *h) { return 1; }
static int kzt_relocate_elf_with_fresh_transaction(elfheader_t *h) { return 0; }
static void kzt_report_main_recovery_if_needed(void) {}
static void kzt_report_main_fallback_once(const char *reason)
{ fallback_reports++; }
static void kzt_report_observer_failure_once(const char *op, int result) {}
static const char *kzt_public_loader_result_name(int result) { return "failure"; }
static void kzt_dynamic_library_change_callback(CPUX86State *env) {}
static void finiReFlesh(elfheader_t *h) { finished++; }
#define AddDebugInfo(...) ((void)0)
static void printf_log(int level, const char *fmt, ...)
{
    active_logs += strstr(fmt, "observer active:") != NULL;
    failed_logs += strstr(fmt, "r_brk callback was not installed") != NULL;
}
static TranslationBlock *kzt_install_guest_pc_callback(
    KztGuestPcCallbackState *state, CPUState *cpu, uintptr_t addr,
    void (*callback)(CPUX86State *))
{
    cpu->env_ptr->eip = addr;
    return install_ok ? &installed : NULL;
}
'''

CALLER_MAIN = r'''
int main(void)
{
    arch.eip = 0x1234;
    install_ok = false;
    kzt_guest_main_entry_callback(&arch);
    CHECK(!kzt_public_loader_observer.active && active_logs == 0);
    CHECK(failed_logs == 1 && finished == 1 && depth == 0);
    CHECK(arch.eip == 0x1234);
    install_ok = true;
    kzt_guest_main_entry_callback(&arch);
    CHECK(kzt_public_loader_observer.active && active_logs == 1);
    CHECK(finished == 2 && arch.eip == 0x1234 && depth == 0);
    kzt_public_loader_observer.active = false;
    install_ok = false;
    kzt_install_runtime_callbacks(&cpu, &info1);
    CHECK(fallback_reports == 1 && arch.eip == 0x1234 && depth == 0);
    install_ok = true;
    kzt_install_runtime_callbacks(&cpu, &info1);
    CHECK(fallback_reports == 1 && arch.eip == 0x1234);
    puts("KZT callback callers: installation failure is not reported as active");
    return 0;
}
'''


def native_source(repo):
    """Execute the installer and production jump encoders on LoongArch.

    A minimal bridge replaces the callback body, keeping the emitted control
    flow executable without a full CPU/translator environment. The separate
    bridge test covers the production register allocator path.
    """
    myalign = (repo / 'target/i386/latx/context/myalign.c').read_text()
    tcg = (repo / 'tcg/loongarch/tcg-target.c.inc').read_text()
    source = COMMON_PRELUDE
    start = source.index('static unsigned int mmap_calls;')
    end = source.index('#define qatomic_read', start)
    source = source[:start] + source[end:]
    source += r'''
#include <assert.h>
#define CONFIG_LATX_LARGE_CC 1
#define TCG_REG_T0 12
#define OPC_B 0x14
#define qatomic_set(p, v) (*(p) = (v))
typedef uint32_t tcg_insn_unit;
typedef int LoongarchInsn;

static intptr_t sextreg(intptr_t value, int pos, int len)
{
    uint64_t sign = UINT64_C(1) << (len - 1);
    uint64_t bits = ((uint64_t)value >> pos) & ((sign << 1) - 1);

    return (intptr_t)((bits ^ sign) - sign);
}
'''
    for name in ('KztFixedBranch', 'KztGuestPcCallbackState'):
        source += source_type(myalign, name)
    for signature in (
        'static int32_t encode_jbimm18(',
        'static int32_t encode_djimm28(',
        'static int32_t encode_dj(',
        'void b_to_addr(',
        'void light_tb_target_set_jmp_target(',
    ):
        source += source_function(tcg, signature)
    for signature in (
        'static int64_t kzt_sign_extend(',
        'static bool kzt_fixed_branch_target(',
        'static bool kzt_prepare_guest_pc_callback(',
        'static bool kzt_publish_guest_pc_callback(',
    ):
        source += source_function(myalign, signature)
    source += source_function(
        myalign, 'static TranslationBlock *kzt_install_guest_pc_callback(')
    fixtures = INSTALLER_TEST[:INSTALLER_TEST.index('static void test_decoder(')]
    for signature in (
        'static void light_tb_target_set_jmp_target(',
        'static void b_to_addr(',
        'static int target_latx_emit_fixed_callback_bridge(',
        'static int target_latx_emit_callback_bridge(',
        'static void flush_idcache_range(',
    ):
        fixtures = fixtures.replace(source_function(fixtures, signature), '')
    source += fixtures
    source += r'''
static void flush_idcache_range(uintptr_t rx, uintptr_t rw, size_t size)
{
    __builtin___clear_cache((char *)rx, (char *)(rx + size));
}

static int target_latx_emit_callback_bridge(
    void *buffer, void (*callback)(CPUX86State *))
{
    uint32_t *p = buffer;
    for (unsigned int i = 0; i < 8; i++) {
        p[i] = NOP_WORD;
    }
    b_to_addr((uintptr_t)p, (uintptr_t)p, (uintptr_t)(p + 4));
    return 8;
}

static unsigned int load_address(uint32_t *p, unsigned int reg, uintptr_t value)
{
    p[0] = 0x14000000 | (((value >> 12) & 0xfffff) << 5) | reg;
    p[1] = 0x03800000 | ((value & 0xfff) << 10) | (reg << 5) | reg;
    p[2] = 0x16000000 | (((value >> 32) & 0xfffff) << 5) | reg;
    p[3] = 0x03000000 | (((value >> 52) & 0xfff) << 10) | (reg << 5) | reg;
    return 4;
}

static int target_latx_emit_fixed_callback_bridge(
    void *buffer, void (*callback)(CPUX86State *),
    unsigned int base_reg, uintptr_t base, unsigned int link_reg,
    uintptr_t link, uintptr_t target)
{
    uint32_t *p = buffer;
    unsigned int count = 0;

    if (link_reg) {
        count += load_address(p + count, link_reg, link);
    }
    count += load_address(p + count, 20, target);
    p[count++] = 0x4c000280; /* jirl zero, t8, 0 */
    flush_idcache_range((uintptr_t)p, (uintptr_t)p, count * 4);
    return count;
}

int main(void)
{
    CPUArchState arch = { 0 };
    CPUState cpu = { .env_ptr = &arch };
    TranslationBlock tb = { 0 };
    KztGuestPcCallbackState state = { 0 };
    uint32_t *area = mmap(NULL, FAR_MAP_SIZE,
                          PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    CHECK(area != MAP_FAILED);
    mock_tb = &tb;
    for (unsigned int direction = 0; direction < 2; direction++) {
      for (unsigned int far = 0; far < 2; far++) {
        for (unsigned int link = 0; link < 2; link++) {
          for (unsigned int kind = 0; kind < 4; kind++) {
            const char *only = getenv("KZT_CALLBACK_CASE");
            uint32_t *code = (void *)((char *)area +
                                     (direction ? FAR_OFFSET : 0));
            intptr_t distance = far ? FAR_OFFSET : 4096;
            uint32_t *bridge = (void *)((char *)code +
                                       (direction ? -distance : distance));
            uintptr_t got, want;

            if (only && kind != (unsigned int)atoi(only)) {
                continue;
            }
            memset(code, 0, 256);
            encode_b(code, (uintptr_t)code,
                      (uintptr_t)(code + (link ? 16 : 1)), link);
            code[16] = 0x4c000020; /* jirl zero, ra, 0 */
            code[2] = NOP_WORD;
            if (kind == 0) {
                code[1] = 0x1800000f; /* pcaddi t3, 0 */
            } else if (kind == 1) {
                encode_b(code + 1, (uintptr_t)(code + 1),
                          (uintptr_t)(code + 3), false);
                code[2] = 0x02c0040f; /* skipped: addi.d t3, zero, 1 */
            } else if (kind == 2) {
                code[1] = 0x02c01c0c; /* addi.d t0, zero, 7 */
                code[2] = 0x02c0018f; /* addi.d t3, t0, 0 */
            } else {
                code[0] = 0x02c01c0c;
                code[1] = 0x02c0018f;
            }
            code[3] = 0x02c001e4; /* addi.d a0, t3, 0 */
            code[4] = 0x4c000240; /* jirl zero, t6, 0 */
            code[32] = 0x02c00032; /* addi.d t6, ra, 0: save test return PC */
            code[33] = 0x02c0000f; /* addi.d t3, zero, 0 */
            encode_b(code + 34, (uintptr_t)(code + 34), (uintptr_t)code, false);
            TranslationBlock *fresh = install_one(&cpu, &tb, &state, bridge,
                                                   code, callback_a);
            CHECK(fresh && fresh != &tb);
            code = fresh->tc.ptr;
            flush_idcache_range((uintptr_t)code, (uintptr_t)code, 256);
            got = ((uintptr_t (*)(void))(code + 32))();
            want = kind == 0 ? (uintptr_t)(code + 1) : kind == 1 ? 0 : 7;
            fprintf(stderr, "native direction=%u far=%u link=%u kind=%u "
                    "got=%#lx want=%#lx\n", direction, far, link, kind,
                    (unsigned long)got, (unsigned long)want);
            CHECK(got == want);
          }
        }
      }
    }
    mock_tb = NULL;
    CHECK(munmap(area, FAR_MAP_SIZE) == 0);
    return 0;
}
'''
    return source


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
        'static bool kzt_prepare_guest_pc_callback(',
        'static bool kzt_publish_guest_pc_callback(',
        'static TranslationBlock *kzt_install_guest_pc_callback(',
    ):
        installer += source_function(myalign, signature)
    installer += INSTALLER_TEST

    bridge = COMMON_PRELUDE
    bridge += source_function(config,
                              'static void generate_kzt_fixed_callback_context(')
    bridge += BRIDGE_TEST

    emitter = COMMON_PRELUDE + EMITTER_TEST
    emitter += source_function(config, 'int target_latx_emit_callback_bridge(')
    emitter += source_function(config, 'int target_latx_emit_fixed_callback_bridge(')
    emitter += EMITTER_MAIN

    callers = COMMON_PRELUDE.replace(
        'typedef struct CPUX86State {\n    uint64_t eip;\n} CPUX86State;',
        'typedef CPUArchState CPUX86State;')
    callers += source_type(myalign, 'KztGuestPcCallbackState')
    callers += CALLER_TEST
    callers += source_function(myalign, 'static void kzt_guest_main_entry_callback(')
    callers += source_function(myalign, 'void kzt_install_runtime_callbacks(')
    callers += CALLER_MAIN

    with tempfile.TemporaryDirectory(prefix='latx-kzt-callback-branch-') as temp:
        tempdir = Path(temp)
        compile_and_run(args.cc, installer, tempdir / 'installer.c')
        compile_and_run(args.cc, bridge, tempdir / 'bridge.c')
        compile_and_run(args.cc, emitter, tempdir / 'emitter.c')
        compile_and_run(args.cc, callers, tempdir / 'callers.c')
        if platform.machine() == 'loongarch64':
            compile_and_run(args.cc, native_source(args.repo),
                            tempdir / 'native.c')


if __name__ == '__main__':
    main()
