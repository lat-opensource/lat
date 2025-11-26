#include "common.h"
#include "ir1.h"
#include "translate.h"
#include "flag-reduction.h"

/**
 * @brief ir1 opcode (x86) per instruction eflags using table
 * @note If the flag is 'unaffected', please DO NOT set at 'used' part!
 */
static const IR1_EFLAG_USEDEF ir1_opcode_eflag_usedef[] = {
    FLAG_DEFINE(INVALID, __NONE, __INVALID, __INVALID),
    FLAG_DEFINE(AAA,     __AF, __ALL_EFLAGS, __OF | __SF | __ZF | __PF),
    FLAG_DEFINE(AAD,     __NONE, __ALL_EFLAGS, __OF | __AF | __CF),
    FLAG_DEFINE(AAM,     __NONE, __ALL_EFLAGS, __OF | __AF | __CF),
    FLAG_DEFINE(AAS,     __AF, __ALL_EFLAGS, __OF | __SF | __ZF | __PF),
    FLAG_DEFINE(ADC,     __CF, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(ADD,     __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(AND,     __NONE, __ALL_EFLAGS, __AF),
    FLAG_DEFINE(ANDN,    __NONE, __ALL_EFLAGS, __AF | __PF),
    FLAG_DEFINE(ARPL,    __NONE, __ZF, __NONE),
    FLAG_DEFINE(BLSI,    __NONE, __ZF | __CF | __SF | __OF, __AF | __PF),
    FLAG_DEFINE(BLSR,    __NONE, __ZF | __CF | __SF | __OF, __AF | __PF),
    FLAG_DEFINE(BLSMSK,  __NONE, __ZF | __CF | __SF | __OF, __AF | __PF),
    FLAG_DEFINE(BZHI,    __NONE, __ZF | __CF | __SF | __OF, __AF | __PF),
    FLAG_DEFINE(BOUND,   __NONE, __NONE, __NONE),
    FLAG_DEFINE(BSF,     __NONE, __ALL_EFLAGS, __OSAPF | __CF),
    FLAG_DEFINE(BSR,     __NONE, __ALL_EFLAGS, __OSAPF | __CF),
    FLAG_DEFINE(BSWAP,   __NONE, __NONE, __NONE),
    FLAG_DEFINE(BT,      __NONE, __OSAPF | __CF, __OSAPF),
    FLAG_DEFINE(BTC,     __NONE, __OSAPF | __CF, __OSAPF),
    FLAG_DEFINE(BTR,     __NONE, __OSAPF | __CF, __OSAPF),
    FLAG_DEFINE(BTS,     __NONE, __OSAPF | __CF, __OSAPF),
    FLAG_DEFINE(CALL,    __NONE, __NONE, __NONE), /* change */
    FLAG_DEFINE(CBW,     __NONE, __NONE, __NONE),
    FLAG_DEFINE(CLC,     __NONE, __CF, __NONE),
    FLAG_DEFINE(CLD,     __NONE, __DF, __NONE),
    FLAG_DEFINE(SALC,     __CF, __NONE, __NONE),
    /* FLAG_DEFINE(CLI,     __NONE, __INVALID, __NONE), */
    /* FLAG_DEFINE(CLTS,     __NONE, __INVALID, __NONE), */
    FLAG_DEFINE(CMC,     __CF, __CF, __NONE),
    /* CMOVcc */
    FLAG_DEFINE(CMOVA,   __ZF | __CF, __NONE, __NONE),
    FLAG_DEFINE(CMOVAE,  __CF, __NONE, __NONE),
    FLAG_DEFINE(CMOVB,   __CF, __NONE, __NONE),
    FLAG_DEFINE(CMOVBE,  __ZF | __CF, __NONE, __NONE),
    FLAG_DEFINE(CMOVE,   __ZF, __NONE, __NONE),
    FLAG_DEFINE(CMOVG,   __OF | __SF | __ZF, __NONE, __NONE),
    FLAG_DEFINE(CMOVGE,  __OF | __SF, __NONE, __NONE),
    FLAG_DEFINE(CMOVL,   __OF | __SF, __NONE, __NONE),
    FLAG_DEFINE(CMOVLE,  __OF | __SF | __ZF, __NONE, __NONE),
    FLAG_DEFINE(CMOVNE,  __ZF, __NONE, __NONE),
    FLAG_DEFINE(CMOVO,   __OF, __NONE, __NONE),
    FLAG_DEFINE(CMOVNO,  __OF, __NONE, __NONE),
    FLAG_DEFINE(CMOVP,   __PF, __NONE, __NONE),
    FLAG_DEFINE(CMOVNP,  __PF, __NONE, __NONE),
    FLAG_DEFINE(CMOVS,   __SF, __NONE, __NONE),
    FLAG_DEFINE(CMOVNS,  __SF, __NONE, __NONE),
    FLAG_DEFINE(CMOVLE,  __OF | __SF | __ZF, __NONE, __NONE),
    FLAG_DEFINE(CMOVLE,  __OF | __SF | __ZF, __NONE, __NONE),
    FLAG_DEFINE(CMOVLE,  __OF | __SF | __ZF, __NONE, __NONE),

    FLAG_DEFINE(CMP,     __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(CMPSB,   __DF, __ALL_EFLAGS, __NONE), /* change */
    FLAG_DEFINE(CMPSW,   __DF, __ALL_EFLAGS, __NONE), /* change */
    FLAG_DEFINE(CMPSD,   __DF, __ALL_EFLAGS, __NONE), /* change */
    FLAG_DEFINE(CMPSQ,   __DF, __ALL_EFLAGS, __NONE), /* change */
    FLAG_DEFINE(CMPXCHG, __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(CMPXCHG8B, __NONE, __ZF, __NONE),
    FLAG_DEFINE(CMPXCHG16B, __NONE, __ZF, __NONE),
    FLAG_DEFINE(COMISD,  __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(COMISS,  __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(CPUID,   __NONE, __NONE, __NONE),
    FLAG_DEFINE(CWD,     __NONE, __NONE, __NONE),
    FLAG_DEFINE(DAA,     __AF | __CF, __ALL_EFLAGS, __OF), /* change */
    FLAG_DEFINE(DAS,     __AF | __CF, __ALL_EFLAGS, __OF), /* change */
    FLAG_DEFINE(DEC,     __NONE, __OF | __SZAPF, __NONE),
    FLAG_DEFINE(DIV,     __NONE, __ALL_EFLAGS, __ALL_EFLAGS),
    FLAG_DEFINE(ENTER,   __NONE, __NONE, __NONE),

    /* FLAG_DEFINE(ESC,     __NONE, __INVALID, __NONE), */
    /* FCMOVcc */
    FLAG_DEFINE(FCMOVB,  __CF, __NONE, __NONE),
    FLAG_DEFINE(FCMOVBE, __ZF | __CF, __NONE, __NONE),
    FLAG_DEFINE(FCMOVE,  __ZF, __NONE, __NONE),
    FLAG_DEFINE(FCMOVNB, __CF, __NONE, __NONE),
    FLAG_DEFINE(FCMOVNBE, __ZF | __CF, __NONE, __NONE),
    FLAG_DEFINE(FCMOVNE, __ZF, __NONE, __NONE),
    FLAG_DEFINE(FCMOVNU, __PF, __NONE, __NONE),
    FLAG_DEFINE(FCMOVU,  __PF, __NONE, __NONE),

    FLAG_DEFINE(FCOMI,   __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(FCOMIP,  __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(FUCOMI,  __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(FUCOMIP, __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(HLT,     __NONE, __NONE, __NONE), /* change */
    FLAG_DEFINE(IDIV,    __NONE, __ALL_EFLAGS, __ALL_EFLAGS),
    FLAG_DEFINE(IMUL,    __NONE, __ALL_EFLAGS, __SZAPF),
    FLAG_DEFINE(IN,      __NONE, __NONE, __NONE),
    FLAG_DEFINE(INC,     __NONE, __OF | __SZAPF, __NONE),
    FLAG_DEFINE(INSB,    __NONE, __DF, __NONE), /* change */
    FLAG_DEFINE(INSW,    __NONE, __DF, __NONE),
    FLAG_DEFINE(INSD,    __NONE, __DF, __NONE),
    FLAG_DEFINE(INT,     __NONE, __NONE, __NONE),
    FLAG_DEFINE(INTO,    __OF, __NONE, __NONE), /* change */
    FLAG_DEFINE(INVD,    __NONE, __NONE, __NONE),
    FLAG_DEFINE(INVLPG,  __NONE, __NONE, __NONE),
    FLAG_DEFINE(UCOMISD, __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(UCOMISS, __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(IRET,    __NONE, __INVALID, __NONE),

    /* Jcc */
    FLAG_DEFINE(JO,      __OF, __NONE, __NONE),
    FLAG_DEFINE(JNO,     __OF, __NONE, __NONE),
    FLAG_DEFINE(JB,      __CF, __NONE, __NONE),
    FLAG_DEFINE(JAE,     __CF, __NONE, __NONE),
    FLAG_DEFINE(JE,      __ZF, __NONE, __NONE),
    FLAG_DEFINE(JNE,     __ZF, __NONE, __NONE),
    FLAG_DEFINE(JBE,     __CF | __ZF, __NONE, __NONE),
    FLAG_DEFINE(JA,      __CF | __ZF, __NONE, __NONE),
    FLAG_DEFINE(JS,      __SF, __NONE, __NONE),
    FLAG_DEFINE(JNS,     __SF, __NONE, __NONE),
    FLAG_DEFINE(JP,      __PF, __NONE, __NONE),
    FLAG_DEFINE(JNP,     __PF, __NONE, __NONE),
    FLAG_DEFINE(JL,      __OF | __SF, __NONE, __NONE),
    FLAG_DEFINE(JGE,     __OF | __SF, __NONE, __NONE),
    FLAG_DEFINE(JLE,     __OF | __SF | __ZF, __NONE, __NONE),
    FLAG_DEFINE(JG,      __OF | __SF | __ZF, __NONE, __NONE),

    FLAG_DEFINE(JCXZ,    __NONE, __NONE, __NONE),
    FLAG_DEFINE(JMP,     __NONE, __NONE, __NONE),
    FLAG_DEFINE(LAHF,    __SZAPCF, __NONE, __NONE), /* change */
    /* INVALID */
    FLAG_DEFINE(LAR,     __NONE, __INVALID, __NONE),
    FLAG_DEFINE(LDS,     __NONE, __INVALID, __NONE),
    FLAG_DEFINE(LES,     __NONE, __INVALID, __NONE),
    FLAG_DEFINE(LSS,     __NONE, __INVALID, __NONE),
    FLAG_DEFINE(LFS,     __NONE, __INVALID, __NONE),
    FLAG_DEFINE(LGS,     __NONE, __INVALID, __NONE),

    FLAG_DEFINE(LEA,     __NONE, __NONE, __NONE),
    FLAG_DEFINE(LEAVE,   __NONE, __NONE, __NONE),
    /* INVALID */
    FLAG_DEFINE(LGDT,    __NONE, __INVALID, __NONE),
    FLAG_DEFINE(LIDT,    __NONE, __INVALID, __NONE),
    FLAG_DEFINE(LLDT,    __NONE, __INVALID, __NONE),
    FLAG_DEFINE(LMSW,    __NONE, __INVALID, __NONE),

    /* FLAG_DEFINE(LOCK,    __NONE, __INVALID, __NONE), */
    /* lods */
    FLAG_DEFINE(LODSB,   __DF, __NONE, __NONE), /* change */
    FLAG_DEFINE(LODSW,   __DF, __NONE, __NONE),
    FLAG_DEFINE(LODSD,   __DF, __NONE, __NONE),
    FLAG_DEFINE(LODSQ,   __DF, __NONE, __NONE),

    FLAG_DEFINE(LOOP,    __NONE, __NONE, __NONE),
    FLAG_DEFINE(LOOPE,   __ZF, __NONE, __NONE),
    FLAG_DEFINE(LOOPNE,  __ZF, __NONE, __NONE),

    FLAG_DEFINE(LSL,     __NONE, __INVALID, __NONE),
    FLAG_DEFINE(LTR,     __NONE, __INVALID, __NONE),
    FLAG_DEFINE(LZCNT,   __NONE, __ALL_EFLAGS, __OSAPF),
    FLAG_DEFINE(MONITOR, __NONE, __INVALID, __NONE),
    FLAG_DEFINE(MWAIT,   __NONE, __INVALID, __NONE),
    FLAG_DEFINE(MOV,     __NONE, __NONE, __NONE),
    FLAG_DEFINE(MOVABS,  __NONE, __NONE, __NONE),
    /* movs */
    FLAG_DEFINE(MOVSB,   __DF, __NONE, __NONE),
    FLAG_DEFINE(MOVSW,   __DF, __NONE, __NONE),
    FLAG_DEFINE(MOVSD,   __DF, __NONE, __NONE),
    FLAG_DEFINE(MOVSQ,   __DF, __NONE, __NONE),

    FLAG_DEFINE(MOVSX,   __NONE, __NONE, __NONE),
    FLAG_DEFINE(MOVZX,   __NONE, __NONE, __NONE),
    FLAG_DEFINE(MUL,     __NONE, __ALL_EFLAGS, __SZAPF),
    FLAG_DEFINE(NEG,     __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(NOP,     __NONE, __NONE, __NONE),
    FLAG_DEFINE(NOT,     __NONE, __NONE, __NONE),
    FLAG_DEFINE(OR,      __NONE, __ALL_EFLAGS, __AF),
    FLAG_DEFINE(OUT,     __NONE, __NONE, __NONE),
    FLAG_DEFINE(OUTSB,   __DF, __NONE, __NONE),
    FLAG_DEFINE(OUTSW,   __DF, __NONE, __NONE),
    FLAG_DEFINE(OUTSD,   __DF, __NONE, __NONE),
    FLAG_DEFINE(POP,     __NONE, __NONE, __NONE),
    FLAG_DEFINE(POPAW,   __NONE, __NONE, __NONE),
    FLAG_DEFINE(POPAL,   __NONE, __NONE, __NONE),
    FLAG_DEFINE(POPF,    __NONE, __ALL_EFLAGS | __DF, __NONE),
    FLAG_DEFINE(POPFD,   __NONE, __ALL_EFLAGS | __DF, __NONE),
    FLAG_DEFINE(POPFQ,   __NONE, __ALL_EFLAGS | __DF, __NONE),
    FLAG_DEFINE(POPCNT,  __NONE, __ALL_EFLAGS , __NONE),
    FLAG_DEFINE(PUSH,    __NONE, __NONE, __NONE),
    FLAG_DEFINE(PUSHAW,  __NONE, __NONE, __NONE),
    FLAG_DEFINE(PUSHAL,  __NONE, __NONE, __NONE),
    FLAG_DEFINE(PUSHF,   __ALL_EFLAGS | __DF, __NONE, __NONE),
    FLAG_DEFINE(PUSHFD,  __ALL_EFLAGS | __DF, __NONE, __NONE),
    FLAG_DEFINE(PUSHFQ,  __ALL_EFLAGS | __DF, __NONE, __NONE),
    FLAG_DEFINE(RCL,     __CF, __CF | __OF, __NONE),
    FLAG_DEFINE(RCR,     __CF, __CF | __OF, __NONE),
    FLAG_DEFINE(RDMSR,   __NONE, __INVALID, __NONE),
    FLAG_DEFINE(RDPMC,   __NONE, __INVALID, __NONE),
    FLAG_DEFINE(RDTSC,   __NONE, __NONE, __NONE),
    /* FLAG_DEFINE(REP,     __NONE, __INVALID, __NONE), */
    /* FLAG_DEFINE(REPNE,     __NONE, __INVALID, __NONE), */
    FLAG_DEFINE(RET,     __NONE, __NONE, __NONE), /* change */
    FLAG_DEFINE(ROL,     __NONE, __OF | __CF, __NONE),
    FLAG_DEFINE(ROR,     __NONE, __OF | __CF, __NONE),
    FLAG_DEFINE(RSM,     __NONE, __ALL_EFLAGS | __DF, __NONE),
    FLAG_DEFINE(SAHF,    __NONE, __SZAPCF, __NONE),
    FLAG_DEFINE(SAL,     __NONE, __ALL_EFLAGS, __AF), /* change */
    FLAG_DEFINE(SAR,     __NONE, __ALL_EFLAGS, __AF),
    FLAG_DEFINE(SHL,     __NONE, __ALL_EFLAGS, __AF),
    FLAG_DEFINE(SHR,     __NONE, __ALL_EFLAGS, __AF),
    FLAG_DEFINE(SBB,     __CF, __ALL_EFLAGS, __NONE),
    /* scas */
    FLAG_DEFINE(SCASB,   __DF, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(SCASW,   __DF, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(SCASD,   __DF, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(SCASQ,   __DF, __ALL_EFLAGS, __NONE),
    /* SETcc */
    FLAG_DEFINE(SETO,    __OF, __NONE, __NONE),
    FLAG_DEFINE(SETNO,   __OF, __NONE, __NONE),
    FLAG_DEFINE(SETB,    __CF, __NONE, __NONE),
    FLAG_DEFINE(SETAE,   __CF, __NONE, __NONE),
    FLAG_DEFINE(SETE,    __ZF, __NONE, __NONE),
    FLAG_DEFINE(SETNE,   __ZF, __NONE, __NONE),
    FLAG_DEFINE(SETBE,   __ZF | __CF, __NONE, __NONE),
    FLAG_DEFINE(SETA,    __ZF | __CF, __NONE, __NONE),
    FLAG_DEFINE(SETS,    __SF, __NONE, __NONE),
    FLAG_DEFINE(SETNS,   __SF, __NONE, __NONE),
    FLAG_DEFINE(SETP,    __PF, __NONE, __NONE),
    FLAG_DEFINE(SETNP,   __PF, __NONE, __NONE),
    FLAG_DEFINE(SETL,    __OF | __SF, __NONE, __NONE),
    FLAG_DEFINE(SETGE,   __OF | __SF, __NONE, __NONE),
    FLAG_DEFINE(SETLE,   __OF | __SF | __ZF, __NONE, __NONE),
    FLAG_DEFINE(SETG,    __OF | __SF | __ZF, __NONE, __NONE),
    /* INVALID */
    FLAG_DEFINE(SGDT,    __NONE, __INVALID, __NONE),
    FLAG_DEFINE(SIDT,    __NONE, __INVALID, __NONE),
    FLAG_DEFINE(SLDT,    __NONE, __INVALID, __NONE),
    FLAG_DEFINE(SMSW,    __NONE, __INVALID, __NONE),

    FLAG_DEFINE(SHLD,    __NONE, __ALL_EFLAGS, __AF), /* change */
    FLAG_DEFINE(SHRD,    __NONE, __ALL_EFLAGS, __AF),

    FLAG_DEFINE(STC,     __NONE, __CF, __NONE),
    FLAG_DEFINE(STD,     __NONE, __DF, __NONE),
    FLAG_DEFINE(STI,     __NONE, __INVALID, __NONE),
    /* stos */
    FLAG_DEFINE(STOSB,   __NONE, __NONE, __NONE),
    FLAG_DEFINE(STOSW,   __NONE, __NONE, __NONE),
    FLAG_DEFINE(STOSD,   __NONE, __NONE, __NONE),
    FLAG_DEFINE(STOSQ,   __NONE, __NONE, __NONE),

    FLAG_DEFINE(STR,     __NONE, __INVALID, __NONE),
    FLAG_DEFINE(SUB,     __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(TEST,    __NONE, __ALL_EFLAGS, __AF),
    FLAG_DEFINE(TZCNT,   __NONE, __ALL_EFLAGS, __OSAPF),
    FLAG_DEFINE(UD0,     __NONE, __NONE, __NONE),
    FLAG_DEFINE(VERR,    __NONE, __ZF, __NONE),
    FLAG_DEFINE(VERW,    __NONE, __ZF, __NONE),
    FLAG_DEFINE(WAIT,    __NONE, __NONE, __NONE),
    FLAG_DEFINE(WBINVD,  __NONE, __INVALID, __NONE),
    FLAG_DEFINE(WRMSR,   __NONE, __INVALID, __NONE),
    FLAG_DEFINE(XADD,    __NONE, __ALL_EFLAGS, __NONE),
    FLAG_DEFINE(XCHG,    __NONE, __NONE, __NONE),
    FLAG_DEFINE(XLATB,   __NONE, __NONE, __NONE),
    FLAG_DEFINE(XOR,     __NONE, __ALL_EFLAGS, __AF),

    FLAG_DEFINE(ADCX,    __CF, __CF, __NONE),
    FLAG_DEFINE(ADOX,    __OF, __OF, __NONE),
    FLAG_DEFINE(ENDING,  __INVALID, __INVALID, __INVALID),
};


static const IR1_EFLAG_USEDEF *ir1_opcode_to_eflag_usedef(IR1_INST *ir1)
{
    if (ir1_opcode(ir1) == dt_X86_INS_MOVSD) {
        if (ir1->info->x86.opcode[0] == 0xa5) {
            return ir1_opcode_eflag_usedef +
                (dt_X86_INS_MOVSB - dt_X86_INS_INVALID);
        }
    }
    lsassertm((ir1_opcode_eflag_usedef +
        (ir1_opcode(ir1) - dt_X86_INS_INVALID))->use !=
         __INVALID, "%s\n", ir1->info->mnemonic);
    return ir1_opcode_eflag_usedef + (ir1_opcode(ir1) - dt_X86_INS_INVALID);
}

#ifdef CONFIG_LATX_FLAG_REDUCTION

static inline uint32_t rotate_shift_get_masked_imm(IR1_OPND *d, IR1_OPND *s)
{
#ifdef TARGET_X86_64
    if (ir1_opnd_size(d) == 64) {
        return ir1_opnd_uimm(s) & 0x3f;
    } else
#endif
    {
        return ir1_opnd_uimm(s) & 0x1f;
    }
}

static inline bool rotate_need_of(IR1_INST *pir1)
{
    IR1_OPCODE op = ir1_opcode(pir1);
    if (op == dt_X86_INS_RCL || op == dt_X86_INS_RCR ||
        op == dt_X86_INS_ROL || op == dt_X86_INS_ROR) {
        if (ir1_get_opnd_num(pir1) == 1) {
            return false;
        }
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
        IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
        if (!ir1_opnd_is_imm(opnd1)) return true;
        if (rotate_shift_get_masked_imm(opnd0, opnd1) == 0) return true;
    }
    return false;
}

static inline bool shift_need_oszpcf(IR1_INST *pir1)
{
    IR1_OPCODE op = ir1_opcode(pir1);
    if (op == dt_X86_INS_SAL || op == dt_X86_INS_SAR ||
        op == dt_X86_INS_SHL || op == dt_X86_INS_SHR) {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
        IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
        if (!ir1_opnd_is_imm(opnd1)) return true;
        if (rotate_shift_get_masked_imm(opnd0, opnd1) == 0) return true;
    }
    return false;
}

static inline bool double_shift_need_all(IR1_INST *pir1)
{
    IR1_OPCODE op = ir1_opcode(pir1);
    if (op == dt_X86_INS_SHLD || op == dt_X86_INS_SHRD) {
        IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
        IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
        if (!ir1_opnd_is_imm(opnd1)) return true;
        if (rotate_shift_get_masked_imm(opnd0, opnd1) == 0) return true;
    }
    return false;
}

static inline bool cmp_scas_need_zf(IR1_INST *pir1)
{
    return (ir1_prefix(pir1) == dt_X86_PREFIX_REPE ||
            ir1_prefix(pir1) == dt_X86_PREFIX_REPNE) &&
           (ir1_opcode(pir1) == dt_X86_INS_CMPSB ||
            ir1_opcode(pir1) == dt_X86_INS_CMPSW ||
            ir1_opcode(pir1) == dt_X86_INS_CMPSD ||
            ir1_opcode(pir1) == dt_X86_INS_CMPSQ ||
            ir1_opcode(pir1) == dt_X86_INS_SCASB ||
            ir1_opcode(pir1) == dt_X86_INS_SCASW ||
            ir1_opcode(pir1) == dt_X86_INS_SCASD ||
            ir1_opcode(pir1) == dt_X86_INS_SCASQ);
}

/**
 * @brief Find if we have enough information by scan TB
 *
 * @param tb Current TB
 * @return true We can find next TB to get more information
 * if you want
 * @return false We need not to scan next TB
 */
static bool flag_reduction_pass1(void *tb)
{
#ifdef CONFIG_LATX_PROFILER
    TCGProfile *prof = &tcg_ctx->prof;
    qatomic_inc(&prof->flag_rdtn_times);
    time_t ti = profile_getclock();
#endif
    TranslationBlock *ptb = (TranslationBlock *)tb;
    IR1_INST *pir1 = dt_X86_INS_INVALID;

    /* scanning if this insts will def ALL_EFLAGS */
    for (int i = tb_ir1_num(ptb) - 1; i >= 0; --i) {
        pir1 = tb_ir1_inst(ptb, i);
        const IR1_EFLAG_USEDEF *usedef = ir1_opcode_to_eflag_usedef(pir1);

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

uint8 pending_use_of_succ(void *tb, int indirect_depth, int max_depth)
{
    TranslationBlock *ptb = (TranslationBlock *)tb;
    IR1_INST *pir1 = tb_ir1_inst_last(ptb);
    if (ir1_is_syscall(pir1) ||
        (ir1_is_call(pir1) && ir1_is_indirect_call(pir1))) {
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
void flag_reduction(IR1_INST *pir1, uint8 *pending_use)
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
    IR1_EFLAG_USEDEF curr_usedef = *ir1_opcode_to_eflag_usedef(pir1);

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
    if (rotate_need_of(pir1)) {
        curr_usedef.def &= ~__OF;
    }

    /*
     * SAL/SAR/SHL/SHR - Shift:
     * The OF flag is affected only for 1-bit shifts, otherwise, it is
     * undefined. The SF, ZF, and PF flags are set according to the result.
     * If the count is 0, the flags are not affected. For a non-zero count,
     * the AF flag is undefined.
     */
    if (shift_need_oszpcf(pir1)) {
        curr_usedef.def &= ~(__OSZPF | __CF);
    }

    /*
     * SHLD/SHRD - Double Precision Shift:
     * If the count operand is 0, the flags are not affected.
     */
    if (double_shift_need_all(pir1)) {
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
    if (cmp_scas_need_zf(pir1)) {
        /* "rep cmps" need add zf */
        curr_usedef.use |= __ZF;
    }

    /*
     * 4. Add the pending which will be used
     *   - Current inst may use some flags, so we need add to pending.
     */
    *pending_use |= curr_usedef.use;
    current_def  |= curr_usedef.def;
    ir1_set_eflag_use(pir1, curr_usedef.use);
    ir1_set_eflag_def(pir1, current_def & (~curr_usedef.undef));
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
uint8 flag_reduction_check(TranslationBlock *tb)
{
    if (flag_reduction_pass1(tb)) {
#ifdef CONFIG_LATX_PROFILER
        TCGProfile *prof = &tcg_ctx->prof;
        time_t ti = profile_getclock();
#endif
        uint8 pending_use = pending_use_of_succ(tb, 1, MAX_DEPTH);
#ifdef CONFIG_LATX_PROFILER
        qatomic_add(&prof->flag_rdtn_search, profile_getclock() - ti);
#endif
        return pending_use;
    }
    return __ALL_EFLAGS;
}

#endif /* CONFIG_LATX_FLAG_REDUCTION */

/**
 * @brief Inst flag generate
 *
 * @param pir1 Current inst
 */
void flag_gen(IR1_INST *pir1)
{
    const IR1_EFLAG_USEDEF curr_usedef = *ir1_opcode_to_eflag_usedef(pir1);

    ir1_set_eflag_use(pir1, curr_usedef.use);
    ir1_set_eflag_def(pir1, curr_usedef.def & (~curr_usedef.undef));
}
