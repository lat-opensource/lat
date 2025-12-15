/*
 * Tiny Code Generator for QEMU
 *
 * Copyright (c) 2020 Loongson Technology Corporation Limited
 * Copyright (c) 2008-2009 Arnaud Patard <arnaud.patard@rtp-net.org>
 * Copyright (c) 2009 Aurelien Jarno <aurelien@aurel32.net>
 * Copyright (c) 2008 Fabrice Bellard
 *
 * Based on i386/tcg-target.c and mips/tcg-target.c
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

 /*
  * Todo list:
  * 1. Softmmu support
  * 2. Some ops(bswap/orc/andc/div/mul/rem) need to be verified
  * 3. Host vec support
  * 4. Remove useless debug messages
  * 5. Movi API need to be rework, tcg pool will be removed.
  * 6. Performance improvements
  */

#include "../tcg-pool.c.inc"

//#define LOONGARCH_DEBUG

#ifdef CONFIG_DEBUG_TCG
static const char * const tcg_target_reg_names[TCG_TARGET_NB_REGS] = {
    "zero",
    "ra",
    "tp",
    "sp",
    "a0",
    "a1",
    "a2",
    "a3",
    "a4",
    "a5",
    "a6",
    "a7",
    "t0",
    "t1",
    "t2",
    "t3",
    "t4",
    "t5",
    "t6",
    "t7",
    "t8",
    "x",
    "fp",
    "s0",
    "s1",
    "s2",
    "s3",
    "s4",
    "s5",
    "s6",
    "s7",
    "s8",
};
#endif

static const int tcg_target_reg_alloc_order[] = {
    /* Call saved registers */
    /* TCG_REG_FP reservered for TCG_AREG0 */
    TCG_REG_S0,
    TCG_REG_S1,
    TCG_REG_S2,
    TCG_REG_S3,
    TCG_REG_S4,
    TCG_REG_S5,
    TCG_REG_S6,
    TCG_REG_S7,
    TCG_REG_S8,

    /* Call clobbered registers */
    TCG_REG_T0,
    TCG_REG_T1,
    TCG_REG_T2,
    TCG_REG_T3,
    TCG_REG_T4,
    TCG_REG_T5,
    TCG_REG_T6,
    TCG_REG_T7,
    TCG_REG_T8,

    /* Argument registers */
    TCG_REG_A0,
    TCG_REG_A1,
    TCG_REG_A2,
    TCG_REG_A3,
    TCG_REG_A4,
    TCG_REG_A5,
    TCG_REG_A6,
    TCG_REG_A7,
};

static const int tcg_target_call_iarg_regs[] = {
    TCG_REG_A0,
    TCG_REG_A1,
    TCG_REG_A2,
    TCG_REG_A3,
    TCG_REG_A4,
    TCG_REG_A5,
    TCG_REG_A6,
    TCG_REG_A7,
};

static const int tcg_target_call_oarg_regs[] = {
    TCG_REG_A0,
    TCG_REG_A1,
};

#define TCG_CT_CONST_ZERO  0x100
#define TCG_CT_CONST_S12   0x200
#define TCG_CT_CONST_N12   0x400
#define TCG_CT_CONST_M12   0x800
#define TCG_CT_CONST_U12   0x1000

#define ALL_GENERAL_REGS      MAKE_64BIT_MASK(0, 32)
/*
 * For softmmu, we need to avoid conflicts with the first 5
 * argument registers to call the helper.  Some of these are
 * also used for the tlb lookup.
 */
#ifdef CONFIG_SOFTMMU
#define SOFTMMU_RESERVE_REGS  MAKE_64BIT_MASK(TCG_REG_A0, 5)
#else
#define SOFTMMU_RESERVE_REGS  0
#endif


static inline tcg_target_long sextreg(tcg_target_long val, int pos, int len)
{
    if (TCG_TARGET_REG_BITS == 32) {
        return sextract32(val, pos, len);
    } else {
        return sextract64(val, pos, len);
    }
}

/* test if a constant matches the constraint */
static int tcg_target_const_match(tcg_target_long val, TCGType type,
                                  const TCGArgConstraint *arg_ct)
{
    int ct = arg_ct->ct;
    if (ct & TCG_CT_CONST) {
        return 1;
    }
    if ((ct & TCG_CT_CONST_ZERO) && val == 0) {
        return 1;
    }
    if ((ct & TCG_CT_CONST_S12) && val == sextreg(val, 0, 12)) {
        return 1;
    }
    if ((ct & TCG_CT_CONST_N12) && -val == sextreg(-val, 0, 12)) {
        return 1;
    }
    if ((ct & TCG_CT_CONST_M12) && val >= -0xfff && val <= 0xfff) {
        return 1;
    }
    if ((ct & TCG_CT_CONST_U12) && val == (val & 0xfff)) {
        return 1;
    }
    return 0;
}

/*
 * Loongarch Base ISA opcodes (IM)
 */

typedef enum {
    /* reg2 */
    OPC_CLZW = 0x05,
    OPC_CTZW = 0x07,
    OPC_CLZD = 0x09,
    OPC_CTZD = 0x0b,
    // OPC_CLZ = TCG_TARGET_REG_BITS == 64 ? OPC_CLZD : OPC_CLZW;
    // OPC_CTZ = TCG_TARGET_REG_BITS == 64 ? OPC_CTZD : OPC_CTZW;
    OPC_EXTWH = 0x16,
    OPC_EXTWB = 0x17,
    /* reg3 */
    OPC_ADDW = 0x20,
    OPC_ADDD =  0x21,
    OPC_ADD = TCG_TARGET_REG_BITS == 64 ? OPC_ADDD : OPC_ADDW,
    OPC_SUBW = 0x22,
    OPC_SUBD = 0x23,
    OPC_SUB = TCG_TARGET_REG_BITS == 64 ? OPC_SUBD : OPC_SUBW,
    OPC_SLT = 0x24,
    OPC_SLTU = 0x25,
    OPC_MASKEQZ = 0x26,
    OPC_MASKNEZ = 0x27,
    OPC_NOR = 0x28,
    OPC_AND = 0x29,
    OPC_OR = 0x2a,
    OPC_XOR = 0x2b,
    OPC_ORN = 0x2c,
    OPC_ANDN = 0x2d,
    OPC_SLLW = 0x2e,
    OPC_SRLW = 0x2f,
    OPC_SRAW = 0x30,
    OPC_SLLD = 0x31,
    OPC_SRLD = 0x32,
    OPC_SRAD = 0x33,
    OPC_SLL = TCG_TARGET_REG_BITS == 64 ? OPC_SLLD : OPC_SLLW,
    OPC_SRL = TCG_TARGET_REG_BITS == 64 ? OPC_SRLD : OPC_SRLW,
    OPC_SRA = TCG_TARGET_REG_BITS == 64 ? OPC_SRAD : OPC_SRAW,
    OPC_ROTRB = 0x34,
    OPC_ROTRH = 0x35,
    OPC_ROTRW = 0x36,
    OPC_ROTRD = 0x37,
    OPC_MULW = 0x38,
    OPC_MULHW = 0x39,
    OPC_MULHWU = 0x3a,
    OPC_MULD = 0x3b,
    OPC_MULHD = 0x3c,
    OPC_MULHDU = 0x3d,
    OPC_MUL = TCG_TARGET_REG_BITS == 64 ? OPC_MULD : OPC_MULW,
    OPC_MULH = TCG_TARGET_REG_BITS == 64 ? OPC_MULHD : OPC_MULHW,
    OPC_MULHU = TCG_TARGET_REG_BITS == 64 ? OPC_MULHDU : OPC_MULHWU,
    OPC_DIVW = 0x40,
    OPC_MODW = 0x41,
    OPC_DIVWU = 0x42,
    OPC_MODWU = 0x43,
    OPC_DIVD = 0x44,
    OPC_MODD = 0x45,
    OPC_DIVDU = 0x46,
    OPC_MODDU = 0x47,
    OPC_DIV = TCG_TARGET_REG_BITS == 64 ? OPC_DIVD : OPC_DIVW,
    OPC_MOD = TCG_TARGET_REG_BITS == 64 ? OPC_MODD : OPC_MODW,
    OPC_DIVU = TCG_TARGET_REG_BITS == 64 ? OPC_DIVDU : OPC_DIVWU,
    OPC_MODU = TCG_TARGET_REG_BITS == 64 ? OPC_MODDU : OPC_MODWU,
    /* reg2 + imm12 */
    OPC_SLTI = 0x8,
    OPC_SLTIU = 0x9,
    OPC_ADDIW = 0xa,
    OPC_ADDID = 0xb,
    OPC_ADDI = TCG_TARGET_REG_BITS == 64 ? OPC_ADDID : OPC_ADDIW,
    OPC_LU52I = 0xc,
    OPC_ANDI = 0xd,
    OPC_ORI = 0xe,
    OPC_XORI = 0xf,
    OPC_LDB = 0xa0,
    OPC_LDH = 0xa1,
    OPC_LDW = 0xa2,
    OPC_LDD = 0xa3,
    OPC_STB = 0xa4,
    OPC_STH = 0xa5,
    OPC_STW = 0xa6,
    OPC_STD = 0xa7,
    OPC_LDBU = 0xa8,
    OPC_LDHU = 0xa9,
    OPC_LDWU = 0xaa,
    /* reg2 + imm16 */
    OPC_JIRL = 0x13,
    OPC_BEQ = 0x16,
    OPC_BNE = 0x17,
    OPC_BLT = 0x18,
    OPC_BGE = 0x19,
    OPC_BLTU = 0x1a,
    OPC_BGEU = 0x1b,
    /* imm26 */
    OPC_B = 0x14,
    OPC_BL = 0x15,
    /* reg1 + imm20 */
    OPC_LU12I = 0xa,
    OPC_LU32I = 0xb,
    OPC_PCADDI = 0xc,
    OPC_PCALAU12I = 0xd,
    OPC_PCADDU12I = 0xe,
    OPC_PCADDU18I = 0xf,
    /* bit-op */
    OPC_SLLIW = 0x81,
    OPC_SLLID = 0x82,
    OPC_SLLI = TCG_TARGET_REG_BITS == 64 ? OPC_SLLID : OPC_SLLIW,
    OPC_SRLIW = 0x89,
    OPC_SRLID = 0x8a,
    OPC_SRLI = TCG_TARGET_REG_BITS == 64 ? OPC_SRLID : OPC_SRLIW,
    OPC_SRAIW = 0x91,
    OPC_SRAID = 0x92,
    OPC_SRAI = TCG_TARGET_REG_BITS == 64 ? OPC_SRAID : OPC_SRAIW,
    // OPC_ROTRID = 0x4d,
    // OPC_ROTRIW = 0x99,
    // OPC_ROTRI = TCG_TARGET_REG_BITS == 64 ? OPC_ROTRID : OPC_ROTRIW;

    OPC_DBAR = 0x70e4,
} LoongarchInsn;
/*
 * RISC-V immediate and instruction encoders (excludes 16-bit RVC)
 */

