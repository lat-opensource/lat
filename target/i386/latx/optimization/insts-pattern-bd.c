/**
 * @file insts-pattern.c
 * @author huqi <spcreply@outlook.com>
 *         liuchaoyi <lcy285183897@gmail.com>
 * @brief insts-ptn optimization
 */
#include "lsenv.h"
#include "reg-alloc.h"
#include "translate-bd.h"
#include "insts-pattern.h"

#ifdef CONFIG_LATX_INSTS_PATTERN

#define WRAP(ins) (ND_INS_##ins)
#define SCAN_CHECK(buf, i) do { \
    if (buf[i] == -1) return false; \
} while (0)
#define SCAN_IDX(buf, i)        (buf[i])
#define SCAN_IR1(tb, buf, i)    (tb_ir1_inst_bd(tb, SCAN_IDX(buf, i)))

static inline bool ir1_can_pattern(IR1_INST *pir1)
{
    switch (ir1_opcode_bd(pir1)) {
    /*head*/
    case WRAP(CMP):
    case WRAP(CQO):
    case WRAP(XOR):
    case WRAP(CDQ):
    case WRAP(TEST):
    case WRAP(UCOMISD):
    /*tail*/
    case WRAP(SBB):
    case WRAP(IDIV):
    case WRAP(DIV):
    case WRAP(SETC):     //SETB
    case WRAP(SETNC):    //SETAE
    case WRAP(SETZ):     //SETE
    case WRAP(SETNZ):    //SETNE
    case WRAP(SETBE):
    case WRAP(SETNBE):   //SETA
    case WRAP(SETL):
    case WRAP(SETNL):    //SETGE
    case WRAP(SETLE):
    case WRAP(SETNLE):   //SETG
    case WRAP(SETS):
    case WRAP(SETNS):
    case WRAP(SETNO):
    case WRAP(SETO):
    case WRAP(CMOVZ):    //CMOVE
    case WRAP(CMOVNZ):   //CMOVNE
    case WRAP(CMOVS):
    case WRAP(CMOVNS):
    case WRAP(CMOVLE):
    case WRAP(CMOVNLE):  //CMOVG
    case WRAP(CMOVNO):
    case WRAP(CMOVO):
    case WRAP(CMOVC):    //CMOVB
    case WRAP(CMOVBE):
    case WRAP(CMOVNBE):  //CMOVA
    case WRAP(CMOVNC):   //CMOVAE
    case WRAP(CMOVL):
    case WRAP(CMOVNL):   //CMOVGE
        return true;
     default:
        return false;
     }
 }

