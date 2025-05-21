/**
 * @file hbr.c
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief HBR optimization
 */
#include "lsenv.h"
#include "ir1.h"
#include "hbr.h"
#include "translate.h"
#include "reg-alloc.h"
#include "latx-options.h"

#if defined(CONFIG_LATX_HBR) && defined(CONFIG_LATX_TU)

#define WRAP(ins) (dt_X86_INS_##ins)

uint8_t get_inst_type(IR1_INST *ir1)
{
    int opnd_num = ir1_get_opnd_num(ir1);
    for (int i = 0; i < opnd_num; ++i) {
        if (ir1_opnd_is_xmm(ir1_get_opnd(ir1, i))) {
            return SHBR_SSE;
        }
    }
    return SHBR_NTYPE;
}

static inline void src_des_update_des(IR1_INST *ir1, uint32_t *xmm)
{

    IR1_OPND *opnd0 = ir1_get_opnd(ir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(ir1, 1);

    if (!ir1_opnd_is_xmm(opnd1)) {
        return;
    } else if (!ir1_opnd_is_xmm(opnd0)) {
        return;
    }

    assert(ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1));
    int dest_num = ir1_opnd_base_reg_num(opnd0);
    int src_num = ir1_opnd_base_reg_num(opnd1);
    ir1->xmm_def |= SHBR_UPDATE_DES;
    ir1->xmm_use |= SHBR_NEED_SRC | SHBR_NEED_DES;
    xmm[dest_num] |= xmm[src_num];
}

static inline void src_update_des(IR1_INST *ir1, uint32_t *xmm)
{
    ir1->xmm_def |= SHBR_UPDATE_DES;
    ir1->xmm_use |= SHBR_NEED_SRC;
    IR1_OPND *opnd0 = ir1_get_opnd(ir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(ir1, 1);
    if (!ir1_opnd_is_xmm(opnd1)) {
        return;
    } else if (!ir1_opnd_is_xmm(opnd0)) {
        return;
    }
    assert(ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1));
    int src_num = ir1_opnd_base_reg_num(opnd1);
    int dest_num = ir1_opnd_base_reg_num(opnd0);
    xmm[dest_num] = xmm[src_num];
}

static inline void other_update_des(IR1_INST *ir1, uint32_t *xmm)
{
    IR1_OPND *opnd0 = ir1_get_opnd(ir1, 0);
    if (!ir1_opnd_is_xmm(opnd0)) {
        return;
    }
    int dest_num = ir1_opnd_base_reg_num(opnd0);
    ir1->xmm_def = SHBR_UPDATE_DES;
    xmm[dest_num] = SHBR_XMM_OTHER;
}

static inline void zero_update_des(IR1_INST *ir1, uint32_t *xmm)
{
    IR1_OPND *opnd0 = ir1_get_opnd(ir1, 0);
    if (!ir1_opnd_is_xmm(opnd0)) {
        return;
    }
    int dest_num = ir1_opnd_base_reg_num(opnd0);
    ir1->xmm_def |= SHBR_UPDATE_DES;
    xmm[dest_num] = SHBR_XMM_ZERO;
}

static inline void src_no_opt(IR1_INST *ir1, uint32_t *xmm)
{
    IR1_OPND *opnd1 = ir1_get_opnd(ir1, 1);
    if (!ir1_opnd_is_xmm(opnd1)) {
        return;
    }
    int src_num = ir1_opnd_base_reg_num(opnd1);
    ir1->xmm_use |= SHBR_NO_OPT_SRC;
    ir1->xmm_use |= xmm[src_num];
}

static inline void des_no_opt(IR1_INST *ir1, uint32_t *xmm)
{
    IR1_OPND *opnd0 = ir1_get_opnd(ir1, 0);
    if (!ir1_opnd_is_xmm(opnd0)) {
        return;
    }
    int des_num = ir1_opnd_base_reg_num(opnd0);
    ir1->xmm_use |= SHBR_NO_OPT_DES;
    ir1->xmm_use |= xmm[des_num];
}

