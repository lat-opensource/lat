/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "translate.h"
#include "hardware-ras.h"
#include "exec/tb-lookup.h"
#include "accel/tcg/internal.h"
#include "latx-options.h"

#if defined(CONFIG_LATX_TU) && defined(CONFIG_LATX_FAST_JMPCACHE) && \
    defined(CONFIG_LATX_LAZYLINK) && defined(CONFIG_LATX_DEBUG)
static const char *hras_test_marker_path;

static void __attribute__((constructor)) hras_test_marker_init(void)
{
    hras_test_marker_path = getenv("LATX_TEST_HRAS_MARKER");
}

static void hras_test_mark_link(TranslationBlock *callee, uintptr_t slot)
{
    bool call, ret = false;
    int fd;

    if (!hras_test_marker_path) {
        return;
    }
    /* Observe the installed CALL and the callee's fast-return dispatch. */
    call = (qatomic_read((uint32_t *)(slot + HRAS_LA_INSN_SIZE)) &
            HRAS_LA_OPCODE_MASK) == HRAS_LA_BL_OPCODE;
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
        if (ret && write(fd, "ret\n", 4) != 4) {
            close(fd);
            return;
        }
        close(fd);
    }
}
#endif

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
     * The first slot only materializes the link target for the ordinary JIRL
     * path. BL replaces that path, so NOP it in the linked call path and keep
     * the original PCADDU18I in lazylinkinst for unlinking.
     */
    tb_target_set_jmp_pair(slot, slot - tcg_splitwx_diff,
                           hras_encode_linked_call_pair(offset));
#ifdef CONFIG_LATX_DEBUG
    if (G_UNLIKELY(hras_test_marker_path)) {
        hras_test_mark_link(callee, slot);
    }
#endif
#endif
}
