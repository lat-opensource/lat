/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file insts-pattern.c
 * @author huqi <spcreply@outlook.com>
 *         liuchaoyi <lcy285183897@gmail.com>
 * @brief insts-ptn optimization
 */
#include "lsenv.h"
#include "reg-alloc.h"
#include "translate.h"
#include "insts-pattern.h"

#ifdef CONFIG_LATX_INSTS_PATTERN

#define WRAP(ins) (dt_X86_INS_##ins)
#define SCAN_CHECK(buf, i) do { \
    if (buf[i] == -1) return false; \
} while (0)
#define SCAN_IDX(buf, i)        (buf[i])
#define SCAN_IR1(tb, buf, i)    (tb_ir1_inst(tb, SCAN_IDX(buf, i)))

// static inline void pattern_invalid(IR1_INST *scan_buf[PTN_BUF_SIZE], int num)
// {
//     assert(num < PTN_BUF_SIZE);
//     for (int i = 0; i <= num; ++i) {
//         scan_buf[i]->cflag |= IR1_INVALID_MASK | IR1_PATTERN_MASK;
//     }
// }

// static inline void pattern_modify(IR1_INST *ir1, IR1_OPCODE opcode)
// {
//     ir1->info->id = opcode;
//     ir1->cflag |= IR1_PATTERN_MASK;
// }

static inline bool ir1_can_pattern(IR1_INST *pir1)
{
    switch (ir1_opcode(pir1)) {
    /*head*/
    case WRAP(CMP):
    case WRAP(CQO):
    case WRAP(XOR):
    case WRAP(CDQ):
    case WRAP(TEST):
    case WRAP(UCOMISD):
#ifdef CONFIG_LATX_SMC_OPT
    case WRAP(MOVAPS):
    case WRAP(MOVDQA):
#endif
    case WRAP(NEG):

    /*tail*/
    case WRAP(SBB):
    case WRAP(IDIV):
    case WRAP(DIV):
    case WRAP(SETB):
    case WRAP(SETAE):
    case WRAP(SETE):
    case WRAP(SETNE):
    case WRAP(SETBE):
    case WRAP(SETA):
    case WRAP(SETL):
    case WRAP(SETGE):
    case WRAP(SETLE):
    case WRAP(SETG):
    case WRAP(SETS):
    case WRAP(SETNS):
    case WRAP(SETNO):
    case WRAP(SETO):
    case WRAP(CMOVE):
    case WRAP(CMOVNE):
    case WRAP(CMOVS):
    case WRAP(CMOVNS):
    case WRAP(CMOVLE):
    case WRAP(CMOVG):
    case WRAP(CMOVNO):
    case WRAP(CMOVO):
    case WRAP(CMOVB):
    case WRAP(CMOVBE):
    case WRAP(CMOVA):
    case WRAP(CMOVAE):
    case WRAP(CMOVL):
    case WRAP(CMOVGE):
        return true;
     default:
        return false;
     }
 }

static inline bool ir1_is_pattern_head(IR1_INST *pir1)
{
    switch (ir1_opcode(pir1)) {
    case WRAP(CMP):
    case WRAP(CQO):
    case WRAP(XOR):
    case WRAP(CDQ):
    case WRAP(TEST):
    case WRAP(UCOMISD):
#ifdef CONFIG_LATX_SMC_OPT
    case WRAP(MOVAPS):
    case WRAP(MOVDQA):
#endif
    case WRAP(NEG):
        return true;
    default:
        return false;
    }
 }

static inline void scan_clear(scan_elem_t *scan)
{
    if (scan[0] == -1) return;
    memset(scan, -1, sizeof(scan_elem_t) * INSTPTN_BUF_SIZE);
}

static inline void scan_push(scan_elem_t *scan, int pir1_index)
{
    for(int i = INSTPTN_BUF_SIZE - 1; i > 0; --i) {
        scan[i] = scan[i-1];
    }
    scan[0] = pir1_index;
}

static bool is_contain_edx(IR1_OPND *opnd)
{
    if (ir1_opnd_is_gpr(opnd)) {
        switch (opnd->reg) {
        case dt_X86_REG_DL: case dt_X86_REG_DH:
        case dt_X86_REG_DX: case dt_X86_REG_EDX:
        case dt_X86_REG_RDX:
            return true;
        default:
            break;
        }
    } else if (ir1_opnd_is_mem(opnd)) {
        switch (opnd->mem.base) {
        case dt_X86_REG_DL: case dt_X86_REG_DH:
        case dt_X86_REG_DX: case dt_X86_REG_EDX:
        case dt_X86_REG_RDX:
            return true;
        default:
            break;
        }
        switch (opnd->mem.index) {
        case dt_X86_REG_DL: case dt_X86_REG_DH:
        case dt_X86_REG_DX: case dt_X86_REG_EDX:
        case dt_X86_REG_RDX:
            return true;
        default:
            break;
        }
    }
    return false;
}

