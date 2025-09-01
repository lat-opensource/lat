#include "common.h"
#include "ir1-bd.h"
#include "translate-bd.h"
#include "flag-reduction.h"

#define FLAG_DEFINE_BD(opcode, _use, _def, _undef) \
[ND_INS_##opcode] = (IR1_EFLAG_USEDEF) \
{.use = _use, .def = _def, .undef = _undef}

/**
 * @brief ir1 opcode (x86) per instruction eflags using table
 * @note If the flag is 'unaffected', please DO NOT set at 'used' part!
 */
static const IR1_EFLAG_USEDEF ir1_opcode_eflag_usedef_bd[] = {
    FLAG_DEFINE_BD(INVALID, __INVALID, __INVALID, __INVALID),
    FLAG_DEFINE_BD(AAA,     __AF, __ALL_EFLAGS, __OF | __SF | __ZF | __PF),
    FLAG_DEFINE_BD(AAD,     __NONE, __ALL_EFLAGS, __OF | __AF | __CF),
    FLAG_DEFINE_BD(AAM,     __NONE, __ALL_EFLAGS, __OF | __AF | __CF),
    FLAG_DEFINE_BD(AAS,     __AF, __ALL_EFLAGS, __OF | __SF | __ZF | __PF),
    FLAG_DEFINE_BD(ADC,     __CF, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE_BD(ADD,     __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE_BD(AND,     __NONE, __ALL_EFLAGS, __AF),
    FLAG_DEFINE_BD(ANDN,    __NONE, __ALL_EFLAGS, __AF | __PF),
    FLAG_DEFINE_BD(ARPL,    __NONE, __ZF, __NONE),
    FLAG_DEFINE_BD(BLSI,    __NONE, __ZF | __CF | __SF | __OF, __AF | __PF),
    FLAG_DEFINE_BD(BLSR,    __NONE, __ZF | __CF | __SF | __OF, __AF | __PF),
    FLAG_DEFINE_BD(BLSMSK,  __NONE, __ZF | __CF | __SF | __OF, __AF | __PF),
    FLAG_DEFINE_BD(BZHI,    __NONE, __ZF | __CF | __SF | __OF, __AF | __PF),
    FLAG_DEFINE_BD(BOUND,   __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(BSF,     __NONE, __ALL_EFLAGS, __OSAPF | __CF),
    FLAG_DEFINE_BD(BSR,     __NONE, __ALL_EFLAGS, __OSAPF | __CF),
    FLAG_DEFINE_BD(BSWAP,   __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(BT,      __NONE, __OSAPF | __CF, __OSAPF),
    FLAG_DEFINE_BD(BTC,     __NONE, __OSAPF | __CF, __OSAPF),
    FLAG_DEFINE_BD(BTR,     __NONE, __OSAPF | __CF, __OSAPF),
    FLAG_DEFINE_BD(BTS,     __NONE, __OSAPF | __CF, __OSAPF),
    FLAG_DEFINE_BD(CALLFD,    __NONE, __NONE, __NONE), /* change */
    FLAG_DEFINE_BD(CALLFI,    __NONE, __NONE, __NONE), /* change */
    FLAG_DEFINE_BD(CALLNR,    __NONE, __NONE, __NONE), /* change */
    FLAG_DEFINE_BD(CALLNI,    __NONE, __NONE, __NONE), /* change */
    FLAG_DEFINE_BD(CBW,     __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(CLC,     __NONE, __CF, __NONE),
    FLAG_DEFINE_BD(CLD,     __NONE, __DF, __NONE),
    FLAG_DEFINE_BD(SALC,     __CF, __NONE, __NONE),
    /* FLAG_DEFINE_BD(CLI,     __NONE, __INVALID, __NONE), */
    /* FLAG_DEFINE_BD(CLTS,     __NONE, __INVALID, __NONE), */
    FLAG_DEFINE_BD(CMC,     __CF, __CF, __NONE),
    /* CMOVcc */
    FLAG_DEFINE_BD(CMOVNBE,   __ZF | __CF, __NONE, __NONE),   //CMOVA
    FLAG_DEFINE_BD(CMOVNC,  __CF, __NONE, __NONE),            //CMOVAE
    FLAG_DEFINE_BD(CMOVC,   __CF, __NONE, __NONE),            //CMOVB
    FLAG_DEFINE_BD(CMOVBE,  __ZF | __CF, __NONE, __NONE),     //CMOVBE
    FLAG_DEFINE_BD(CMOVZ,   __ZF, __NONE, __NONE),            //CMOVE
    FLAG_DEFINE_BD(CMOVNLE,   __OF | __SF | __ZF, __NONE, __NONE),//CMOVG
    FLAG_DEFINE_BD(CMOVNL,  __OF | __SF, __NONE, __NONE),     //CMOVGE
    FLAG_DEFINE_BD(CMOVL,   __OF | __SF, __NONE, __NONE),     //CMOVL
    FLAG_DEFINE_BD(CMOVLE,  __OF | __SF | __ZF, __NONE, __NONE),//CMOVLE
    FLAG_DEFINE_BD(CMOVNZ,  __ZF, __NONE, __NONE),            //CMOVNE
    FLAG_DEFINE_BD(CMOVO,   __OF, __NONE, __NONE),            //CMOVO
    FLAG_DEFINE_BD(CMOVNO,  __OF, __NONE, __NONE),            //CMOVNO
    FLAG_DEFINE_BD(CMOVP,   __PF, __NONE, __NONE),            //CMOVP
    FLAG_DEFINE_BD(CMOVNP,  __PF, __NONE, __NONE),            //CMOVNP
    FLAG_DEFINE_BD(CMOVS,   __SF, __NONE, __NONE),            //CMOVS
    FLAG_DEFINE_BD(CMOVNS,  __SF, __NONE, __NONE),            //CMOVNS

    FLAG_DEFINE_BD(CMP,     __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE_BD(CMPS,   __DF, __ALL_EFLAGS, __NONE), /* change */
    FLAG_DEFINE_BD(CMPXCHG, __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE_BD(CMPXCHG8B, __NONE, __ZF, __NONE),
    FLAG_DEFINE_BD(CMPXCHG16B, __NONE, __ZF, __NONE),
    FLAG_DEFINE_BD(COMISD,  __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE_BD(COMISS,  __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE_BD(CPUID,   __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(CWD,     __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(DAA,     __AF | __CF, __ALL_EFLAGS, __OF), /* change */
    FLAG_DEFINE_BD(DAS,     __AF | __CF, __ALL_EFLAGS, __OF), /* change */
    FLAG_DEFINE_BD(DEC,     __NONE, __OF | __SZAPF, __NONE),
    FLAG_DEFINE_BD(DIV,     __NONE, __ALL_EFLAGS, __ALL_EFLAGS),
    FLAG_DEFINE_BD(ENTER,   __NONE, __NONE, __NONE),

    /* FLAG_DEFINE_BD(ESC,     __NONE, __INVALID, __NONE), */
    /* FCMOVcc */
    FLAG_DEFINE_BD(FCMOVB,  __CF, __NONE, __NONE),
    FLAG_DEFINE_BD(FCMOVBE, __ZF | __CF, __NONE, __NONE),
    FLAG_DEFINE_BD(FCMOVE,  __ZF, __NONE, __NONE),
    FLAG_DEFINE_BD(FCMOVNB, __CF, __NONE, __NONE),
    FLAG_DEFINE_BD(FCMOVNBE, __ZF | __CF, __NONE, __NONE),
    FLAG_DEFINE_BD(FCMOVNE, __ZF, __NONE, __NONE),
    FLAG_DEFINE_BD(FCMOVNU, __PF, __NONE, __NONE),
    FLAG_DEFINE_BD(FCMOVU,  __PF, __NONE, __NONE),

    FLAG_DEFINE_BD(FCOMI,   __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE_BD(FCOMIP,  __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE_BD(FUCOMI,  __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE_BD(FUCOMIP, __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE_BD(HLT,     __NONE, __NONE, __NONE), /* change */
    FLAG_DEFINE_BD(IDIV,    __NONE, __ALL_EFLAGS, __ALL_EFLAGS),
    FLAG_DEFINE_BD(IMUL,    __NONE, __ALL_EFLAGS, __SZAPF),
    FLAG_DEFINE_BD(IN,      __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(INC,     __NONE, __OF | __SZAPF, __NONE),
    FLAG_DEFINE_BD(INS,    __NONE, __DF, __NONE), /* change */
    FLAG_DEFINE_BD(INT,     __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(INTO,    __OF, __NONE, __NONE), /* change */
    FLAG_DEFINE_BD(INVD,    __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(INVLPG,  __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(UCOMISD, __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE_BD(UCOMISS, __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE_BD(IRET,    __NONE, __INVALID, __NONE),

    /* Jcc */
    FLAG_DEFINE_BD(JO,      __OF, __NONE, __NONE),
    FLAG_DEFINE_BD(JNO,     __OF, __NONE, __NONE),
    FLAG_DEFINE_BD(JC,      __CF, __NONE, __NONE), //JB
    FLAG_DEFINE_BD(JNC,     __CF, __NONE, __NONE), //JAE
    FLAG_DEFINE_BD(JZ,      __ZF, __NONE, __NONE), //JE
    FLAG_DEFINE_BD(JNZ,     __ZF, __NONE, __NONE), //JNE
    FLAG_DEFINE_BD(JBE,     __CF | __ZF, __NONE, __NONE),
    FLAG_DEFINE_BD(JNBE,      __CF | __ZF, __NONE, __NONE), //JA
    FLAG_DEFINE_BD(JS,      __SF, __NONE, __NONE),
    FLAG_DEFINE_BD(JNS,     __SF, __NONE, __NONE),
    FLAG_DEFINE_BD(JP,      __PF, __NONE, __NONE),
    FLAG_DEFINE_BD(JNP,     __PF, __NONE, __NONE),
    FLAG_DEFINE_BD(JL,      __OF | __SF, __NONE, __NONE),
    FLAG_DEFINE_BD(JNL,     __OF | __SF, __NONE, __NONE), //JGE
    FLAG_DEFINE_BD(JLE,     __OF | __SF | __ZF, __NONE, __NONE),
    FLAG_DEFINE_BD(JNLE,      __OF | __SF | __ZF, __NONE, __NONE), //JG

    FLAG_DEFINE_BD(JrCXZ,    __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(LAHF,    __SZAPCF, __NONE, __NONE), /* change */
    /* INVALID */
    FLAG_DEFINE_BD(LAR,     __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(LDS,     __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(LES,     __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(LSS,     __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(LFS,     __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(LGS,     __NONE, __INVALID, __NONE),

    FLAG_DEFINE_BD(LEA,     __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(LEAVE,   __NONE, __NONE, __NONE),
    /* INVALID */
    FLAG_DEFINE_BD(LGDT,    __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(LIDT,    __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(LLDT,    __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(LMSW,    __NONE, __INVALID, __NONE),

    /* FLAG_DEFINE_BD(LOCK,    __NONE, __INVALID, __NONE), */
    /* lods */
    FLAG_DEFINE_BD(LODS,   __DF, __NONE, __NONE), /* change */

    FLAG_DEFINE_BD(LOOP,    __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(LOOPZ,   __ZF, __NONE, __NONE),
    FLAG_DEFINE_BD(LOOPNZ,  __ZF, __NONE, __NONE),

    FLAG_DEFINE_BD(LSL,     __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(LTR,     __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(LZCNT,   __NONE, __ALL_EFLAGS, __OSAPF),
    FLAG_DEFINE_BD(MONITOR, __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(MWAIT,   __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(MOV,     __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(MOV_CR,     __NONE, __NONE, __ALL_EFLAGS),
    FLAG_DEFINE_BD(MOV_DR,     __NONE, __NONE, __ALL_EFLAGS),
    /* movs */
    FLAG_DEFINE_BD(MOVS,   __DF, __NONE, __NONE),

    FLAG_DEFINE_BD(MOVSX,   __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(MOVZX,   __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(MUL,     __NONE, __ALL_EFLAGS, __SZAPF),
    FLAG_DEFINE_BD(NEG,     __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE_BD(NOP,     __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(NOT,     __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(OR,      __NONE, __ALL_EFLAGS, __AF),
    FLAG_DEFINE_BD(OUT,     __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(OUTS,   __DF, __NONE, __NONE),
    FLAG_DEFINE_BD(POP,     __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(POPA,   __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(POPF,    __NONE, __ALL_EFLAGS | __DF, __NONE),
    FLAG_DEFINE_BD(POPCNT,  __NONE, __ALL_EFLAGS , __NONE),
    FLAG_DEFINE_BD(PUSH,    __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(PUSHA,  __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(PUSHF,   __ALL_EFLAGS | __DF, __NONE, __NONE),
    FLAG_DEFINE_BD(RCL,     __CF, __CF | __OF, __NONE),
    FLAG_DEFINE_BD(RCR,     __CF, __CF | __OF, __NONE),
    FLAG_DEFINE_BD(RDMSR,   __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(RDPMC,   __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(RDTSC,   __NONE, __NONE, __NONE),
    /* FLAG_DEFINE_BD(REP,     __NONE, __INVALID, __NONE), */
    /* FLAG_DEFINE_BD(REPNE,     __NONE, __INVALID, __NONE), */
    FLAG_DEFINE_BD(RETF,     __NONE, __NONE, __NONE), /* change */
    FLAG_DEFINE_BD(RETN,     __NONE, __NONE, __NONE), /* change */
    FLAG_DEFINE_BD(ROL,     __CF, __OF | __CF, __NONE),
    FLAG_DEFINE_BD(ROR,     __CF, __OF | __CF, __NONE),
    FLAG_DEFINE_BD(RSM,     __NONE, __ALL_EFLAGS | __DF, __NONE),
    FLAG_DEFINE_BD(SAHF,    __NONE, __SZAPCF, __NONE),
    FLAG_DEFINE_BD(SAL,     __NONE, __ALL_EFLAGS, __AF), /* change */
    FLAG_DEFINE_BD(SAR,     __NONE, __ALL_EFLAGS, __AF),
    FLAG_DEFINE_BD(SHL,     __NONE, __ALL_EFLAGS, __AF),
    FLAG_DEFINE_BD(SHR,     __NONE, __ALL_EFLAGS, __AF),
    FLAG_DEFINE_BD(SBB,     __CF, __ALL_EFLAGS, __NONE),
    /* scas */
    FLAG_DEFINE_BD(SCAS,   __DF, __ALL_EFLAGS, __NONE),
    /* SETcc */
    FLAG_DEFINE_BD(SETO,    __OF, __NONE, __NONE),
    FLAG_DEFINE_BD(SETNO,   __OF, __NONE, __NONE),
    FLAG_DEFINE_BD(SETC,    __CF, __NONE, __NONE),            //SETB
    FLAG_DEFINE_BD(SETNC,   __CF, __NONE, __NONE),            //SETAE
    FLAG_DEFINE_BD(SETZ,    __ZF, __NONE, __NONE),            //SETE
    FLAG_DEFINE_BD(SETNZ,   __ZF, __NONE, __NONE),            //SETNE
    FLAG_DEFINE_BD(SETBE,   __ZF | __CF, __NONE, __NONE),
    FLAG_DEFINE_BD(SETNBE,    __ZF | __CF, __NONE, __NONE),     //SETA
    FLAG_DEFINE_BD(SETS,    __SF, __NONE, __NONE),
    FLAG_DEFINE_BD(SETNS,   __SF, __NONE, __NONE),
    FLAG_DEFINE_BD(SETP,    __PF, __NONE, __NONE),
    FLAG_DEFINE_BD(SETNP,   __PF, __NONE, __NONE),
    FLAG_DEFINE_BD(SETL,    __OF | __SF, __NONE, __NONE),
    FLAG_DEFINE_BD(SETNL,   __OF | __SF, __NONE, __NONE),     //SETGE
    FLAG_DEFINE_BD(SETLE,   __OF | __SF | __ZF, __NONE, __NONE),
    FLAG_DEFINE_BD(SETNLE,    __OF | __SF | __ZF, __NONE, __NONE),//SETG
    /* INVALID */
    FLAG_DEFINE_BD(SGDT,    __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(SIDT,    __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(SLDT,    __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(SMSW,    __NONE, __INVALID, __NONE),

    FLAG_DEFINE_BD(SHLD,    __NONE, __ALL_EFLAGS, __AF), /* change */
    FLAG_DEFINE_BD(SHRD,    __NONE, __ALL_EFLAGS, __AF),

    FLAG_DEFINE_BD(STC,     __NONE, __CF, __NONE),
    FLAG_DEFINE_BD(STD,     __NONE, __DF, __NONE),
    FLAG_DEFINE_BD(STI,     __NONE, __INVALID, __NONE),
    /* stos */
    FLAG_DEFINE_BD(STOS,   __NONE, __NONE, __NONE),

    FLAG_DEFINE_BD(STR,     __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(SUB,     __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE_BD(TEST,    __NONE, __ALL_EFLAGS, __AF),
    FLAG_DEFINE_BD(TZCNT,   __NONE, __ALL_EFLAGS, __OSAPF),
    FLAG_DEFINE_BD(UD0,     __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(VERR,    __NONE, __ZF, __NONE),
    FLAG_DEFINE_BD(VERW,    __NONE, __ZF, __NONE),
    FLAG_DEFINE_BD(WAIT,    __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(WBINVD,  __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(WRMSR,   __NONE, __INVALID, __NONE),
    FLAG_DEFINE_BD(XADD,    __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE_BD(XCHG,    __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(XLATB,   __NONE, __NONE, __NONE),
    FLAG_DEFINE_BD(XOR,     __NONE, __ALL_EFLAGS, __AF),

    FLAG_DEFINE_BD(ADCX,    __CF, __CF, __NONE),
    FLAG_DEFINE_BD(ADOX,    __OF, __OF, __NONE),
};


const IR1_EFLAG_USEDEF *ir1_opcode_to_eflag_usedef_bd(IR1_INST *ir1)
{
    lsassertm((ir1_opcode_eflag_usedef_bd +
        (ir1_opcode_bd(ir1) - ND_INS_INVALID))->use !=
         __INVALID, "%s\n", ((INSTRUX *)(ir1->info))->Mnemonic);
    return ir1_opcode_eflag_usedef_bd + (ir1_opcode_bd(ir1) - ND_INS_INVALID);
}

#ifdef CONFIG_LATX_FLAG_REDUCTION
static inline uint32_t rotate_shift_get_masked_imm_bd(IR1_OPND_BD *d, IR1_OPND_BD *s)
{
#ifdef TARGET_X86_64
    if (ir1_opnd_size_bd(d) == 64) {
        return ir1_opnd_uimm_bd(s) & 0x3f;
    } else
#endif
    {
        return ir1_opnd_uimm_bd(s) & 0x1f;
    }
}

static inline bool rotate_need_of_bd(IR1_INST *pir1)
{
    IR1_OPCODE_BD op = ir1_opcode_bd(pir1);
    if (op == ND_INS_RCL || op == ND_INS_RCR ||
        op == ND_INS_ROL || op == ND_INS_ROR) {
        IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
        IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
        if (!ir1_opnd_is_imm_bd(opnd1)) return true;
        if (rotate_shift_get_masked_imm_bd(opnd0, opnd1) == 0) return true;
    }
    return false;
}

static inline bool shift_need_oszpcf_bd(IR1_INST *pir1)
{
    IR1_OPCODE_BD op = ir1_opcode_bd(pir1);
    if (op == ND_INS_SAL || op == ND_INS_SAR ||
        op == ND_INS_SHL || op == ND_INS_SHR) {
        IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
        IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
        if (!ir1_opnd_is_imm_bd(opnd1)) return true;
        if (rotate_shift_get_masked_imm_bd(opnd0, opnd1) == 0) return true;
    }
    return false;
}

static inline bool double_shift_need_all_bd(IR1_INST *pir1)
{
    IR1_OPCODE_BD op = ir1_opcode_bd(pir1);
    if (op == ND_INS_SHLD || op == ND_INS_SHRD) {
        IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
        IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
        if (!ir1_opnd_is_imm_bd(opnd1)) return true;
        if (rotate_shift_get_masked_imm_bd(opnd0, opnd1) == 0) return true;
    }
    return false;
}

static inline bool cmp_scas_need_zf_bd(IR1_INST *pir1)
{
    return (ir1_has_prefix_repe_bd(pir1) ||
            ir1_has_prefix_repne_bd(pir1)) &&
           (ir1_opcode_bd(pir1) == ND_INS_CMPS ||
            ir1_opcode_bd(pir1) == ND_INS_SCAS);
}

/**
 * @brief Find if we have enough information by scan TB
 *
 * @param tb Current TB
 * @return true We can find next TB to get more information
 * if you want
 * @return false We need not to scan next TB
 */
#ifndef CONFIG_LATX_DECODE_DEBUG
static bool flag_reduction_pass1(void *tb)
{
#ifdef CONFIG_LATX_PROFILER
    TCGProfile *prof = &tcg_ctx->prof;
    qatomic_inc(&prof->flag_rdtn_times);
    time_t ti = profile_getclock();
#endif
    TranslationBlock *ptb = (TranslationBlock *)tb;
    IR1_INST *pir1 = NULL;

    /* scanning if this insts will def ALL_EFLAGS */
    for (int i = tb_ir1_num_bd(ptb) - 1; i >= 0; --i) {
        pir1 = tb_ir1_inst_bd(ptb, i);
        lsassert(pir1->decode_engine == OPT_DECODE_BY_BDDISASM);
        const IR1_EFLAG_USEDEF *usedef = ir1_opcode_to_eflag_usedef_bd(pir1);
        /*
        * NOTE: if you find some insts will use ALL_EFLAGS
        * you can add this case:
        * if (usedef->use == __ALL_EFLAGS) return false;
        */
        if (usedef->use != __NONE) {
            goto _false_path;
        } else if (usedef->def == __NONE) {
            /* curr_inst not def any flags */
            continue;
        } else {
            break;
        }
    }
#ifdef CONFIG_LATX_PROFILER
    qatomic_add(&prof->flag_rdtn_pass1, profile_getclock() - ti);
    qatomic_inc(&prof->flag_rdtn_stimes);
#endif
    return true;
_false_path:
#ifdef CONFIG_LATX_PROFILER
    qatomic_add(&prof->flag_rdtn_pass1, profile_getclock() - ti);
#endif
    return false;
}
#endif

uint8 pending_use_of_succ_bd(void *tb, int indirect_depth, int max_depth)
{
    TranslationBlock *ptb = (TranslationBlock *)tb;
    IR1_INST *pir1 = tb_ir1_inst_last_bd(ptb);
    if (ir1_is_syscall_bd(pir1) ||
        (ir1_is_call_bd(pir1) && ir1_is_indirect_call_bd(pir1))) {
        return __NONE;
    } else {
        return __ALL_EFLAGS;
    }
}

/**
 * @brief Caculate current flag information
 *
 * @param pir1 Current inst
 * @param pending_use The flag after this inst need use
 */
void flag_reduction_bd(IR1_INST *pir1, uint8 *pending_use)
{
    uint8 current_def = __NONE;
#ifdef CONFIG_LATX_PROFILER
    TCGProfile *prof = &tcg_ctx->prof;
    time_t ti = profile_getclock();
#endif
    /*
     * 1. Get current inst flag defination
     *   - flag def:     current inst will generate flags
     *   - flag use:     current inst will use flags
     *   - flag undef:   current inst mark undef flags
     */
    IR1_EFLAG_USEDEF curr_usedef = *ir1_opcode_to_eflag_usedef_bd(pir1);

#ifndef CONFIG_LATX_RADICAL_EFLAGS
    current_def = curr_usedef.def;
    /*
     * RCL/RCR/ROL/ROR - Rotate:
     * If the masked count is 0, the flags are not affected. If the masked
     * count is 1, then the OF flag is affected, otherwise (masked count is
     * greater than 1) the OF flag is undefined. The CF flag is affected when
     * the masked count is non-zero. The SF, ZF, AF, and PF flags are always
     * unaffected.
     */
    if (rotate_need_of_bd(pir1)) {
        curr_usedef.def &= ~__OF;
    }

    /*
     * SAL/SAR/SHL/SHR - Shift:
     * The OF flag is affected only for 1-bit shifts, otherwise, it is
     * undefined. The SF, ZF, and PF flags are set according to the result.
     * If the count is 0, the flags are not affected. For a non-zero count,
     * the AF flag is undefined.
     */
    if (shift_need_oszpcf_bd(pir1)) {
        curr_usedef.def &= ~(__OSZPF | __CF);
    }

    /*
     * SHLD/SHRD - Double Precision Shift:
     * If the count operand is 0, the flags are not affected.
     */
    if (double_shift_need_all_bd(pir1)) {
        curr_usedef.def &= ~(__OSZPF);
    }

    current_def &= *pending_use;
#endif

    /*
     * 2. Drop the flag which did not need.
     *   - Pending will mark which are needed.
     */
    curr_usedef.def &= *pending_use;

    /*
     * 3. Recalculate the pending
     *   - Drop the current calculated flags.
     */
    *pending_use &= (~curr_usedef.def);

    /*
     * CMPSx and SCASx will use REPNZ/REPZ
     */
    if (cmp_scas_need_zf_bd(pir1)) {
        /* "rep cmps" need add zf */
        curr_usedef.use |= __ZF;
    }

    /*
     * 4. Add the pending which will be used
     *   - Current inst may use some flags, so we need add to pending.
     */
    *pending_use |= curr_usedef.use;
    current_def  |= curr_usedef.def;
    ir1_set_eflag_use_bd(pir1, curr_usedef.use);
    ir1_set_eflag_def_bd(pir1, current_def & (~curr_usedef.undef));
#ifdef CONFIG_LATX_PROFILER
    qatomic_add(&prof->flag_rdtn_pass2, profile_getclock() - ti);
#endif
}

/**
 * @brief do cross tb check and information
 *
 * @param tb current TB
 * @return uint8 checked flag information
 */
#ifndef CONFIG_LATX_DECODE_DEBUG
uint8 flag_reduction_check(TranslationBlock *tb)
{
    if (flag_reduction_pass1(tb)) {
#ifdef CONFIG_LATX_PROFILER
        TCGProfile *prof = &tcg_ctx->prof;
        time_t ti = profile_getclock();
#endif
        uint8 pending_use = pending_use_of_succ_bd(tb, 1, MAX_DEPTH);
#ifdef CONFIG_LATX_PROFILER
        qatomic_add(&prof->flag_rdtn_search, profile_getclock() - ti);
#endif
        return pending_use;
    }
    return __ALL_EFLAGS;
}
#endif
#endif /* CONFIG_LATX_FLAG_REDUCTION */

/**
 * @brief Inst flag generate
 *
 * @param pir1 Current inst
 */
void flag_gen_bd(IR1_INST *pir1)
{
    const IR1_EFLAG_USEDEF curr_usedef = *ir1_opcode_to_eflag_usedef_bd(pir1);

    ir1_set_eflag_use_bd(pir1, curr_usedef.use);
    ir1_set_eflag_def_bd(pir1, curr_usedef.def & (~curr_usedef.undef));
}