static bool deal_xmm_common(TranslationBlock *tb, IR1_INST *ir1, uint32_t *xmm)
{
    IR1_OPND *des_opnd = ir1_get_opnd(ir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(ir1, 1);
    int src_num = -1, des_num = -2;
    if (ir1_opnd_is_xmm(src_opnd)) {
        src_num = ir1_opnd_base_reg_num(src_opnd);
    }
    if (ir1_opnd_is_xmm(des_opnd)) {
        des_num = ir1_opnd_base_reg_num(des_opnd);
    }

    switch (ir1_opcode(ir1)) {
    case WRAP(ADDPD):
    case WRAP(ADDPS):
    case WRAP(ADDSUBPD):
    case WRAP(ADDSUBPS):
    case WRAP(PADDB):
    case WRAP(PADDW):
    case WRAP(PADDD):
    case WRAP(PADDQ):
    case WRAP(PADDSB):
    case WRAP(PADDSW):
    case WRAP(PADDUSB):
    case WRAP(PADDUSW):
    case WRAP(PMADDWD):
    case WRAP(PMADDUBSW):
    case WRAP(PSUBB):
    case WRAP(PSUBW):
    case WRAP(PSUBD):
    case WRAP(PSUBQ):
    case WRAP(PSUBSB):
    case WRAP(PSUBSW):
    case WRAP(PSUBUSB):
    case WRAP(PSUBUSW):
    case WRAP(SUBPS):
    case WRAP(SUBPD):
    case WRAP(DIVPD):
    case WRAP(DIVPS):
    case WRAP(PMULDQ):
    case WRAP(PMULUDQ):
    case WRAP(MULPD):
    case WRAP(MULPS):
    case WRAP(PMULLW):
    case WRAP(PMULLD):
    case WRAP(PMULHW):
    case WRAP(PMULHUW):
    case WRAP(PMULHRSW):
        /* src is xmm. */
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
        }
        return true;
    /* 32 ~ 127 Unmodified. */
    case WRAP(ADDSS):
    case WRAP(DIVSS):
    case WRAP(MULSS):
    case WRAP(SUBSS):
    case WRAP(CMPSS):
    case WRAP(MAXSS):
    case WRAP(MINSS):
    case WRAP(RSQRTSS):
    case WRAP(SQRTSS):
    case WRAP(RCPSS):
    case WRAP(COMISS):  // no change
    case WRAP(UCOMISS): // no change
    /* des 0 ~ 63 from src 0 ~ 31. */
    case WRAP(CVTSS2SI):
    case WRAP(CVTTSS2SI):
    case WRAP(CVTSS2SD):
    /* des: xmm, src: r/m . */
    /* if src is 64 bit: dest 0 ~ 31 from src 0 ~ 63. */
    /* if src is 32 bit: dest 0 ~ 31 from src 0 ~ 31. */
    case WRAP(CVTSI2SS):
    case WRAP(CVTSI2SD):
        return true;
    case WRAP(PSIGNB):
    case WRAP(PSIGNW):
    case WRAP(PSIGND):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_update_des(ir1, xmm);
        }
        return true;
    /* mov 128. */
    case WRAP(MOVUPS):
    case WRAP(MOVUPD):
    case WRAP(MOVAPS):
    case WRAP(MOVAPD):
    case WRAP(MOVDQA):
    case WRAP(MOVDQU):
    case WRAP(MOVNTPS):
    case WRAP(MOVNTPD):
        if (!ir1_opnd_is_xmm(des_opnd)) {
            /* src will write to mm. */
            src_no_opt(ir1, xmm);
        } else if (ir1_opnd_is_mem(src_opnd)) {
            other_update_des(ir1, xmm);
        } else {
            src_update_des(ir1, xmm);
        }
        return true;
    /* MOVD r/m32, xmm */
    /* MOVD xmm, r/m32 : 32 ~ 127 to be zero. */
    case WRAP(MOVD):
        zero_update_des(ir1, xmm);
        return true;
    /* MOVSS xmm1, xmm2 : 32 ~ 127 keep. */
    /* MOVSS xmm1, m32 : 32 ~ 127 zero. */
    case WRAP(MOVSS):
        if (ir1_opnd_is_mem(src_opnd)) {
            zero_update_des(ir1, xmm);
        }
        return true;
    /* need 0 ~ 127 every high bit. */
    case WRAP(PMOVMSKB):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_no_opt(ir1, xmm);
        }
        return true;
    /* src 0 ~ 31 update des 0 ~ 127. */
    case WRAP(PMOVSXBD):
    case WRAP(PMOVSXBQ):
    case WRAP(PMOVSXWQ):
        other_update_des(ir1, xmm);
        return true;
    case WRAP(PCMPEQB):
    case WRAP(PCMPEQW):
    case WRAP(PCMPEQD):
    case WRAP(PCMPEQQ):
    case WRAP(CMPPD):
    case WRAP(CMPPS):
    case WRAP(PCMPGTB):
    case WRAP(PCMPGTW):
    case WRAP(PCMPGTD):
    case WRAP(PCMPGTQ):
    case WRAP(PCMPISTRM):
    case WRAP(PCMPESTRM):
        /* ALL is 1. */
        if (src_num == des_num) {
            other_update_des(ir1, xmm);
        } else if(ir1_opnd_is_xmm(src_opnd)){
            src_des_update_des(ir1, xmm);
        }
        return true;

    case WRAP(PCMPESTRI):
    case WRAP(PCMPISTRI):
        des_no_opt(ir1, xmm);
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
            src_no_opt(ir1, xmm);
        }
        return true;
    case WRAP(PSLLDQ):
        des_no_opt(ir1, xmm);
        return true;
    case WRAP(PSLLW):
    case WRAP(PSLLD):
    case WRAP(PSLLQ):
    case WRAP(PSRAW):
    case WRAP(PSRAD):
    /* case WRAP(PSRAQ): */
    case WRAP(PSRLW):
    case WRAP(PSRLD):
    case WRAP(PSRLQ):
        return true;
    /* src and des h64 change des l64. */
    case WRAP(PUNPCKHBW):
    case WRAP(PUNPCKHDQ):
    case WRAP(PUNPCKHWD):
    case WRAP(UNPCKHPS):
    case WRAP(PACKUSDW):
    case WRAP(PACKSSWB):
    case WRAP(PACKSSDW):
    case WRAP(PACKUSWB):
        des_no_opt(ir1, xmm);
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
            src_no_opt(ir1, xmm);
        }
        return true;
    default:
        break;
    }

    switch (ir1_opcode(ir1)) {
    case WRAP(POR):
    case WRAP(ORPS):
    case WRAP(ORPD):
    case WRAP(ANDNPD):
    case WRAP(ANDNPS):
    case WRAP(PANDN):
    case WRAP(PMINSW):
    case WRAP(PMINSD):
    case WRAP(PMINSB):
    case WRAP(PMINUW):
    case WRAP(PMINUD):
    case WRAP(PMINUB):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
        }
        return true;
    case WRAP(ANDPD):
    case WRAP(ANDPS):
    case WRAP(PAND):
        if (!ir1_opnd_is_xmm(src_opnd)) {
            return true;
        }
        if (xmm[des_num] == SHBR_XMM_ZERO) {
            /* ZERO! */
            /* ir1->xmm_def = SHBR_UPDATE_DES; */
            /* xmm[dest_num] = SHBR_XMM_ZERO; */
        } else if (xmm[src_num] == SHBR_XMM_ZERO) {
            src_update_des(ir1, xmm);
        } else if (src_num == des_num) {
            /* inherit dest => keep */
        } else {
            src_des_update_des(ir1, xmm);
        }
        return true;
    case WRAP(PXOR):
    case WRAP(XORPD):
    case WRAP(XORPS):
        if (!ir1_opnd_is_xmm(src_opnd)) {
            return true;
        }
        if (src_num == des_num) {
            zero_update_des(ir1, xmm);
        } else {
            src_des_update_des(ir1, xmm);
        }
        return true;
    default:
        break;
    }
    return false;
}