static int inst_pattern(TranslationBlock *tb,
        IR1_INST *pir1, scan_elem_t *scan)
{
    IR1_INST *ir1 = NULL;
    IR1_OPND *opnd0 = NULL;
    IR1_OPND *opnd1 = NULL;

    /*
     * pir1 is pattern head
     * scan[] contains ir1 following the head
     */
    switch (ir1_opcode(pir1)) {
    case WRAP(CMP): {
        SCAN_CHECK(scan, 0);
        ir1 = SCAN_IR1(tb, scan, 0);
        if(ir1_opcode(ir1) == WRAP(SBB)) {
            instptn_check_cmp_sbb_0();

            opnd0 = ir1_get_opnd(ir1, 0);
            opnd1 = ir1_get_opnd(ir1, 1);
            if (!ir1_opnd_is_same_reg(opnd0, opnd1)) {
                return 0;
            }
            pir1->instptn.opc  = INSTPTN_OPC_CMP_SBB;
            pir1->instptn.next = ir1;
            ir1->instptn.opc  = INSTPTN_OPC_NOP;
            // ir1->instptn.next = NULL;
            return 1;
        }
        instptn_check_cmp_xxcc_0();
        switch (ir1_opcode(ir1)) {
        case WRAP(SETB):
        case WRAP(SETAE):
        case WRAP(SETE):
        case WRAP(SETNE):
        case WRAP(SETBE):
        case WRAP(SETA):
        case WRAP(SETL):
        case WRAP(SETGE):
        case WRAP(SETLE):
        case WRAP(SETG):
        case WRAP(CMOVB):
        case WRAP(CMOVAE):
        case WRAP(CMOVE):
        case WRAP(CMOVNE):
        case WRAP(CMOVBE):
        case WRAP(CMOVA):
        case WRAP(CMOVL):
        case WRAP(CMOVGE):
        case WRAP(CMOVLE):
        case WRAP(CMOVG):
            pir1->instptn.opc  = INSTPTN_OPC_CMP_XXCC;
            pir1->instptn.next = ir1;
            ir1->instptn.opc  = INSTPTN_OPC_NOP;
            // ir1->instptn.next = NULL;
            return 1;
        default:
            return 0;
        }
    }
    case WRAP(TEST): {
        SCAN_CHECK(scan, 0);
        instptn_check_test_xxcc_0();
        ir1 = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1)) {
        case WRAP(SETS):
        case WRAP(SETNS):
        case WRAP(SETLE):
        case WRAP(SETG):
        case WRAP(CMOVS):
        case WRAP(CMOVNS):
        case WRAP(CMOVLE):
        case WRAP(CMOVG):
            opnd0 = ir1_get_opnd(pir1, 0);
            opnd1 = ir1_get_opnd(pir1, 1);
            if (!ir1_opnd_is_same_reg(opnd0, opnd1)) {
                return 1;
            }
            __attribute__((fallthrough));
        case WRAP(SETE):
        case WRAP(SETNE):
        case WRAP(SETNO):
        case WRAP(SETO):
        case WRAP(SETB):
        case WRAP(SETBE):
        case WRAP(SETA):
        case WRAP(SETAE):
        case WRAP(CMOVE):
        case WRAP(CMOVNE):
        case WRAP(CMOVNO):
        case WRAP(CMOVO):
        case WRAP(CMOVB):
        case WRAP(CMOVBE):
        case WRAP(CMOVA):
        case WRAP(CMOVAE):
            pir1->instptn.opc  = INSTPTN_OPC_TEST_XXCC;
            pir1->instptn.next = ir1;
            ir1->instptn.opc  = INSTPTN_OPC_NOP;
            // ir1->instptn.next = NULL;
            return 1;
        default:
            return 0;
        }
    }
    case WRAP(CQO):
        SCAN_CHECK(scan, 0);
        instptn_check_cqo_idiv_0();

        ir1 = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1)) {
        case WRAP(IDIV):
            opnd0 = ir1_get_opnd(ir1, 0);
            if (!ir1_opnd_is_gpr(opnd0))
                return 0;
            if (ir1_opnd_size(opnd0) != 64)
                return 0;
            if (is_contain_edx(opnd0))
                return 0;
            pir1->instptn.opc  = INSTPTN_OPC_CQO_IDIV;
            pir1->instptn.next = ir1;
            ir1->instptn.opc  = INSTPTN_OPC_NOP_DIV;
            // ir1->instptn.next = NULL;
            return 1;
        default:
            return 0;
        }
    case WRAP(XOR):
        SCAN_CHECK(scan, 0);
        instptn_check_xor_div_0();

        opnd0 = ir1_get_opnd(pir1, 0);
        opnd1 = ir1_get_opnd(pir1, 1);

        ir1 = SCAN_IR1(tb, scan, 0);
        if (ir1_opcode(ir1) == WRAP(DIV)) {
            if (ir1_opnd_is_gpr(opnd0) && ir1_opnd_is_gpr(opnd1) &&
                ((opnd0->reg == dt_X86_REG_EDX && opnd1->reg == dt_X86_REG_EDX &&
                ir1_opnd_size(ir1_get_opnd(ir1, 0)) == 32 &&
                ir1_opnd_is_gpr(ir1_get_opnd(ir1, 0))) ||
                (opnd0->reg == dt_X86_REG_RDX && opnd1->reg == dt_X86_REG_RDX &&
                ir1_opnd_size(ir1_get_opnd(ir1, 0)) == 64 &&
                ir1_opnd_is_gpr(ir1_get_opnd(ir1, 0))))) {
                    if (is_contain_edx(opnd0))
                        return 0;
                    pir1->instptn.opc  = INSTPTN_OPC_XOR_DIV;
                    pir1->instptn.next = ir1;
                    ir1->instptn.opc  = INSTPTN_OPC_NOP_DIV;
                    // ir1->instptn.next = NULL;
                    return 1;
                }
        }
        return 0;
    case WRAP(CDQ):
        SCAN_CHECK(scan, 0);
        instptn_check_cdq_idiv_0();

        ir1 = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1)) {
        case WRAP(IDIV):
            opnd0 = ir1_get_opnd(ir1, 0);
            if (!ir1_opnd_is_gpr(opnd0))
                return 0;
            if (ir1_opnd_size(opnd0) != 32)
                return 0;
            if (is_contain_edx(opnd0))
                return 0;
            pir1->instptn.opc  = INSTPTN_OPC_CDQ_IDIV;
            pir1->instptn.next = ir1;
            ir1->instptn.opc  = INSTPTN_OPC_NOP_DIV;
            // ir1->instptn.next = NULL;
            return 1;
        default:
            return 0;
        }
    case WRAP(UCOMISD):
        SCAN_CHECK(scan, 0);
        instptn_check_ucomisd_seta_0();

        ir1 = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1)) {
        case WRAP(SETA):
            pir1->instptn.opc  = INSTPTN_OPC_UCOMISD_SETA;
            pir1->instptn.next = ir1;
            ir1->instptn.opc  = INSTPTN_OPC_NOP;
            // ir1->instptn.next = NULL;
            return 1;
        default:
            return 0;
        }
#ifdef CONFIG_LATX_SMC_OPT
    case WRAP(MOVAPS):
    case WRAP(MOVDQA): {
        SCAN_CHECK(scan, 0);
        instptn_check_movaps_vst_x4_0();
        if (!tb_use_smc_opt(tb))
            return 0;
        if (scan[0] >= 0 && scan[1] >= 0 && scan[2] >= 0) {
            ir1 = SCAN_IR1(tb, scan, 0);
            IR1_INST *ir2 = SCAN_IR1(tb, scan, 1);
            IR1_INST *ir3 = SCAN_IR1(tb, scan, 2);
            if (ir1_opcode(ir1) == WRAP(MOVAPS) &&
                    ir1_opcode(ir2) == WRAP(MOVAPS) &&
                    ir1_opcode(ir3) == WRAP(MOVAPS)) {
                pir1->instptn.opc  = INSTPTN_OPC_MOVAPS_VST_X4;
                pir1->instptn.next = ir1;
                ir1->instptn.opc  = INSTPTN_OPC_NOP;
                ir1->instptn.next = ir2;
                ir2->instptn.opc  = INSTPTN_OPC_NOP;
                ir2->instptn.next = ir3;
                ir3->instptn.opc  = INSTPTN_OPC_NOP;
                return 1;
            }
        }
        return 0;
    }