/* Type-R3 */

static int32_t encode_r2(LoongarchInsn opc, TCGReg d, TCGReg j)
{
    return (d & 0x1f) | (j & 0x1f) << 5 | opc << 10;
}

/* Type-R2 */

static int32_t encode_r3(LoongarchInsn opc, TCGReg d, TCGReg j, TCGReg k)
{
    return (d & 0x1f) | (j & 0x1f) << 5 | (k & 0x1f) << 10 | opc << 15;
}

/* Type-SHIFT */

static int32_t encode_shift(LoongarchInsn opc, TCGReg d, TCGReg j, uint32_t shamt)
{
    return (d & 0x1f) | (j & 0x1f) << 5 | shamt << 10 | opc << 15;
}

/* Type-I 12-bit (include load and store) */

static int32_t encode_imm12(uint32_t imm)
{
    return (imm << 10) & 0x3ffc00;
}

static int32_t encode_i(LoongarchInsn opc, TCGReg d, TCGReg j, uint32_t imm)
{
    return (d & 0x1f) | (j & 0x1f) << 5 | encode_imm12(imm) | opc << 22;
}

/* Type-U 20-bit */

static int32_t encode_uimm20(uint32_t imm)
{
    return (imm >> 7) & 0x1ffffe0;
}

static int32_t encode_u(LoongarchInsn opc, TCGReg d, uint32_t imm)
{
    return (d & 0x1f) | encode_uimm20(imm) | opc << 25;
}

/* Type-BJ */

static int32_t encode_jbimm18(uint32_t imm)
{
    return (imm << 8) & 0x3fffc00;
}

/* Type-JB (indirect jump and conditional branch) */

static int32_t encode_jb(LoongarchInsn opc, TCGReg d, TCGReg j, uint32_t imm)
{
    return (d & 0x1f) | (j & 0x1f) << 5 | encode_jbimm18(imm) | opc << 26;
}

static int32_t encode_djimm28(uint32_t imm)
{
    return ((imm >> 18) & 0x3ff) | encode_jbimm18(imm);
}

/* Type-DJ (direct jump) */

static int32_t encode_dj(LoongarchInsn opc, uint32_t imm)
{
    return encode_djimm28(imm) | opc << 26;
}

/* TODO: Type-BZ (conditional branch compared with zero) */

// static int32_t encode_bz(LoongarchInsn opc, TCGReg j, uint32_t imm)
// {
    // return ((imm >> 18) & 0x1f) | (j & 0x1f) << 5
           // | encode_jbimm18(imm) | opc << 26;
// }

/* Type-BSTR-W */
// static int32_t encode_bstrwimm(int lsb, bool pick, int msb)
// {
    // return (lsb & 0x1f) << 10 | pick << 15 | (msb & 0x1f) << 16 | 3 << 21;
// }
//
// static int32_t encode_bstrpickw(TCGReg d, TCGReg j, int lsb, int msb)
// {
    // return (d & 0x1f) | (j & 0x1f) << 5 | encode_bstrwimm(lsb, true, msb);
// }
//
// static int32_t encode_bstrinsw(TCGReg d, TCGReg j, int lsb, int msb)
// {
    // return (d & 0x1f) | (j & 0x1f) << 5 | encode_bstrwimm(lsb, false, msb);
// }

/* Type-BSTR-D */

static int32_t encode_bstrdimm(int lsb, int msb, bool pick)
{
    return (lsb & 0x3f) << 10 | (msb & 0x3f) << 16 | pick << 22 | 1 << 23;
}

static int32_t encode_bstrpickd(TCGReg d, TCGReg j, int lsb, int msb)
{
    return (d & 0x1f) | (j & 0x1f) << 5 | encode_bstrdimm(lsb, msb, true);
}

// static int32_t encode_bstrinsd(TCGReg d, TCGReg j, int lsb, int msb)
// {
    // return (d & 0x1f) | (j & 0x1f) << 5 | encode_bstrdimm(lsb, msb, false);
// }

/*
 * Loonarch instruction emitters
 */

static void tcg_out_opc_reg3(TCGContext *s, LoongarchInsn opc,
                            TCGReg d, TCGReg j, TCGReg k)
{
    tcg_out32(s, encode_r3(opc, d, j, k));
}

static void tcg_out_opc_reg2(TCGContext *s, LoongarchInsn opc,
                            TCGReg d, TCGReg j)
{
    tcg_out32(s, encode_r2(opc, d, j));
}

static void tcg_out_opc_shift(TCGContext *s, LoongarchInsn opc,
                              TCGReg d, TCGReg j, TCGArg imm)
{
    tcg_out32(s, encode_shift(opc, d, j, imm));
}

static void tcg_out_opc_imm(TCGContext *s, LoongarchInsn opc,
                            TCGReg d, TCGReg j, TCGArg imm)
{
    tcg_out32(s, encode_i(opc, d, j, imm));
}

static void tcg_out_opc_upp(TCGContext *s, LoongarchInsn opc,
                            TCGReg d, uint32_t imm)
{
    tcg_out32(s, encode_u(opc, d, imm));
}

static void tcg_out_opc_djp(TCGContext *s, LoongarchInsn opc,
                            uint32_t imm)
{
    tcg_out32(s, encode_dj(opc, imm));
}

static void tcg_out_opc_jbr(TCGContext *s, LoongarchInsn opc,
                            TCGReg d, TCGReg j, uint32_t imm)
{
    tcg_out32(s, encode_jb(opc, d, j, imm));
}

// static void tcg_out_opc_brz(TCGContext *s, LoongarchInsn opc,
                            // TCGReg j, uint32_t imm)
// {
    // tcg_out32(s, encode_bz(opc, j, imm));
// }
//
// static void tcg_out_opc_bstrpickw(TCGContext *s, TCGReg d, TCGReg j,
                                  // int lsb, int msb)
// {
    // tcg_out32(s, encode_bstrpickw(d, j, lsb, msb));
// }
//
// static void tcg_out_opc_bstrinsw(TCGContext *s, TCGReg d, TCGReg j,
                                 // int lsb, int msb)
// {
    // tcg_out32(s, encode_bstrinsw(d, j, lsb, msb));
// }
//
static void tcg_out_opc_bstrpickd(TCGContext *s, TCGReg d, TCGReg j,
                                  int lsb, int msb)
{
    tcg_out32(s, encode_bstrpickd(d, j, lsb, msb));
}

// static void tcg_out_opc_bstrinsd(TCGContext *s, TCGReg d, TCGReg j,
                                 // int lsb, int msb)
// {
    // tcg_out32(s, encode_bstrinsd(d, j, lsb, msb));
// }

static void tcg_out_nop_fill(tcg_insn_unit *p, int count)
{
    int i;
    for (i = 0; i < count; ++i) {
        p[i] = encode_i(OPC_ADDI, TCG_REG_ZERO, TCG_REG_ZERO, 0);
    }
}

/*
 * Relocations
 */

static bool reloc_jbimm18(tcg_insn_unit *src_rw, const tcg_insn_unit *target)
{
    const tcg_insn_unit *src_rx = tcg_splitwx_to_rx(src_rw);
    intptr_t offset = (intptr_t)target - (intptr_t)src_rx;

    tcg_debug_assert((offset & 3) == 0);
    if (offset == sextreg(offset, 0, 18)) {
        *src_rw |= encode_jbimm18(offset);
        return true;
    }

    return false;
}

static bool reloc_djimm28(tcg_insn_unit *src_rw, const tcg_insn_unit *target)
{
    const tcg_insn_unit *src_rx = tcg_splitwx_to_rx(src_rw);
    intptr_t offset = (intptr_t)target - (intptr_t)src_rx;

    tcg_debug_assert((offset & 3) == 0);
    if (offset == sextreg(offset, 0, 28)) {
        *src_rw |= encode_djimm28(offset);
        return true;
    }

    return false;
}

static bool patch_reloc(tcg_insn_unit *code_ptr, int type,
                        intptr_t value, intptr_t addend)
{
    tcg_debug_assert(addend == 0);
    switch (type) {
    case R_LOONGARCH_BRANCH:
        return reloc_jbimm18(code_ptr, (tcg_insn_unit *)value);
    case R_LOONGARCH_JUMP:
        return reloc_djimm28(code_ptr, (tcg_insn_unit *)value);
    default:
        g_assert_not_reached();
    }
}

/*
 * TCG intrinsics
 */

static bool tcg_out_mov(TCGContext *s, TCGType type, TCGReg ret, TCGReg arg)
{
    if (ret == arg) {
        return true;
    }
    switch (type) {
    case TCG_TYPE_I32:
    case TCG_TYPE_I64:
        tcg_out_opc_imm(s, OPC_ADDI, ret, arg, 0);
        break;
    default:
        g_assert_not_reached();
    }
    return true;
}

#define LU32I_B (0xfffffUL<<32) 
#define LU52I_B (0xfffUL<<52)