/* FIX ME! */
static bool deal_dest_not_xmm_32(TranslationBlock *tb,
        IR1_INST *ir1, uint32_t xmm[XMM_NUM])
{
    IR1_OPND *opnd1 = ir1_get_opnd(ir1, 1);
    if (!ir1_opnd_is_xmm(opnd1)) {
        return false;
    }

    switch (ir1_opcode(ir1)) {
    /* MOVDQ2Q mm, xmm : xmm mov l64 to mmx. */
    case WRAP(MOVDQ2Q):
        src_no_opt(ir1, xmm);
        return true;
    default:
        break;
    }

    src_no_opt(ir1, xmm);
    return false;
}

/* FIX ME! */
static bool deal_src_not_xmm_32(TranslationBlock *tb,
        IR1_INST *ir1, uint32_t xmm[XMM_NUM])
{
    des_no_opt(ir1, xmm);
    return false;
}


/* analyse 32 ~ 127 bit. */
static bool xmm_analyse_32(TranslationBlock *tb,
        IR1_INST *ir1, uint32_t xmm[XMM_NUM])
{
    if (deal_xmm_common(tb, ir1, xmm)) {
        return true;
    }

    IR1_OPND *src_opnd = ir1_get_opnd(ir1, 1);
    IR1_OPND *des_opnd = ir1_get_opnd(ir1, 0);

    ir1_opnd_is_imm(src_opnd);
    switch (ir1_opcode(ir1)) {
    /* src 0 ~ 63 from src and des 0 ~ 63. */
    case WRAP(CMPSD):
    case WRAP(ADDSD):
    case WRAP(DIVSD):
    case WRAP(MAXSD):
    case WRAP(MINSD):
    case WRAP(MULSD):
    case WRAP(SUBSD):
    case WRAP(SQRTSD):
    case WRAP(COMISD):
    case WRAP(UCOMISD):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
        }
        return true;
    /* MOVSD xmm, m64 : h64 zero. */
    /* MOVSD xmm, xmm : h64 keep. */
    /* MOVSD m64, xmm : */
    case WRAP(MOVSD):
        if (ir1_opnd_is_mem(src_opnd)) {
            other_update_des(ir1, xmm);
        } else if(!ir1_opnd_is_xmm(des_opnd)) {
            src_no_opt(ir1, xmm);
        } else {
            /* 0 ~ 63 from src, 64 ~ 127 from des, so 32 ~ 127 need src and des. */
            src_des_update_des(ir1, xmm);
        }
        return true;
    /* xmm, m64 or m64, xmm */
    /* mov src to src h64. */
    case WRAP(MOVHPS):
    case WRAP(MOVHPD):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_no_opt(ir1, xmm);
        }
        return true;

    /* MOVQ xmm1, xmm2 : h64 zero. */
    /* MOVQ xmm1, mm : h64 zero. */
    /* MOVQ r/m64, xmm : h64 zero. */
    case WRAP(MOVQ):
        if (ir1_opnd_is_mem(src_opnd)) {
            other_update_des(ir1, xmm);
        } else if (!ir1_opnd_is_xmm(des_opnd)) {
            src_no_opt(ir1, xmm);
        } else if(ir1_opnd_is_xmm(src_opnd)) {
            src_update_des(ir1, xmm);
        }
        return true;
    /* mov l64 to h64. */
    case WRAP(MOVLHPS):
    /* mov l64 to l64, h64 unchange. */
    case WRAP(MOVLPD):
    case WRAP(MOVLPS):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
        }
        return true;
    /* mov h64 to l64. */
    case WRAP(MOVHLPS):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
            src_no_opt(ir1, xmm);
        }
        return true;
    /* des 0 ~ 64: need des and src 0 ~ 31. */
    /* des 64 ~ 127: need des and src 32 ~ 64. */
    case WRAP(PUNPCKLBW):
    case WRAP(PUNPCKLWD):
    case WRAP(PUNPCKLDQ):
    /* des 0 ~ 64: need src 0 ~ 64 keep. */
    /* des 64 ~ 127: need des 0 ~ 64. */
    case WRAP(PUNPCKLQDQ):
    case WRAP(UNPCKLPS):
    case WRAP(UNPCKLPD):
        src_des_update_des(ir1, xmm);
        return true;
    /* src 0 ~ 63 update des 0 ~ 127. */
    case WRAP(PMOVSXBW):
    case WRAP(PMOVSXWD):
    case WRAP(PMOVSXDQ):
        other_update_des(ir1, xmm);
        src_no_opt(ir1,xmm);
        return true;
    /* des 0 ~ 31 from src 0 ~ 63. */
    case WRAP(CVTSD2SS):
    /* des 0 ~ 63 from src 0 ~ 63. */
    case WRAP(CVTSD2SI):
    case WRAP(CVTTSD2SI):
    case WRAP(CVTTPS2PI):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_no_opt(ir1, xmm);
        }
        return true;
    case WRAP(PSRLDQ):
        assert(ir1_opnd_is_imm(src_opnd));
        if (ir1_opnd_uimm(src_opnd) >= 12) {
            other_update_des(ir1, xmm);
        }
        return true;
    /* des h64 to des l64, src h64 to des h64. */
    case WRAP(PUNPCKHQDQ):
    case WRAP(UNPCKHPD):
        des_no_opt(ir1, xmm);
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
        }
    default:
        break;
    }

    if (ir1_get_opnd_num(ir1) == 3 && ir1_opnd_is_imm(ir1_get_opnd(ir1, 2))) {
        uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(ir1, 2));
        switch (ir1_opcode(ir1)) {
        case WRAP(PSHUFD):
            if (ir1_opnd_is_mem(src_opnd) || imm < 1) {
                other_update_des(ir1, xmm);
            } else {
               src_no_opt(ir1, xmm);
               src_update_des(ir1, xmm);
            }
            return true;
        /* tmp[255:0] := ((des[127:0] << 128)[255:0] OR src[127:0]) >> (imm8*8) */
        /* des[127:0] := tmp[127:0] */
        case WRAP(PALIGNR):
            if (imm <= 4) {
                if (ir1_opnd_is_xmm(src_opnd)) {
                    src_update_des(ir1, xmm);
                    src_no_opt(ir1, xmm);
                } else {
                    other_update_des(ir1, xmm);
                }
            } else if (imm < 16) {
                if (ir1_opnd_is_xmm(src_opnd)) {
                    src_des_update_des(ir1, xmm);
                    src_no_opt(ir1, xmm);
                }
            } else if (imm > 16) {
                des_no_opt(ir1, xmm);
            }
            return true;
        case WRAP(SHUFPS):
            if (imm & 0x3) {
                des_no_opt(ir1, xmm);
            }
            if (imm & 0xf0) {
                if (imm & 0x0f) {
                    src_des_update_des(ir1, xmm);
                } else {
                    src_update_des(ir1, xmm);
                }
            } else if (imm == 0x00) {
                other_update_des(ir1, xmm);
            }
            return true;
        default:
            break;
        }
    }

    /* deal dest not xmm. */
    if (!ir1_opnd_is_xmm(des_opnd)) {
        return deal_dest_not_xmm_32(tb, ir1, xmm);
    }
    /* src not xmm. */
    if(!ir1_opnd_is_xmm(src_opnd)) {
        return deal_src_not_xmm_32(tb, ir1, xmm);
    }

    src_des_update_des(ir1, xmm);
    src_no_opt(ir1, xmm);
    des_no_opt(ir1, xmm);
    return false;
}