#endif
    case WRAP(NEG): {
        SCAN_CHECK(scan, 0);
        instptn_check_neg_cmovcc_0();
        ir1 = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1)) {
            case WRAP(CMOVS):
            case WRAP(CMOVNS):
                pir1->instptn.opc  = INSTPTN_OPC_NEG_CMOVCC;
                pir1->instptn.next = ir1;
                ir1->instptn.opc  = INSTPTN_OPC_NOP;
                return 1;
            default:
                return 0;
        }
    }
    default:
        return 0;
    }
}

void insts_pattern_scan_con(TranslationBlock *tb, IR1_INST *ir1, int index, scan_elem_t *scan_buf)
{
    if (!ir1_can_pattern(ir1)) {
        scan_clear(scan_buf);
        return;
    }
    if (!ir1_is_pattern_head(ir1)) {
        scan_push(scan_buf, index);
        return;
    }

    if (inst_pattern(tb, ir1, scan_buf)) {
        scan_clear(scan_buf);
    } else {
        scan_push(scan_buf, index);
    }
}

bool insts_pattern_scan_jcc_end(TranslationBlock *tb, IR1_INST *pir1, int pir1_index, scan_elem_t *scan)
{

    if (pir1_index == tb_ir1_num(tb) - 1) {
        if (!pir1_index) return false; /* tb->icount > 1*/
        switch (ir1_opcode(pir1)) {
        case WRAP(JA):
        case WRAP(JAE):
        case WRAP(JB):
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JBE):
        case WRAP(JL):
        case WRAP(JGE):
        case WRAP(JLE):
        case WRAP(JG):
        case WRAP(JNO):
        case WRAP(JO):
        case WRAP(JS):
        case WRAP(JNS):
            scan[0] = pir1_index;
            return true;
        default:
            return false;
        }
    }

    IR1_INST *ir1_jcc = NULL;
    IR1_OPND *opnd0 = NULL;
    IR1_OPND *opnd1 = NULL;
    switch (ir1_opcode(pir1)) {
        case WRAP(CMP):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JB):
        case WRAP(JAE):
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JBE):
        case WRAP(JA):
        case WRAP(JL):
        case WRAP(JGE):
        case WRAP(JLE):
        case WRAP(JG):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_cmp_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_CMP_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else {
                instptn_check_cmp_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_CMP_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_CMP_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst(tb, pir1_index);
                tb->has_jcc_end_ptn = true;
            }
            return false;
        default:
            return false;
        }
    case WRAP(TEST):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JS):
        case WRAP(JNS):
        case WRAP(JLE):
        case WRAP(JG):
            opnd0 = ir1_get_opnd(pir1, 0);
            opnd1 = ir1_get_opnd(pir1, 1);
            if (!ir1_opnd_is_same_reg(opnd0, opnd1)) {
                return false;
            }
            __attribute__((fallthrough));
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JNO):
        case WRAP(JO):
        case WRAP(JB):
        case WRAP(JBE):
        case WRAP(JA):
        case WRAP(JAE):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_test_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_TEST_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else {
                instptn_check_test_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_TEST_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_TEST_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst(tb, pir1_index);
                tb->has_jcc_end_ptn = true;
            }
            return false;
        default:
            return false;
        }
    case WRAP(BT):
        SCAN_CHECK(scan, 0);
        opnd0 = ir1_get_opnd(pir1, 0);
        if (!ir1_opnd_is_gpr(opnd0))
            return false;
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JB):
        case WRAP(JAE):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_bt_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_BT_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else {
                instptn_check_bt_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_BT_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_BT_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst(tb, pir1_index);
                tb->has_jcc_end_ptn = true;
            }
            return false;
        default:
            return false;
        }
    case WRAP(SUB):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JB):
        case WRAP(JAE):
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JBE):
        case WRAP(JA):
        case WRAP(JL):
        case WRAP(JGE):
        case WRAP(JLE):
        case WRAP(JG):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_sub_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_SUB_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            }
            return false;
        default:
            return false;
        }
    case WRAP(SHR):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        opnd1 = ir1_get_opnd(pir1, 1);
        if (!ir1_opnd_is_imm(opnd1))
            return false;
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JNE):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_shr_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_SHR_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            }
            return false;
        default:
            return false;
        }
    case WRAP(AND):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JNE):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_and_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_AND_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            }
            return false;
        default:
            return false;
        }
#ifdef CONFIG_LATX_XCOMISX_OPT
    case WRAP(COMISD):
    case WRAP(VCOMISD):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JA):
        case WRAP(JAE):
        case WRAP(JB):
        case WRAP(JBE):
        case WRAP(JNE):
        case WRAP(JE):
        case WRAP(JL):
        case WRAP(JGE):
        case WRAP(JLE):
        case WRAP(JG):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_comisd_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_COMISD_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else if (ir1_opcode(pir1) == WRAP(COMISD)) {
                instptn_check_comisd_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_COMISD_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_COMISD_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst(tb, pir1_index);
                tb->has_jcc_end_ptn = true;
            }
            return false;
        default:
            return false;
        }
    case WRAP(COMISS):
    case WRAP(VCOMISS):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JA):
        case WRAP(JAE):
        case WRAP(JB):
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JBE):
        case WRAP(JL):
        case WRAP(JGE):
        case WRAP(JLE):
        case WRAP(JG):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_comiss_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_COMISS_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else if (ir1_opcode(pir1) == WRAP(COMISS)) {
                instptn_check_comiss_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_COMISS_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_COMISS_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst(tb, pir1_index);
                tb->has_jcc_end_ptn = true;
            }
            return false;
        default:
            return false;
        }
    case WRAP(UCOMISD):
    case WRAP(VUCOMISD):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JA):
        case WRAP(JAE):
        case WRAP(JB):
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JBE):
        case WRAP(JL):
        case WRAP(JGE):
        case WRAP(JLE):
        case WRAP(JG):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_ucomisd_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_UCOMISD_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else if (ir1_opcode(pir1) == WRAP(UCOMISD)) {
                instptn_check_ucomisd_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_UCOMISD_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_UCOMISD_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst(tb, pir1_index);
                tb->has_jcc_end_ptn = true;
            }
            return false;
        default:
            return false;
        }
    case WRAP(UCOMISS):
    case WRAP(VUCOMISS):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JA):
        case WRAP(JAE):
        case WRAP(JB):
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JBE):
        case WRAP(JL):
        case WRAP(JGE):
        case WRAP(JLE):
        case WRAP(JG):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_ucomiss_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_UCOMISS_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                ir1_jcc->instptn.next = NULL;
            } else if (ir1_opcode(pir1) == WRAP(UCOMISS)) {
                instptn_check_ucomiss_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_UCOMISS_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_UCOMISS_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst(tb, pir1_index);
                tb->has_jcc_end_ptn = true;
            }
            return false;
        default:
            return false;
        }