static void tcg_out_movi(TCGContext *s, TCGType type, TCGReg rd,
                         tcg_target_long val)
{
    tcg_target_long lo;

    if (TCG_TARGET_REG_BITS == 64 && type == TCG_TYPE_I32) {
        val = (int32_t)val;
    }

    if (val == sextreg(val, 0, 12)) {
        tcg_out_opc_imm(s, OPC_ADDI, rd, TCG_REG_ZERO, val);
        return;
    }

    lo = val & 0xfff;
    if (val == lo) {
        tcg_out_opc_imm(s, OPC_ORI, rd, TCG_REG_ZERO, val);
        return;
    }

    tcg_out_opc_upp(s, OPC_LU12I, rd, val);
    if (lo != 0) {
        tcg_out_opc_imm(s, OPC_ORI, rd, rd, val);
    }

    if (TCG_TARGET_REG_BITS == 64 && val != (int32_t)val) {
        bool lu32i[2] = {(val & LU32I_B) == 0, (val & LU32I_B) == LU32I_B};
        bool lu52i[2] = {(val & LU52I_B) == 0, (val & LU52I_B) == LU52I_B};

        int sign31 = (val >> 31) & 1;
        if (lu32i[sign31] && lu52i[sign31]) {
            return;
        }
        else if (lu32i[sign31]) {
            tcg_out_opc_imm(s, OPC_LU52I, rd, rd, val>>52);
        }
        else {
            tcg_out_opc_upp(s, OPC_LU32I, rd, val>>20);
            if (!lu52i[(val >> 51) & 1]) {
                tcg_out_opc_imm(s, OPC_LU52I, rd, rd, val>>52);
            }
        }
    }
}

/* TODO: consider 32-bit host machine */
static void tcg_out_ext8u(TCGContext *s, TCGReg ret, TCGReg arg)
{
    tcg_out_opc_imm(s, OPC_ANDI, ret, arg, 0xff);
}

static void tcg_out_ext16u(TCGContext *s, TCGReg ret, TCGReg arg)
{
    tcg_out_opc_bstrpickd(s, ret, arg, 0, 15);
}

static void tcg_out_ext32u(TCGContext *s, TCGReg ret, TCGReg arg)
{
    tcg_out_opc_bstrpickd(s, ret, arg, 0, 31);
}

static void tcg_out_ext8s(TCGContext *s, TCGReg ret, TCGReg arg)
{
    tcg_out_opc_reg2(s, OPC_EXTWB, ret, arg);
}

static void tcg_out_ext16s(TCGContext *s, TCGReg ret, TCGReg arg)
{
    tcg_out_opc_reg2(s, OPC_EXTWH, ret, arg);
}

static void tcg_out_ext32s(TCGContext *s, TCGReg ret, TCGReg arg)
{
    tcg_out_opc_imm(s, OPC_ADDIW, ret, arg, 0);
}