static bool deal_dest_not_xmm_64(TranslationBlock *tb,
        IR1_INST *ir1, uint32_t xmm[XMM_NUM])
{
    src_no_opt(ir1, xmm);
    return false;
}

static bool deal_src_not_xmm_64(TranslationBlock *tb,
        IR1_INST *ir1, uint32_t xmm[XMM_NUM])
{
    des_no_opt(ir1, xmm);
    return false;
}

/* analyse 64 ~ 127 bit. */
/* TODO: analyze more instructions. */
static bool xmm_analyse_64(TranslationBlock *tb,
        IR1_INST *ir1, uint32_t xmm[XMM_NUM])
{
    if (deal_xmm_common(tb, ir1, xmm)) {
        return true;
    }

    /* src_opnd. */
    IR1_OPND *src_opnd = ir1_get_opnd(ir1, 1);

    switch (ir1_opcode(ir1)) {
    /* src 0 ~ 63 from src and des 0 ~ 63. */
    case WRAP(CMPSD):
    case WRAP(ADDSD):
    case WRAP(DIVSD):
    case WRAP(MAXSD):
    case WRAP(MINSD):
    case WRAP(MULSD):
    case WRAP(SUBSD):
    case WRAP(SQRTSD):
    case WRAP(COMISD):
    case WRAP(UCOMISD):
    /* mov l64 to l64, h64 unchange. */
    case WRAP(MOVLPD):
    case WRAP(MOVLPS):
    /* des 0 ~ 31 from src 0 ~ 63. */
    case WRAP(CVTSD2SS):
    /* des 0 ~ 63 from src 0 ~ 63. */
    case WRAP(CVTSD2SI):
    case WRAP(CVTTSD2SI):
    case WRAP(CVTTPS2PI):
        return true;
    /* MOVSD xmm, xmm : h64 keep. */
    /* MOVSD xmm, m64 : h64 zero. */
    case WRAP(MOVSD):
        if (ir1_opnd_is_mem(src_opnd)) {
            zero_update_des(ir1, xmm);
        }
        return true;
    /* MOVQ xmm1, xmm2 : h64 zero. */
    /* MOVQ xmm1, mm : h64 zero. */
    case WRAP(MOVQ):
        zero_update_des(ir1, xmm);
        return true;
    /* xmm, m64 or m64, xmm */
    /* mov src to src h64. */
    case WRAP(MOVHPS):
    case WRAP(MOVHPD):
        if (ir1_opnd_is_mem(src_opnd)) {
            other_update_des(ir1, xmm);
        } else {
            src_no_opt(ir1, xmm);
        }
        return true;
    /* des 0 ~ 64: need des and src 0 ~ 31. */
    /* des 64 ~ 127: need des and src 32 ~ 64. */
    case WRAP(PUNPCKLBW):
    case WRAP(PUNPCKLWD):
    case WRAP(PUNPCKLDQ):
    /* des 0 ~ 64: need src 0 ~ 64. */
    /* des 64 ~ 127: need des 0 ~ 64. */
    case WRAP(PUNPCKLQDQ):
    case WRAP(UNPCKLPS):
    case WRAP(UNPCKLPD):
    /* mov l64 to h64. */
    case WRAP(MOVLHPS):
    /* src 0 ~ 63 update des 0 ~ 127. */
    case WRAP(PMOVSXBW):
    case WRAP(PMOVSXWD):
    case WRAP(PMOVSXDQ):
        other_update_des(ir1, xmm);
        return true;
    /* mov h64 to l64. */
    case WRAP(MOVHLPS):
        src_no_opt(ir1, xmm);
        return true;
    case WRAP(PSRLDQ):
        assert(ir1_opnd_is_imm(src_opnd));
        if (ir1_opnd_uimm(src_opnd) >= 8) {
            other_update_des(ir1, xmm);
        }
        return true;
    default:
        break;
    }

    if (ir1_get_opnd_num(ir1) == 3 && ir1_opnd_is_imm(ir1_get_opnd(ir1, 2))) {
        uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(ir1, 2));
        switch (ir1_opcode(ir1)) {
        case WRAP(PSHUFD):
            if (ir1_opnd_is_mem(src_opnd) || imm <= 1) {
                other_update_des(ir1, xmm);
            } else {
               src_no_opt(ir1, xmm);
               src_update_des(ir1, xmm);
            }
            return true;
        /* tmp[255:0] := ((des[127:0] << 128)[255:0] OR src[127:0]) >> (imm8*8) */
        /* des[127:0] := tmp[127:0] */
        case WRAP(PALIGNR):
            if (imm <= 8) {
                if (ir1_opnd_is_xmm(src_opnd)) {
                    src_update_des(ir1, xmm);
                    src_no_opt(ir1, xmm);
                } else {
                    other_update_des(ir1, xmm);
                }
            } else if (imm < 16) {
                if (ir1_opnd_is_xmm(src_opnd)) {
                    src_des_update_des(ir1, xmm);
                    src_no_opt(ir1, xmm);
                }
            } else if (imm > 16) {
                des_no_opt(ir1, xmm);
            }
            return true;
        case WRAP(SHUFPS):
            if (imm & 0xa) {
                des_no_opt(ir1, xmm);
            }
            if (imm & 0xa0) {
                if (imm & 0x0a) {
                    src_des_update_des(ir1, xmm);
                } else {
                    src_update_des(ir1, xmm);
                }
            } else if ((imm & 0xaa) == 0x00) {
                other_update_des(ir1, xmm);
            }
            return true;
        default:
            break;
        }
    }

    IR1_OPND *des_opnd = ir1_get_opnd(ir1, 0);
    /* deal dest not xmm. */
    if (!ir1_opnd_is_xmm(des_opnd)) {
        return deal_dest_not_xmm_64(tb, ir1, xmm);
    }
    /* src not xmm. */
    if(!ir1_opnd_is_xmm(src_opnd)) {
        return deal_src_not_xmm_64(tb, ir1, xmm);
    }

    src_des_update_des(ir1, xmm);
    src_no_opt(ir1, xmm);
    des_no_opt(ir1, xmm);
    return false;
}

