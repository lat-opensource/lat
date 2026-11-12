/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "translate.h"
#include "hardware-ras.h"
#include "exec/tb-lookup.h"
#include "accel/tcg/internal.h"
#include "latx-options.h"
#include "tu.h"
#include "qemu/cacheflush.h"

#if defined(CONFIG_LATX_TU) && defined(CONFIG_LATX_FAST_JMPCACHE) && \
    defined(CONFIG_LATX_LAZYLINK) && defined(CONFIG_LATX_DEBUG)
static const char *hras_test_marker_path;

static void __attribute__((constructor)) hras_test_marker_init(void)
{
    hras_test_marker_path = getenv("LATX_TEST_HRAS_MARKER");
}

static void hras_test_mark_link(TranslationBlock *callee, uintptr_t slot,
                               bool single)
{
    bool call, pair, ret = false;
    int fd;

    if (!hras_test_marker_path) {
        return;
    }
    /* Observe the installed CALL and the callee's fast-return dispatch. */
    call = (qatomic_read((uint32_t *)(slot +
            (single ? 0 : HRAS_LA_INSN_SIZE))) &
            HRAS_LA_OPCODE_MASK) == HRAS_LA_BL_OPCODE;
    pair = !single &&
           (qatomic_read((uint32_t *)slot) & HRAS_LA_PCADDU18I_MASK) ==
               HRAS_LA_PCADDU18I_OPCODE;
    if ((callee->bool_flags & IS_INDIRECT_JMP) &&
        callee->jmp_indirect != TB_JMP_RESET_OFFSET_INVALID) {
        for (size_t off = callee->jmp_indirect;
             off + HRAS_LA_INSN_PAIR_SIZE <= callee->tc.size &&
             off < callee->jmp_indirect + HRAS_LA_INSN_PAIR_SIZE * 4;
             off += HRAS_LA_INSN_SIZE) {
            /* ld.d ra, a7 (V0_RENAME), 8; jirl zero, ra, 0. */
            if (qatomic_read((uint32_t *)(callee->tc.ptr + off)) ==
                HRAS_LA_LDD_RA_FROM_A7_8 &&
                qatomic_read((uint32_t *)(callee->tc.ptr + off +
                                          HRAS_LA_INSN_SIZE)) ==
                HRAS_LA_JIRL_ZERO_RA) {
                ret = true;
                break;
            }
        }
    }
    fd = open(hras_test_marker_path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd >= 0) {
        if (call && write(fd, "call\n", 5) != 5) {
            close(fd);
            return;
        }
        if (call && single && write(fd, "single-call\n", 12) != 12) {
            close(fd);
            return;
        }
        if (call && pair && write(fd, "pair-call\n", 10) != 10) {
            close(fd);
            return;
        }
        if (ret && write(fd, "ret\n", 4) != 4) {
            close(fd);
            return;
        }
        close(fd);
    }
}

static void hras_test_mark_indirect_call(TranslationBlock *tb)
{
    uintptr_t jirl_addr;
    uintptr_t bne_addr;
    int64_t bne_imm;
    uint32_t jirl;
    uint32_t bne;
    int fd;

    if (!hras_test_marker_path) {
        return;
    }
    jirl_addr = (uintptr_t)tb->tc.ptr + tb->jmp_target_arg[1];
    bne_addr = (uintptr_t)tb->tc.ptr + tb->jmp_reset_offset[1];
    jirl = qatomic_read((uint32_t *)jirl_addr);
    bne = qatomic_read((uint32_t *)bne_addr);
    bne_imm = (bne >> 10) & 0xffff;
    if (bne_imm & (1LL << 15)) {
        bne_imm -= 1LL << 16;
    }
    if ((jirl & HRAS_LA_OPCODE_MASK) != HRAS_LA_JIRL_OPCODE ||
        (jirl & 0x1f) != 1 ||
        (uintptr_t)((int64_t)bne_addr + (bne_imm << 2)) !=
            (uintptr_t)tb->tc.ptr + tb->jmp_target_arg[0]) {
        return;
    }

    fd = open(hras_test_marker_path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd >= 0) {
        if (write(fd, "indirect-call\n", 14) == 14) {
            close(fd);
            return;
        }
        close(fd);
    }
}
#endif

/*
 * The slow path starts immediately after the indirect-call JIRL. It cannot
 * remain there when the JIRL is the RA-based call slot, because the next host
 * instruction must be the continuation TB. Check the labels before TU moves
 * the cold tail out of line.
 */