static void tcg_out_ldst(TCGContext *s, LoongarchInsn opc, TCGReg data,
                         TCGReg addr, intptr_t offset)
{
    intptr_t imm12 = sextreg(offset, 0, 12);

    if (offset != imm12) {
        intptr_t hi = offset - imm12;
        intptr_t diff = hi - ((uintptr_t)s->code_ptr & ~0xfff); // FIXME: why uintptr_t?

        if (addr == TCG_REG_ZERO && diff == (int32_t)diff) {
            tcg_out_opc_upp(s, OPC_PCALAU12I, TCG_REG_TMP2, diff);
        } else {
            tcg_out_movi(s, TCG_TYPE_PTR, TCG_REG_TMP2, hi);
            if (addr != TCG_REG_ZERO) {
                tcg_out_opc_reg3(s, OPC_ADD, TCG_REG_TMP2, TCG_REG_TMP2, addr);
            }
        }
        addr = TCG_REG_TMP2;
    }

    switch (opc) {
    case OPC_STB:
    case OPC_STH:
    case OPC_STW:
    case OPC_STD:
        tcg_out_opc_imm(s, opc, data, addr, imm12);
        break;
    case OPC_LDB:
    case OPC_LDBU:
    case OPC_LDH:
    case OPC_LDHU:
    case OPC_LDW:
    case OPC_LDWU:
    case OPC_LDD:
        tcg_out_opc_imm(s, opc, data, addr, imm12);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_ld(TCGContext *s, TCGType type, TCGReg arg,
                       TCGReg arg1, intptr_t arg2)
{
    bool is32bit = (TCG_TARGET_REG_BITS == 32 || type == TCG_TYPE_I32);
    tcg_out_ldst(s, is32bit ? OPC_LDW : OPC_LDD, arg, arg1, arg2);
}

static void tcg_out_st(TCGContext *s, TCGType type, TCGReg arg,
                       TCGReg arg1, intptr_t arg2)
{
    bool is32bit = (TCG_TARGET_REG_BITS == 32 || type == TCG_TYPE_I32);
    tcg_out_ldst(s, is32bit ? OPC_STW : OPC_STD, arg, arg1, arg2);
}

/* TODO */
static bool tcg_out_sti(TCGContext *s, TCGType type, TCGArg val,
                        TCGReg base, intptr_t ofs)
{
    if (val == 0) {
        tcg_out_st(s, type, TCG_REG_ZERO, base, ofs);
        return true;
    }
    return false;
}

static void tcg_out_addsub2(TCGContext *s,
                            TCGReg rl, TCGReg rh,
                            TCGReg al, TCGReg ah,
                            TCGArg bl, TCGArg bh,
                            bool cbl, bool cbh, bool is_sub, bool is32bit)
{
    const LoongarchInsn opc_add = is32bit ? OPC_ADDW : OPC_ADD;
    const LoongarchInsn opc_addi = is32bit ? OPC_ADDIW : OPC_ADDI;
    const LoongarchInsn opc_sub = is32bit ? OPC_SUBW : OPC_SUB;
    TCGReg th = TCG_REG_TMP1;

    /* If we have a negative constant such that negating it would
       make the high part zero, we can (usually) eliminate one insn.  */
    if (cbl && cbh && bh == -1 && bl != 0) {
        bl = -bl;
        bh = 0;
        is_sub = !is_sub;
    }

    /* By operating on the high part first, we get to use the final
       carry operation to move back from the temporary.  */
    if (!cbh) {
        tcg_out_opc_reg3(s, (is_sub ? opc_sub : opc_add), th, ah, bh);
    } else if (bh != 0 || ah == rl) {
        tcg_out_opc_imm(s, opc_addi, th, ah, (is_sub ? -bh : bh));
    } else {
        th = ah;
    }

    /* Note that tcg optimization should eliminate the bl == 0 case.  */
    if (is_sub) {
        if (cbl) {
            tcg_out_opc_imm(s, OPC_SLTIU, TCG_REG_TMP0, al, bl);
            tcg_out_opc_imm(s, opc_addi, rl, al, -bl);
        } else {
            tcg_out_opc_reg3(s, OPC_SLTU, TCG_REG_TMP0, al, bl);
            tcg_out_opc_reg3(s, opc_sub, rl, al, bl);
        }
        tcg_out_opc_reg3(s, opc_sub, rh, th, TCG_REG_TMP0);
    } else {
        if (cbl) {
            tcg_out_opc_imm(s, opc_addi, rl, al, bl);
            tcg_out_opc_imm(s, OPC_SLTIU, TCG_REG_TMP0, rl, bl);
        } else if (rl == al && rl == bl) {
            tcg_out_opc_imm(s, OPC_SLTI, TCG_REG_TMP0, al, 0);
            tcg_out_opc_reg3(s, opc_addi, rl, al, bl);
        } else {
            tcg_out_opc_reg3(s, opc_add, rl, al, bl);
            tcg_out_opc_reg3(s, OPC_SLTU, TCG_REG_TMP0,
                            rl, (rl == bl ? al : bl));
        }
        tcg_out_opc_reg3(s, opc_add, rh, th, TCG_REG_TMP0);
    }
}

static const struct {
    LoongarchInsn op;
    bool swap;
} tcg_brcond_to_riscv[] = {
    [TCG_COND_EQ] =  { OPC_BEQ,  false },
    [TCG_COND_NE] =  { OPC_BNE,  false },
    [TCG_COND_LT] =  { OPC_BLT,  false },
    [TCG_COND_GE] =  { OPC_BGE,  false },
    [TCG_COND_LE] =  { OPC_BGE,  true  },
    [TCG_COND_GT] =  { OPC_BLT,  true  },
    [TCG_COND_LTU] = { OPC_BLTU, false },
    [TCG_COND_GEU] = { OPC_BGEU, false },
    [TCG_COND_LEU] = { OPC_BGEU, true  },
    [TCG_COND_GTU] = { OPC_BLTU, true  }
};

static void tcg_out_brcond(TCGContext *s, TCGCond cond, TCGReg arg1,
                           TCGReg arg2, TCGLabel *l)
{
    LoongarchInsn op = tcg_brcond_to_riscv[cond].op;

    tcg_debug_assert(op != 0);

    if (tcg_brcond_to_riscv[cond].swap) {
        TCGReg t = arg1;
        arg1 = arg2;
        arg2 = t;
    }
    /* TODO: some optimization? */
    tcg_out_reloc(s, s->code_ptr, R_LOONGARCH_BRANCH, l, 0);
    tcg_out_opc_jbr(s, op, arg2, arg1, 0);
}

static void tcg_out_setcond(TCGContext *s, TCGCond cond, TCGReg ret,
                            TCGReg arg1, TCGReg arg2)
{
    switch (cond) {
    case TCG_COND_EQ:
        tcg_out_opc_reg3(s, OPC_SUB, ret, arg1, arg2);
        tcg_out_opc_imm(s, OPC_SLTIU, ret, ret, 1);
        break;
    case TCG_COND_NE:
        tcg_out_opc_reg3(s, OPC_SUB, ret, arg1, arg2);
        tcg_out_opc_reg3(s, OPC_SLTU, ret, TCG_REG_ZERO, ret);
        break;
    case TCG_COND_LT:
        tcg_out_opc_reg3(s, OPC_SLT, ret, arg1, arg2);
        break;
    case TCG_COND_GE:
        tcg_out_opc_reg3(s, OPC_SLT, ret, arg1, arg2);
        tcg_out_opc_imm(s, OPC_XORI, ret, ret, 1);
        break;
    case TCG_COND_LE:
        tcg_out_opc_reg3(s, OPC_SLT, ret, arg2, arg1);
        tcg_out_opc_imm(s, OPC_XORI, ret, ret, 1);
        break;
    case TCG_COND_GT:
        tcg_out_opc_reg3(s, OPC_SLT, ret, arg2, arg1);
        break;
    case TCG_COND_LTU:
        tcg_out_opc_reg3(s, OPC_SLTU, ret, arg1, arg2);
        break;
    case TCG_COND_GEU:
        tcg_out_opc_reg3(s, OPC_SLTU, ret, arg1, arg2);
        tcg_out_opc_imm(s, OPC_XORI, ret, ret, 1);
        break;
    case TCG_COND_LEU:
        tcg_out_opc_reg3(s, OPC_SLTU, ret, arg2, arg1);
        tcg_out_opc_imm(s, OPC_XORI, ret, ret, 1);
        break;
    case TCG_COND_GTU:
        tcg_out_opc_reg3(s, OPC_SLTU, ret, arg2, arg1);
        break;
    default:
         g_assert_not_reached();
         break;
     }
}

static void tcg_out_brcond2(TCGContext *s, TCGCond cond, TCGReg al, TCGReg ah,
                            TCGReg bl, TCGReg bh, TCGLabel *l)
{
    /* todo */
    g_assert_not_reached();
}

static void tcg_out_setcond2(TCGContext *s, TCGCond cond, TCGReg ret,
                             TCGReg al, TCGReg ah, TCGReg bl, TCGReg bh)
{
    /* todo */
    g_assert_not_reached();
}

/* TODO: ptrdiff_t? tcg_pcrel_diff */
/* TODO: tcg_target_long */
static void tcg_out_call_int(TCGContext *s, const tcg_insn_unit *arg, bool tail)
{
    TCGReg link = tail ? TCG_REG_ZERO : TCG_REG_RA;
    ptrdiff_t offset = tcg_pcrel_diff(s, arg);


    tcg_debug_assert((offset & 3) == 0);
    if (offset == sextreg(offset, 0, 28)) {
        /* short jump */
        tcg_out_opc_djp(s, tail ? OPC_B : OPC_BL, offset);
    } else if (TCG_TARGET_REG_BITS == 32 || offset == sextreg(offset, 0, 38)) {
        /* long jump: 38-bit */
        tcg_target_long imm = sextreg((tcg_target_long)offset, 0, 18);
        tcg_target_long base = ((tcg_target_long)offset - imm) >> 6;
        tcg_out_opc_upp(s, OPC_PCADDU18I, TCG_REG_TMP0, base);
        tcg_out_opc_jbr(s, OPC_JIRL, link, TCG_REG_TMP0, imm);
    } else if (TCG_TARGET_REG_BITS == 64) {
        /* far jump: 64-bit */
        tcg_target_long imm = (tcg_target_long)arg & 0xfff;
        tcg_target_long base = (tcg_target_long)arg & ~0xfff;
        tcg_out_movi(s, TCG_TYPE_PTR, TCG_REG_TMP0, base);
        tcg_out_opc_jbr(s, OPC_JIRL, link, TCG_REG_TMP0, imm);
    } else {
        g_assert_not_reached();
    }
}

static void tcg_out_call(TCGContext *s, const tcg_insn_unit *arg)
{
    tcg_out_call_int(s, arg, false);
}

/* TODO */
static void tcg_out_mb(TCGContext *s, TCGArg a0)
{
    tcg_out32(s, 0x38720000);
}

/*
 * Load/store and TLB
 */

#if defined(CONFIG_SOFTMMU)
#include "../tcg-ldst.c.inc"

/* helper signature: helper_ret_ld_mmu(CPUState *env, target_ulong addr,
 *                                     TCGMemOpIdx oi, uintptr_t ra)
 */
static void * const qemu_ld_helpers[16] = {
    [MO_UB]   = helper_ret_ldub_mmu,
    [MO_SB]   = helper_ret_ldsb_mmu,
    [MO_LEUW] = helper_le_lduw_mmu,
    [MO_LESW] = helper_le_ldsw_mmu,
    [MO_LEUL] = helper_le_ldul_mmu,
#if TCG_TARGET_REG_BITS == 64
    [MO_LESL] = helper_le_ldsl_mmu,
#endif
    [MO_LEQ]  = helper_le_ldq_mmu,
    [MO_BEUW] = helper_be_lduw_mmu,
    [MO_BESW] = helper_be_ldsw_mmu,
    [MO_BEUL] = helper_be_ldul_mmu,
#if TCG_TARGET_REG_BITS == 64
    [MO_BESL] = helper_be_ldsl_mmu,
#endif
    [MO_BEQ]  = helper_be_ldq_mmu,
};

/* helper signature: helper_ret_st_mmu(CPUState *env, target_ulong addr,
 *                                     uintxx_t val, TCGMemOpIdx oi,
 *                                     uintptr_t ra)
 */
static void * const qemu_st_helpers[16] = {
    [MO_UB]   = helper_ret_stb_mmu,
    [MO_LEUW] = helper_le_stw_mmu,
    [MO_LEUL] = helper_le_stl_mmu,
    [MO_LEQ]  = helper_le_stq_mmu,
    [MO_BEUW] = helper_be_stw_mmu,
    [MO_BEUL] = helper_be_stl_mmu,
    [MO_BEQ]  = helper_be_stq_mmu,
};

/* We don't support oversize guests */
QEMU_BUILD_BUG_ON(TCG_TARGET_REG_BITS < TARGET_LONG_BITS);

/* We expect to use a 12-bit negative offset from ENV.  */
QEMU_BUILD_BUG_ON(TLB_MASK_TABLE_OFS(0) > 0);
QEMU_BUILD_BUG_ON(TLB_MASK_TABLE_OFS(0) < -(1 << 11));
/* TODO:-1 */
static void tcg_out_goto(TCGContext *s, const tcg_insn_unit *target)
{
    tcg_out_opc_djp(s, OPC_B, 0);
    bool ok = reloc_djimm28(s->code_ptr - 1, target);
    tcg_debug_assert(ok);
}

static void tcg_out_tlb_load(TCGContext *s, TCGReg addrl,
                             TCGReg addrh, TCGMemOpIdx oi,
                             tcg_insn_unit **label_ptr, bool is_load)
{
    MemOp opc = get_memop(oi);
    unsigned s_bits = opc & MO_SIZE;
    unsigned a_bits = get_alignment_bits(opc);
    tcg_target_long compare_mask;
    int mem_index = get_mmuidx(oi);
    int fast_ofs = TLB_MASK_TABLE_OFS(mem_index);
    int mask_ofs = fast_ofs + offsetof(CPUTLBDescFast, mask);
    int table_ofs = fast_ofs + offsetof(CPUTLBDescFast, table);
    TCGReg mask_base = TCG_AREG0, table_base = TCG_AREG0;

    tcg_out_ld(s, TCG_TYPE_PTR, TCG_REG_TMP0, mask_base, mask_ofs);
    tcg_out_ld(s, TCG_TYPE_PTR, TCG_REG_TMP1, table_base, table_ofs);

    tcg_out_opc_shift(s, OPC_SRLI, TCG_REG_TMP2, addrl,
                    TARGET_PAGE_BITS - CPU_TLB_ENTRY_BITS);
    tcg_out_opc_reg3(s, OPC_AND, TCG_REG_TMP2, TCG_REG_TMP2, TCG_REG_TMP0);
    tcg_out_opc_reg3(s, OPC_ADD, TCG_REG_TMP2, TCG_REG_TMP2, TCG_REG_TMP1);

    /* Load the tlb comparator and the addend.  */
    tcg_out_ld(s, TCG_TYPE_TL, TCG_REG_TMP0, TCG_REG_TMP2,
               is_load ? offsetof(CPUTLBEntry, addr_read)
               : offsetof(CPUTLBEntry, addr_write));
    tcg_out_ld(s, TCG_TYPE_PTR, TCG_REG_TMP2, TCG_REG_TMP2,
               offsetof(CPUTLBEntry, addend));

    /* We don't support unaligned accesses. */
    if (a_bits < s_bits) {
        a_bits = s_bits;
    }
    /* Clear the non-page, non-alignment bits from the address.  */
    compare_mask = (tcg_target_long)TARGET_PAGE_MASK | ((1 << a_bits) - 1);
    if (compare_mask == sextreg(compare_mask, 0, 12)) {
        tcg_out_opc_imm(s, OPC_ANDI, TCG_REG_TMP1, addrl, compare_mask);
    } else {
        tcg_out_movi(s, TCG_TYPE_TL, TCG_REG_TMP1, compare_mask);
        tcg_out_opc_reg3(s, OPC_AND, TCG_REG_TMP1, TCG_REG_TMP1, addrl);
    }

    /* Compare masked address with the TLB entry. */
    label_ptr[0] = s->code_ptr;
    tcg_out_opc_jbr(s, OPC_BNE, TCG_REG_TMP0, TCG_REG_TMP1, 0);

    /* TLB Hit - translate address using addend.  */
    if (TCG_TARGET_REG_BITS > TARGET_LONG_BITS) {
        tcg_out_ext32u(s, TCG_REG_TMP0, addrl);
        addrl = TCG_REG_TMP0;
    }
    tcg_out_opc_reg3(s, OPC_ADD, TCG_REG_TMP0, TCG_REG_TMP2, addrl);
}

static void add_qemu_ldst_label(TCGContext *s, int is_ld, TCGMemOpIdx oi,
                                TCGType ext,
                                TCGReg datalo, TCGReg datahi,
                                TCGReg addrlo, TCGReg addrhi,
                                void *raddr, tcg_insn_unit **label_ptr)
{
    TCGLabelQemuLdst *label = new_ldst_label(s);

    label->is_ld = is_ld;
    label->oi = oi;
    label->type = ext;
    label->datalo_reg = datalo;
    label->datahi_reg = datahi;
    label->addrlo_reg = addrlo;
    label->addrhi_reg = addrhi;
    label->raddr = tcg_splitwx_to_rx(raddr);
    label->label_ptr[0] = label_ptr[0];
}

static bool tcg_out_qemu_ld_slow_path(TCGContext *s, TCGLabelQemuLdst *l)
{
    TCGMemOpIdx oi = l->oi;
    MemOp opc = get_memop(oi);
    TCGReg a0 = tcg_target_call_iarg_regs[0];
    TCGReg a1 = tcg_target_call_iarg_regs[1];
    TCGReg a2 = tcg_target_call_iarg_regs[2];
    TCGReg a3 = tcg_target_call_iarg_regs[3];

    /* We don't support oversize guests */
    if (TCG_TARGET_REG_BITS < TARGET_LONG_BITS) {
        g_assert_not_reached();
    }

    /* resolve label address */
    if (!reloc_jbimm18(l->label_ptr[0], tcg_splitwx_to_rx(s->code_ptr))) {
        return false;
    }

    /* call load helper */
    tcg_out_mov(s, TCG_TYPE_PTR, a0, TCG_AREG0);
    tcg_out_mov(s, TCG_TYPE_PTR, a1, l->addrlo_reg);
    tcg_out_movi(s, TCG_TYPE_PTR, a2, oi);
    tcg_out_movi(s, TCG_TYPE_PTR, a3, (tcg_target_long)l->raddr);

    tcg_out_call(s, qemu_ld_helpers[opc & (MO_BSWAP | MO_SSIZE)]);
    tcg_out_mov(s, (opc & MO_SIZE) == MO_64, l->datalo_reg, a0);

    tcg_out_goto(s, l->raddr);
    return true;
}

static bool tcg_out_qemu_st_slow_path(TCGContext *s, TCGLabelQemuLdst *l)
{
    TCGMemOpIdx oi = l->oi;
    MemOp opc = get_memop(oi);
    MemOp s_bits = opc & MO_SIZE;
    TCGReg a0 = tcg_target_call_iarg_regs[0];
    TCGReg a1 = tcg_target_call_iarg_regs[1];
    TCGReg a2 = tcg_target_call_iarg_regs[2];
    TCGReg a3 = tcg_target_call_iarg_regs[3];
    TCGReg a4 = tcg_target_call_iarg_regs[4];

    /* We don't support oversize guests */
    if (TCG_TARGET_REG_BITS < TARGET_LONG_BITS) {
        g_assert_not_reached();
    }

    /* resolve label address */
    if (!reloc_jbimm18(l->label_ptr[0], tcg_splitwx_to_rx(s->code_ptr))) {
        return false;
    }

    /* call store helper */
    tcg_out_mov(s, TCG_TYPE_PTR, a0, TCG_AREG0);
    tcg_out_mov(s, TCG_TYPE_PTR, a1, l->addrlo_reg);
    tcg_out_mov(s, TCG_TYPE_PTR, a2, l->datalo_reg);
    switch (s_bits) {
    case MO_8:
        tcg_out_ext8u(s, a2, a2);
        break;
    case MO_16:
        tcg_out_ext16u(s, a2, a2);
        break;
    default:
        break;
    }
    tcg_out_movi(s, TCG_TYPE_PTR, a3, oi);
    tcg_out_movi(s, TCG_TYPE_PTR, a4, (tcg_target_long)l->raddr);

    tcg_out_call(s, qemu_st_helpers[opc & (MO_BSWAP | MO_SSIZE)]);

    tcg_out_goto(s, l->raddr);
    return true;
}
#endif /* CONFIG_SOFTMMU */

static void tcg_out_qemu_ld_direct(TCGContext *s, TCGReg lo, TCGReg hi,
                                   TCGReg base, MemOp opc, bool is_64)
{
    const MemOp bswap = opc & MO_BSWAP;

    /* We don't yet handle byteswapping, assert */
    g_assert(!bswap);

    switch (opc & (MO_SSIZE)) {
    case MO_UB:
        tcg_out_opc_imm(s, OPC_LDBU, lo, base, 0);
        break;
    case MO_SB:
        tcg_out_opc_imm(s, OPC_LDB, lo, base, 0);
        break;
    case MO_UW:
        tcg_out_opc_imm(s, OPC_LDHU, lo, base, 0);
        break;
    case MO_SW:
        tcg_out_opc_imm(s, OPC_LDH, lo, base, 0);
        break;
    case MO_UL:
        if (TCG_TARGET_REG_BITS == 64 && is_64) {
            tcg_out_opc_imm(s, OPC_LDWU, lo, base, 0);
            break;
        }
        /* FALLTHRU */
    case MO_SL:
        tcg_out_opc_imm(s, OPC_LDW, lo, base, 0);
        break;
    case MO_Q:
        /* Prefer to load from offset 0 first, but allow for overlap.  */
        if (TCG_TARGET_REG_BITS == 64) {
            tcg_out_opc_imm(s, OPC_LDD, lo, base, 0);
        } else if (lo != base) {
            tcg_out_opc_imm(s, OPC_LDW, lo, base, 0);
            tcg_out_opc_imm(s, OPC_LDW, hi, base, 4);
        } else {
            tcg_out_opc_imm(s, OPC_LDW, hi, base, 4);
            tcg_out_opc_imm(s, OPC_LDW, lo, base, 0);
        }
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_qemu_ld(TCGContext *s, const TCGArg *args, bool is_64)
{
    TCGReg addr_regl, addr_regh __attribute__((unused));
    TCGReg data_regl, data_regh;
    TCGMemOpIdx oi;
    MemOp opc;
#if defined(CONFIG_SOFTMMU)
    tcg_insn_unit *label_ptr[1];
#endif
    TCGReg base = TCG_REG_TMP0;

    data_regl = *args++;
    data_regh = (TCG_TARGET_REG_BITS == 32 && is_64 ? *args++ : 0);
    addr_regl = *args++;
    addr_regh = (TCG_TARGET_REG_BITS < TARGET_LONG_BITS ? *args++ : 0);
    oi = *args++;
    opc = get_memop(oi);

#if defined(CONFIG_SOFTMMU)
    tcg_out_tlb_load(s, addr_regl, addr_regh, oi, label_ptr, 1);
    tcg_out_qemu_ld_direct(s, data_regl, data_regh, base, opc, is_64);
    add_qemu_ldst_label(s, 1, oi,
                        (is_64 ? TCG_TYPE_I64 : TCG_TYPE_I32),
                        data_regl, data_regh, addr_regl, addr_regh,
                        s->code_ptr, label_ptr);
#else
    /*
     * 32bit X86 binary
     * (gdb) p data_regl
     * $9 = TCG_REG_S3
     * (gdb) p data_regh
     * $10 = TCG_REG_ZERO
     * (gdb) p addr_regh
     * $11 = TCG_REG_ZERO
     * (gdb) p addr_regl
     * $12 = TCG_REG_S2
     */
    if (TCG_TARGET_REG_BITS > TARGET_LONG_BITS) {
        tcg_out_ext32u(s, base, addr_regl);
        addr_regl = base;
    }

    if (guest_base == 0) {
        tcg_out_opc_reg3(s, OPC_ADD, base, addr_regl, TCG_REG_ZERO);
    } else {
        tcg_out_opc_reg3(s, OPC_ADD, base, TCG_GUEST_BASE_REG, addr_regl);
    }
    tcg_out_qemu_ld_direct(s, data_regl, data_regh, base, opc, is_64);
#endif
}

static void tcg_out_qemu_st_direct(TCGContext *s, TCGReg lo, TCGReg hi,
                                   TCGReg base, MemOp opc)
{
    const MemOp bswap = opc & MO_BSWAP;

    /* We don't yet handle byteswapping, assert */
    g_assert(!bswap);

    switch (opc & (MO_SSIZE)) {
    case MO_8:
        tcg_out_opc_imm(s, OPC_STB, lo, base, 0);
        break;
    case MO_16:
        tcg_out_opc_imm(s, OPC_STH, lo, base, 0);
        break;
    case MO_32:
        tcg_out_opc_imm(s, OPC_STW, lo, base, 0);
        break;
    case MO_64:
        if (TCG_TARGET_REG_BITS == 64) {
            tcg_out_opc_imm(s, OPC_STD, lo, base, 0);
        } else {
            tcg_out_opc_imm(s, OPC_STW, lo, base, 0);
            tcg_out_opc_imm(s, OPC_STW, hi, base, 4);
        }
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_qemu_st(TCGContext *s, const TCGArg *args, bool is_64)
{
    TCGReg addr_regl, addr_regh __attribute__((unused));
    TCGReg data_regl, data_regh;
    TCGMemOpIdx oi;
    MemOp opc;
#if defined(CONFIG_SOFTMMU)
    tcg_insn_unit *label_ptr[1];
#endif
    TCGReg base = TCG_REG_TMP0;

    data_regl = *args++;
    data_regh = (TCG_TARGET_REG_BITS == 32 && is_64 ? *args++ : 0);
    addr_regl = *args++;
    addr_regh = (TCG_TARGET_REG_BITS < TARGET_LONG_BITS ? *args++ : 0);
    oi = *args++;
    opc = get_memop(oi);

#if defined(CONFIG_SOFTMMU)
    tcg_out_tlb_load(s, addr_regl, addr_regh, oi, label_ptr, 0);
    tcg_out_qemu_st_direct(s, data_regl, data_regh, base, opc);
    add_qemu_ldst_label(s, 0, oi,
                        (is_64 ? TCG_TYPE_I64 : TCG_TYPE_I32),
                        data_regl, data_regh, addr_regl, addr_regh,
                        s->code_ptr, label_ptr);
#else
    if (TCG_TARGET_REG_BITS > TARGET_LONG_BITS) {
        tcg_out_ext32u(s, base, addr_regl);
        addr_regl = base;
    }

    if (guest_base == 0) {
        tcg_out_opc_reg3(s, OPC_ADD, base, addr_regl, TCG_REG_ZERO);
    } else {
        tcg_out_opc_reg3(s, OPC_ADD, base, TCG_GUEST_BASE_REG, addr_regl);
    }
    tcg_out_qemu_st_direct(s, data_regl, data_regh, base, opc);
#endif
}

static const tcg_insn_unit *tb_ret_addr;

static void tcg_out_op(TCGContext *s, TCGOpcode opc,
                       const TCGArg args[TCG_MAX_OP_ARGS],
                       const int const_args[TCG_MAX_OP_ARGS])
{
    TCGArg a0 = args[0];
    TCGArg a1 = args[1];
    TCGArg a2 = args[2];
    int c2 = const_args[2];

    switch (opc) {
    case INDEX_op_exit_tb:
        /* Reuse the zeroing that exists for goto_ptr.  */
        if (a0 == 0) {
            tcg_out_call_int(s, tcg_code_gen_epilogue, true);
        } else {
            tcg_out_movi(s, TCG_TYPE_PTR, TCG_REG_A0, a0);
            tcg_out_call_int(s, tb_ret_addr, true);
        }
        break;

    case INDEX_op_goto_tb:
        if (s->tb_jmp_insn_offset) {
            /* direct jump method */
            s->tb_jmp_insn_offset[a0] = tcg_current_code_size(s);
            /* actual branch destination will be patched by
               tb_target_set_jmp_target later. */
            tcg_out_opc_djp(s, OPC_B, 0);
        } else {
            assert(s->tb_jmp_insn_offset == 0);
            /* indirect jump method */
            tcg_out_ld(s, TCG_TYPE_PTR, TCG_REG_TMP0, TCG_REG_ZERO,
                       (uintptr_t)(s->tb_jmp_target_addr + a0));
            tcg_out_opc_jbr(s, OPC_JIRL, TCG_REG_ZERO, TCG_REG_TMP0, 0);
        }
        set_jmp_reset_offset(s, a0);
        break;

    case INDEX_op_goto_ptr:
        tcg_out_opc_jbr(s, OPC_JIRL, TCG_REG_ZERO, a0, 0);
        break;

    case INDEX_op_br:
        tcg_out_reloc(s, s->code_ptr, R_LOONGARCH_JUMP, arg_label(a0), 0);
        tcg_out_opc_djp(s, OPC_B, 0);
        break;

    case INDEX_op_ld8u_i32:
    case INDEX_op_ld8u_i64:
        tcg_out_ldst(s, OPC_LDBU, a0, a1, a2);
        break;
    case INDEX_op_ld8s_i32:
    case INDEX_op_ld8s_i64:
        tcg_out_ldst(s, OPC_LDB, a0, a1, a2);
        break;
    case INDEX_op_ld16u_i32:
    case INDEX_op_ld16u_i64:
        tcg_out_ldst(s, OPC_LDHU, a0, a1, a2);
        break;
    case INDEX_op_ld16s_i32:
    case INDEX_op_ld16s_i64:
        tcg_out_ldst(s, OPC_LDH, a0, a1, a2);
        break;
    case INDEX_op_ld32u_i64:
        tcg_out_ldst(s, OPC_LDWU, a0, a1, a2);
        break;
    case INDEX_op_ld_i32:
    case INDEX_op_ld32s_i64:
        tcg_out_ldst(s, OPC_LDW, a0, a1, a2);
        break;
    case INDEX_op_ld_i64:
        tcg_out_ldst(s, OPC_LDD, a0, a1, a2);
        break;

    case INDEX_op_st8_i32:
    case INDEX_op_st8_i64:
        tcg_out_ldst(s, OPC_STB, a0, a1, a2);
        break;
    case INDEX_op_st16_i32:
    case INDEX_op_st16_i64:
        tcg_out_ldst(s, OPC_STH, a0, a1, a2);
        break;
    case INDEX_op_st_i32:
    case INDEX_op_st32_i64:
        tcg_out_ldst(s, OPC_STW, a0, a1, a2);
        break;
    case INDEX_op_st_i64:
        tcg_out_ldst(s, OPC_STD, a0, a1, a2);
        break;

    case INDEX_op_add_i32:
        if (c2) {
            tcg_out_opc_imm(s, OPC_ADDIW, a0, a1, a2);
        } else {
            tcg_out_opc_reg3(s, OPC_ADDW, a0, a1, a2);
        }
        break;
    case INDEX_op_add_i64:
        if (c2) {
            tcg_out_opc_imm(s, OPC_ADDI, a0, a1, a2);
        } else {
            tcg_out_opc_reg3(s, OPC_ADD, a0, a1, a2);
        }
        break;

    case INDEX_op_sub_i32:
        if (c2) {
            tcg_out_opc_imm(s, OPC_ADDIW, a0, a1, -a2);
        } else {
            tcg_out_opc_reg3(s, OPC_SUBW, a0, a1, a2);
        }
        break;
    case INDEX_op_sub_i64:
        if (c2) {
            tcg_out_opc_imm(s, OPC_ADDI, a0, a1, -a2);
        } else {
            tcg_out_opc_reg3(s, OPC_SUB, a0, a1, a2);
        }
        break;

    case INDEX_op_and_i32:
    case INDEX_op_and_i64:
        if (c2) {
            tcg_out_opc_imm(s, OPC_ANDI, a0, a1, a2);
        } else {
            tcg_out_opc_reg3(s, OPC_AND, a0, a1, a2);
        }
        break;

    case INDEX_op_or_i32:
    case INDEX_op_or_i64:
        if (c2) {
            tcg_out_opc_imm(s, OPC_ORI, a0, a1, a2);
        } else {
            tcg_out_opc_reg3(s, OPC_OR, a0, a1, a2);
        }
        break;

    case INDEX_op_xor_i32:
    case INDEX_op_xor_i64:
        if (c2) {
            tcg_out_opc_imm(s, OPC_XORI, a0, a1, a2);
        } else {
            tcg_out_opc_reg3(s, OPC_XOR, a0, a1, a2);
        }
        break;

    case INDEX_op_not_i32:
    case INDEX_op_not_i64:
        tcg_out_opc_reg3(s, OPC_NOR, a0, a1, a1);
        break;

    case INDEX_op_neg_i32:
        tcg_out_opc_reg3(s, OPC_SUBW, a0, TCG_REG_ZERO, a1);
        break;
    case INDEX_op_neg_i64:
        tcg_out_opc_reg3(s, OPC_SUB, a0, TCG_REG_ZERO, a1);
        break;

    case INDEX_op_mul_i32:
        tcg_out_opc_reg3(s, OPC_MULW, a0, a1, a2);
        break;
    case INDEX_op_mul_i64:
        tcg_out_opc_reg3(s, OPC_MUL, a0, a1, a2);
        break;

    case INDEX_op_div_i32:
        tcg_out_opc_reg3(s, OPC_DIVW, a0, a1, a2);
        break;
    case INDEX_op_div_i64:
        tcg_out_opc_reg3(s, OPC_DIV, a0, a1, a2);
        break;

    case INDEX_op_divu_i32:
        tcg_out_opc_reg3(s, OPC_DIVWU, a0, a1, a2);
        break;
    case INDEX_op_divu_i64:
        tcg_out_opc_reg3(s, OPC_DIVU, a0, a1, a2);
        break;

    case INDEX_op_rem_i32:
        tcg_out_opc_reg3(s, OPC_MODW, a0, a1, a2);
        break;
    case INDEX_op_rem_i64:
        tcg_out_opc_reg3(s, OPC_MOD, a0, a1, a2);
        break;

    case INDEX_op_remu_i32:
        tcg_out_opc_reg3(s, OPC_MODWU, a0, a1, a2);
        break;
    case INDEX_op_remu_i64:
        tcg_out_opc_reg3(s, OPC_MODU, a0, a1, a2);
        break;

    case INDEX_op_shl_i32:
        if (c2) {
            tcg_out_opc_shift(s, OPC_SLLIW, a0, a1, a2 & 0x1f);
        } else {
            tcg_out_opc_reg3(s, OPC_SLLW, a0, a1, a2);
        }
        break;
    case INDEX_op_shl_i64:
        if (c2) {
            tcg_out_opc_shift(s, OPC_SLLI, a0, a1, a2 & 0x3f);
        } else {
            tcg_out_opc_reg3(s, OPC_SLL, a0, a1, a2);
        }
        break;

    case INDEX_op_shr_i32:
        if (c2) {
            tcg_out_opc_shift(s, OPC_SRLIW, a0, a1, a2 & 0x1f);
        } else {
            tcg_out_opc_reg3(s, OPC_SRLW, a0, a1, a2);
        }
        break;
    case INDEX_op_shr_i64:
        if (c2) {
            tcg_out_opc_shift(s, OPC_SRLI, a0, a1, a2 & 0x3f);
        } else {
            tcg_out_opc_reg3(s, OPC_SRL, a0, a1, a2);
        }
        break;

    case INDEX_op_sar_i32:
        if (c2) {
            tcg_out_opc_shift(s, OPC_SRAIW, a0, a1, a2 & 0x1f);
        } else {
            tcg_out_opc_reg3(s, OPC_SRAW, a0, a1, a2);
        }
        break;
    case INDEX_op_sar_i64:
        if (c2) {
            tcg_out_opc_shift(s, OPC_SRAI, a0, a1, a2 & 0x3f);
        } else {
            tcg_out_opc_reg3(s, OPC_SRA, a0, a1, a2);
        }
        break;

    case INDEX_op_add2_i32:
        tcg_out_addsub2(s, a0, a1, a2, args[3], args[4], args[5],
                        const_args[4], const_args[5], false, true);
        break;
    case INDEX_op_add2_i64:
        tcg_out_addsub2(s, a0, a1, a2, args[3], args[4], args[5],
                        const_args[4], const_args[5], false, false);
        break;
    case INDEX_op_sub2_i32:
        tcg_out_addsub2(s, a0, a1, a2, args[3], args[4], args[5],
                        const_args[4], const_args[5], true, true);
        break;
    case INDEX_op_sub2_i64:
        tcg_out_addsub2(s, a0, a1, a2, args[3], args[4], args[5],
                        const_args[4], const_args[5], true, false);
        break;

    case INDEX_op_brcond_i32:
    case INDEX_op_brcond_i64:
        tcg_out_brcond(s, a2, a0, a1, arg_label(args[3]));
        break;
    case INDEX_op_brcond2_i32:
        tcg_out_brcond2(s, args[4], a0, a1, a2, args[3], arg_label(args[5]));
        break;

    case INDEX_op_setcond_i32:
    case INDEX_op_setcond_i64:
        tcg_out_setcond(s, args[3], a0, a1, a2);
        break;
    case INDEX_op_setcond2_i32:
        tcg_out_setcond2(s, args[5], a0, a1, a2, args[3], args[4]);
        break;

    case INDEX_op_qemu_ld_i32:
        tcg_out_qemu_ld(s, args, false);
        break;
    case INDEX_op_qemu_ld_i64:
        tcg_out_qemu_ld(s, args, true);
        break;
    case INDEX_op_qemu_st_i32:
        tcg_out_qemu_st(s, args, false);
        break;
    case INDEX_op_qemu_st_i64:
        tcg_out_qemu_st(s, args, true);
        break;

    case INDEX_op_ext8u_i32:
    case INDEX_op_ext8u_i64:
        tcg_out_ext8u(s, a0, a1);
        break;

    case INDEX_op_ext16u_i32:
    case INDEX_op_ext16u_i64:
        tcg_out_ext16u(s, a0, a1);
        break;

    case INDEX_op_ext32u_i64:
    case INDEX_op_extu_i32_i64:
        tcg_out_ext32u(s, a0, a1);
        break;

    case INDEX_op_ext8s_i32:
    case INDEX_op_ext8s_i64:
        tcg_out_ext8s(s, a0, a1);
        break;

    case INDEX_op_ext16s_i32:
    case INDEX_op_ext16s_i64:
        tcg_out_ext16s(s, a0, a1);
        break;

    case INDEX_op_ext32s_i64:
    case INDEX_op_extrl_i64_i32:
    case INDEX_op_ext_i32_i64:
        tcg_out_ext32s(s, a0, a1);
        break;

    case INDEX_op_extrh_i64_i32:
        tcg_out_opc_shift(s, OPC_SRAI, a0, a1, 32);
        break;

    case INDEX_op_mulsh_i32:
    case INDEX_op_mulsh_i64:
        tcg_out_opc_reg3(s, OPC_MULH, a0, a1, a2);
        break;

    case INDEX_op_muluh_i32:
    case INDEX_op_muluh_i64:
        tcg_out_opc_reg3(s, OPC_MULHU, a0, a1, a2);
        break;

    case INDEX_op_mb:
        tcg_out_mb(s, a0);
        break;

    case INDEX_op_mov_i32:  /* Always emitted via tcg_out_mov.  */
    case INDEX_op_mov_i64:
    case INDEX_op_call:     /* Always emitted via tcg_out_call.  */
    default:
        g_assert_not_reached();
    }
}

static TCGConstraintSetIndex tcg_target_op_def(TCGOpcode op)
{
    switch (op) {
    case INDEX_op_goto_ptr:
        return C_O0_I1(r);

    case INDEX_op_ld8u_i32:
    case INDEX_op_ld8s_i32:
    case INDEX_op_ld16u_i32:
    case INDEX_op_ld16s_i32:
    case INDEX_op_ld_i32:
    case INDEX_op_not_i32:
    case INDEX_op_neg_i32:
    case INDEX_op_ld8u_i64:
    case INDEX_op_ld8s_i64:
    case INDEX_op_ld16u_i64:
    case INDEX_op_ld16s_i64:
    case INDEX_op_ld32s_i64:
    case INDEX_op_ld32u_i64:
    case INDEX_op_ld_i64:
    case INDEX_op_not_i64:
    case INDEX_op_neg_i64:
    case INDEX_op_ext8u_i32:
    case INDEX_op_ext8u_i64:
    case INDEX_op_ext16u_i32:
    case INDEX_op_ext16u_i64:
    case INDEX_op_ext32u_i64:
    case INDEX_op_extu_i32_i64:
    case INDEX_op_ext8s_i32:
    case INDEX_op_ext8s_i64:
    case INDEX_op_ext16s_i32:
    case INDEX_op_ext16s_i64:
    case INDEX_op_ext32s_i64:
    case INDEX_op_extrl_i64_i32:
    case INDEX_op_extrh_i64_i32:
    case INDEX_op_ext_i32_i64:
        return C_O1_I1(r, r);

    case INDEX_op_st8_i32:
    case INDEX_op_st16_i32:
    case INDEX_op_st_i32:
    case INDEX_op_st8_i64:
    case INDEX_op_st16_i64:
    case INDEX_op_st32_i64:
    case INDEX_op_st_i64:
        return C_O0_I2(rZ, r);

    case INDEX_op_and_i32:
    case INDEX_op_or_i32:
    case INDEX_op_xor_i32:
    case INDEX_op_and_i64:
    case INDEX_op_or_i64:
    case INDEX_op_xor_i64:
        return C_O1_I2(r, r, rU);

    case INDEX_op_add_i32:
    case INDEX_op_add_i64:
        return C_O1_I2(r, r, rI);

    case INDEX_op_sub_i32:
    case INDEX_op_sub_i64:
        return C_O1_I2(r, rZ, rN);

    case INDEX_op_mul_i32:
    case INDEX_op_mulsh_i32:
    case INDEX_op_muluh_i32:
    case INDEX_op_div_i32:
    case INDEX_op_divu_i32:
    case INDEX_op_rem_i32:
    case INDEX_op_remu_i32:
    case INDEX_op_setcond_i32:
    case INDEX_op_mul_i64:
    case INDEX_op_mulsh_i64:
    case INDEX_op_muluh_i64:
    case INDEX_op_div_i64:
    case INDEX_op_divu_i64:
    case INDEX_op_rem_i64:
    case INDEX_op_remu_i64:
    case INDEX_op_setcond_i64:
        return C_O1_I2(r, rZ, rZ);

    case INDEX_op_shl_i32:
    case INDEX_op_shr_i32:
    case INDEX_op_sar_i32:
    case INDEX_op_shl_i64:
    case INDEX_op_shr_i64:
    case INDEX_op_sar_i64:
        return C_O1_I2(r, r, ri);

    case INDEX_op_brcond_i32:
    case INDEX_op_brcond_i64:
        return C_O0_I2(rZ, rZ);

    case INDEX_op_add2_i32:
    case INDEX_op_add2_i64:
    case INDEX_op_sub2_i32:
    case INDEX_op_sub2_i64:
        return C_O2_I4(r, r, rZ, rZ, rM, rM);

    case INDEX_op_brcond2_i32:
        return C_O0_I4(rZ, rZ, rZ, rZ);

    case INDEX_op_setcond2_i32:
        return C_O1_I4(r, rZ, rZ, rZ, rZ);

    case INDEX_op_qemu_ld_i32:
        return (TARGET_LONG_BITS <= TCG_TARGET_REG_BITS
                ? C_O1_I1(r, L) : C_O1_I2(r, L, L));
    case INDEX_op_qemu_st_i32:
        return (TARGET_LONG_BITS <= TCG_TARGET_REG_BITS
                ? C_O0_I2(LZ, L) : C_O0_I3(LZ, L, L));
    case INDEX_op_qemu_ld_i64:
        return (TCG_TARGET_REG_BITS == 64 ? C_O1_I1(r, L)
               : TARGET_LONG_BITS <= TCG_TARGET_REG_BITS ? C_O2_I1(r, r, L)
               : C_O2_I2(r, r, L, L));
    case INDEX_op_qemu_st_i64:
        return (TCG_TARGET_REG_BITS == 64 ? C_O0_I2(LZ, L)
               : TARGET_LONG_BITS <= TCG_TARGET_REG_BITS ? C_O0_I3(LZ, LZ, L)
               : C_O0_I4(LZ, LZ, L, L));

    default:
        g_assert_not_reached();
    }
}

static const int tcg_target_callee_save_regs[] = {
    TCG_REG_FP,       /* used for the global env (TCG_AREG0) */
    TCG_REG_S0,
    TCG_REG_S1,
    TCG_REG_S2,
    TCG_REG_S3,
    TCG_REG_S4,
    TCG_REG_S5,
    TCG_REG_S6,
    TCG_REG_S7,
    TCG_REG_S8,
    TCG_REG_RA,       /* should be last for ABI compliance */
};

/* Stack frame parameters.  */
#define REG_SIZE   (TCG_TARGET_REG_BITS / 8)
#define SAVE_SIZE  ((int)ARRAY_SIZE(tcg_target_callee_save_regs) * REG_SIZE)
#define TEMP_SIZE  (CPU_TEMP_BUF_NLONGS * (int)sizeof(long))
#define FRAME_SIZE ((TCG_STATIC_CALL_ARGS_SIZE + TEMP_SIZE + SAVE_SIZE \
                     + TCG_TARGET_STACK_ALIGN - 1) \
                    & -TCG_TARGET_STACK_ALIGN)
#define SAVE_OFS   (TCG_STATIC_CALL_ARGS_SIZE + TEMP_SIZE)

/* We're expecting to be able to use an immediate for frame allocation.  */
QEMU_BUILD_BUG_ON(FRAME_SIZE > 0x7ff);

#ifdef CONFIG_LATX
#include "latx-config.h"
#include "latx-options.h"

/* Generate global QEMU prologue and epilogue code */
static void tcg_target_qemu_prologue(TCGContext *s)
{

    int inst_nr = 0;

    latx_interpret_glue_init();
// --------------------------------- prologue ------------------------------- //
    inst_nr = target_latx_prologue(s->code_ptr);
    if (option_dump)
        qemu_log("[prologue] bt to native : %p, size = %d\n", s->code_ptr, inst_nr * 4);
    s->code_ptr += inst_nr;


// --------------------------------- epilogue ------------------------------- //
    inst_nr = target_latx_epilogue(s->code_ptr);
    if (option_dump)
        qemu_log("[epilogue] native to bt %p, size = %d\n", s->code_ptr, inst_nr * 4);
    tcg_code_gen_epilogue = s->code_ptr;
    s->code_ptr += inst_nr;


// --------------------------------- fpu rotate------------------------------ //
    inst_nr = target_latx_fpu_rotate(s->code_ptr);
    s->code_ptr += inst_nr;
#if defined(CONFIG_LATX_TBMINI_ENABLE)
    if (option_split_tb) {
        s->code_ptr += qemu_icache_linesize / 4;
    }
#endif

}
#else
/* Generate global QEMU prologue and epilogue code */
static void tcg_target_qemu_prologue(TCGContext *s)
{
    int i;

    tcg_set_frame(s, TCG_REG_SP, TCG_STATIC_CALL_ARGS_SIZE, TEMP_SIZE);

    /* TB prologue */
    tcg_out_opc_imm(s, OPC_ADDI, TCG_REG_SP, TCG_REG_SP, -FRAME_SIZE);
    for (i = 0; i < ARRAY_SIZE(tcg_target_callee_save_regs); i++) {
        tcg_out_st(s, TCG_TYPE_REG, tcg_target_callee_save_regs[i],
                   TCG_REG_SP, SAVE_OFS + i * REG_SIZE);
    }

#if !defined(CONFIG_SOFTMMU)
    tcg_out_movi(s, TCG_TYPE_PTR, TCG_GUEST_BASE_REG, guest_base);
    tcg_regset_set_reg(s->reserved_regs, TCG_GUEST_BASE_REG);
#endif

    /* Call generated code */
    tcg_out_mov(s, TCG_TYPE_PTR, TCG_AREG0, tcg_target_call_iarg_regs[0]);
    tcg_out_opc_jbr(s, OPC_JIRL, TCG_REG_ZERO, tcg_target_call_iarg_regs[1], 0);

    /* Return path for goto_ptr. Set return value to 0 */
    tcg_code_gen_epilogue = tcg_splitwx_to_rx(s->code_ptr);
    tcg_out_mov(s, TCG_TYPE_REG, TCG_REG_A0, TCG_REG_ZERO);

    /* TB epilogue */
    tb_ret_addr = tcg_splitwx_to_rx(s->code_ptr);
    for (i = 0; i < ARRAY_SIZE(tcg_target_callee_save_regs); i++) {
        tcg_out_ld(s, TCG_TYPE_REG, tcg_target_callee_save_regs[i],
                   TCG_REG_SP, SAVE_OFS + i * REG_SIZE);
    }

    tcg_out_opc_imm(s, OPC_ADDI, TCG_REG_SP, TCG_REG_SP, FRAME_SIZE);
    tcg_out_opc_jbr(s, OPC_JIRL, TCG_REG_ZERO, TCG_REG_RA, 0);
}
#endif

static void tcg_target_init(TCGContext *s)
{
    tcg_target_available_regs[TCG_TYPE_I32] = 0xffffffff;
    if (TCG_TARGET_REG_BITS == 64) {
        tcg_target_available_regs[TCG_TYPE_I64] = 0xffffffff;
    }

    tcg_target_call_clobber_regs = -1u;
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_FP);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_S0);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_S1);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_S2);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_S3);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_S4);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_S5);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_S6);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_S7);
    tcg_regset_reset_reg(tcg_target_call_clobber_regs, TCG_REG_S8);

    s->reserved_regs = 0;
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_ZERO);
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_TMP0);
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_TMP1);
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_TMP2);
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_SP);
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_X);
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_TP);
}