#endif
    case WRAP(MOV): {
        opnd0 = ir1_get_opnd(pir1, 0);
        opnd1 = ir1_get_opnd(pir1, 1);
        if (ir1_opnd_is_mem(opnd0) && ir1_opnd_is_gpr(opnd1) &&
            (!ir1_opnd_is_8h(opnd1)) && tb_use_smc_opt(lsenv->tr_data->curr_tb)) {
                return false;
        }
        return true;
    }
    case WRAP(ADDSD):
    case WRAP(ADDSS):
    case WRAP(LEA):
    case WRAP(MOVAPD):
    case WRAP(MOVHPS):
    case WRAP(MOVLPS):
    case WRAP(MOVSD):
    case WRAP(MOVSS):
    case WRAP(MOVSX):
    case WRAP(MOVSXD):
    case WRAP(MOVZX):
    case WRAP(MULPS):
    case WRAP(MULSD):
    case WRAP(MULSS):
    case WRAP(NOP):
    case WRAP(PSHUFD):
    case WRAP(PUNPCKLWD):
    case WRAP(PUSH):
        return true;
    default:
        return false;
    }
}

static bool repeat_add_same_operands(IR1_INST *first, IR1_INST *next)
{
    IR1_OPND *first_dest;
    IR1_OPND *first_src;
    IR1_OPND *next_dest;
    IR1_OPND *next_src;

    if (ir1_opcode(next) != WRAP(ADD) || ir1_get_opnd_num(next) != 2 ||
        next->instptn.opc != INSTPTN_OPC_NONE ||
        ir1_get_eflag_def(next) != 0 || ir1_is_prefix_lock(next)) {
        return false;
    }
    first_dest = ir1_get_opnd(first, 0);
    first_src = ir1_get_opnd(first, 1);
    next_dest = ir1_get_opnd(next, 0);
    next_src = ir1_get_opnd(next, 1);

    return
           ir1_opnd_is_gpr(next_dest) && ir1_opnd_is_gpr(next_src) &&
           ir1_opnd_size(next_dest) == 64 &&
           ir1_opnd_size(next_src) == 64 &&
           ir1_opnd_base_reg_num(next_dest) ==
               ir1_opnd_base_reg_num(first_dest) &&
           ir1_opnd_base_reg_num(next_src) ==
               ir1_opnd_base_reg_num(first_src);
}

static int avx_sum3_xmm(IR1_INST *ir1, int opnd_index)
{
    if (opnd_index >= ir1_get_opnd_num(ir1) ||
        !ir1_opnd_is_xmm(ir1_get_opnd(ir1, opnd_index))) {
        return -1;
    }
    return ir1_opnd_base_reg_num(ir1_get_opnd(ir1, opnd_index));
}

static bool avx_sum3_imm(IR1_INST *ir1, int opnd_index, ulongx value)
{
    return opnd_index < ir1_get_opnd_num(ir1) &&
           ir1_opnd_is_imm(ir1_get_opnd(ir1, opnd_index)) &&
           ir1_opnd_uimm(ir1_get_opnd(ir1, opnd_index)) == value;
}

static bool avx_sum3_match_group(TranslationBlock *tb, int pos,
                                 int *product, int *zero)
{
    IR1_INST *mul = tb_ir1_inst(tb, pos);
    IR1_INST *blend = tb_ir1_inst(tb, pos + 1);
    IR1_INST *insert = tb_ir1_inst(tb, pos + 2);
    IR1_INST *add = tb_ir1_inst(tb, pos + 3);
    int temp;

    if (ir1_opcode(mul) != WRAP(VMULPS) ||
        ir1_opcode(blend) != WRAP(VBLENDPS) ||
        ir1_opcode(insert) != WRAP(VINSERTPS) ||
        ir1_opcode(add) != WRAP(VADDPS)) {
        return false;
    }
    *product = avx_sum3_xmm(mul, 0);
    temp = avx_sum3_xmm(blend, 0);
    if (*product < 0 || temp < 0 || temp == *product ||
        avx_sum3_xmm(blend, 1) != *product ||
        avx_sum3_xmm(blend, 2) < 0 ||
        !avx_sum3_imm(blend, 3, 0x08) ||
        avx_sum3_xmm(insert, 0) != *product ||
        avx_sum3_xmm(insert, 1) != *product ||
        avx_sum3_xmm(insert, 2) != *product ||
        !avx_sum3_imm(insert, 3, 0x4c) ||
        avx_sum3_xmm(add, 0) != *product ||
        avx_sum3_xmm(add, 1) != temp ||
        avx_sum3_xmm(add, 2) != *product) {
        return false;
    }
    if (*zero < 0) {
        *zero = avx_sum3_xmm(blend, 2);
    }
    return avx_sum3_xmm(blend, 2) == *zero;
}

static bool avx_sum3_match_insert(IR1_INST *ir1, int dest, int src1,
                                  int src2, ulongx imm)
{
    return ir1_opcode(ir1) == WRAP(VINSERTPS) &&
           avx_sum3_xmm(ir1, 0) == dest &&
           avx_sum3_xmm(ir1, 1) == src1 &&
           avx_sum3_xmm(ir1, 2) == src2 &&
           avx_sum3_imm(ir1, 3, imm);
}

static bool avx_sum3_match_add(IR1_INST *ir1, IR1_OPCODE opcode,
                               int dest, int src1, int src2)
{
    return ir1_opcode(ir1) == opcode &&
           avx_sum3_xmm(ir1, 0) == dest &&
           avx_sum3_xmm(ir1, 1) == src1 &&
           avx_sum3_xmm(ir1, 2) == src2;
}