bool hras_callin_can_layout(TranslationBlock *tb, TranslationBlock *next)
{
    target_ulong continuation_pc;
    uintptr_t jirl_off = tb ? tb->jmp_target_arg[1] : 0;
    uintptr_t miss_off = tb ? tb->jmp_target_arg[0] : 0;
    uint32_t jirl;
    uint32_t bne;

    if (!tb || !tb->s_data ||
        tb->s_data->last_ir1_type != IR1_TYPE_CALLIN ||
        !use_indirect_jmp(tb) || !option_hw_cache_ras ||
        !hras_tb_in_tu(tb) || use_tu_jmp(tb) || !next ||
        !hras_unpack_call_offset(tb->lazypc[1], tb->pc,
                                 &continuation_pc) ||
        next->pc != continuation_pc || next->flags != tb->flags ||
        tb_cflags(next) != tb_cflags(tb) ||
        (tb_cflags(tb) & ~CF_PARALLEL) || (tb->flags & HF_TF_MASK) ||
        qemu_loglevel_mask(CPU_LOG_TB_NOCHAIN) ||
        miss_off == TB_JMP_RESET_OFFSET_INVALID ||
        tb->jmp_reset_offset[0] != TB_JMP_RESET_OFFSET_INVALID ||
        tb->jmp_reset_offset[1] == TB_JMP_RESET_OFFSET_INVALID ||
        miss_off > UINT16_MAX || miss_off >= tb->tc.size ||
        jirl_off >= tb->tc.size ||
        jirl_off + HRAS_LA_INSN_SIZE != miss_off) {
        return false;
    }
    jirl = qatomic_read((uint32_t *)((uintptr_t)tb->tc.ptr + jirl_off));
    bne = qatomic_read((uint32_t *)((uintptr_t)tb->tc.ptr +
                                    tb->jmp_reset_offset[1]));
    return (jirl & HRAS_LA_OPCODE_MASK) == HRAS_LA_JIRL_OPCODE &&
           (jirl & 0x1f) == 0 && (jirl & 0x03fffc00) == 0 &&
           (bne & HRAS_LA_OPCODE_MASK) == (UINT32_C(0x17) << 26);
}

bool hras_callin_activate_layout(TranslationBlock *tb, TranslationBlock *next,
                                 uintptr_t cold)
{
    uintptr_t bne_addr;
    uintptr_t bne_rw;
    uintptr_t jirl_addr;
    uintptr_t jirl_rw;
    int64_t bne_offset;
    uint32_t bne;
    uint32_t jirl;
    bool ra_call;

    if (!tb || tb->lazypc[0] != HRAS_CALLIN_PENDING) {
        return false;
    }
    bne_addr = (uintptr_t)tb->tc.ptr + tb->jmp_reset_offset[1];
    bne_rw = bne_addr - tcg_splitwx_diff;
    jirl_addr = (uintptr_t)tb->tc.ptr + tb->jmp_target_arg[1];
    jirl_rw = jirl_addr - tcg_splitwx_diff;
    bne_offset = ((int64_t)cold - (int64_t)bne_addr) >> 2;
    lsassert(bne_offset == sextract64(bne_offset, 0, 16));

    bne = qatomic_read((uint32_t *)bne_addr);
    bne = (bne & ~UINT32_C(0x03fffc00)) |
          (((uint32_t)bne_offset & UINT32_C(0xffff)) << 10);
    qatomic_set((uint32_t *)bne_rw, bne);
    flush_idcache_range(bne_addr, bne_rw, HRAS_LA_INSN_SIZE);

    ra_call = next &&
        (uintptr_t)next->tc.ptr == (uintptr_t)tb->tc.ptr + tb->tc.size;
    jirl = qatomic_read((uint32_t *)jirl_addr);
    if (ra_call && (jirl & HRAS_LA_OPCODE_MASK) == HRAS_LA_JIRL_OPCODE &&
        (jirl & 0x1f) == 0 && (jirl & 0x03fffc00) == 0) {
        qatomic_set((uint32_t *)jirl_rw, jirl | 1);
        flush_idcache_range(jirl_addr, jirl_rw, HRAS_LA_INSN_SIZE);
    }

    tb->jmp_target_arg[0] = cold - (uintptr_t)tb->tc.ptr;
    tb->jmp_target_arg[1] = jirl_addr - (uintptr_t)tb->tc.ptr;
    tb->lazypc[0] = HRAS_CALLIN_ACTIVE;
#if defined(CONFIG_LATX_TU) && defined(CONFIG_LATX_FAST_JMPCACHE) && \
    defined(CONFIG_LATX_LAZYLINK) && defined(CONFIG_LATX_DEBUG)
    if (ra_call) {
        hras_test_mark_indirect_call(tb);
    }
#endif
    return ra_call;
}

bool hras_callin_has_cold_exit(const TranslationBlock *tb)
{
    return tb && tb->lazypc[0] == HRAS_CALLIN_ACTIVE;
}