#ifdef CONFIG_LATX_XCOMISX_OPT
void tb_target_set_nop(uintptr_t tc_ptr, uintptr_t jmp_rx,
                       uintptr_t jmp_rw, uintptr_t addr)
{
    tcg_insn_unit nop;
    nop = 0x03400000;
#ifdef CONFIG_LATX_LARGE_CC
    uint64_t pair = (uint64_t)nop << 32 | nop;
    qatomic_set((uint64_t *)jmp_rw, pair);
    flush_idcache_range(jmp_rx, jmp_rw, 8);
#else
    qatomic_set((uint32_t *)jmp_rw, nop);
    flush_idcache_range(jmp_rx, jmp_rw, 4);
#endif
}
#endif

void tb_target_set_jmp_target(uintptr_t tc_ptr, uintptr_t jmp_rx,
                              uintptr_t jmp_rw, uintptr_t addr)
{
    ptrdiff_t offset = addr - jmp_rx;

#ifdef CONFIG_LATX_LARGE_CC
    tcg_insn_unit i1, i2;
    if (offset == sextreg(offset, 0, 28)) {
        i1 = encode_dj(OPC_B, offset);
        i2 = 0x03400000; /* NOP */
    } else {
        offset >>= 2;

        ptrdiff_t upper, lower;
        upper = ((offset + (1 << 15)) >> 16) & 0xfffff;
        lower = (offset & 0xffff);
        /* pcaddu18i */
        i1 = 0x1e000000 | upper << 5 | TCG_REG_T0;
        /* jirl */
        i2 = 0x4c000000 | lower << 10 | TCG_REG_T0 << 5;
    }
    uint64_t pair = (uint64_t)i2 << 32 | i1;
    *(uint64_t *)jmp_rw = pair;
    flush_idcache_range(jmp_rx, jmp_rw, 8);
#else
    tcg_insn_unit insn;

    if (offset == sextreg(offset, 0, 28)) {
        insn = encode_dj(OPC_B, offset);
    } else { // TODO
        tcg_debug_assert(0);
    }

    // Update insn with new address.
    qatomic_set((uint32_t *)jmp_rw, insn);
    flush_idcache_range(jmp_rx, jmp_rw, 4);
#endif
}