static bool avx_sum3_match_shuffle(IR1_INST *ir1, int dest, int src)
{
    return ir1_opcode(ir1) == WRAP(VSHUFPD) &&
           avx_sum3_xmm(ir1, 0) == dest &&
           avx_sum3_xmm(ir1, 1) == src &&
           avx_sum3_xmm(ir1, 2) == src &&
           avx_sum3_imm(ir1, 3, 1);
}

static bool avx_sum3_safe_overwrite(IR1_INST *ir1, int reg)
{
    if (avx_sum3_xmm(ir1, 0) != reg) {
        return false;
    }
    for (int i = 1; i < ir1_get_opnd_num(ir1); ++i) {
        if (avx_sum3_xmm(ir1, i) == reg) {
            return false;
        }
    }
    switch (ir1_opcode(ir1)) {
    case WRAP(VMOVSD):
        return ir1_opnd_is_mem(ir1_get_opnd(ir1, 1));
    case WRAP(VINSERTPS):
    case WRAP(VSQRTSS):
        return true;
    default:
        return false;
    }
}

static bool avx_sum3_overwritten_before_use(TranslationBlock *tb, int pos,
                                            int reg)
{
    for (int i = pos; i < tb_ir1_num(tb); ++i) {
        IR1_INST *ir1 = tb_ir1_inst(tb, i);

        if (avx_sum3_safe_overwrite(ir1, reg)) {
            return true;
        }
        for (int j = 0; j < ir1_get_opnd_num(ir1); ++j) {
            if (avx_sum3_xmm(ir1, j) == reg) {
                return false;
            }
        }
    }
    return false;
}

void insts_pattern_avx_sum3(TranslationBlock *tb)
{
    static const int group_offset[] = {0, 4, 9, 13, 17, 21};
    static const int skipped_offset[] = {
        1, 2, 3, 5, 6, 7, 10, 11, 12, 14, 15, 16,
        18, 19, 20, 23, 24, 25, 26, 27, 28, 29, 30,
        31, 32, 33, 34, 35, 36, 37, 38,
    };
    int count = tb_ir1_num(tb);

    for (int start = 0; start + 39 <= count; ++start) {
        int product[6];
        int group_temp[6];
        int zero = -1;
        int temp1;
        int temp2;
        int temp3;
        bool match = true;

        for (int i = 0; i < 39; ++i) {
            if (tb_ir1_inst(tb, start + i)->instptn.opc !=
                INSTPTN_OPC_NONE) {
                match = false;
            }
        }
        if (!match) {
            continue;
        }
        for (int i = 0; i < 6; ++i) {
            if (!avx_sum3_match_group(tb, start + group_offset[i],
                                      &product[i], &zero)) {
                match = false;
                break;
            }
            group_temp[i] = avx_sum3_xmm(
                tb_ir1_inst(tb, start + group_offset[i] + 1), 0);
            for (int j = 0; j < i; ++j) {
                if (product[i] == product[j] ||
                    group_temp[i] == product[j]) {
                    match = false;
                }
            }
        }
        if (!match || zero < 0 ||
            ir1_opcode(tb_ir1_inst(tb, start + 8)) != WRAP(VMOVAPS) ||
            !ir1_opnd_is_mem(ir1_get_opnd(tb_ir1_inst(tb, start + 8), 1)) ||
            avx_sum3_xmm(tb_ir1_inst(tb, start + 8), 0) == product[0] ||
            avx_sum3_xmm(tb_ir1_inst(tb, start + 8), 0) == product[1]) {
            continue;
        }
        for (int i = 0; i < 6; ++i) {
            if (zero == product[i]) {
                match = false;
            }
            for (int opnd = 1;
                 opnd < ir1_get_opnd_num(
                     tb_ir1_inst(tb, start + group_offset[i])); ++opnd) {
                int src = avx_sum3_xmm(
                    tb_ir1_inst(tb, start + group_offset[i]), opnd);

                for (int previous = 0; previous < i; ++previous) {
                    if (src == product[previous]) {
                        match = false;
                    }
                }
            }
        }
        if (!match) {
            continue;
        }
        temp1 = avx_sum3_xmm(tb_ir1_inst(tb, start + 25), 0);
        temp2 = avx_sum3_xmm(tb_ir1_inst(tb, start + 29), 0);
        temp3 = avx_sum3_xmm(tb_ir1_inst(tb, start + 34), 0);
        if (temp1 < 0 || temp2 < 0 || temp3 < 0 ||
            group_temp[0] != product[1] ||
            group_temp[1] != avx_sum3_xmm(
                tb_ir1_inst(tb, start + 8), 0) ||
            group_temp[2] != temp1 || group_temp[3] != temp1 ||
            group_temp[4] != temp1 || group_temp[5] != temp1 ||
            temp2 != product[1] || temp3 != product[4] ||
            zero == temp1 || zero == group_temp[1] ||
            !avx_sum3_match_insert(tb_ir1_inst(tb, start + 25),
                                   temp1, product[1], product[3], 0x1c) ||
            !avx_sum3_match_shuffle(tb_ir1_inst(tb, start + 26),
                                    product[1], product[1]) ||
            !avx_sum3_match_insert(tb_ir1_inst(tb, start + 27),
                                   product[3], product[1], product[3], 0x9c) ||
            !avx_sum3_match_add(tb_ir1_inst(tb, start + 28), WRAP(VADDPS),
                                product[3], temp1, product[3]) ||
            !avx_sum3_match_insert(tb_ir1_inst(tb, start + 29),
                                   temp2, product[0], product[4], 0x1c) ||
            !avx_sum3_match_shuffle(tb_ir1_inst(tb, start + 30),
                                    product[0], product[0]) ||
            !avx_sum3_match_insert(tb_ir1_inst(tb, start + 31),
                                   product[4], product[0], product[4], 0x9c) ||
            !avx_sum3_match_add(tb_ir1_inst(tb, start + 32), WRAP(VADDPS),
                                product[4], temp2, product[4]) ||
            !avx_sum3_match_add(tb_ir1_inst(tb, start + 33), WRAP(VADDPS),
                                product[3], product[4], product[3]) ||
            !avx_sum3_match_insert(tb_ir1_inst(tb, start + 34),
                                   temp3, product[2], product[5], 0x1c) ||
            !avx_sum3_match_shuffle(tb_ir1_inst(tb, start + 35),
                                    product[0], product[2]) ||
            !avx_sum3_match_insert(tb_ir1_inst(tb, start + 36),
                                   product[0], product[0], product[5], 0x9c) ||
            !avx_sum3_match_add(tb_ir1_inst(tb, start + 37), WRAP(VADDPS),
                                product[4], temp3, product[0]) ||
            !avx_sum3_match_add(tb_ir1_inst(tb, start + 38), WRAP(VSUBPS),
                                product[3], product[3], product[4])) {
            continue;
        }
        if (!avx_sum3_overwritten_before_use(tb, start + 39, product[0]) ||
            !avx_sum3_overwritten_before_use(tb, start + 39, product[1]) ||
            !avx_sum3_overwritten_before_use(tb, start + 39, product[2]) ||
            !avx_sum3_overwritten_before_use(tb, start + 39, product[4])) {
            continue;
        }

        for (size_t i = 0; i < ARRAY_SIZE(skipped_offset); ++i) {
            tb_ir1_inst(tb, start + skipped_offset[i])->instptn.opc =
                INSTPTN_OPC_NOP;
        }
        tb_ir1_inst(tb, start + 22)->instptn.opc = INSTPTN_OPC_AVX_SUM3;
        start += 38;
    }
}