static void init_xmm_state(uint32_t xmm[XMM_NUM])
{
    for (int i = 0; i < XMM_NUM; ++i) {
        xmm[i] = 1 << i;
    }
}

#include "tu.h"
typedef bool (*xmm_analyse_func)(TranslationBlock *, IR1_INST *, uint32_t *);

/* static int xcount, scount, ncount; */
static void tb_xmm_analyse(TranslationBlock *tb,
        xmm_analyse_func analyse_func, uint32_t *xmm)
{
    init_xmm_state(xmm);
    tb->s_data->shbr_type = SHBR_NTYPE;
    tb->s_data->xmm_use = 0;
    tb->s_data->xmm_def = 0;
    IR1_INST *ir1 = dt_X86_INS_INVALID;
    for (int i = 0; i < tb_ir1_num(tb); ++i) {
        ir1 = tb_ir1_inst(tb, i);
        ir1->xmm_def = 0;
        ir1->xmm_use = 0;
        uint8_t curr = get_inst_type(ir1);
        if (curr == SHBR_NTYPE) {
            continue;
        } else if (tb->s_data->shbr_type == SHBR_NTYPE) {
            tb->s_data->shbr_type = SHBR_SSE;
        }
        if (!analyse_func(tb, ir1, xmm)) {
            /* fprintf (stderr, "%d\n ", i); */
        }
        /* tb->s_data->xmm_def |= ir1->xmm_def; */
        tb->s_data->xmm_use |= ir1->xmm_use;
    }
    tb->s_data->xmm_use &= SHBR_XMM_MASK;
}

static void get_xmm_in(TranslationBlock *tb, uint32 *xmm)
{
    /* curr tb use. */
    tb->s_data->xmm_in = tb->s_data->xmm_use;
    for (int i = 0; i < XMM_NUM; i++) {
        /* next tb use but curr tb not cover. */
        if (tb->s_data->xmm_out & (1 << i)) {
            tb->s_data->xmm_in |= xmm[i] & SHBR_XMM_MASK;
        }
    }
}

static void over_tb_shbr_opt(TranslationBlock **tb_list, int tb_num_in_tu,
        uint32 opt_flag, uint32 xmm[][XMM_NUM])
{
    bool continue_flag = true;
    uint32_t old_live_in, old_live_out;
    while(continue_flag) {
        continue_flag = false;
        for (int i = tb_num_in_tu - 1; i >= 0; i--) {
            TranslationBlock *tb = tb_list[i];
            old_live_in = tb->s_data->xmm_in;
            old_live_out = tb->s_data->xmm_out;
            TranslationBlock *next_tb =
                (TranslationBlock *)tb->s_data->next_tb[TU_TB_INDEX_NEXT];
            TranslationBlock *target_tb =
                (TranslationBlock *)tb->s_data->next_tb[TU_TB_INDEX_TARGET];
            switch (tb->s_data->last_ir1_type) {
                case IR1_TYPE_BRANCH:
                    tb->s_data->xmm_out =
                        (next_tb ? next_tb->s_data->xmm_in : SHBR_XMM_ALL);
                    tb->s_data->xmm_out |=
                        (target_tb ? target_tb->s_data->xmm_in : SHBR_XMM_ALL);
                    break;
                case IR1_TYPE_JUMP:
                case IR1_TYPE_CALL:
                    tb->s_data->xmm_out =
                        (target_tb ? target_tb->s_data->xmm_in : SHBR_XMM_ALL);
                    break;
                case IR1_TYPE_NORMAL:
                    tb->s_data->xmm_out =
                        (next_tb ? next_tb->s_data->xmm_in : SHBR_XMM_ALL);
                    break;
                case IR1_TYPE_SYSCALL:
                    tb->s_data->xmm_out = 0;
                    break;
                case IR1_TYPE_CALLIN:
                case IR1_TYPE_JUMPIN:
                    tb->s_data->xmm_out = SHBR_XMM_ALL;
                    break;
                case IR1_TYPE_RET:
                    tb->s_data->xmm_out = SHBR_XMM_ALL;
                    /* tb->s_data->xmm_out = 0x3; */
                    break;
                default:
                    lsassert(0);
            }
            get_xmm_in(tb, xmm[i]);
            if (old_live_in != tb->s_data->xmm_in || old_live_out != tb->s_data->xmm_out) {
                continue_flag = true;
            }
        }

    }

    int src_num, dest_num;
    /* bool need_print_tu = false; */
    for (int i = 0; i < tb_num_in_tu; i++) {
        TranslationBlock *tb = tb_list[i];
        if (tb->s_data->shbr_type != SHBR_NTYPE) {
            IR1_INST *ir1 = dt_X86_INS_INVALID;
            uint32_t no_opt_xmm = tb->s_data->xmm_out & SHBR_XMM_MASK;
            for (int j = tb_ir1_num(tb) - 1; j >= 0; j--) {
                /* fprintf(stderr, "%x\n", tb->s_data->xmm_out); */
                ir1 = tb_ir1_inst(tb, j);
                uint8_t curr = get_inst_type(ir1);
                if (curr == SHBR_NTYPE) {
                    continue;
                }
                IR1_OPND *opnd0 = ir1_get_opnd(ir1, 0);
                IR1_OPND *opnd1 = ir1_get_opnd(ir1, 1);
                if (ir1_opnd_is_xmm(opnd0)) {
                    dest_num = ir1_opnd_base_reg_num(opnd0);
                    /* have no_opt_xmm. */
                    if (no_opt_xmm & (1 << dest_num)) {
                        /* reverse transmission. */
                        if ((ir1->xmm_def & SHBR_UPDATE_DES) && !(ir1->xmm_use & SHBR_NEED_DES)) {
                            no_opt_xmm &= ~(1 << dest_num);
                        }
                        if ((ir1->xmm_use & SHBR_NEED_SRC) && ir1_opnd_is_xmm(opnd1)) {
                            src_num = ir1_opnd_base_reg_num(opnd1);
                            no_opt_xmm |= (1 << src_num);
                        }
                    } else if (ir1->xmm_use & SHBR_NO_OPT_DES) {
                        assert(ir1_opnd_is_xmm(opnd0));
                        no_opt_xmm |= (1 << dest_num);
                    } else {
                        /* can opt. */
                        ir1->hbr_flag |= opt_flag;
                    }
                }

                if (ir1->xmm_use & SHBR_NO_OPT_SRC) {
                    assert(ir1_opnd_is_xmm(opnd1));
                    src_num = ir1_opnd_base_reg_num(opnd1);
                    no_opt_xmm |= (1 << src_num);
                }

            }
        }

    }
    /* if (need_print_tu) { */
    /*     for (int i = 0; i < tb_num_in_tu; i++) { */
    /*         fprintf(stderr, "tb i %d\n", i); */
    /*         print_ir1(tb_list[i]); */
    /*     } */
    /* } */
}