void b_to_addr(uintptr_t jmp_rx, uintptr_t jmp_rw, uintptr_t addr)
{
    ptrdiff_t offset = addr - jmp_rx;
    assert(offset == sextreg(offset, 0, 28));
    tcg_insn_unit insn = encode_dj(OPC_B, offset);
    qatomic_set((uint32_t *)jmp_rw, insn);
    flush_idcache_range(jmp_rx, jmp_rw, 4);
}

void light_tb_target_set_jmp_target(uintptr_t tc_ptr, uintptr_t jmp_rx,
                              uintptr_t jmp_rw, uintptr_t addr)
{
    ptrdiff_t offset = addr - jmp_rx;
    tcg_insn_unit insn;

#ifdef CONFIG_LATX_LARGE_CC
    if (offset == sextreg(offset, 0, 28)) {
        insn = encode_dj(OPC_B, offset);
        *((uint32_t *)jmp_rw) = insn;
        *((uint32_t *)(jmp_rw + 4)) = 0x03400000;
    } else {
        tcg_insn_unit i1, i2;
        offset >>= 2;
        int64_t upper, lower;
        upper = ((offset + (1 << 15)) >> 16) & 0xfffff;
        lower = (offset & 0xffff);
        /* $t0 -> temp reg, was not used at end of tb */
        i1 = 0x1e000000 | upper << 5 | TCG_REG_T0;       /* pcaddu18i */
        i2 = 0x4c000000 | lower << 10 | TCG_REG_T0 << 5; /* jirl */
        *((uint32_t *)(jmp_rw + 4)) = i2;
        *((uint32_t *)jmp_rw) = i1;
    }
#else
    if (offset == sextreg(offset, 0, 28)) {
        insn = encode_dj(OPC_B, offset);
    } else { // TODO
        tcg_debug_assert(0);
    }

    // Update insn with new address.
    *((uint32_t *)jmp_rw) = insn;
#endif
}