bool hras_callin_is_active(const TranslationBlock *tb)
{
    uint32_t jirl;

    /*
     * AOT recovery uses the shared temporary s_data while allocating a TU,
     * so last_ir1_type can describe a later member here.  The tagged lazypc
     * state and the emitted instruction pair are the persistent identity.
     */
    if (!hras_callin_has_cold_exit(tb) ||
        !(tb->bool_flags & IS_INDIRECT_JMP) ||
        tb->jmp_target_arg[0] == TB_JMP_RESET_OFFSET_INVALID ||
        tb->jmp_target_arg[1] >= tb->tc.size ||
        tb->jmp_target_arg[1] + HRAS_LA_INSN_SIZE != tb->tc.size ||
        tb->jmp_reset_offset[1] == TB_JMP_RESET_OFFSET_INVALID ||
        tb->jmp_reset_offset[1] >= tb->tc.size) {
        return false;
    }
    jirl = qatomic_read((uint32_t *)((uintptr_t)tb->tc.ptr +
                                     tb->jmp_target_arg[1]));
    return (jirl & HRAS_LA_OPCODE_MASK) == HRAS_LA_JIRL_OPCODE &&
           ((jirl & 0x1f) == 0 || (jirl & 0x1f) == 1) &&
           (jirl & 0x03fffc00) == 0;
}

/* Called after all members have been assembled, before TU compaction. */
bool hras_call_can_layout(TranslationBlock *tb, TranslationBlock *next)
{
#if defined(CONFIG_LATX_TU) && defined(CONFIG_LATX_FAST_JMPCACHE) && \
    defined(CONFIG_LATX_LAZYLINK)
    target_ulong next_pc;
    uintptr_t slot;
    uint32_t *pair;

    if (tb->s_data->last_ir1_type != IR1_TYPE_CALL ||
        !option_hw_cache_ras || !hras_tb_in_tu(tb) || use_tu_jmp(tb) ||
        !next || hras_call_has_cold_exit(tb) ||
        !hras_unpack_call_offset(tb->lazypc[1], tb->pc, &next_pc) ||
        (tb_cflags(tb) & ~CF_PARALLEL) || (tb->flags & HF_TF_MASK) ||
        qemu_loglevel_mask(CPU_LOG_TB_NOCHAIN) ||
        tb->jmp_target_arg[0] == TB_JMP_RESET_OFFSET_INVALID ||
        tb->jmp_reset_offset[1] != TB_JMP_RESET_OFFSET_INVALID ||
        next->pc != next_pc || next->flags != tb->flags ||
        tb_cflags(next) != tb_cflags(tb) ||
        (uintptr_t)next->tc.ptr != (uintptr_t)tb->tc.ptr + tb->tc.size ||
        tb->jmp_target_arg[0] + 8 != tb->tc.size) {
        return false;
    }
    slot = (uintptr_t)tb->tc.ptr + tb->jmp_target_arg[0];
    pair = (void *)slot;
    return (pair[0] & HRAS_LA_PCADDU18I_MASK) ==
               HRAS_LA_PCADDU18I_OPCODE &&
           (pair[1] & (HRAS_LA_OPCODE_MASK | 0x1f)) ==
               (HRAS_LA_JIRL_OPCODE | 4); /* jirl a0 */
#else
    return false;
#endif
}

/* A single word is always independently executable during unlink/relink. */
bool hras_set_call_target(TranslationBlock *tb, int n, uintptr_t addr)
{
#if defined(CONFIG_LATX_TU) && defined(CONFIG_LATX_FAST_JMPCACHE) && \
    defined(CONFIG_LATX_LAZYLINK)
    uintptr_t slot, cold;
    target_ulong continuation;
    TranslationBlock *next;
    uint32_t branch;
    intptr_t offset;

    if (n != 0 || !hras_call_is_single(tb)) {
        return false;
    }
    slot = (uintptr_t)tb->tc.ptr + tb->jmp_target_arg[0];
    cold = (uintptr_t)tb->tc.ptr + tb->jmp_target_arg[1];
    if (addr == (uintptr_t)tb->tc.ptr + tb->jmp_reset_offset[0] ||
        !tb_target_jmp_in_range(slot, addr) ||
        (addr % HRAS_LA_INSN_SIZE)) {
        tb->lazylink[0] = 1;
        b_to_addr(slot, slot - tcg_splitwx_diff, cold);
        return true;
    }
    branch = B_OPCODE;
    if (option_hw_cache_ras && hras_tb_in_tu(tb) &&
        hras_unpack_call_offset(tb->lazypc[1], tb->pc, &continuation) &&
        !(tb_cflags(tb) & ~CF_PARALLEL) && !(tb->flags & HF_TF_MASK) &&
        !qemu_loglevel_mask(CPU_LOG_TB_NOCHAIN) &&
        tb_htable_lookup(thread_cpu, tb->pc, 0, tb->flags,
                         tb_cflags(tb)) == tb) {
        next = tb_htable_lookup(thread_cpu, continuation, 0,
                                tb->flags, tb_cflags(tb));
        if (next && !(tb_cflags(next) & CF_INVALID) &&
            (uintptr_t)next->tc.ptr == slot + HRAS_LA_INSN_SIZE) {
            branch = BL_OPCODE;
        }
    }
    offset = (intptr_t)addr - (intptr_t)slot;
    branch |= (((offset / HRAS_LA_INSN_SIZE) & HRAS_LA_BL_IMM_LOW_MASK) <<
               HRAS_LA_BL_IMM_LOW_SHIFT) |
              (((offset / HRAS_LA_INSN_SIZE) >> HRAS_LA_BL_IMM_HIGH_SHIFT) &
               HRAS_LA_BL_IMM_HIGH_MASK);
    tb->lazylink[0] = 2;
    qatomic_set((uint32_t *)(slot - tcg_splitwx_diff), branch);
    flush_idcache_range(slot, slot - tcg_splitwx_diff, 4);
    return true;
#else
    return false;
#endif
}