static void do_shbr_opt32(TranslationBlock **tb_list, int tb_num_in_tu)
{
    uint32_t xmm[tb_num_in_tu][XMM_NUM];
    for (int i = 0; i < tb_num_in_tu; i++) {
        TranslationBlock *tb = tb_list[i];
        tb_xmm_analyse(tb, xmm_analyse_32, xmm[i]);
        tb->s_data->xmm_in = tb->s_data->xmm_use;
        tb->s_data->xmm_out = 0;
    }
    over_tb_shbr_opt(tb_list, tb_num_in_tu, SHBR_CAN_OPT32, xmm);
}

static void do_shbr_opt64(TranslationBlock **tb_list, int tb_num_in_tu)
{
    uint32_t xmm[tb_num_in_tu][XMM_NUM];
    for (int i = 0; i < tb_num_in_tu; i++) {
        TranslationBlock *tb = tb_list[i];
        tb_xmm_analyse(tb, xmm_analyse_64, xmm[i]);
        tb->s_data->xmm_in = tb->s_data->xmm_use;
        tb->s_data->xmm_out = 0;
    }
    over_tb_shbr_opt(tb_list, tb_num_in_tu, SHBR_CAN_OPT64, xmm);
}

static void clear_ir1_flag(TranslationBlock **tb_list, int tb_num_in_tu)
{
    for (int i = 0; i < tb_num_in_tu; i++) {
        TranslationBlock *tb = tb_list[i];
        if (tb->s_data->shbr_type != SHBR_NTYPE) {
            IR1_INST *ir1 = dt_X86_INS_INVALID;
            for (int j = 0; j < tb_ir1_num(tb); j++) {
                ir1 = tb_ir1_inst(tb, j);
                ir1->hbr_flag = 0;
            }
        }
    }
}

#ifdef TARGET_X86_64
static void over_tb_gpr_opt(TranslationBlock **tb_list, int tb_num_in_tu)
{
    bool continue_flag = true;
    uint32_t old_live_in, old_live_out;
    while(continue_flag) {
        continue_flag = false;
        for (int i = tb_num_in_tu - 1; i >= 0; i--) {
            TranslationBlock *tb = tb_list[i];
            old_live_in = tb->s_data->gpr_in;
            old_live_out = tb->s_data->gpr_out;
            TranslationBlock *next_tb =
                (TranslationBlock *)tb->s_data->next_tb[TU_TB_INDEX_NEXT];
            TranslationBlock *target_tb =
                (TranslationBlock *)tb->s_data->next_tb[TU_TB_INDEX_TARGET];
            switch (tb->s_data->last_ir1_type) {
                case IR1_TYPE_BRANCH:
                    tb->s_data->gpr_out =
                        (next_tb ? next_tb->s_data->gpr_in : GHBR_GPR_ALL);
                    tb->s_data->gpr_out |=
                        (target_tb ? target_tb->s_data->gpr_in : GHBR_GPR_ALL);
                    break;
                case IR1_TYPE_JUMP:
                case IR1_TYPE_CALL:
                    tb->s_data->gpr_out =
                        (target_tb ? target_tb->s_data->gpr_in : GHBR_GPR_ALL);
                    break;
                case IR1_TYPE_NORMAL:
                    tb->s_data->gpr_out =
                        (next_tb ? next_tb->s_data->gpr_in : GHBR_GPR_ALL);
                    break;
                case IR1_TYPE_SYSCALL:
                    tb->s_data->gpr_out = GHBR_GPR_ALL;
                    break;
                case IR1_TYPE_CALLIN:
                case IR1_TYPE_JUMPIN:
                    tb->s_data->gpr_out = GHBR_GPR_ALL;
                    break;
                case IR1_TYPE_RET:
                    tb->s_data->gpr_out = GHBR_GPR_ALL;
                    break;
                default:
                    lsassert(0);
            }

            tb->s_data->gpr_in = tb->s_data->gpr_use |
                (tb->s_data->gpr_out & ~tb->s_data->gpr_def);

            if (old_live_in != tb->s_data->gpr_in || old_live_out != tb->s_data->gpr_out) {
                continue_flag = true;
            }
        }
    }

    for (int i = 0; i < tb_num_in_tu; i++) {
        TranslationBlock *tb = tb_list[i];
        uint32_t gpr_out = tb->s_data->gpr_out;
        IR1_INST *ir1;
        for (int j = tb_ir1_num(tb) - 1; j >= 0; j--) {
            ir1 = tb_ir1_inst(tb, j);
            if (ir1->gpr_def & gpr_out) {
                gpr_out &= ~ir1->gpr_def;
            } else if(ir1->gpr_def) {
                /* fprintf(stderr, "%x %x\n", ir1->gpr_def, gpr_out); */
                ir1->hbr_flag |= GHBR_CAN_OPT;
            }
            gpr_out |= ir1->gpr_use;
        }
    }
}