void reset_pcaddu18i_jirl_target(uintptr_t tc_ptr, uintptr_t jmp_rx,
                              uintptr_t jmp_rw, uintptr_t addr)
{
    ptrdiff_t offset = addr - jmp_rx;
    unsigned int rd;

    tcg_insn_unit i1, i2;
    offset >>= 2;
    int64_t upper, lower;
    upper = ((offset + (1 << 15)) >> 16) & 0xfffff;
    lower = (offset & 0xffff);
    rd = *((uint32_t *)(jmp_rw + 4)) & 0x1f;
    i1 = 0x1e000000 | upper << 5 | TCG_REG_T0;       /* pcaddu18i */
    i2 = 0x4c000000 | lower << 10 | TCG_REG_T0 << 5 | rd; /* jirl */
    *((uint32_t *)(jmp_rw + 4)) = i2;
    *((uint32_t *)jmp_rw) = i1;
}

typedef struct {
    DebugFrameHeader h;
    uint8_t fde_def_cfa[4];
    uint8_t fde_reg_ofs[ARRAY_SIZE(tcg_target_callee_save_regs) * 2];
} DebugFrame;

#define ELF_HOST_MACHINE EM_LOONGARCH

static const DebugFrame debug_frame = {
    .h.cie.len = sizeof(DebugFrameCIE) - 4, /* length after .len member */
    .h.cie.id = -1,
    .h.cie.version = 1,
    .h.cie.code_align = 1,
    .h.cie.data_align = -(TCG_TARGET_REG_BITS / 8) & 0x7f, /* sleb128 */
    .h.cie.return_column = TCG_REG_RA,

    /* Total FDE size does not include the "len" member.  */
    .h.fde.len = sizeof(DebugFrame) - offsetof(DebugFrame, h.fde.cie_offset),

    .fde_def_cfa = {
        12, TCG_REG_SP,                 /* DW_CFA_def_cfa sp, ... */
        (FRAME_SIZE & 0x7f) | 0x80,     /* ... uleb128 FRAME_SIZE */
        (FRAME_SIZE >> 7)
    },
    .fde_reg_ofs = {
        0x80 + 23, 10,                  /* DW_CFA_offset, s0,  -80 */
        0x80 + 24, 9,                   /* DW_CFA_offset, s1,  -72 */
        0x80 + 25, 8,                   /* DW_CFA_offset, s2,  -64 */
        0x80 + 26, 7,                   /* DW_CFA_offset, s3,  -56 */
        0x80 + 27, 6,                   /* DW_CFA_offset, s4,  -48 */
        0x80 + 28, 5,                   /* DW_CFA_offset, s5,  -40 */
        0x80 + 29, 4,                   /* DW_CFA_offset, s6,  -32 */
        0x80 + 30, 3,                   /* DW_CFA_offset, s7,  -24 */
        0x80 + 31, 2,                   /* DW_CFA_offset, s8,  -16 */
        0x80 + 1 , 1,                   /* DW_CFA_offset, ra,  -8 */
    }
};

void tcg_register_jit(const void *buf, size_t buf_size)
{
    tcg_register_jit_int(buf, buf_size, &debug_frame, sizeof(debug_frame));
}
