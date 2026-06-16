/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "env.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate.h"
#include "flag-lbt.h"

bool generate_eflag_by_lbt(IR2_OPND dest, IR2_OPND src0, IR2_OPND src1,
                           IR1_INST *pir1, bool is_imm)
{
    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_XADD:
    case dt_X86_INS_ADD: {
        GENERATE_EFLAG_IR2_2(la_x86add);
        break;
    }
    case dt_X86_INS_ADC: {
        GENERATE_EFLAG_IR2_2(la_x86adc);
        break;
    }
    case dt_X86_INS_INC: {
        GENERATE_EFLAG_IR2_1(la_x86inc);
        break;
    }
    case dt_X86_INS_DEC: {
        GENERATE_EFLAG_IR2_1(la_x86dec);
        break;
    }
    case dt_X86_INS_CMPSB:
    case dt_X86_INS_CMPSW:
    case dt_X86_INS_CMPSD:
    case dt_X86_INS_CMPSQ:
    case dt_X86_INS_SCASB:
    case dt_X86_INS_SCASW:
    case dt_X86_INS_SCASD:
    case dt_X86_INS_SCASQ:
    case dt_X86_INS_CMPXCHG:
    case dt_X86_INS_NEG:
    case dt_X86_INS_CMP:
    case dt_X86_INS_SUB: {
        GENERATE_EFLAG_IR2_2(la_x86sub);
        break;
    }
    case dt_X86_INS_SBB: {
        GENERATE_EFLAG_IR2_2(la_x86sbc);
        break;
    }
    case dt_X86_INS_BLSMSK:
    case dt_X86_INS_XOR: {
        GENERATE_EFLAG_IR2_2(la_x86xor);
        break;
    }
    case dt_X86_INS_TEST:
    case dt_X86_INS_BLSI:
    case dt_X86_INS_BLSR:
    case dt_X86_INS_BZHI:
    case dt_X86_INS_AND:
    case dt_X86_INS_ANDN: {
        GENERATE_EFLAG_IR2_2(la_x86and);
        break;
    }
    case dt_X86_INS_OR: {
        GENERATE_EFLAG_IR2_2(la_x86or);
        break;
    }
    case dt_X86_INS_SAL:
    case dt_X86_INS_SHL: {
        if (is_imm) {
            GENERATE_EFLAG_IR2_2_I(la_x86slli);
        } else {
            GENERATE_EFLAG_IR2_2(la_x86sll);
        }
        break;
    }
    case dt_X86_INS_SHR: {
        if (is_imm) {
            GENERATE_EFLAG_IR2_2_I(la_x86srli);
        } else {
            GENERATE_EFLAG_IR2_2(la_x86srl);
        }
        break;
    }
    case dt_X86_INS_SAR: {
        if (is_imm) {
            GENERATE_EFLAG_IR2_2_I(la_x86srai);
        } else {
            GENERATE_EFLAG_IR2_2(la_x86sra);
        }
        break;
    }
    case dt_X86_INS_RCL: {
        GENERATE_EFLAG_IR2_2(la_x86rcl);
        break;
    }
    case dt_X86_INS_RCR: {
        GENERATE_EFLAG_IR2_2(la_x86rcr);
        break;
    }
    case dt_X86_INS_MUL: {
        /*
         * 3A500 has new MUL insn which could leverage directly.
         */
        GENERATE_EFLAG_IR2_2_U(la_x86mul);
        break;
    }
    case dt_X86_INS_IMUL: {
        GENERATE_EFLAG_IR2_2(la_x86mul);
        break;
    }
    case dt_X86_INS_ROR:
    case dt_X86_INS_ROL: {
	    break;
	}
#ifdef CONFIG_LATX_XCOMISX_OPT
    case dt_X86_INS_COMISS:
    case dt_X86_INS_COMISD:
    case dt_X86_INS_UCOMISS:
    case dt_X86_INS_UCOMISD:
#endif
    case dt_X86_INS_AAM:
    case dt_X86_INS_AAD:
    case dt_X86_INS_AAA:
    case dt_X86_INS_DAA:
    case dt_X86_INS_DAS:
    case dt_X86_INS_BSF:
    case dt_X86_INS_BSR:
    case dt_X86_INS_BT:
    case dt_X86_INS_BTS:
    case dt_X86_INS_BTR:
    case dt_X86_INS_BTC:
    case dt_X86_INS_SHLD:
    case dt_X86_INS_SHRD:
    case dt_X86_INS_CMPXCHG8B:
    case dt_X86_INS_CMPXCHG16B:
    case dt_X86_INS_LZCNT: {
	    return false;
        }
        default:
            lsassertm(0, "%s (%x) is not implemented in %s\n",
                      pir1->info->mnemonic, ir1_opcode(pir1), __func__);
            return false;
        }
    return true;
}

