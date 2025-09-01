#include "env.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate-bd.h"
#include "flag-lbt-bd.h"

bool generate_eflag_by_lbt_bd(IR2_OPND dest, IR2_OPND src0, IR2_OPND src1,
                           IR1_INST *pir1, bool is_imm)
{
    switch (ir1_opcode_bd(pir1)) {
    case ND_INS_XADD:
    case ND_INS_ADD: {
        GENERATE_EFLAG_IR2_2(la_x86add);
        break;
    }
    case ND_INS_ADC: {
        GENERATE_EFLAG_IR2_2(la_x86adc);
        break;
    }
    case ND_INS_INC: {
        GENERATE_EFLAG_IR2_1(la_x86inc);
        break;
    }
    case ND_INS_DEC: {
        GENERATE_EFLAG_IR2_1(la_x86dec);
        break;
    }
    case ND_INS_CMPS:
    case ND_INS_SCAS:
    case ND_INS_CMPXCHG:
    case ND_INS_NEG:
    case ND_INS_CMP:
    case ND_INS_SUB: {
        GENERATE_EFLAG_IR2_2(la_x86sub);
        break;
    }
    case ND_INS_SBB: {
        GENERATE_EFLAG_IR2_2(la_x86sbc);
        break;
    }
    case ND_INS_BLSMSK:
    case ND_INS_XOR: {
        GENERATE_EFLAG_IR2_2(la_x86xor);
        break;
    }
    case ND_INS_TEST:
    case ND_INS_BLSI:
    case ND_INS_BLSR:
    case ND_INS_BZHI:
    case ND_INS_AND:
    case ND_INS_ANDN: {
        GENERATE_EFLAG_IR2_2(la_x86and);
        break;
    }
    case ND_INS_OR: {
        GENERATE_EFLAG_IR2_2(la_x86or);
        break;
    }
    case ND_INS_SAL:
    case ND_INS_SHL: {
        if (is_imm) {
            GENERATE_EFLAG_IR2_2_I(la_x86slli);
        } else {
            GENERATE_EFLAG_IR2_2(la_x86sll);
        }
        break;
    }
    case ND_INS_SHR: {
        if (is_imm) {
            GENERATE_EFLAG_IR2_2_I(la_x86srli);
        } else {
            GENERATE_EFLAG_IR2_2(la_x86srl);
        }
        break;
    }
    case ND_INS_SAR: {
        if (is_imm) {
            GENERATE_EFLAG_IR2_2_I(la_x86srai);
        } else {
            GENERATE_EFLAG_IR2_2(la_x86sra);
        }
        break;
    }
    case ND_INS_RCL: {
        GENERATE_EFLAG_IR2_2(la_x86rcl);
        break;
    }
    case ND_INS_RCR: {
        GENERATE_EFLAG_IR2_2(la_x86rcr);
        break;
    }
    case ND_INS_MUL: {
        /*
         * 3A500 has new MUL insn which could leverage directly.
         */
        GENERATE_EFLAG_IR2_2_U(la_x86mul);
        break;
    }
    case ND_INS_IMUL: {
        GENERATE_EFLAG_IR2_2(la_x86mul);
        break;
    }
    case ND_INS_ROR:
    case ND_INS_ROL: {
	    break;
	}
#ifdef CONFIG_LATX_XCOMISX_OPT
    case ND_INS_COMISS:
    case ND_INS_COMISD:
    case ND_INS_UCOMISS:
    case ND_INS_UCOMISD:
#endif
    case ND_INS_AAM:
    case ND_INS_AAD:
    case ND_INS_AAA:
    case ND_INS_DAA:
    case ND_INS_DAS:
    case ND_INS_BSF:
    case ND_INS_BSR:
    case ND_INS_BT:
    case ND_INS_BTS:
    case ND_INS_BTR:
    case ND_INS_BTC:
    case ND_INS_SHLD:
    case ND_INS_SHRD:
    case ND_INS_CMPXCHG8B:
    case ND_INS_CMPXCHG16B:
    case ND_INS_LZCNT: {
	    return false;
        }
        default:
            lsassertm(0, "%s (%x) is not implemented in %s\n",
                      ((INSTRUX *)(pir1->info))->Mnemonic, ir1_opcode_bd(pir1), __func__);
            return false;
        }
    return true;
}