static void des_def_gpr(TranslationBlock *tb, IR1_INST *ir1)
{
    IR1_OPND *des_opnd = ir1_get_opnd(ir1, 0);
    if (!ir1_opnd_is_gpr(des_opnd)) {
        return;
    }
    int dest_num = ir1_opnd_base_reg_num(des_opnd);
    assert(dest_num >= 0 && dest_num < 16);
    ir1->gpr_def |= 1 << dest_num;
    tb->s_data->gpr_def |= 1 << dest_num;
}

static void deal_hide_opnd_def(TranslationBlock *tb, IR1_INST *ir1)
{
    switch (ir1_opcode(ir1)) {
    case WRAP(CWDE):
        ir1->gpr_def |= 1 << eax_index;
        tb->s_data->gpr_def |= 1 << eax_index;
        break;
    case WRAP(CQO):
        ir1->gpr_def |= 1 << edx_index;
        tb->s_data->gpr_def |= 1 << edx_index;
        break;
    case WRAP(POPAW):
        ir1->gpr_def |= 0xff & ~esp_index;
        tb->s_data->gpr_def |= 0xff & ~esp_index;
        break;
    default:
        break;
    }
}

/* Some ins can update the h32 bits of des opnd
 * without using the h32 bits of des opnd. */
static bool def_h32(TranslationBlock *tb, IR1_INST *ir1)
{
    deal_hide_opnd_def(tb, ir1);

    if (!ir1_get_opnd_num(ir1)) {
        return false;
    }
    IR1_OPND *des_opnd = ir1_get_opnd(ir1, 0);
    if (!ir1_opnd_is_gpr(des_opnd)) {
        return false;
    }

    switch (ir1_opcode(ir1)) {
    case WRAP(MOVSX):
    case WRAP(MOVZX):
    /* case WRAP(MOVSXD): */
        if (ir1_opnd_size(des_opnd) == 64 &&
                ir1_opnd_size(ir1_get_opnd(ir1, 1)) == 32) {
            des_def_gpr(tb, ir1);
            return true;
        }
        return false;
    case WRAP(MOV):
        if ((ir1_opnd_size(ir1_get_opnd(ir1, 0)) == 64)
                && ir1_opnd_size(des_opnd) == 64) {
            des_def_gpr(tb, ir1);
            return true;
        }
        return false;
    case WRAP(MOVD):
        if (ir1_opnd_is_xmm(ir1_get_opnd(ir1, 1))) {
            des_def_gpr(tb, ir1);
            return true;
        }
        return false;
    default:
        break;
    }

    if (ir1_opnd_size(des_opnd) != 32) {
        return false;
    }
    switch (ir1_opcode(ir1)) {
    case WRAP(XOR):
    case WRAP(AND):
    case WRAP(OR):
    case WRAP(NOT):
    case WRAP(ROL):
    case WRAP(ROR):
    case WRAP(RCL):
    case WRAP(RCR):
    case WRAP(SHRD):
    case WRAP(SHLD):
    case WRAP(ADD):
    case WRAP(ADC):
    case WRAP(INC):
    case WRAP(DEC):
    case WRAP(SUB):
    case WRAP(SBB):
    case WRAP(NEG):
    case WRAP(XADD):
        des_def_gpr(tb, ir1);
        return true;
    case WRAP(XCHG):
        if (ir1_opnd_is_same_reg(des_opnd, ir1_get_opnd(ir1, 1))) {
            des_def_gpr(tb, ir1);
            return true;
        }
        return false;
    default:
        return false;
    }

    return false;
}

static void set_use_reg(TranslationBlock *tb, IR1_INST *ir1, int reg_num)
{
    if (reg_num >= 0 && reg_num < 16) {
        ir1->gpr_use |= 1 << reg_num;
        /* Need pre tb provide if curr tb  not def this reg. */
        if (!(tb->s_data->gpr_def & (1 << reg_num))) {
            tb->s_data->gpr_use |= 1 << reg_num;
        }
    }
}