void insts_pattern_repeat_add(TranslationBlock *tb)
{
    int count = tb_ir1_num(tb);

    for (int i = 0; i < count; ++i) {
        IR1_INST *first = tb_ir1_inst(tb, i);
        IR1_OPND *dest;
        IR1_OPND *src;
        int end;

        if (ir1_opcode(first) != WRAP(ADD) || ir1_get_opnd_num(first) != 2 ||
            first->instptn.opc != INSTPTN_OPC_NONE ||
            ir1_get_eflag_def(first) != 0 || ir1_is_prefix_lock(first)) {
            continue;
        }
        dest = ir1_get_opnd(first, 0);
        src = ir1_get_opnd(first, 1);
        if (!ir1_opnd_is_gpr(dest) || !ir1_opnd_is_gpr(src) ||
            ir1_opnd_size(dest) != 64 || ir1_opnd_size(src) != 64 ||
            ir1_opnd_base_reg_num(dest) == ir1_opnd_base_reg_num(src)) {
            continue;
        }

        for (end = i + 1; end < count; ++end) {
            if (!repeat_add_same_operands(first, tb_ir1_inst(tb, end))) {
                break;
            }
        }
        if (end - i < 3) {
            continue;
        }

        first->instptn.opc = INSTPTN_OPC_REPEAT_ADD;
        first->instptn.next = tb_ir1_inst(tb, end - 1);
        for (int j = i + 1; j < end; ++j) {
            tb_ir1_inst(tb, j)->instptn.opc = INSTPTN_OPC_NOP;
        }
        i = end - 1;
    }
}

static bool scalar_hdr_same_reg(IR1_OPND *a, IR1_OPND *b)
{
    return ir1_opnd_is_gpr(a) && ir1_opnd_is_gpr(b) &&
           ir1_opnd_base_reg_num(a) == ir1_opnd_base_reg_num(b);
}

static bool scalar_hdr_same_xmm(IR1_OPND *a, IR1_OPND *b)
{
    return ir1_opnd_is_xmm(a) && ir1_opnd_is_xmm(b) &&
           ir1_opnd_base_reg_num(a) == ir1_opnd_base_reg_num(b);
}

static bool scalar_hdr_same_mem(IR1_OPND *a, IR1_OPND *b, int64_t delta)
{
    return ir1_opnd_is_mem(a) && ir1_opnd_is_mem(b) &&
           a->mem.segment == b->mem.segment &&
           a->mem.default_segment == b->mem.default_segment &&
           a->mem.base == b->mem.base && a->mem.index == b->mem.index &&
           a->mem.scale == b->mem.scale &&
           b->mem.disp == a->mem.disp + delta;
}

static bool scalar_hdr_mem_uses_reg(IR1_OPND *mem, int reg)
{
    return (ir1_opnd_has_base(mem) && ir1_opnd_base_reg_num(mem) == reg) ||
           (ir1_opnd_has_index(mem) && ir1_opnd_index_reg_num(mem) == reg);
}

static bool scalar_hdr_match_channel(TranslationBlock *tb, int pos,
                                     IR1_OPND *first_input,
                                     IR1_OPND *first_table,
                                     IR1_OPND *first_sub,
                                     IR1_OPND *first_output,
                                     IR1_OPND *index_reg,
                                     IR1_OPND *value_xmm,
                                     IR1_OPND *factor_xmm,
                                     int channel)
{
    IR1_INST *movzx = tb_ir1_inst(tb, pos);
    IR1_INST *lea = tb_ir1_inst(tb, pos + 1);
    IR1_INST *load = tb_ir1_inst(tb, pos + 2);
    IR1_INST *sub = tb_ir1_inst(tb, pos + 3);
    IR1_INST *fma = tb_ir1_inst(tb, pos + 4);
    IR1_INST *store = tb_ir1_inst(tb, pos + 5);
    IR1_OPND *input;
    IR1_OPND *lea_mem;
    IR1_OPND *table;
    IR1_OPND *sub_mem;
    IR1_OPND *output;

    if (ir1_opcode(movzx) != WRAP(MOVZX) ||
        ir1_opcode(lea) != WRAP(LEA) ||
        ir1_opcode(load) != WRAP(VMOVSS) ||
        ir1_opcode(sub) != WRAP(VSUBSS) ||
        ir1_opcode(fma) != WRAP(VFMADD213SS) ||
        ir1_opcode(store) != WRAP(VMOVSS) ||
        ir1_get_opnd_num(movzx) != 2 || ir1_get_opnd_num(lea) != 2 ||
        ir1_get_opnd_num(load) != 2 || ir1_get_opnd_num(sub) != 3 ||
        ir1_get_opnd_num(fma) != 3 || ir1_get_opnd_num(store) != 2) {
        return false;
    }

    input = ir1_get_opnd(movzx, 1);
    lea_mem = ir1_get_opnd(lea, 1);
    table = ir1_get_opnd(load, 1);
    sub_mem = ir1_get_opnd(sub, 2);
    output = ir1_get_opnd(fma, 2);
    if (!scalar_hdr_same_reg(ir1_get_opnd(movzx, 0), index_reg) ||
        !scalar_hdr_same_reg(ir1_get_opnd(lea, 0), index_reg) ||
        !ir1_opnd_is_mem(input) || ir1_opnd_size(input) != 8 ||
        !scalar_hdr_same_mem(first_input, input, channel) ||
        !ir1_opnd_is_mem(lea_mem) || lea_mem->mem.disp != 0 ||
        lea_mem->mem.scale != 2 ||
        ir1_opnd_base_reg_num(lea_mem) !=
            ir1_opnd_base_reg_num(index_reg) ||
        ir1_opnd_index_reg_num(lea_mem) !=
            ir1_opnd_base_reg_num(index_reg) ||
        !scalar_hdr_same_xmm(ir1_get_opnd(load, 0), value_xmm) ||
        !scalar_hdr_same_mem(first_table, table, channel * 4) ||
        !scalar_hdr_mem_uses_reg(table,
                                 ir1_opnd_base_reg_num(index_reg)) ||
        table->mem.scale != 4 ||
        ir1_opnd_size(table) != 32 ||
        !scalar_hdr_same_xmm(ir1_get_opnd(sub, 0), value_xmm) ||
        !scalar_hdr_same_xmm(ir1_get_opnd(sub, 1), value_xmm) ||
        !scalar_hdr_same_mem(first_sub, sub_mem, 0) ||
        !scalar_hdr_same_xmm(ir1_get_opnd(fma, 0), value_xmm) ||
        !scalar_hdr_same_xmm(ir1_get_opnd(fma, 1), factor_xmm) ||
        !scalar_hdr_same_mem(first_output, output, channel * 4) ||
        !scalar_hdr_same_mem(output, ir1_get_opnd(store, 0), 0) ||
        !scalar_hdr_same_xmm(ir1_get_opnd(store, 1), value_xmm)) {
        return false;
    }
    return true;
}