void get_eflag_condition(IR2_OPND *cond, IR1_INST *pir1) {
    switch(ir1_opcode(pir1)) {
        /* CF */
        case dt_X86_INS_SETAE:
        case dt_X86_INS_CMOVAE:
        case dt_X86_INS_FCMOVNB:
        case dt_X86_INS_JAE:
        {
            la_setx86j(*cond, COND_AE);
            break;
        }
        case dt_X86_INS_SETB:
        case dt_X86_INS_CMOVB: 
        case dt_X86_INS_FCMOVB:
        case dt_X86_INS_JB:
        {
            la_setx86j(*cond, COND_B);
            break;
        }
        case dt_X86_INS_RCL:
        case dt_X86_INS_RCR: {
            la_x86mfflag(*cond, 0x1);
            break;
        }
        /* PF */
        case dt_X86_INS_SETNP:
        case dt_X86_INS_CMOVNP:
        case dt_X86_INS_FCMOVNU:
        case dt_X86_INS_JNP:
        {
            la_setx86j(*cond, COND_PO);
            break;
        }
        case dt_X86_INS_SETP:
        case dt_X86_INS_CMOVP:
        case dt_X86_INS_FCMOVU:
        case dt_X86_INS_JP:
        {
            la_setx86j(*cond, COND_PE);
            break;
        }
        /* ZF */
        case dt_X86_INS_SETE:
        case dt_X86_INS_CMOVE:
        case dt_X86_INS_FCMOVE:
        case dt_X86_INS_JE:
        {
            la_setx86j(*cond, COND_E);
            break;
        }
        case dt_X86_INS_SETNE:
        case dt_X86_INS_CMOVNE:
        case dt_X86_INS_FCMOVNE:
        case dt_X86_INS_JNE:
        {
            la_setx86j(*cond, COND_NE);
            break;
        }
        /* SF */
        case dt_X86_INS_SETS:
        case dt_X86_INS_CMOVS:
        case dt_X86_INS_JS:
        {
            la_setx86j(*cond, COND_S);
            break;
        }
        case dt_X86_INS_SETNS:
        case dt_X86_INS_CMOVNS:
        case dt_X86_INS_JNS:
        {
            la_setx86j(*cond, COND_NS);
            break;
        }
        /* OF */
        case dt_X86_INS_SETO:
        case dt_X86_INS_CMOVO:
        case dt_X86_INS_JO:
        {
            la_setx86j(*cond, COND_O);
            break;
        }
        case dt_X86_INS_SETNO:
        case dt_X86_INS_CMOVNO:
        case dt_X86_INS_JNO:
        {
            la_setx86j(*cond, COND_NO);
            break;
        }
        /* CF ZF */
        case dt_X86_INS_SETBE:
        case dt_X86_INS_CMOVBE:
        case dt_X86_INS_FCMOVBE:
        case dt_X86_INS_JBE:
        {
            la_setx86j(*cond, COND_BE);
            break;
        }
        case dt_X86_INS_SETA:
        case dt_X86_INS_CMOVA:
        case dt_X86_INS_FCMOVNBE:
        case dt_X86_INS_JA:
        {
            la_setx86j(*cond, COND_A);
            break;
        }
        case dt_X86_INS_LOOPE:
        case dt_X86_INS_LOOPNE: {
            la_x86mfflag(*cond, 0x8);
            la_slli_d(*cond, *cond, 63 - ZF_BIT_INDEX);
            la_srai_d(*cond, *cond, 63);
            break;
        }

        /* SF OF */
        case dt_X86_INS_SETL:
        case dt_X86_INS_CMOVL:
        case dt_X86_INS_JL:
        {
            la_setx86j(*cond, COND_L);
            break;
        }
        case dt_X86_INS_SETGE:
        case dt_X86_INS_CMOVGE:
        case dt_X86_INS_JGE:
        {
            la_setx86j(*cond, COND_GE);
            break;
        }
        /* ZF SF OF */
        case dt_X86_INS_SETLE:
        case dt_X86_INS_CMOVLE:
        case dt_X86_INS_JLE:
        {
            la_setx86j(*cond, COND_LE);
            break;
        }
        case dt_X86_INS_SETG:
        case dt_X86_INS_CMOVG:
        case dt_X86_INS_JG:
        {
            la_setx86j(*cond, COND_G);
            break;
        }
    default: {
        lsassertm(0, "%s for %s is not implemented\n", __func__,
                  ir1_name(ir1_opcode(pir1)));
    }
    }
}