void hras_link_call(TranslationBlock *tb, TranslationBlock *callee)
{
#if defined(CONFIG_LATX_TU) && defined(CONFIG_LATX_FAST_JMPCACHE) && \
    defined(CONFIG_LATX_LAZYLINK)
    uintptr_t slot;
    intptr_t offset;
    uint32_t base_insn;
    TranslationBlock *next;

    target_ulong hras_next_pc;

    if (!option_hw_cache_ras ||
        !hras_unpack_call_offset(tb->lazypc[1], tb->pc, &hras_next_pc) ||
        !hras_tb_in_tu(tb)) {
        return;
    }
    if (!callee || tb->lazylink[0] != 2 ||
        (tb_cflags(tb) & ~CF_PARALLEL) || (tb->flags & HF_TF_MASK) ||
        (tb_cflags(callee) & CF_INVALID) ||
        qemu_loglevel_mask(CPU_LOG_TB_NOCHAIN) ||
        tb_htable_lookup(thread_cpu, tb->pc, 0, tb->flags,
                         tb_cflags(tb)) != tb ||
        tb->jmp_target_arg[0] == TB_JMP_RESET_OFFSET_INVALID) {
        return;
    }
    if (hras_call_is_single(tb)) {
        slot = (uintptr_t)tb->tc.ptr + tb->jmp_target_arg[0];
        hras_set_call_target(tb, 0, (uintptr_t)callee->tc.ptr);
#ifdef CONFIG_LATX_DEBUG
        if (G_UNLIKELY(hras_test_marker_path)) {
            hras_test_mark_link(callee, slot, true);
        }
#endif
        return;
    }
    next = tb_htable_lookup(thread_cpu, hras_next_pc, 0,
                            tb->flags, tb_cflags(tb));
    slot = (uintptr_t)tb->tc.ptr + tb->jmp_target_arg[0];
    if (!next || (tb_cflags(next) & CF_INVALID) ||
        (uintptr_t)next->tc.ptr != slot + HRAS_LA_INSN_PAIR_SIZE) {
        return;
    }
    offset = (intptr_t)callee->tc.ptr -
             (intptr_t)(slot + HRAS_LA_INSN_SIZE);
    if (offset % HRAS_LA_INSN_SIZE ||
        offset < -HRAS_LA_BL_RANGE || offset >= HRAS_LA_BL_RANGE) {
        return;
    }
    base_insn = tb->lazylinkinst[0];
    if ((base_insn & HRAS_LA_PCADDU18I_MASK) !=
            HRAS_LA_PCADDU18I_OPCODE ||
        ((tb->lazylinkinst[0] >> 32) & HRAS_LA_OPCODE_MASK) !=
            HRAS_LA_JIRL_OPCODE) {
        return;
    }
    /*
     * Keep PCADDU18I in both states. An executor can pass the first word
     * before another thread unlinks the second; JIRL then still sees its
     * initialized base. Replacing the first word with NOP is unsafe even
     * when the writer restores PCADDU18I before JIRL.
     */
    tb_target_set_jmp_pair(slot, slot - tcg_splitwx_diff,
                           hras_encode_linked_call_pair(offset, base_insn));
#ifdef CONFIG_LATX_DEBUG
    if (G_UNLIKELY(hras_test_marker_path)) {
        hras_test_mark_link(callee, slot, false);
    }
#endif
#endif
}
