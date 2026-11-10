/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LATX_HARDWARE_RAS_H
#define LATX_HARDWARE_RAS_H

#include "latx-options.h"

#define HRAS_LAZYPC_MAGIC UINT64_C(0x4852415300000000)
#define HRAS_LAZYPC_MASK  UINT64_C(0xffffffff00000000)

/* LoongArch encodings used by the linked CALL pair. */
#define HRAS_LA_NOP_ENCODING      UINT32_C(0x03400000)
#define HRAS_LA_PCADDU18I_MASK    UINT32_C(0xfe000000)
#define HRAS_LA_PCADDU18I_OPCODE  UINT32_C(0x1e000000)
#define HRAS_LA_OPCODE_MASK       UINT32_C(0xfc000000)
#define HRAS_LA_JIRL_OPCODE       UINT32_C(0x4c000000)
#define HRAS_LA_BL_OPCODE         UINT32_C(0x54000000)
#define HRAS_LA_BL_IMM_LOW_MASK   UINT32_C(0x0000ffff)
#define HRAS_LA_BL_IMM_HIGH_MASK  UINT32_C(0x000003ff)
#define HRAS_LA_BL_IMM_LOW_SHIFT  10
#define HRAS_LA_BL_IMM_HIGH_SHIFT 16
#define HRAS_LA_INSN_SIZE         4
#define HRAS_LA_INSN_PAIR_SIZE    (2 * HRAS_LA_INSN_SIZE)
#define HRAS_LA_BL_RANGE          (INT64_C(1) << 27)
#define HRAS_LA_LDD_RA_FROM_A7_8  UINT32_C(0x28c02161)
#define HRAS_LA_JIRL_ZERO_RA      UINT32_C(0x4c000020)

static inline uint32_t hras_encode_bl(intptr_t offset)
{
    uint32_t scaled_offset = (uint32_t)(offset / HRAS_LA_INSN_SIZE);

    return HRAS_LA_BL_OPCODE |
           (((uint32_t)scaled_offset & HRAS_LA_BL_IMM_LOW_MASK) <<
            HRAS_LA_BL_IMM_LOW_SHIFT) |
           (((uint32_t)scaled_offset >> HRAS_LA_BL_IMM_HIGH_SHIFT) &
            HRAS_LA_BL_IMM_HIGH_MASK);
}

static inline uint64_t hras_encode_linked_call_pair(intptr_t offset)
{
    return ((uint64_t)hras_encode_bl(offset) << 32) | HRAS_LA_NOP_ENCODING;
}

static inline int64_t hras_pack_call_offset(target_ulong pc,
                                            target_ulong next_pc)
{
    target_ulong offset = next_pc - pc;

    lsassert(next_pc >= pc && offset <= UINT32_MAX);
    return (int64_t)(HRAS_LAZYPC_MAGIC | offset);
}

static inline int64_t hras_pack_call_continuation(target_ulong pc,
                                                   target_ulong next_pc,
                                                   unsigned return_size)
{
    /*
     * A 16-bit near CALL stores only the low 16 bits of its guest return PC.
     * The tagged metadata records the full successor PC, so it is valid for
     * a 16-bit CALL only after the return-PC truncation is modelled too.
     */
    return return_size == 2 ? 0 : hras_pack_call_offset(pc, next_pc);
}

static inline bool hras_unpack_call_offset(int64_t value, target_ulong pc,
                                           target_ulong *next_pc)
{
    uint64_t encoded = value;

    if ((encoded & HRAS_LAZYPC_MASK) != HRAS_LAZYPC_MAGIC) {
        return false;
    }
    *next_pc = pc + (target_ulong)(encoded & UINT32_MAX);
    return true;
}

static inline bool hras_tb_in_tu(const TranslationBlock *tb)
{
#ifdef CONFIG_LATX_TU
    /*
     * IS_TU_TB is published after assembly; tu_id is available while the TU
     * is being translated.  Recovered AOT TBs carry the persistent flag.
     */
    return option_tu && tb && ((tb->bool_flags & IS_TU_TB) ||
                               (tb->s_data && tb->s_data->tu_id));
#else
    return false;
#endif
}

void hras_link_call(TranslationBlock *tb, TranslationBlock *callee);

#endif