static inline bool ir1_is_pattern_head(IR1_INST *pir1)
{
    switch (ir1_opcode_bd(pir1)) {
    case WRAP(CMP):
    case WRAP(CQO):
    case WRAP(XOR):
    case WRAP(CDQ):
    case WRAP(TEST):
    case WRAP(UCOMISD):
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

static bool is_contain_edx(IR1_OPND_BD *opnd)
{
    if (ir1_opnd_is_gpr_bd(opnd)) {
        if (ir1_opnd_size_bd(opnd) == ND_SIZE_8BIT) {
            if (opnd->Info.Register.Reg == NDR_DL ||
                opnd->Info.Register.Reg == NDR_DH)
                return true;
        }
        if (opnd->Info.Register.Reg == NDR_DX) //NDR_EDX NDR_RDX
            return true;
    } else if (ir1_opnd_is_mem_bd(opnd)) {
        if (opnd->Info.Memory.Base == NDR_DX ||
            opnd->Info.Memory.Index == NDR_DX) //NDR_EDX NDR_RDX
            return true;
    }
    return false;
}

static int inst_pattern(TranslationBlock *tb,
        IR1_INST *pir1, scan_elem_t *scan)
{
    IR1_INST *ir1 = NULL;
    IR1_OPND_BD *opnd0 = NULL;
    IR1_OPND_BD *opnd1 = NULL;

    /*
     * pir1 is pattern head
     * scan[] contains ir1 following the head
     */
    switch (ir1_opcode_bd(pir1)) {
    case WRAP(CMP): {
        SCAN_CHECK(scan, 0);
        ir1 = SCAN_IR1(tb, scan, 0);
        if(ir1_opcode_bd(ir1) == WRAP(SBB)) {
            instptn_check_cmp_sbb_0();

            opnd0 = ir1_get_opnd_bd(ir1, 0);
            opnd1 = ir1_get_opnd_bd(ir1, 1);
            if (!ir1_opnd_is_same_reg_bd(opnd0, opnd1)) {
                return 0;
            }
            pir1->instptn.opc  = INSTPTN_OPC_CMP_SBB;
            pir1->instptn.next = ir1;
            ir1->instptn.opc  = INSTPTN_OPC_NOP;
            // ir1->instptn.next = NULL;
            return 1;
        }
        instptn_check_cmp_xxcc_con_0();
        IR1_INST *curr = ir1;
        IR1_INST *prev = pir1;
        for (int index = 0; index < INSTPTN_BUF_SIZE && scan[index] >=0; index++) {
            curr = SCAN_IR1(tb, scan, index);
            switch (ir1_opcode_bd(curr)) {
            case WRAP(SETC):     //SETB
            case WRAP(SETNC):    //SETAE
            case WRAP(SETZ):     //SETE
            case WRAP(SETNZ):    //SETNE
            case WRAP(SETBE):
            case WRAP(SETNBE):   //SETA
            case WRAP(SETL):
            case WRAP(SETNL):    //SETGE
            case WRAP(SETLE):
            case WRAP(SETNLE):   //SETG
            case WRAP(CMOVC):    //CMOVB
            case WRAP(CMOVNC):   //CMOVAE
            case WRAP(CMOVZ):    //CMOVE
            case WRAP(CMOVNZ):   //CMOVNE
            case WRAP(CMOVBE):
            case WRAP(CMOVNBE):  //CMOVA
            case WRAP(CMOVL):
            case WRAP(CMOVNL):   //CMOVGE
            case WRAP(CMOVLE):
            case WRAP(CMOVNLE):  //CMOVG
                if (index == 0) {
                    prev->instptn.opc  = INSTPTN_OPC_CMP_XXCC_CON;
                } else {
                    prev->instptn.opc  = INSTPTN_OPC_NOP_FOR_EXP;
                }
                prev->instptn.next = curr;
                curr->instptn.opc  = INSTPTN_OPC_NOP_FOR_EXP;
                // curr->instptn.next = NULL;
                prev = curr;
                break;
            default:
                return 1;
            }
        }
        return 1;
    }
    case WRAP(TEST): {
        SCAN_CHECK(scan, 0);
        instptn_check_test_xxcc_con_0();
        IR1_INST *curr = ir1;
        IR1_INST *prev = pir1;
        for (int index = 0; index < INSTPTN_BUF_SIZE && scan[index] >=0; index++) {
            curr = SCAN_IR1(tb, scan, index);
            switch (ir1_opcode_bd(curr)) {
            case WRAP(SETS):
            case WRAP(SETNS):
            case WRAP(SETLE):
            case WRAP(SETNLE):   //SETG
            case WRAP(CMOVS):
            case WRAP(CMOVNS):
            case WRAP(CMOVLE):
            case WRAP(CMOVNLE):  //CMOVG
                opnd0 = ir1_get_opnd_bd(pir1, 0);
                opnd1 = ir1_get_opnd_bd(pir1, 1);
                if (!ir1_opnd_is_same_reg_bd(opnd0, opnd1)) {
                    return 1;
                }
                __attribute__((fallthrough));
            case WRAP(SETZ):     //SETE
            case WRAP(SETNZ):    //SETNE
            case WRAP(SETNO):
            case WRAP(SETO):
            case WRAP(SETC):     //SETB
            case WRAP(SETBE):
            case WRAP(SETNBE):   //SETA
            case WRAP(SETNC):    //SETAE
            case WRAP(CMOVZ):    //CMOVE
            case WRAP(CMOVNZ):   //CMOVNE
            case WRAP(CMOVNO):
            case WRAP(CMOVO):
            case WRAP(CMOVC):    //CMOVB
            case WRAP(CMOVBE):
            case WRAP(CMOVNBE):  //CMOVA
            case WRAP(CMOVNC):   //CMOVAE
                if (index == 0) {
                    prev->instptn.opc  = INSTPTN_OPC_TEST_XXCC_CON;
                } else {
                    prev->instptn.opc  = INSTPTN_OPC_NOP_FOR_EXP;
                }
                prev->instptn.next = curr;
                curr->instptn.opc  = INSTPTN_OPC_NOP_FOR_EXP;
                // curr->instptn.next = NULL;
                prev = curr;
                break;
            default:
                return 1;
            }
        }
        return 1;
    }
    case WRAP(CQO):
        SCAN_CHECK(scan, 0);
        instptn_check_cqo_idiv_0();

        ir1 = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode_bd(ir1)) {
        case WRAP(IDIV):
            opnd0 = ir1_get_opnd_bd(ir1, 0);
            if (!ir1_opnd_is_gpr_bd(opnd0))
                return 0;
            if (ir1_opnd_size_bd(opnd0) != 64)
                return 0;
            if (is_contain_edx(opnd0))
                return 0;
            // if (ir1_opnd_is_mem(ir1_get_opnd_bd(ir1, 0))) {
            //     return 0;
            // }
            pir1->instptn.opc  = INSTPTN_OPC_CQO_IDIV;
            pir1->instptn.next = ir1;
            ir1->instptn.opc  = INSTPTN_OPC_NOP;
            // ir1->instptn.next = NULL;
            return 1;
        default:
            return 0;
        }
    case WRAP(XOR):
        SCAN_CHECK(scan, 0);
        instptn_check_xor_div_0();

        opnd0 = ir1_get_opnd_bd(pir1, 0);
        opnd1 = ir1_get_opnd_bd(pir1, 1);

        ir1 = SCAN_IR1(tb, scan, 0);
        if (ir1_opcode_bd(ir1) == WRAP(DIV)) {
            if (ir1_opnd_is_gpr_bd(opnd0) && ir1_opnd_is_gpr_bd(opnd1) &&
                ((opnd0->Info.Register.Reg == NDR_EDX && opnd1->Info.Register.Reg == NDR_EDX &&
                ir1_opnd_size_bd(ir1_get_opnd_bd(ir1, 0)) == 32 &&
                ir1_opnd_is_gpr_bd(ir1_get_opnd_bd(ir1, 0))) ||
                (opnd0->Info.Register.Reg == NDR_RDX && opnd1->Info.Register.Reg == NDR_RDX &&
                ir1_opnd_size_bd(ir1_get_opnd_bd(ir1, 0)) == 64 &&
                ir1_opnd_is_gpr_bd(ir1_get_opnd_bd(ir1, 0))))) {
                    if (is_contain_edx(opnd0))
                        return 0;
                    pir1->instptn.opc  = INSTPTN_OPC_XOR_DIV;
                    pir1->instptn.next = ir1;
                    ir1->instptn.opc  = INSTPTN_OPC_NOP;
                    // ir1->instptn.next = NULL;
                    return 1;
                }
        }
        return 0;
    case WRAP(CDQ):
        SCAN_CHECK(scan, 0);
        instptn_check_cdq_idiv_0();

        ir1 = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode_bd(ir1)) {
        case WRAP(IDIV):
            opnd0 = ir1_get_opnd_bd(ir1, 0);
            if (!ir1_opnd_is_gpr_bd(opnd0))
                return 0;
            if (ir1_opnd_size_bd(opnd0) != 32)
                return 0;
            if (is_contain_edx(opnd0))
                return 0;
            // if (ir1_opnd_is_mem(opnd0)) {
            //     return 0;
            // }
            pir1->instptn.opc  = INSTPTN_OPC_CDQ_IDIV;
            pir1->instptn.next = ir1;
            ir1->instptn.opc  = INSTPTN_OPC_NOP;
            // ir1->instptn.next = NULL;
            return 1;
        default:
            return 0;
        }
    case WRAP(UCOMISD):
        SCAN_CHECK(scan, 0);
        instptn_check_ucomisd_seta_0();

        ir1 = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode_bd(ir1)) {
        case WRAP(SETNBE):
            pir1->instptn.opc  = INSTPTN_OPC_UCOMISD_SETA;
            pir1->instptn.next = ir1;
            ir1->instptn.opc  = INSTPTN_OPC_NOP_FOR_EXP;
            // ir1->instptn.next = NULL;
            return 1;
        default:
            return 0;
        }
    default:
        return 0;
    }
}