void insts_pattern_scalar_hdr(TranslationBlock *tb)
{
    int count = tb_ir1_num(tb);

    if (!option_enable_lasx) {
        return;
    }
    for (int start = 0; start + 36 <= count; ++start) {
        IR1_INST *first = tb_ir1_inst(tb, start);
        IR1_INST *factor_load = tb_ir1_inst(tb, start + 10);
        IR1_INST *sign_extend = tb_ir1_inst(tb, start + 11);
        IR1_INST *accumulate = tb_ir1_inst(tb, start + 12);
        IR1_INST *acc_store = tb_ir1_inst(tb, start + 13);
        IR1_OPND *weighted0;
        IR1_OPND *weighted1;
        IR1_OPND *weighted_index;
        IR1_OPND *factor_xmm;
        IR1_OPND *value_xmm;
        IR1_OPND *first_input;
        IR1_OPND *first_table;
        IR1_OPND *first_sub;
        IR1_OPND *first_output;
        IR1_INST *imul0 = tb_ir1_inst(tb, start + 1);
        IR1_INST *movzx1 = tb_ir1_inst(tb, start + 2);
        IR1_INST *imul1 = tb_ir1_inst(tb, start + 3);
        IR1_INST *add_weights = tb_ir1_inst(tb, start + 4);
        IR1_INST *movzx2 = tb_ir1_inst(tb, start + 5);
        IR1_INST *lea9 = tb_ir1_inst(tb, start + 6);
        IR1_INST *lea19 = tb_ir1_inst(tb, start + 7);
        IR1_INST *add_index = tb_ir1_inst(tb, start + 8);
        IR1_INST *shift_index = tb_ir1_inst(tb, start + 9);
        int modified[3];
        bool match = true;

        for (int i = 0; i < 36; ++i) {
            if (tb_ir1_inst(tb, start + i)->instptn.opc !=
                INSTPTN_OPC_NONE) {
                match = false;
                break;
            }
        }
        if (!match || ir1_opcode(first) != WRAP(MOVZX) ||
            ir1_opcode(imul0) != WRAP(IMUL) ||
            ir1_opcode(movzx1) != WRAP(MOVZX) ||
            ir1_opcode(imul1) != WRAP(IMUL) ||
            ir1_opcode(add_weights) != WRAP(ADD) ||
            ir1_opcode(movzx2) != WRAP(MOVZX) ||
            ir1_opcode(lea9) != WRAP(LEA) ||
            ir1_opcode(lea19) != WRAP(LEA) ||
            ir1_opcode(add_index) != WRAP(ADD) ||
            ir1_opcode(shift_index) != WRAP(SHR) ||
            ir1_opcode(factor_load) != WRAP(VMOVSS) ||
            ir1_opcode(sign_extend) != WRAP(MOVSXD) ||
            ir1_opcode(accumulate) != WRAP(VADDSS) ||
            ir1_opcode(acc_store) != WRAP(VMOVSS) ||
            ir1_opcode(tb_ir1_inst(tb, start + 32)) != WRAP(INC) ||
            ir1_opcode(tb_ir1_inst(tb, start + 33)) != WRAP(ADD) ||
            ir1_opcode(tb_ir1_inst(tb, start + 34)) != WRAP(DEC) ||
            ir1_opcode(tb_ir1_inst(tb, start + 35)) != WRAP(JNE)) {
            continue;
        }

        weighted0 = ir1_get_opnd(first, 0);
        weighted1 = ir1_get_opnd(movzx1, 0);
        weighted_index = ir1_get_opnd(lea9, 0);
        factor_xmm = ir1_get_opnd(factor_load, 0);
        value_xmm = ir1_get_opnd(accumulate, 0);
        first_input = ir1_get_opnd(first, 1);
        first_table = ir1_get_opnd(tb_ir1_inst(tb, start + 16), 1);
        first_sub = ir1_get_opnd(tb_ir1_inst(tb, start + 17), 2);
        first_output = ir1_get_opnd(tb_ir1_inst(tb, start + 18), 2);
        modified[0] = ir1_opnd_base_reg_num(weighted0);
        modified[1] = ir1_opnd_base_reg_num(weighted1);
        modified[2] = ir1_opnd_base_reg_num(weighted_index);

        if (!ir1_opnd_is_gpr(weighted0) || !ir1_opnd_is_gpr(weighted1) ||
            !ir1_opnd_is_gpr(weighted_index) ||
            modified[0] == modified[1] || modified[0] == modified[2] ||
            modified[1] == modified[2] ||
            !ir1_opnd_is_xmm(factor_xmm) || !ir1_opnd_is_xmm(value_xmm) ||
            scalar_hdr_same_xmm(factor_xmm, value_xmm) ||
            ir1_addr_size(first) != 64 ||
            ir1_get_opnd_num(first) != 2 || ir1_get_opnd_num(imul0) != 3 ||
            ir1_get_opnd_num(movzx1) != 2 ||
            ir1_get_opnd_num(imul1) != 3 ||
            ir1_get_opnd_num(add_weights) != 2 ||
            ir1_get_opnd_num(movzx2) != 2 ||
            ir1_get_opnd_num(lea9) != 2 || ir1_get_opnd_num(lea19) != 2 ||
            ir1_get_opnd_num(add_index) != 2 ||
            ir1_get_opnd_num(shift_index) != 2 ||
            !scalar_hdr_same_reg(ir1_get_opnd(imul0, 0), weighted0) ||
            !scalar_hdr_same_reg(ir1_get_opnd(imul0, 1), weighted0) ||
            !ir1_opnd_is_imm(ir1_get_opnd(imul0, 2)) ||
            ir1_get_opnd(imul0, 2)->imm != 0x36 ||
            !scalar_hdr_same_mem(first_input, ir1_get_opnd(movzx1, 1), 1) ||
            !scalar_hdr_same_reg(ir1_get_opnd(imul1, 0), weighted1) ||
            !scalar_hdr_same_reg(ir1_get_opnd(imul1, 1), weighted1) ||
            !ir1_opnd_is_imm(ir1_get_opnd(imul1, 2)) ||
            ir1_get_opnd(imul1, 2)->imm != 0xb7 ||
            !scalar_hdr_same_reg(ir1_get_opnd(add_weights, 0), weighted1) ||
            !scalar_hdr_same_reg(ir1_get_opnd(add_weights, 1), weighted0) ||
            !scalar_hdr_same_reg(ir1_get_opnd(movzx2, 0), weighted0) ||
            !scalar_hdr_same_mem(first_input, ir1_get_opnd(movzx2, 1), 2) ||
            !ir1_opnd_is_mem(ir1_get_opnd(lea9, 1)) ||
            ir1_get_opnd(lea9, 1)->mem.disp != 0 ||
            ir1_get_opnd(lea9, 1)->mem.scale != 8 ||
            ir1_opnd_base_reg_num(ir1_get_opnd(lea9, 1)) != modified[0] ||
            ir1_opnd_index_reg_num(ir1_get_opnd(lea9, 1)) != modified[0] ||
            !scalar_hdr_same_reg(ir1_get_opnd(lea19, 0), weighted_index) ||
            !ir1_opnd_is_mem(ir1_get_opnd(lea19, 1)) ||
            ir1_get_opnd(lea19, 1)->mem.disp != 0 ||
            ir1_get_opnd(lea19, 1)->mem.scale != 2 ||
            ir1_opnd_base_reg_num(ir1_get_opnd(lea19, 1)) != modified[0] ||
            ir1_opnd_index_reg_num(ir1_get_opnd(lea19, 1)) != modified[2] ||
            !scalar_hdr_same_reg(ir1_get_opnd(add_index, 0), weighted_index) ||
            !scalar_hdr_same_reg(ir1_get_opnd(add_index, 1), weighted1) ||
            !scalar_hdr_same_reg(ir1_get_opnd(shift_index, 0), weighted_index) ||
            !ir1_opnd_is_imm(ir1_get_opnd(shift_index, 1)) ||
            ir1_get_opnd(shift_index, 1)->imm != 8) {
            continue;
        }

        if (ir1_get_opnd_num(factor_load) != 2 ||
            !ir1_opnd_is_mem(ir1_get_opnd(factor_load, 1)) ||
            ir1_opnd_size(ir1_get_opnd(factor_load, 1)) != 32 ||
            ir1_opnd_index_reg_num(ir1_get_opnd(factor_load, 1)) !=
                modified[2] ||
            ir1_get_opnd(factor_load, 1)->mem.scale != 4 ||
            ir1_get_opnd_num(sign_extend) != 2 ||
            !scalar_hdr_same_reg(ir1_get_opnd(sign_extend, 0),
                                 ir1_get_opnd(sign_extend, 1)) ||
            ir1_get_opnd_num(accumulate) != 3 ||
            !scalar_hdr_same_xmm(ir1_get_opnd(accumulate, 1), factor_xmm) ||
            !ir1_opnd_is_mem(ir1_get_opnd(accumulate, 2)) ||
            ir1_get_opnd_num(acc_store) != 2 ||
            !scalar_hdr_same_mem(ir1_get_opnd(accumulate, 2),
                                 ir1_get_opnd(acc_store, 0), 0) ||
            !scalar_hdr_same_xmm(ir1_get_opnd(acc_store, 1), value_xmm) ||
            !ir1_opnd_is_gpr(ir1_get_opnd(tb_ir1_inst(tb, start + 32), 0)) ||
            !ir1_opnd_is_gpr(ir1_get_opnd(tb_ir1_inst(tb, start + 33), 0)) ||
            !ir1_opnd_is_gpr(ir1_get_opnd(tb_ir1_inst(tb, start + 34), 0))) {
            continue;
        }

        for (int i = 0; i < 3 && match; ++i) {
            if (scalar_hdr_mem_uses_reg(first_input, modified[i]) ||
                scalar_hdr_mem_uses_reg(first_sub, modified[i]) ||
                scalar_hdr_mem_uses_reg(first_output, modified[i])) {
                match = false;
            }
        }
        if (!match ||
            !scalar_hdr_match_channel(tb, start + 14, first_input,
                                      first_table, first_sub, first_output,
                                      weighted_index, value_xmm, factor_xmm, 0) ||
            !scalar_hdr_match_channel(tb, start + 20, first_input,
                                      first_table, first_sub, first_output,
                                      weighted_index, value_xmm, factor_xmm, 1) ||
            !scalar_hdr_match_channel(tb, start + 26, first_input,
                                      first_table, first_sub, first_output,
                                      weighted_index, value_xmm, factor_xmm, 2)) {
            continue;
        }

        first->instptn.opc = INSTPTN_OPC_SCALAR_HDR;
        for (int i = 1; i < 32; ++i) {
            tb_ir1_inst(tb, start + i)->instptn.opc = INSTPTN_OPC_NOP;
        }
        start += 31;
    }
}

#undef WRAP

#endif