static void deal_hide_opnd_use(TranslationBlock *tb, IR1_INST *ir1)
{
    switch (ir1_opcode(ir1)) {
    case WRAP(CMPXCHG):
    case WRAP(SALC):
    case WRAP(CWD):
    case WRAP(CDQ):
    case WRAP(CBW):
    case WRAP(CWDE):
    case WRAP(CDQE):
        set_use_reg(tb, ir1, eax_index);
        break;
    case WRAP(XLATB):
        set_use_reg(tb, ir1, ebx_index);
        break;
    case WRAP(JRCXZ):
    case WRAP(JCXZ):
    case WRAP(JECXZ):
    case WRAP(LOOPE):
    case WRAP(LOOPNE):
    case WRAP(LOOP):
        set_use_reg(tb, ir1, ecx_index);
        break;
    case WRAP(MASKMOVQ):
    case WRAP(MASKMOVDQU):
        set_use_reg(tb, ir1, edi_index);
        break;
    case WRAP(POPF):
    case WRAP(POP):
    case WRAP(PUSHF):
    case WRAP(PUSH):
    case WRAP(CALL):
    case WRAP(RET):
    case WRAP(RETF):
    case WRAP(IRET):
    case WRAP(IRETQ):
        set_use_reg(tb, ir1, esp_index);
        break;
    case WRAP(LODSB):
    case WRAP(LODSQ):
    case WRAP(LODSW):
    case WRAP(LODSD):
        set_use_reg(tb, ir1, esi_index);
        set_use_reg(tb, ir1, ecx_index);
        break;
    case WRAP(MUL):
    case WRAP(DIV):
    case WRAP(IDIV):
    case WRAP(RDTSC):
    case WRAP(CQO):
        set_use_reg(tb, ir1, eax_index);
        break;
    case WRAP(IMUL):
        if (ir1_opnd_num(ir1) == 1) {
            set_use_reg(tb, ir1, eax_index);
            set_use_reg(tb, ir1, edx_index);
        }
        break;
    case WRAP(ENTER):
    case WRAP(LEAVE):
        set_use_reg(tb, ir1, esp_index);
        set_use_reg(tb, ir1, ebp_index);
        break;
    case WRAP(MOVSD):
    case WRAP(CMPSD):
        set_use_reg(tb, ir1, esi_index);
        set_use_reg(tb, ir1, edi_index);
        set_use_reg(tb, ir1, ecx_index);
        break;
    case WRAP(STOSB):
    case WRAP(STOSW):
    case WRAP(STOSD):
    case WRAP(STOSQ):
    case WRAP(SCASB):
    case WRAP(SCASW):
    case WRAP(SCASD):
    case WRAP(SCASQ):
        set_use_reg(tb, ir1, eax_index);
        set_use_reg(tb, ir1, edi_index);
        set_use_reg(tb, ir1, ecx_index);
        break;
    case WRAP(RDTSCP):
        set_use_reg(tb, ir1, eax_index);
        set_use_reg(tb, ir1, ecx_index);
        set_use_reg(tb, ir1, edx_index);
        break;
    case WRAP(CMPXCHG8B):
    case WRAP(CMPXCHG16B):
        set_use_reg(tb, ir1, eax_index);
        set_use_reg(tb, ir1, ebx_index);
        set_use_reg(tb, ir1, ecx_index);
        set_use_reg(tb, ir1, edx_index);
        break;
    case WRAP(INT3):
    case WRAP(PUSHAW):
    case WRAP(PUSHAL):
        ir1->gpr_use |= GHBR_GPR_ALL;
        tb->s_data->gpr_use |= ~tb->s_data->gpr_def;
        break;
    default:
        break;
    }
}

static void use_h32(TranslationBlock *tb, IR1_INST *ir1)
{
    deal_hide_opnd_use(tb, ir1);
    int opnd_num = ir1_get_opnd_num(ir1);
    /* We roughly assume that the high 32 bit of all gpr in curr ins will be used,
     * except for updating their own h32 des opnd without using their own h32 bit. */
    int i = 0;
    if (ir1->gpr_def) {
        i = 1;
    }
    for (; i < opnd_num; ++i) {
        IR1_OPND *opnd = ir1_get_opnd(ir1, i);
        if (ir1_opnd_is_gpr(opnd) && ir1_opnd_size(opnd) == 64) {
            set_use_reg(tb, ir1, ir1_opnd_base_reg_num(opnd));
        } else if (ir1_opnd_is_mem(opnd)) {
            if (ir1_opnd_has_base(opnd)) {
                int base_num = ir1_opnd_base_reg_num(opnd);
                set_use_reg(tb, ir1, base_num);
            }
            if (ir1_opnd_has_index(opnd)) {
                int index_num = ir1_opnd_index_reg_num(opnd);
                set_use_reg(tb, ir1, index_num);
            }
        }
    }
}

/* The current strategy used is relatively conservative, */
/* only including analysis of a small number of instructions.
 * If the strategy is changed to analyze more instructions,
 * better results may be achieved. */
static void get_gpr_use_def(TranslationBlock *tb)
{
    tb->s_data->gpr_use = 0;
    tb->s_data->gpr_def = 0;
    tb->s_data->gpr_out = 0;
    tb->s_data->gpr_in = 0;
    IR1_INST *ir1;
    for (int i = 0; i < tb_ir1_num(tb); ++i) {
        ir1 = tb_ir1_inst(tb, i);
        ir1->gpr_def = 0;
        ir1->gpr_use = 0;
        def_h32(tb, ir1);
        use_h32(tb, ir1);
    }
}

static void do_gpr_opt(TranslationBlock **tb_list, int tb_num_in_tu)
{
    for (int i = 0; i < tb_num_in_tu; i++) {
        get_gpr_use_def(tb_list[i]);
    }
    over_tb_gpr_opt(tb_list, tb_num_in_tu);
}
#endif

void hbr_opt(TranslationBlock **tb_list, int tb_num_in_tu)
{
    clear_ir1_flag(tb_list, tb_num_in_tu);
    do_shbr_opt32(tb_list, tb_num_in_tu);
    do_shbr_opt64(tb_list, tb_num_in_tu);
#ifdef TARGET_X86_64
    do_gpr_opt(tb_list, tb_num_in_tu);
#endif
}

bool can_shbr_opt64(IR1_INST *ir1)
{
    if (!in_pre_translate) {
        return false;
    }
    if (ir1->hbr_flag & SHBR_CAN_OPT64) {
        return true;
    }
    return false;
}

bool can_shbr_opt32(IR1_INST *ir1)
{
    if (!in_pre_translate) {
        return false;
    }
    if (ir1->hbr_flag & SHBR_CAN_OPT32) {
        return true;
    }
    return false;
}

#ifdef TARGET_X86_64
/* static int opt, noopt; */
bool can_ghbr_opt(IR1_INST *ir1)
{
    if (!in_pre_translate) {
        return false;
    }
    if (ir1->hbr_flag & GHBR_CAN_OPT) {
        return true;
    }
    return false;
}
#endif

#undef WRAP
#endif