void insts_pattern_scan_con_bd(TranslationBlock *tb, IR1_INST *ir1, int index, scan_elem_t *scan_buf)
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

bool insts_pattern_scan_jcc_end_bd(TranslationBlock *tb, IR1_INST *pir1, int pir1_index, scan_elem_t *scan)
{

    if (pir1_index == tb_ir1_num_bd(tb) - 1) {
        if (!pir1_index) return false; /* tb->icount > 1*/
        switch (ir1_opcode_bd(pir1)) {
        case WRAP(JNBE):    //JA
        case WRAP(JNC):     //JAE
        case WRAP(JC):      //JB
        case WRAP(JZ):      //JE
        case WRAP(JNZ):     //JNE
        case WRAP(JBE):
        case WRAP(JL):
        case WRAP(JNL):     //JGE
        case WRAP(JLE):
        case WRAP(JNLE):    //JG
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
    IR1_OPND_BD *opnd0 = NULL;
    IR1_OPND_BD *opnd1 = NULL;
    switch (ir1_opcode_bd(pir1)) {
        case WRAP(CMP):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode_bd(ir1_jcc)) {
        case WRAP(JC):
        case WRAP(JNC):
        case WRAP(JZ):
        case WRAP(JNZ):
        case WRAP(JBE):
        case WRAP(JNBE):
        case WRAP(JL):
        case WRAP(JNL):
        case WRAP(JLE):
        case WRAP(JNLE):
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
                ir1_jcc->instptn.next = tb_ir1_inst_bd(tb, pir1_index);
            }
            return false;
        default:
            return false;
        }
    case WRAP(TEST):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode_bd(ir1_jcc)) {
        case WRAP(JS):
        case WRAP(JNS):
        case WRAP(JLE):
        case WRAP(JNLE):
            opnd0 = ir1_get_opnd_bd(pir1, 0);
            opnd1 = ir1_get_opnd_bd(pir1, 1);
            if (!ir1_opnd_is_same_reg_bd(opnd0, opnd1)) {
                return false;
            }
            __attribute__((fallthrough));
        case WRAP(JZ):
        case WRAP(JNZ):
        case WRAP(JNO):
        case WRAP(JO):
        case WRAP(JC):
        case WRAP(JBE):
        case WRAP(JNBE):
        case WRAP(JNC):
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
                ir1_jcc->instptn.next = tb_ir1_inst_bd(tb, pir1_index);
            }
            return false;
        default:
            return false;
        }
    case WRAP(BT):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode_bd(ir1_jcc)) {
        case WRAP(JC):
        case WRAP(JNC):
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
                ir1_jcc->instptn.next = tb_ir1_inst_bd(tb, pir1_index);
            }
            return false;
        default:
            return false;
        }
    case WRAP(SUB):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode_bd(ir1_jcc)) {
        case WRAP(JC):
        case WRAP(JNC):
        case WRAP(JZ):
        case WRAP(JNZ):
        case WRAP(JBE):
        case WRAP(JNBE):
        case WRAP(JL):
        case WRAP(JNL):
        case WRAP(JLE):
        case WRAP(JNLE):
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
#ifdef CONFIG_LATX_XCOMISX_OPT
    case WRAP(COMISD):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode_bd(ir1_jcc)) {
        case WRAP(JNBE):
        case WRAP(JNC):
        case WRAP(JC):
        case WRAP(JBE):
        case WRAP(JNZ):
        case WRAP(JZ):
        case WRAP(JL):
        case WRAP(JNL):
        case WRAP(JLE):
        case WRAP(JNLE):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_comisd_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_COMISD_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else {
                instptn_check_comisd_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_COMISD_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_COMISD_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst_bd(tb, pir1_index);
            }
            return false;
        default:
            return false;
        }
    case WRAP(COMISS):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode_bd(ir1_jcc)) {
        case WRAP(JNBE):
        case WRAP(JNC):
        case WRAP(JC):
        case WRAP(JZ):
        case WRAP(JNZ):
        case WRAP(JBE):
        case WRAP(JL):
        case WRAP(JNL):
        case WRAP(JLE):
        case WRAP(JNLE):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_comiss_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_COMISS_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else {
                instptn_check_comiss_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_COMISS_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_COMISS_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst_bd(tb, pir1_index);
            }
            return false;
        default:
            return false;
        }
    case WRAP(UCOMISD):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode_bd(ir1_jcc)) {
        case WRAP(JNBE):
        case WRAP(JNC):
        case WRAP(JC):
        case WRAP(JZ):
        case WRAP(JNZ):
        case WRAP(JBE):
        case WRAP(JL):
        case WRAP(JNL):
        case WRAP(JLE):
        case WRAP(JNLE):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_ucomisd_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_UCOMISD_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else {
                instptn_check_ucomisd_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_UCOMISD_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_UCOMISD_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst_bd(tb, pir1_index);
            }
            return false;
        default:
            return false;
        }
    case WRAP(UCOMISS):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode_bd(ir1_jcc)) {
        case WRAP(JNBE):
        case WRAP(JNC):
        case WRAP(JC):
        case WRAP(JZ):
        case WRAP(JNZ):
        case WRAP(JBE):
        case WRAP(JL):
        case WRAP(JNL):
        case WRAP(JLE):
        case WRAP(JNLE):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_ucomiss_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_UCOMISS_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                ir1_jcc->instptn.next = NULL;
            } else {
                instptn_check_ucomiss_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_UCOMISS_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_UCOMISS_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst_bd(tb, pir1_index);
            }
            return false;
        default:
            return false;
        }
#endif
    case WRAP(ADDSD):
    case WRAP(ADDSS):
    case WRAP(LEA):
    case WRAP(MOV):
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

#undef WRAP

#endif
