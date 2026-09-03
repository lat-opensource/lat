/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"
#include "lsenv.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate.h"
#include "latx-native-asm.h"

#ifdef CONFIG_LATX_FAST_TRANSLATOR

static void qt_host_emit_u32(void **code_addr, uint32_t insn)
{
    *(uint32_t *)*code_addr = insn;
    *code_addr = (uint8_t *)*code_addr + 4;
}

static inline uint32_t qt_host_reg(IR2_OPND opnd)
{
    return opnd._reg_num & 0x1f;
}

static inline uint32_t qt_host_gpr3(uint32_t opcode, IR2_OPND rd,
                                    IR2_OPND rj, IR2_OPND rk)
{
    return opcode | qt_host_reg(rd) | (qt_host_reg(rj) << 5) |
           (qt_host_reg(rk) << 10);
}

static inline uint32_t qt_host_gpr3_imm12(uint32_t opcode, IR2_OPND rd,
                                          IR2_OPND rj, int imm)
{
    return opcode | qt_host_reg(rd) | (qt_host_reg(rj) << 5) |
           (((uint32_t)imm & 0xfff) << 10);
}

static inline uint32_t qt_host_gpr2_imm20(uint32_t opcode, IR2_OPND rd,
                                          int imm)
{
    return opcode | qt_host_reg(rd) | (((uint32_t)imm & 0xfffff) << 5);
}

static inline uint32_t qt_host_gpr2_imm6(uint32_t opcode, IR2_OPND rd,
                                         IR2_OPND rj, int imm)
{
    return opcode | qt_host_reg(rd) | (qt_host_reg(rj) << 5) |
           (((uint32_t)imm & 0x3f) << 10);
}

static inline uint32_t qt_host_bstrpick_w(IR2_OPND rd, IR2_OPND rj,
                                          int msb, int lsb)
{
    return 0x00608000 | qt_host_reg(rd) | (qt_host_reg(rj) << 5) |
           (((uint32_t)lsb & 0x1f) << 10) |
           (((uint32_t)msb & 0x1f) << 16);
}

static inline uint32_t qt_host_bstrpick_d(IR2_OPND rd, IR2_OPND rj,
                                          int msb, int lsb)
{
    return 0x00c00000 | qt_host_reg(rd) | (qt_host_reg(rj) << 5) |
           (((uint32_t)lsb & 0x3f) << 10) |
           (((uint32_t)msb & 0x3f) << 16);
}

static inline uint32_t qt_host_alsl_d(IR2_OPND rd, IR2_OPND rj,
                                      IR2_OPND rk, int imm)
{
    return 0x002c0000 | qt_host_reg(rd) | (qt_host_reg(rj) << 5) |
           (qt_host_reg(rk) << 10) | (((uint32_t)imm & 0x3) << 15);
}

static inline uint32_t qt_host_jirl(IR2_OPND rd, IR2_OPND rj, int off)
{
    return 0x4c000000 | qt_host_reg(rd) | (qt_host_reg(rj) << 5) |
           (((uint32_t)off & 0xffff) << 10);
}

static inline uint32_t qt_host_b26(int off)
{
    return 0x50000000 | (((uint32_t)off & 0xffff) << 10) |
           (((uint32_t)off >> 16) & 0x3ff);
}

static inline uint32_t qt_host_branch2(uint32_t opcode, IR2_OPND rj,
                                       IR2_OPND rd, int off)
{
    return opcode | qt_host_reg(rd) | (qt_host_reg(rj) << 5) |
           (((uint32_t)off & 0xffff) << 10);
}

static void qt_host_emit_li64(void **code_addr, IR2_OPND rd, uint64_t value)
{
    uint32_t lo32 = value;
    uint32_t hi32 = value >> 32;

    qt_host_emit_u32(code_addr,
        qt_host_gpr2_imm20(0x14000000, rd, lo32 >> 12));
    qt_host_emit_u32(code_addr,
        qt_host_gpr3_imm12(0x03800000, rd, rd, lo32));
    qt_host_emit_u32(code_addr,
        qt_host_gpr2_imm20(0x16000000, rd, hi32));
    qt_host_emit_u32(code_addr,
        qt_host_gpr3_imm12(0x03000000, rd, rd, hi32 >> 20));
}