void get_eflag_condition_bd(IR2_OPND *cond, IR1_INST *pir1) {
    switch(ir1_opcode_bd(pir1)) {
        /* CF */
        case ND_INS_SETNC:  //SETAE:
        case ND_INS_CMOVNC: //CMOVAE:
        case ND_INS_FCMOVNB:
        case ND_INS_JNC:    //JAE
        {
            la_setx86j(*cond, COND_AE);
            break;
        }
        case ND_INS_SETC:   //SETB:
        case ND_INS_CMOVC:  //CMOVB:
        case ND_INS_FCMOVB:
        case ND_INS_JC:     //JB
        {
            la_setx86j(*cond, COND_B);
            break;
        }
        case ND_INS_RCL:
        case ND_INS_RCR: {
            la_x86mfflag(*cond, 0x1);
            break;
        }
        /* PF */
        case ND_INS_SETNP:
        case ND_INS_CMOVNP:
        case ND_INS_FCMOVNU:
        case ND_INS_JNP:
        {
            la_setx86j(*cond, COND_PO);
            break;
        }
        case ND_INS_SETP:
        case ND_INS_CMOVP:
        case ND_INS_FCMOVU:
        case ND_INS_JP:
        {
            la_setx86j(*cond, COND_PE);
            break;
        }
        /* ZF */
        case ND_INS_SETZ:   //SETE:
        case ND_INS_CMOVZ:  //CMOVE:
        case ND_INS_FCMOVE:
        case ND_INS_JZ:     //JE:
        {
            la_setx86j(*cond, COND_E);
            break;
        }
        case ND_INS_SETNZ:  //SETNE
        case ND_INS_CMOVNZ: //CMOVNE
        case ND_INS_FCMOVNE:
        case ND_INS_JNZ:    //JNE
        {
            la_setx86j(*cond, COND_NE);
            break;
        }
        /* SF */
        case ND_INS_SETS:
        case ND_INS_CMOVS:
        case ND_INS_JS:
        {
            la_setx86j(*cond, COND_S);
            break;
        }
        case ND_INS_SETNS:
        case ND_INS_CMOVNS:
        case ND_INS_JNS:
        {
            la_setx86j(*cond, COND_NS);
            break;
        }
        /* OF */
        case ND_INS_SETO:
        case ND_INS_CMOVO:
        case ND_INS_JO:
        {
            la_setx86j(*cond, COND_O);
            break;
        }
        case ND_INS_SETNO:
        case ND_INS_CMOVNO:
        case ND_INS_JNO:
        {
            la_setx86j(*cond, COND_NO);
            break;
        }
        /* CF ZF */
        case ND_INS_SETBE:
        case ND_INS_CMOVBE:
        case ND_INS_FCMOVBE:
        case ND_INS_JBE:
        {
            la_setx86j(*cond, COND_BE);
            break;
        }
        case ND_INS_SETNBE: //SETA:
        case ND_INS_CMOVNBE://CMOVA:
        case ND_INS_FCMOVNBE:
        case ND_INS_JNBE:   //JA:
        {
            la_setx86j(*cond, COND_A);
            break;
        }
        case ND_INS_LOOPZ:      //LOOPE:
        case ND_INS_LOOPNZ: {   //LOOPNE:
            la_x86mfflag(*cond, 0x8);
            la_slli_d(*cond, *cond, 63 - ZF_BIT_INDEX);
            la_srai_d(*cond, *cond, 63);
            break;
        }

        /* SF OF */
        case ND_INS_SETL:
        case ND_INS_CMOVL:
        case ND_INS_JL:
        {
            la_setx86j(*cond, COND_L);
            break;
        }
        case ND_INS_SETNL:  //SETGE
        case ND_INS_CMOVNL: //CMOVGE
        case ND_INS_JNL:    //JGE
        {
            la_setx86j(*cond, COND_GE);
            break;
        }
        /* ZF SF OF */
        case ND_INS_SETLE:
        case ND_INS_CMOVLE:
        case ND_INS_JLE:
        {
            la_setx86j(*cond, COND_LE);
            break;
        }
        case ND_INS_SETNLE: //SETG
        case ND_INS_CMOVNLE://CMOVG:
        case ND_INS_JNLE:   //JG:
        {
            la_setx86j(*cond, COND_G);
            break;
        }
    default: {
        lsassertm(0, "%s for %s is not implemented\n", __func__,
                  ir1_name_bd(pir1));
    }
    }
}