static void qt_host_emit_fixed_far_jump(void **code_addr, ADDR target,
                                        IR2_OPND scratch)
{
    ptrdiff_t offset = ((uintptr_t)target - (uintptr_t)*code_addr) >> 2;

#ifdef CONFIG_LATX_LARGE_CC
    if (offset == sextract64(offset, 0, 26)) {
        qt_host_emit_u32(code_addr, qt_host_b26(offset));
        qt_host_emit_u32(code_addr,
            qt_host_gpr3_imm12(0x03400000, zero_ir2_opnd,
                               zero_ir2_opnd, 0));
    } else {
        ptrdiff_t lower = (int16_t)offset;
        ptrdiff_t upper = (offset - lower) >> 16;

        qt_host_emit_u32(code_addr,
            qt_host_gpr2_imm20(0x1e000000, scratch, upper));
        qt_host_emit_u32(code_addr, qt_host_jirl(zero_ir2_opnd, scratch,
                                                 lower));
    }
#else
    lsassert(offset == sextract64(offset, 0, 26));
    qt_host_emit_u32(code_addr, qt_host_b26(offset));
#endif
}

static int qt_host_emit_indirect_goto(void *code_addr)
{
    void *start = code_addr;
    IR2_OPND next_x86_addr = ra_alloc_dbt_arg2();
    IR2_OPND next_tb = V0_RENAME_OPND;
    IR2_OPND jmp_entry = ir2_opnd_new(IR2_OPND_GPR, la_t0);
    IR2_OPND far_tmp = ir2_opnd_new(IR2_OPND_GPR, la_t1);
    IR2_OPND jmp_cache_addr = ra_alloc_static0();

    qt_host_emit_u32(&code_addr,
        qt_host_gpr2_imm6(0x00450000, next_tb, next_x86_addr,
                          TB_JMP_CACHE_BITS));
    qt_host_emit_u32(&code_addr,
        qt_host_gpr3(0x00158000, next_tb, next_x86_addr, next_tb));
    qt_host_emit_u32(&code_addr,
        qt_host_bstrpick_d(next_tb, next_tb, TB_JMP_CACHE_BITS - 1, 0));

#ifdef CONFIG_LATX_FAST_JMPCACHE
#ifdef CONFIG_LATX_GLUE_MASK
#error "Quicktrans indirect exit does not support CONFIG_LATX_GLUE_MASK"
#endif
    qt_host_emit_u32(&code_addr,
        qt_host_alsl_d(next_tb, next_tb, jmp_cache_addr, 3));
    qt_host_emit_u32(&code_addr,
        qt_host_gpr3_imm12(0x28c00000, jmp_entry, next_tb, 0));
    qt_host_emit_u32(&code_addr,
        qt_host_branch2(0x5c000000, jmp_entry, next_x86_addr, 3));
    qt_host_emit_u32(&code_addr,
        qt_host_gpr3_imm12(0x28c00000, next_tb, next_tb, 8));
    qt_host_emit_u32(&code_addr,
        qt_host_jirl(zero_ir2_opnd, next_tb, 0));
#else
    qt_host_emit_u32(&code_addr,
        qt_host_gpr2_imm6(0x00410000, next_tb, next_tb, 3));
    qt_host_emit_u32(&code_addr,
        qt_host_gpr3(0x380c0000, next_tb, next_tb, jmp_cache_addr));
    qt_host_emit_u32(&code_addr,
        qt_host_branch2(0x58000000, next_tb, zero_ir2_opnd, 8));
    qt_host_emit_u32(&code_addr,
        qt_host_gpr3_imm12(0x28c00000, jmp_entry, next_tb,
                           offsetof(TranslationBlock, pc)));
    qt_host_emit_u32(&code_addr,
        qt_host_branch2(0x5c000000, jmp_entry, next_x86_addr, 6));
    qt_host_emit_u32(&code_addr,
        qt_host_gpr3_imm12(0x28c00000, jmp_entry, next_tb,
                           offsetof(TranslationBlock, cflags)));
    qt_host_emit_u32(&code_addr,
        qt_host_bstrpick_w(jmp_entry, jmp_entry, 18, 18));
    qt_host_emit_u32(&code_addr,
        qt_host_branch2(0x5c000000, jmp_entry, zero_ir2_opnd, 3));
    qt_host_emit_u32(&code_addr,
        qt_host_gpr3_imm12(0x28c00000, jmp_entry, next_tb,
                           offsetof(TranslationBlock, tc) +
                           offsetof(struct tb_tc, ptr)));
    qt_host_emit_u32(&code_addr,
        qt_host_jirl(zero_ir2_opnd, jmp_entry, 0));
#endif

#ifdef CONFIG_LATX_KZT
    {
        IR2_OPND reserved = ir2_opnd_new(IR2_OPND_GPR, la_t2);
        IR2_OPND helper_addr = ir2_opnd_new(IR2_OPND_GPR, la_t3);
        int skip_offset = 1 + 8 + 4 + 1 + 1 + 1 + 8;

        qt_host_emit_li64(&code_addr, reserved, (ADDR)reserved_va);
        qt_host_emit_u32(&code_addr,
            qt_host_branch2(0x68000000, next_x86_addr, reserved,
                            skip_offset));
        for (int i = 0; i < 8; ++i) {
            qt_host_emit_u32(&code_addr,
                qt_host_gpr3_imm12(0x29c00000, ra_alloc_gpr(i + 8),
                                   env_ir2_opnd,
                                   lsenv_offset_of_gpr(lsenv, i + 8)));
        }
        qt_host_emit_li64(&code_addr, helper_addr,
                          (ADDR)kzt_get_alternate_pc);
        qt_host_emit_u32(&code_addr,
            qt_host_gpr3(0x00150000, a0_ir2_opnd, next_x86_addr,
                         zero_ir2_opnd));
        qt_host_emit_u32(&code_addr,
            qt_host_jirl(ra_ir2_opnd, helper_addr, 0));
        qt_host_emit_u32(&code_addr,
            qt_host_gpr3(0x00150000, next_x86_addr, a0_ir2_opnd,
                         zero_ir2_opnd));
        for (int i = 0; i < 8; ++i) {
            qt_host_emit_u32(&code_addr,
                qt_host_gpr3_imm12(0x28c00000, ra_alloc_gpr(i + 8),
                                   env_ir2_opnd,
                                   lsenv_offset_of_gpr(lsenv, i + 8)));
        }
    }
#endif

    qt_host_emit_fixed_far_jump(&code_addr,
                                 context_switch_native_to_bt_ret_0,
                                 far_tmp);
    return ((uintptr_t)code_addr - (uintptr_t)start) >> 2;
}

int latx_indirect_goto_macro_emit_for_fast(void *code_addr)
{
    return qt_host_emit_indirect_goto(code_addr);
}

int latx_ret_indirect_goto_macro_emit_for_fast(void *code_addr,
                                               int rsp_adjust)
{
    void *start = code_addr;
    IR2_OPND rsp = ra_alloc_gpr(esp_index);
    IR2_OPND return_addr = ra_alloc_dbt_arg2();

    lsassert(rsp_adjust == sextract64(rsp_adjust, 0, 12));
    qt_host_emit_u32(&code_addr,
        qt_host_gpr3_imm12(0x28c00000, return_addr, rsp, 0));
    qt_host_emit_u32(&code_addr,
        qt_host_gpr3_imm12(0x02c00000, rsp, rsp, rsp_adjust));
    code_addr = (uint8_t *)code_addr + qt_host_emit_indirect_goto(code_addr) * 4;

    return ((uintptr_t)code_addr - (uintptr_t)start) >> 2;
}

#endif /* CONFIG_LATX_FAST_TRANSLATOR */
