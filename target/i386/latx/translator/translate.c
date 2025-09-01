#include "lsenv.h"
#include "reg-alloc.h"
#include "flag-lbt.h"
#include <string.h>
#include "latx-options.h"
#include "fpu/softfloat.h"
#include "profile.h"
#include "translate.h"
#include "runtime-trace.h"
#include "exec/translate-all.h"
#include "ir2-relocate.h"
#include "tu.h"
#include "imm-cache.h"

static inline uint8_t cpu_read_code_via_qemu(void *cpu, ADDRX pc)
{
    return cpu_ldub_code((CPUX86State *)cpu, (target_ulong)pc);
}

#ifdef CONFIG_LATX_TU
static char insn_info[MAX_IR1_IN_TU * IR1_INST_SIZE] = {0};
static IR1_INST ir1_list_rel[MAX_IR1_IN_TU];
#else
/* we creat an array to fill all 255 ir1's info. */
#ifdef CONFIG_LATX_TU
static char insn_info[MAX_TB_IN_TU * MAX_IR1_NUM_PER_TB * IR1_INST_SIZE] = {0};
#else
static char insn_info[MAX_IR1_NUM_PER_TB * IR1_INST_SIZE] = {0};
#endif
static IR1_INST ir1_list[MAX_IR1_NUM_PER_TB];
#endif

IR1_INST *get_ir1_list(struct TranslationBlock *tb, ADDRX pc, int max_insns)
{
    static uint8_t inst_cache[TCG_MAX_INSNS];
    uint8_t  *pins = inst_cache;
    IR1_INST *pir1 = NULL;
    void *pir1_base = insn_info;
    ADDRX start_pc = pc;

#ifdef CONFIG_LATX_TU
    /* TODO */
    //IR1_INST *ir1_list = (IR1_INST *)mm_calloc(max_insns, sizeof(IR1_INST));
    uint32_t *ir1_num_in_tu = &(tu_data->ir1_num_in_tu);
    if (tb->s_data->tu_tb_mode == TB_GEN_CODE) {
        *ir1_num_in_tu = 0;
    }
    IR1_INST *ir1_list = ir1_list_rel + (*ir1_num_in_tu);
#endif

    if (max_insns == 1) {
        max_insns++;
    }

#if defined(TARGET_VSYSCALL_PAGE) && defined(TARGET_X86_64)
    /*
     * Detect entry into the vsyscall page and invoke the syscall.
     */
    if (CODEIS64 && (pc & TARGET_PAGE_MASK) == TARGET_VSYSCALL_PAGE) {
            mmap_unlock();
            helper_raise_exception((CPUX86State *)lsenv->cpu_state,
                                    EXCP_VSYSCALL);
    }
#endif

    int ir1_num = 0;
    do {
        /* read 32 instructioin bytes */
        lsassert(lsenv->cpu_state != NULL);
        /*
         * Wine-6.0 implement try/except via C code. So there has chance to access some
         * iliigal address, such as 0.
         * LATX need to identify this kind of address via qemu cpu_ldub_code api to handle
         * this scenario.
         * Here are some side effect for this solution, performance might be downgrade if
         * running some benchmarks.
         * TODO: To fix it completely, it's better to use cpu_ldub_code inside capstone to
         * avoid perforamnce downgrade.
         */
        pins = inst_cache;

        int max_insn_len = get_insn_len_readable(pc);
        for (int i = 0; i < max_insn_len; ++i) {
            *pins = cpu_read_code_via_qemu(lsenv->cpu_state, pc + i);
            pins++;
        }
        /* disasemble this instruction */
        pir1 = &ir1_list[ir1_num];
        /* get next pc */
        if(debug_with_dis((void *)inst_cache)) {
#ifdef CONFIG_LATX_TU
            pc = ir1_disasm_bd(pir1, inst_cache, pc, *ir1_num_in_tu + ir1_num, pir1_base);
#else
            pc = ir1_disasm_bd(pir1, inst_cache, pc, ir1_num, pir1_base);
#endif
        } else {
#ifdef CONFIG_LATX_TU
            pc = ir1_disasm(pir1, inst_cache, pc, *ir1_num_in_tu + ir1_num, pir1_base);
#else
            pc = ir1_disasm(pir1, inst_cache, pc, ir1_num, pir1_base);
#endif
        }
        if (pir1->info == NULL) {
#if defined(CONFIG_LATX_TU)
            tb->s_data->tu_tb_mode = TU_TB_MODE_BROKEN;
            tb->s_data->next_pc = tb->pc;
#endif
            break;
        }
        ir1_num++;
        lsassert(ir1_num <= 255);

        if (pir1->decode_engine == OPT_DECODE_BY_CAPSTONE) {
            /* check if TB is too large */
#ifdef CONFIG_LATX_DEBUG
            if (ir1_num == max_insns) {
#else
            if (ir1_num == max_insns && !ir1_is_tb_ending(pir1)) {
#endif
                pc = ir1_addr(pir1);
                ir1_make_ins_JMP(pir1, pc, 0);
                break;
            }
        } else {
            /* check if TB is too large */
#ifdef CONFIG_LATX_DEBUG
            if (ir1_num == max_insns) {
#else
            if (ir1_num == max_insns && !ir1_is_tb_ending_bd(pir1)) {
#endif
                pc = ir1_addr_bd(pir1);
                ir1_make_ins_JMP_bd(pir1, pc);
                break;
            }
        }
    } while (!(pir1->decode_engine == OPT_DECODE_BY_CAPSTONE ? 
                ir1_is_tb_ending(pir1) : ir1_is_tb_ending_bd(pir1)));
    tb->size = pc - start_pc;
    tb->icount = ir1_num;
#if defined(CONFIG_LATX_TU) || defined(CONFIG_LATX_AOT)
    if (ir1_list[ir1_num - 1].decode_engine == OPT_DECODE_BY_CAPSTONE)
        get_last_info(tb, &ir1_list[ir1_num - 1]);
    else
        get_last_info_bd(tb, &ir1_list[ir1_num - 1]);
#endif

    if (!ir1_num)
        return NULL;

#ifndef CONFIG_LATX_TU
    /* when using TU to translate, dont need the last jmp. */
    if (ir1_num < MAX_IR1_NUM_PER_TB) {
        IR1_INST *next_pir1 = &ir1_list[ir1_num];
        next_pir1->info = NULL;
    }
#endif
    if (pir1->info != NULL && ir1_num == 2 &&
        (pir1->decode_engine == OPT_DECODE_BY_CAPSTONE ? ir1_is_return(pir1): ir1_is_return_bd(pir1)) &&
        (ir1_list[0].decode_engine == OPT_DECODE_BY_CAPSTONE ? ir1_opcode(&ir1_list[0]) == dt_X86_INS_MOV :
        ir1_opcode_bd(&ir1_list[0]) == ND_INS_MOV)) {
        IR1_INST *insert_ir1 = &ir1_list[0];
        if (insert_ir1->decode_engine == OPT_DECODE_BY_CAPSTONE) {
            IR1_OPND *opnd1 = ir1_get_opnd(insert_ir1, 1);
            if (ir1_opnd_type(opnd1) == dt_X86_OP_MEM &&
            ir1_opnd_base_reg(opnd1) == dt_X86_REG_ESP &&
            ir1_opnd_simm(opnd1) == 0) {
            IR1_OPND *opnd0 = ir1_get_opnd(insert_ir1, 0);
            int reg_index = ir1_opnd_base_reg_num(opnd0);
            ht_pc_thunk_insert(start_pc, reg_index);
            }
        } else {
            IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(insert_ir1, 1);
            if (ir1_opnd_type_bd(opnd1) == ND_OP_MEM &&
                (ir1_opnd_has_base_bd(opnd1) && ir1_opnd_base_reg_bd(opnd1) == NDR_ESP) &&
                ir1_opnd_simm_bd(opnd1) == 0) {
                IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(insert_ir1, 0);
                int reg_index = ir1_opnd_base_reg_num_bd(opnd0);
                ht_pc_thunk_insert(start_pc, reg_index);
            }
        }
    }

#ifdef CONFIG_LATX_TU
    *ir1_num_in_tu += ir1_num;
#endif

    return ir1_list;
}

#if defined CONFIG_LATX_FLAG_REDUCTION && \
    defined(CONFIG_LATX_FLAG_REDUCTION_EXTEND)
int8 get_etb_type(IR1_INST *pir1)
{
    if (ir1_is_branch(pir1))
        return (int8)TB_TYPE_BRANCH;
    else if (ir1_is_call(pir1) && !ir1_is_indirect_call(pir1))
        return (int8)TB_TYPE_CALL;
    else if (ir1_is_jump(pir1) && !ir1_is_indirect_jmp(pir1))
        return (int8)TB_TYPE_JUMP;
    else if (ir1_is_return(pir1))
        return (int8)TB_TYPE_RETURN;
    else if (ir1_opcode(pir1) == dt_X86_INS_CALL &&
        ir1_is_indirect_call(pir1)) {
        return (int8)TB_TYPE_CALLIN;
    } else if (ir1_opcode(pir1) == dt_X86_INS_JMP &&
        ir1_is_indirect_jmp(pir1)) {
        return (int8)TB_TYPE_JUMPIN;
    } else {
        return (int8)TB_TYPE_NONE;
    }

}
#endif

bool ir1_need_calculate_cf(IR1_INST *ir1)
{
    return (ir1_is_cf_def(ir1) &&
           BITS_ARE_CLEAR(lsenv->tr_data->curr_ir1_skipped_eflags,
                          1 << CF_USEDEF_BIT_INDEX) &&
                          ir1_opcode(ir1) != dt_X86_INS_DAA &&
                          ir1_opcode(ir1) != dt_X86_INS_DAS) ||
                          ir1_opcode(ir1) == dt_X86_INS_LZCNT;
}

bool ir1_need_calculate_pf(IR1_INST *ir1)
{
    return ir1_is_pf_def(ir1) &&
           BITS_ARE_CLEAR(lsenv->tr_data->curr_ir1_skipped_eflags,
                          1 << PF_USEDEF_BIT_INDEX);
}

bool ir1_need_calculate_af(IR1_INST *ir1)
{
    return ir1_is_af_def(ir1) &&
           BITS_ARE_CLEAR(lsenv->tr_data->curr_ir1_skipped_eflags,
                          1 << AF_USEDEF_BIT_INDEX) &&
                          ir1_opcode(ir1) != dt_X86_INS_DAA &&
                          ir1_opcode(ir1) != dt_X86_INS_DAS;
}

bool ir1_need_calculate_zf(IR1_INST *ir1)
{
    return ir1_is_zf_def(ir1) &&
           BITS_ARE_CLEAR(lsenv->tr_data->curr_ir1_skipped_eflags,
                          1 << ZF_USEDEF_BIT_INDEX);
}

bool ir1_need_calculate_sf(IR1_INST *ir1)
{
    return ir1_is_sf_def(ir1) &&
           BITS_ARE_CLEAR(lsenv->tr_data->curr_ir1_skipped_eflags,
                          1 << SF_USEDEF_BIT_INDEX);
}

bool ir1_need_calculate_of(IR1_INST *ir1)
{
    return ir1_is_of_def(ir1) &&
           BITS_ARE_CLEAR(lsenv->tr_data->curr_ir1_skipped_eflags,
                          1 << OF_USEDEF_BIT_INDEX);
}

bool ir1_need_calculate_any_flag(IR1_INST *ir1)
{
    return (ir1_get_eflag_def(ir1) &
            ~(lsenv->tr_data->curr_ir1_skipped_eflags)) != 0;
}

bool ir1_need_reserve_h128(IR1_INST *ir1)
{
    if (ir1_get_opnd_num(ir1) == 0)
        return false;
    IR1_OPND *dest = ir1_get_opnd(ir1, 0);
    if (ir1_opnd_is_xmm(dest) && ir1->info->x86.opcode[0] != 0xc4
        && ir1->info->x86.opcode[0] != 0xc5) {
        return true;
    }
    return false;
}

IR2_OPND save_h128_of_ymm(IR1_INST *ir1)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(ir1, 0)));

    IR1_OPND *dest = ir1_get_opnd(ir1, 0);
    IR2_OPND temp = ra_alloc_ftemp();

    la_xvori_b(temp, ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), 0);
    return temp;
}

void restore_h128_of_ymm(IR1_INST *ir1, IR2_OPND temp)
{
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(ir1, 0)));
    lsassert(temp._type != IR2_OPND_NONE);

    IR2_OPND opnd2 = ra_alloc_xmm(ir1_opnd_base_reg_num(ir1_get_opnd(ir1, 0)));

    la_xvpermi_q(opnd2, temp, 0x12);
}

static bool (*translate_functions[])(IR1_INST *) = {
    TRANS_FUNC_GEN_REAL(INVALID, NULL),

    TRANS_FUNC_GEN(AAA, aaa),
    TRANS_FUNC_GEN(AAD, aad),
    TRANS_FUNC_GEN(AAM, aam),
    TRANS_FUNC_GEN(AAS, aas),
    TRANS_FUNC_GEN(ADC, adc),
    TRANS_FUNC_GEN(ADD, add),
    TRANS_FUNC_GEN(ADDPD, addpd),
    TRANS_FUNC_GEN(ADDPS, addps),
    TRANS_FUNC_GEN(ADDSD, addsd),
    TRANS_FUNC_GEN(ADDSS, addss),
    TRANS_FUNC_GEN(ADDSUBPD, addsubpd),
    TRANS_FUNC_GEN(ADDSUBPS, addsubps),
    TRANS_FUNC_GEN(AND, and),
    TRANS_FUNC_GEN(ANDNPD, andnpd),
    TRANS_FUNC_GEN(ANDNPS, andnps),
    TRANS_FUNC_GEN(ANDPD, andpd),
    TRANS_FUNC_GEN(ANDPS, andps),
    TRANS_FUNC_GEN(BSF, bsf),
    TRANS_FUNC_GEN(BSR, bsr),
    TRANS_FUNC_GEN(BSWAP, bswap),
    TRANS_FUNC_GEN(BT, btx),
    TRANS_FUNC_GEN(BTC, btx),
    TRANS_FUNC_GEN(BTR, btx),
    TRANS_FUNC_GEN(BTS, btx),
    TRANS_FUNC_GEN(BLSR, blsr),
    TRANS_FUNC_GEN(CALL, call),
    TRANS_FUNC_GEN(CBW, cbw),
    TRANS_FUNC_GEN(CDQ, cdq),
    TRANS_FUNC_GEN(CDQE, cdqe),
    TRANS_FUNC_GEN(CLC, clc),
    TRANS_FUNC_GEN(CLD, cld),
    TRANS_FUNC_GEN(CMC, cmc),
    TRANS_FUNC_GEN(CMOVA, cmovcc),
    TRANS_FUNC_GEN(CMOVAE, cmovcc),
    TRANS_FUNC_GEN(CMOVB, cmovcc),
    TRANS_FUNC_GEN(CMOVBE, cmovcc),
    TRANS_FUNC_GEN(CMOVE, cmovcc),
    TRANS_FUNC_GEN(CMOVG, cmovcc),
    TRANS_FUNC_GEN(CMOVGE, cmovcc),
    TRANS_FUNC_GEN(CMOVL, cmovcc),
    TRANS_FUNC_GEN(CMOVLE, cmovcc),
    TRANS_FUNC_GEN(CMOVNE, cmovcc),
    TRANS_FUNC_GEN(CMOVNO, cmovcc),
    TRANS_FUNC_GEN(CMOVNP, cmovcc),
    TRANS_FUNC_GEN(CMOVNS, cmovcc),
    TRANS_FUNC_GEN(CMOVO, cmovcc),
    TRANS_FUNC_GEN(CMOVP, cmovcc),
    TRANS_FUNC_GEN(CMOVS, cmovcc),
    TRANS_FUNC_GEN(CMP, cmp),
    TRANS_FUNC_GEN(CMPSB, cmps),
    TRANS_FUNC_GEN(CMPSW, cmps),
    TRANS_FUNC_GEN(CMPSQ, cmps),
    TRANS_FUNC_GEN(CMPXCHG, cmpxchg),
    TRANS_FUNC_GEN(CMPXCHG8B, cmpxchg8b),
    TRANS_FUNC_GEN(CMPXCHG16B, cmpxchg16b),
#ifdef CONFIG_LATX_XCOMISX_OPT
    TRANS_FUNC_GEN(COMISD, xcomisx),
    TRANS_FUNC_GEN(COMISS, xcomisx),
#else
    TRANS_FUNC_GEN(COMISD, comisd),
    TRANS_FUNC_GEN(COMISS, comiss),
#endif
    TRANS_FUNC_GEN(CPUID, cpuid),
    TRANS_FUNC_GEN(CQO, cqo),
    TRANS_FUNC_GEN(CVTDQ2PD, cvtdq2pd),
    TRANS_FUNC_GEN(CVTDQ2PS, cvtdq2ps),
    TRANS_FUNC_GEN(CVTPD2DQ, cvtpd2dq),
    TRANS_FUNC_GEN(CVTPD2PS, cvtpd2ps),
    TRANS_FUNC_GEN(CVTPS2DQ, cvtps2dq),
    TRANS_FUNC_GEN(CVTPS2PD, cvtps2pd),
    TRANS_FUNC_GEN(CVTSD2SI, cvtsx2si),
    TRANS_FUNC_GEN(CVTSD2SS, cvtsd2ss),
    TRANS_FUNC_GEN(CVTSI2SD, cvtsi2sd),
    TRANS_FUNC_GEN(CVTSI2SS, cvtsi2ss),
    TRANS_FUNC_GEN(CVTSS2SD, cvtss2sd),
    TRANS_FUNC_GEN(CVTSS2SI, cvtsx2si),
    TRANS_FUNC_GEN(CVTTPD2DQ, cvttpx2dq),
    TRANS_FUNC_GEN(CVTTPS2DQ, cvttpx2dq),
    TRANS_FUNC_GEN(CVTTSD2SI, cvttsx2si),
    TRANS_FUNC_GEN(CVTTSS2SI, cvttsx2si),
    TRANS_FUNC_GEN(CVTTPD2PI, cvttpd2pi),
    TRANS_FUNC_GEN(CVTTPS2PI, cvttps2pi),
    TRANS_FUNC_GEN(CWD, cwd),
    TRANS_FUNC_GEN(CWDE, cwde),
    TRANS_FUNC_GEN(DAA, daa),
    TRANS_FUNC_GEN(DAS, das),
    TRANS_FUNC_GEN(DEC, dec),
    TRANS_FUNC_GEN(DIV, div),
    TRANS_FUNC_GEN(DIVPD, divpd),
    TRANS_FUNC_GEN(DIVPS, divps),
    TRANS_FUNC_GEN(DIVSD, divsd),
    TRANS_FUNC_GEN(DIVSS, divss),
    TRANS_FUNC_GEN(RET, ret),
    TRANS_FUNC_GEN(MOVAPD, movapd),
    TRANS_FUNC_GEN(MOVAPS, movaps),
    TRANS_FUNC_GEN(ORPD, orpd),
    TRANS_FUNC_GEN(ORPS, orps),
    TRANS_FUNC_GEN(XORPD, xorpd),
    TRANS_FUNC_GEN(XORPS, xorps),
    TRANS_FUNC_GEN(HLT, hlt),
    TRANS_FUNC_GEN(IDIV, idiv),
    TRANS_FUNC_GEN(IMUL, imul),
    TRANS_FUNC_GEN(IN, in),
    TRANS_FUNC_GEN(INC, inc),
    TRANS_FUNC_GEN(INSD, ins),
    TRANS_FUNC_GEN(INT, int),
    TRANS_FUNC_GEN(INT3, int_3),
    TRANS_FUNC_GEN(IRET, iret),
    TRANS_FUNC_GEN(IRETD, iret),
    TRANS_FUNC_GEN(IRETQ, iretq),
#ifdef CONFIG_LATX_XCOMISX_OPT
    TRANS_FUNC_GEN(UCOMISD, xcomisx),
    TRANS_FUNC_GEN(UCOMISS, xcomisx),
#else
    TRANS_FUNC_GEN(UCOMISD, ucomisd),
    TRANS_FUNC_GEN(UCOMISS, ucomiss),
#endif
    TRANS_FUNC_GEN(JCXZ, jcxz),
    TRANS_FUNC_GEN(JECXZ, jecxz),
    TRANS_FUNC_GEN(JRCXZ, jrcxz),
    TRANS_FUNC_GEN(JMP, jmp),
    TRANS_FUNC_GEN(JAE, jcc),
    TRANS_FUNC_GEN(JA, jcc),
    TRANS_FUNC_GEN(JBE, jcc),
    TRANS_FUNC_GEN(JB, jcc),
    TRANS_FUNC_GEN(JE, jcc),
    TRANS_FUNC_GEN(JGE, jcc),
    TRANS_FUNC_GEN(JG, jcc),
    TRANS_FUNC_GEN(JLE, jcc),
    TRANS_FUNC_GEN(JL, jcc),
    TRANS_FUNC_GEN(JNE, jcc),
    TRANS_FUNC_GEN(JNO, jcc),
    TRANS_FUNC_GEN(JNP, jcc),
    TRANS_FUNC_GEN(JNS, jcc),
    TRANS_FUNC_GEN(JO, jcc),
    TRANS_FUNC_GEN(JP, jcc),
    TRANS_FUNC_GEN(JS, jcc),
    TRANS_FUNC_GEN(LAHF, lahf),
    TRANS_FUNC_GEN(LDDQU, lddqu),
    TRANS_FUNC_GEN(LDMXCSR, ldmxcsr),
#ifdef CONFIG_LATX_AVX_OPT
    TRANS_FUNC_GEN(VLDMXCSR, ldmxcsr),
#endif
    TRANS_FUNC_GEN(LEA, lea),
    TRANS_FUNC_GEN(LEAVE, leave),
    TRANS_FUNC_GEN(LFENCE, lfence),
    TRANS_FUNC_GEN(OR, or),
    TRANS_FUNC_GEN(SUB, sub),
    TRANS_FUNC_GEN(XOR, xor),
    TRANS_FUNC_GEN(LODSB, lods),
    TRANS_FUNC_GEN(LODSD, lods),
    TRANS_FUNC_GEN(LODSW, lods),
    TRANS_FUNC_GEN(LODSQ, lods),
    TRANS_FUNC_GEN(LOOP, loop),
    TRANS_FUNC_GEN(LOOPE, loopz),
    TRANS_FUNC_GEN(LOOPNE, loopnz),
    TRANS_FUNC_GEN(XADD, xadd),
    TRANS_FUNC_GEN(MASKMOVDQU, maskmovdqu),
    TRANS_FUNC_GEN(MAXPD, maxpd),
    TRANS_FUNC_GEN(MAXPS, maxps),
    TRANS_FUNC_GEN(MAXSD, maxsd),
    TRANS_FUNC_GEN(MAXSS, maxss),
    TRANS_FUNC_GEN(MFENCE, mfence),
    TRANS_FUNC_GEN(MINPD, minpd),
    TRANS_FUNC_GEN(MINPS, minps),
    TRANS_FUNC_GEN(MINSD, minsd),
    TRANS_FUNC_GEN(MINSS, minss),
    TRANS_FUNC_GEN(CVTPD2PI, cvtpd2pi),
    TRANS_FUNC_GEN(CVTPI2PD, cvtpi2pd),
    TRANS_FUNC_GEN(CVTPI2PS, cvtpi2ps),
    TRANS_FUNC_GEN(CVTPS2PI, cvtps2pi),
    TRANS_FUNC_GEN(EMMS, emms),
    TRANS_FUNC_GEN(MASKMOVQ, maskmovq),
    TRANS_FUNC_GEN(MOVD, movd),
    TRANS_FUNC_GEN(MOVDQ2Q, movdq2q),
    TRANS_FUNC_GEN(MOVNTQ, movntq),
    TRANS_FUNC_GEN(MOVQ2DQ, movq2dq),
    TRANS_FUNC_GEN(MOVQ, movq),
    TRANS_FUNC_GEN(PACKSSDW, packssdw),
    TRANS_FUNC_GEN(PACKSSWB, packsswb),
    TRANS_FUNC_GEN(PACKUSWB, packuswb),
    TRANS_FUNC_GEN(PADDB, paddb),
    TRANS_FUNC_GEN(PADDD, paddd),
    TRANS_FUNC_GEN(PADDQ, paddq),
    TRANS_FUNC_GEN(PADDSB, paddsb),
    TRANS_FUNC_GEN(PADDSW, paddsw),
    TRANS_FUNC_GEN(PADDUSB, paddusb),
    TRANS_FUNC_GEN(PADDUSW, paddusw),
    TRANS_FUNC_GEN(PADDW, paddw),
    TRANS_FUNC_GEN(PANDN, pandn),
    TRANS_FUNC_GEN(PAND, pand),
    TRANS_FUNC_GEN(PAVGB, pavgb),
    TRANS_FUNC_GEN(PAVGW, pavgw),
    TRANS_FUNC_GEN(PCMPEQB, pcmpeqb),
    TRANS_FUNC_GEN(PCMPEQD, pcmpeqd),
    TRANS_FUNC_GEN(PCMPEQW, pcmpeqw),
    TRANS_FUNC_GEN(PCMPGTB, pcmpgtb),
    TRANS_FUNC_GEN(PCMPGTW, pcmpgtw),
    TRANS_FUNC_GEN(PCMPGTD, pcmpgtd),
    TRANS_FUNC_GEN(PCMPGTQ, pcmpgtq),
    TRANS_FUNC_GEN(PEXTRW, pextrw),
    TRANS_FUNC_GEN(PINSRW, pinsrw),
    TRANS_FUNC_GEN(PMADDWD, pmaddwd),
    TRANS_FUNC_GEN(PMAXSW, pmaxsw),
    TRANS_FUNC_GEN(PMAXUB, pmaxub),
    TRANS_FUNC_GEN(PMINSW, pminsw),
    TRANS_FUNC_GEN(PMINUB, pminub),
    TRANS_FUNC_GEN(PMOVMSKB, pmovmskb),
    TRANS_FUNC_GEN(PMULHUW, pmulhuw),
    TRANS_FUNC_GEN(PMULHW, pmulhw),
    TRANS_FUNC_GEN(PMULLW, pmullw),
    TRANS_FUNC_GEN(PMULUDQ, pmuludq),
    TRANS_FUNC_GEN(POR, por),
    TRANS_FUNC_GEN(PSADBW, psadbw),
    TRANS_FUNC_GEN(PSHUFW, pshufw),
    TRANS_FUNC_GEN(PSLLD, pslld),
    TRANS_FUNC_GEN(PSLLQ, psllq),
    TRANS_FUNC_GEN(PSLLW, psllw),
    TRANS_FUNC_GEN(PSRAD, psrad),
    TRANS_FUNC_GEN(PSRAW, psraw),
    TRANS_FUNC_GEN(PSRLD, psrld),
    TRANS_FUNC_GEN(PSRLQ, psrlq),
    TRANS_FUNC_GEN(PSRLW, psrlw),
    TRANS_FUNC_GEN(PSUBB, psubb),
    TRANS_FUNC_GEN(PSUBD, psubd),
    TRANS_FUNC_GEN(PSUBQ, psubq),
    TRANS_FUNC_GEN(PSUBSB, psubsb),
    TRANS_FUNC_GEN(PSUBSW, psubsw),
    TRANS_FUNC_GEN(PSUBUSB, psubusb),
    TRANS_FUNC_GEN(PSUBUSW, psubusw),
    TRANS_FUNC_GEN(PSUBW, psubw),
    TRANS_FUNC_GEN(PUNPCKHBW, punpckhbw),
    TRANS_FUNC_GEN(PUNPCKHDQ, punpckhdq),
    TRANS_FUNC_GEN(PUNPCKHWD, punpckhwd),
    TRANS_FUNC_GEN(PUNPCKLBW, punpcklbw),
    TRANS_FUNC_GEN(PUNPCKLDQ, punpckldq),
    TRANS_FUNC_GEN(PUNPCKLWD, punpcklwd),
    TRANS_FUNC_GEN(PXOR, pxor),
    TRANS_FUNC_GEN(HADDPD, haddpd),
    TRANS_FUNC_GEN(HADDPS, haddps),
    TRANS_FUNC_GEN(HSUBPD, hsubpd),
    TRANS_FUNC_GEN(HSUBPS, hsubps),
    TRANS_FUNC_GEN(MOV, mov),
    TRANS_FUNC_GEN(MOVABS, mov),
    TRANS_FUNC_GEN(MOVDDUP, movddup),
    TRANS_FUNC_GEN(MOVDQA, movdqa),
    TRANS_FUNC_GEN(MOVDQU, movdqu),
    TRANS_FUNC_GEN(MOVHLPS, movhlps),
    TRANS_FUNC_GEN(MOVHPD, movhpd),
    TRANS_FUNC_GEN(MOVHPS, movhps),
    TRANS_FUNC_GEN(MOVLHPS, movlhps),
    TRANS_FUNC_GEN(MOVLPD, movlpd),
    TRANS_FUNC_GEN(MOVLPS, movlps),
    TRANS_FUNC_GEN(MOVMSKPD, movmskpd),
    TRANS_FUNC_GEN(MOVMSKPS, movmskps),
    TRANS_FUNC_GEN(MOVNTDQ, movntdq),
    TRANS_FUNC_GEN(MOVNTI, movnti),
    TRANS_FUNC_GEN(MOVNTPD, movntpd),
    TRANS_FUNC_GEN(MOVNTPS, movntps),
    TRANS_FUNC_GEN(MOVSB, movs),
    TRANS_FUNC_GEN(MOVSHDUP, movshdup),
    TRANS_FUNC_GEN(MOVSLDUP, movsldup),
    TRANS_FUNC_GEN(MOVSQ, movs),
    TRANS_FUNC_GEN(MOVSS, movss),
    TRANS_FUNC_GEN(MOVSW, movs),
    TRANS_FUNC_GEN(MOVSX, movsx),
    TRANS_FUNC_GEN(MOVSXD, movsxd),
    TRANS_FUNC_GEN(MOVUPD, movupd),
    TRANS_FUNC_GEN(MOVUPS, movups),
    TRANS_FUNC_GEN(MOVZX, movzx),
    TRANS_FUNC_GEN(MUL, mul),
    TRANS_FUNC_GEN(MULX, mulx),
    TRANS_FUNC_GEN(MULPD, mulpd),
    TRANS_FUNC_GEN(MULPS, mulps),
    TRANS_FUNC_GEN(MULSD, mulsd),
    TRANS_FUNC_GEN(MULSS, mulss),
    TRANS_FUNC_GEN(NEG, neg),
    TRANS_FUNC_GEN(NOP, nop),
    TRANS_FUNC_GEN(NOT, not),
    TRANS_FUNC_GEN(OUT, out),
    TRANS_FUNC_GEN(PAUSE, pause),
    TRANS_FUNC_GEN(POPCNT, popcnt),
    TRANS_FUNC_GEN(POP, pop),
    TRANS_FUNC_GEN(POPAW, popaw),
    TRANS_FUNC_GEN(POPAL, popal),
    TRANS_FUNC_GEN(POPF, popf),
    TRANS_FUNC_GEN(POPFD, popf),
    TRANS_FUNC_GEN(POPFQ, popf),
    TRANS_FUNC_GEN(PREFETCH, prefetch),
    TRANS_FUNC_GEN(PREFETCHNTA, prefetchnta),
    TRANS_FUNC_GEN(PREFETCHT0, prefetcht0),
    TRANS_FUNC_GEN(PREFETCHT1, prefetcht1),
    TRANS_FUNC_GEN(PREFETCHT2, prefetcht2),
    TRANS_FUNC_GEN(PREFETCHW, prefetchw),
    TRANS_FUNC_GEN(PSHUFD, pshufd),
    TRANS_FUNC_GEN(PSHUFHW, pshufhw),
    TRANS_FUNC_GEN(PSHUFLW, pshuflw),
    TRANS_FUNC_GEN(PSLLDQ, pslldq),
    TRANS_FUNC_GEN(PSRLDQ, psrldq),
    TRANS_FUNC_GEN(PUNPCKHQDQ, punpckhqdq),
    TRANS_FUNC_GEN(PUNPCKLQDQ, punpcklqdq),
    TRANS_FUNC_GEN(PUSH, push),
    TRANS_FUNC_GEN(PUSHAL, pushal),
    TRANS_FUNC_GEN(PUSHAW, pushaw),
    TRANS_FUNC_GEN(PUSHF, pushf),
    TRANS_FUNC_GEN(PUSHFD, pushf),
    TRANS_FUNC_GEN(PUSHFQ, pushf),
    TRANS_FUNC_GEN(RCL, rcl),
    TRANS_FUNC_GEN(RCPPS, rcpps),
    TRANS_FUNC_GEN(RCPSS, rcpss),
    TRANS_FUNC_GEN(RCR, rcr),
    TRANS_FUNC_GEN(RDTSC, rdtsc),
    TRANS_FUNC_GEN(RDTSCP, rdtscp),
    TRANS_FUNC_GEN(ROL, rol),
    TRANS_FUNC_GEN(ROR, ror),
    TRANS_FUNC_GEN(RSQRTPS, rsqrtps),
    TRANS_FUNC_GEN(RSQRTSS, rsqrtss),
    TRANS_FUNC_GEN(SAHF, sahf),
    TRANS_FUNC_GEN(SAL, sal),
    TRANS_FUNC_GEN(SAR, sar),
    TRANS_FUNC_GEN(SBB, sbb),
    TRANS_FUNC_GEN(SCASB, scas),
    TRANS_FUNC_GEN(SCASD, scas),
    TRANS_FUNC_GEN(SCASQ, scas),
    TRANS_FUNC_GEN(SCASW, scas),
    TRANS_FUNC_GEN(SETAE, setcc),
    TRANS_FUNC_GEN(SETA, setcc),
    TRANS_FUNC_GEN(SETBE, setcc),
    TRANS_FUNC_GEN(SETB, setcc),
    TRANS_FUNC_GEN(SETE, setcc),
    TRANS_FUNC_GEN(SETGE, setcc),
    TRANS_FUNC_GEN(SETG, setcc),
    TRANS_FUNC_GEN(SETLE, setcc),
    TRANS_FUNC_GEN(SETL, setcc),
    TRANS_FUNC_GEN(SETNE, setcc),
    TRANS_FUNC_GEN(SETNO, setcc),
    TRANS_FUNC_GEN(SETNP, setcc),
    TRANS_FUNC_GEN(SETNS, setcc),
    TRANS_FUNC_GEN(SETO, setcc),
    TRANS_FUNC_GEN(SETP, setcc),
    TRANS_FUNC_GEN(SETS, setcc),
    TRANS_FUNC_GEN(SFENCE, sfence),
    TRANS_FUNC_GEN(CLFLUSH, clflush),
    TRANS_FUNC_GEN(CLFLUSHOPT, clflushopt),
    TRANS_FUNC_GEN(SHL, shl),
    TRANS_FUNC_GEN(SHLD, shld),
    TRANS_FUNC_GEN(SHR, shr),
    TRANS_FUNC_GEN(SHRD, shrd),
    TRANS_FUNC_GEN(SARX, sarx),
    TRANS_FUNC_GEN(SHLX, shlx),
    TRANS_FUNC_GEN(SHRX, shrx),
    TRANS_FUNC_GEN(SHUFPD, shufpd),
    TRANS_FUNC_GEN(SHUFPS, shufps),
    TRANS_FUNC_GEN(SQRTPD, sqrtpd),
    TRANS_FUNC_GEN(SQRTPS, sqrtps),
    TRANS_FUNC_GEN(SQRTSD, sqrtsd),
    TRANS_FUNC_GEN(SQRTSS, sqrtss),
    TRANS_FUNC_GEN(STC, stc),
    TRANS_FUNC_GEN(STD, std),
    TRANS_FUNC_GEN(STMXCSR, stmxcsr),
#ifdef CONFIG_LATX_AVX_OPT
    TRANS_FUNC_GEN(VSTMXCSR, stmxcsr),
#endif
    TRANS_FUNC_GEN(STOSB, stos),
    TRANS_FUNC_GEN(STOSD, stos),
    TRANS_FUNC_GEN(STOSQ, stos),
    TRANS_FUNC_GEN(STOSW, stos),
    TRANS_FUNC_GEN(SUBPD, subpd),
    TRANS_FUNC_GEN(SUBPS, subps),
    TRANS_FUNC_GEN(SUBSD, subsd),
    TRANS_FUNC_GEN(SUBSS, subss),
    TRANS_FUNC_GEN(SYSCALL, syscall),
    TRANS_FUNC_GEN(TEST, test),
    TRANS_FUNC_GEN(UD2, ud2),
    TRANS_FUNC_GEN(UD1, ud2),
    TRANS_FUNC_GEN(UD0, ud2),
    TRANS_FUNC_GEN(TZCNT, tzcnt),
    TRANS_FUNC_GEN(UNPCKHPD, unpckhpd),
    TRANS_FUNC_GEN(UNPCKHPS, unpckhps),
    TRANS_FUNC_GEN(UNPCKLPD, unpcklpd),
    TRANS_FUNC_GEN(UNPCKLPS, unpcklps),
    TRANS_FUNC_GEN(WAIT, wait_wrap),
    TRANS_FUNC_GEN(XCHG, xchg),
    TRANS_FUNC_GEN(XLATB, xlat),
    TRANS_FUNC_GEN(CMPPD, cmppd),
    TRANS_FUNC_GEN(CMPPS, cmpps),
    TRANS_FUNC_GEN(CMPSS, cmpss),
    TRANS_FUNC_GEN(CMPEQSS, cmpeqss),
    TRANS_FUNC_GEN(CMPLTSS, cmpltss),
    TRANS_FUNC_GEN(CMPLESS, cmpless),
    TRANS_FUNC_GEN(CMPUNORDSS, cmpunordss),
    TRANS_FUNC_GEN(CMPNEQSS, cmpneqss),
    TRANS_FUNC_GEN(CMPNLTSS, cmpnltss),
    TRANS_FUNC_GEN(CMPNLESS, cmpnless),
    TRANS_FUNC_GEN(CMPORDSS, cmpordss),
    TRANS_FUNC_GEN(CMPEQSD, cmpeqsd),
    TRANS_FUNC_GEN(CMPLTSD, cmpltsd),
    TRANS_FUNC_GEN(CMPLESD, cmplesd),
    TRANS_FUNC_GEN(CMPUNORDSD, cmpunordsd),
    TRANS_FUNC_GEN(CMPNEQSD, cmpneqsd),
    TRANS_FUNC_GEN(CMPNLTSD, cmpnltsd),
    TRANS_FUNC_GEN(CMPNLESD, cmpnlesd),
    TRANS_FUNC_GEN(CMPORDSD, cmpordsd),
    TRANS_FUNC_GEN(CMPEQPS, cmpeqps),
    TRANS_FUNC_GEN(CMPLTPS, cmpltps),
    TRANS_FUNC_GEN(CMPLEPS, cmpleps),
    TRANS_FUNC_GEN(CMPUNORDPS, cmpunordps),
    TRANS_FUNC_GEN(CMPNEQPS, cmpneqps),
    TRANS_FUNC_GEN(CMPNLTPS, cmpnltps),
    TRANS_FUNC_GEN(CMPNLEPS, cmpnleps),
    TRANS_FUNC_GEN(CMPORDPS, cmpordps),
    TRANS_FUNC_GEN(CMPEQPD, cmpeqpd),
    TRANS_FUNC_GEN(CMPLTPD, cmpltpd),
    TRANS_FUNC_GEN(CMPLEPD, cmplepd),
    TRANS_FUNC_GEN(CMPUNORDPD, cmpunordpd),
    TRANS_FUNC_GEN(CMPNEQPD, cmpneqpd),
    TRANS_FUNC_GEN(CMPNLTPD, cmpnltpd),
    TRANS_FUNC_GEN(CMPNLEPD, cmpnlepd),
    TRANS_FUNC_GEN(CMPORDPD, cmpordpd),
    TRANS_FUNC_GEN(ENDBR32, endbr32),
    TRANS_FUNC_GEN(ENDBR64, endbr64),
    TRANS_FUNC_GEN(LJMP, ljmp),
    TRANS_FUNC_GEN(LCALL, nop),
    TRANS_FUNC_GEN(LDS, nop),
    TRANS_FUNC_GEN(ENTER, enter),
    TRANS_FUNC_GEN(LES, nop),
    TRANS_FUNC_GEN(OUTSB, nop),
    TRANS_FUNC_GEN(CLI, nop),
    TRANS_FUNC_GEN(STI, nop),
    TRANS_FUNC_GEN(SALC, salc),
    TRANS_FUNC_GEN(BOUND, nop),
    TRANS_FUNC_GEN(INTO, nop),
    TRANS_FUNC_GEN(INSB, nop),
    TRANS_FUNC_GEN(RETF, retf),
    TRANS_FUNC_GEN(INT1, nop),
    TRANS_FUNC_GEN(OUTSD, nop),
    TRANS_FUNC_GEN(SLDT, nop),
    TRANS_FUNC_GEN(ARPL, nop),
    TRANS_FUNC_GEN(SIDT, nop),

    /* ssse3 */
    TRANS_FUNC_GEN(PSIGNB, psignb),
    TRANS_FUNC_GEN(PSIGNW, psignw),
    TRANS_FUNC_GEN(PSIGND, psignd),
    TRANS_FUNC_GEN(PABSB, pabsb),
    TRANS_FUNC_GEN(PABSW, pabsw),
    TRANS_FUNC_GEN(PABSD, pabsd),
    TRANS_FUNC_GEN(PALIGNR, palignr),
    TRANS_FUNC_GEN(PSHUFB, pshufb),
    TRANS_FUNC_GEN(PMULHRSW, pmulhrsw),
    TRANS_FUNC_GEN(PMADDUBSW, pmaddubsw),
    TRANS_FUNC_GEN(PHSUBW, phsubw),
    TRANS_FUNC_GEN(PHSUBD, phsubd),
    TRANS_FUNC_GEN(PHSUBSW, phsubsw),
    TRANS_FUNC_GEN(PHADDW, phaddw),
    TRANS_FUNC_GEN(PHADDD, phaddd),
    TRANS_FUNC_GEN(PHADDSW, phaddsw),

    /* sse 4.1 fp */
    TRANS_FUNC_GEN(DPPS, dpps),
    TRANS_FUNC_GEN(DPPD, dppd),
    TRANS_FUNC_GEN(BLENDPS, blendps),
    TRANS_FUNC_GEN(BLENDPD, blendpd),
    TRANS_FUNC_GEN(BLENDVPS, blendvps),
    TRANS_FUNC_GEN(BLENDVPD, blendvpd),
    TRANS_FUNC_GEN(ROUNDPS, roundps),
    TRANS_FUNC_GEN(ROUNDSS, roundss),
    TRANS_FUNC_GEN(ROUNDPD, roundpd),
    TRANS_FUNC_GEN(ROUNDSD, roundsd),
    TRANS_FUNC_GEN(INSERTPS, insertps),
    TRANS_FUNC_GEN(EXTRACTPS, extractps),

    /* sse 4.1 int */
    TRANS_FUNC_GEN(MPSADBW, mpsadbw),
    TRANS_FUNC_GEN(PHMINPOSUW, phminposuw),
    TRANS_FUNC_GEN(PMULLD, pmulld),
    TRANS_FUNC_GEN(PMULDQ, pmuldq),
    TRANS_FUNC_GEN(PBLENDVB, pblendvb),
    TRANS_FUNC_GEN(PBLENDW, pblendw),
    TRANS_FUNC_GEN(PMINSB, pminsb),
    TRANS_FUNC_GEN(PMINUW, pminuw),
    TRANS_FUNC_GEN(PMINSD, pminsd),
    TRANS_FUNC_GEN(PMINUD, pminud),
    TRANS_FUNC_GEN(PMAXSB, pmaxsb),
    TRANS_FUNC_GEN(PMAXUW, pmaxuw),
    TRANS_FUNC_GEN(PMAXSD, pmaxsd),
    TRANS_FUNC_GEN(PMAXUD, pmaxud),
    TRANS_FUNC_GEN(PINSRB, pinsrb),
    TRANS_FUNC_GEN(PINSRD, pinsrd),
    TRANS_FUNC_GEN(PINSRQ, pinsrq),
    TRANS_FUNC_GEN(PEXTRB, pextrb),
    TRANS_FUNC_GEN(PEXTRD, pextrd),
    TRANS_FUNC_GEN(PEXTRQ, pextrq),
    TRANS_FUNC_GEN(PMOVSXBW, pmovsxbw),
    TRANS_FUNC_GEN(PMOVZXBW, pmovzxbw),
    TRANS_FUNC_GEN(PMOVSXBD, pmovsxbd),
    TRANS_FUNC_GEN(PMOVZXBD, pmovzxbd),
    TRANS_FUNC_GEN(PMOVSXBQ, pmovsxbq),
    TRANS_FUNC_GEN(PMOVZXBQ, pmovzxbq),
    TRANS_FUNC_GEN(PMOVSXWD, pmovsxwd),
    TRANS_FUNC_GEN(PMOVZXWD, pmovzxwd),
    TRANS_FUNC_GEN(PMOVSXWQ, pmovsxwq),
    TRANS_FUNC_GEN(PMOVZXWQ, pmovzxwq),
    TRANS_FUNC_GEN(PMOVSXDQ, pmovsxdq),
    TRANS_FUNC_GEN(PMOVZXDQ, pmovzxdq),
    TRANS_FUNC_GEN(PTEST, ptest),
    TRANS_FUNC_GEN(PCMPEQQ, pcmpeqq),
    TRANS_FUNC_GEN(PACKUSDW, packusdw),
    TRANS_FUNC_GEN(MOVNTDQA, movntdqa),


    /* fpu */
    TRANS_FUNC_GEN(F2XM1, f2xm1_wrap),
    TRANS_FUNC_GEN(FABS, fabs_wrap),
    TRANS_FUNC_GEN(FADD, fadd_wrap),
    TRANS_FUNC_GEN(FADDP, faddp_wrap),
    TRANS_FUNC_GEN(FBLD, fbld_wrap),
    TRANS_FUNC_GEN(FBSTP, fbstp_wrap),
    TRANS_FUNC_GEN(FCHS, fchs_wrap),
    TRANS_FUNC_GEN(FCMOVB, fcmovcc_wrap),
    TRANS_FUNC_GEN(FCMOVBE, fcmovcc_wrap),
    TRANS_FUNC_GEN(FCMOVE, fcmovcc_wrap),
    TRANS_FUNC_GEN(FCMOVNB, fcmovcc_wrap),
    TRANS_FUNC_GEN(FCMOVNBE, fcmovcc_wrap),
    TRANS_FUNC_GEN(FCMOVNE, fcmovcc_wrap),
    TRANS_FUNC_GEN(FCMOVNU, fcmovcc_wrap),
    TRANS_FUNC_GEN(FCMOVU, fcmovcc_wrap),
    TRANS_FUNC_GEN(FCOM, fcom_wrap),
    TRANS_FUNC_GEN(FCOMI, fcomi_wrap),
    TRANS_FUNC_GEN(FCOMIP, fcomip_wrap),
    TRANS_FUNC_GEN(FCOMP, fcomp_wrap),
    TRANS_FUNC_GEN(FCOMPP, fcompp_wrap),
    TRANS_FUNC_GEN(FCOS, fcos_wrap),
    TRANS_FUNC_GEN(FDECSTP, fdecstp_wrap),
    TRANS_FUNC_GEN(FDIV, fdiv_wrap),
    TRANS_FUNC_GEN(FDIVP, fdivp_wrap),
    TRANS_FUNC_GEN(FDIVR, fdivr_wrap),
    TRANS_FUNC_GEN(FDIVRP, fdivrp_wrap),
    TRANS_FUNC_GEN(FFREE, ffree_wrap),
    TRANS_FUNC_GEN(FFREEP, ffreep_wrap),
    TRANS_FUNC_GEN(FIADD, fiadd_wrap),
    TRANS_FUNC_GEN(FICOM, ficom_wrap),
    TRANS_FUNC_GEN(FICOMP, ficomp_wrap),
    TRANS_FUNC_GEN(FIDIV, fidiv_wrap),
    TRANS_FUNC_GEN(FIDIVR, fidivr_wrap),
    TRANS_FUNC_GEN(FILD, fild_wrap),
    TRANS_FUNC_GEN(FIMUL, fimul_wrap),
    TRANS_FUNC_GEN(FINCSTP, fincstp_wrap),
    TRANS_FUNC_GEN(FIST, fist_wrap),
    TRANS_FUNC_GEN(FISTP, fistp_wrap),
    TRANS_FUNC_GEN(FISTTP, fisttp_wrap),
    TRANS_FUNC_GEN(FISUB, fisub_wrap),
    TRANS_FUNC_GEN(FISUBR, fisubr_wrap),
    TRANS_FUNC_GEN(FLD1, fld1_wrap),
    TRANS_FUNC_GEN(FLD, fld_wrap),
    TRANS_FUNC_GEN(FLDCW, fldcw_wrap),
    TRANS_FUNC_GEN(FLDENV, fldenv_wrap),
    TRANS_FUNC_GEN(FLDL2E, fldl2e_wrap),
    TRANS_FUNC_GEN(FLDL2T, fldl2t_wrap),
    TRANS_FUNC_GEN(FLDLG2, fldlg2_wrap),
    TRANS_FUNC_GEN(FLDLN2, fldln2_wrap),
    TRANS_FUNC_GEN(FLDPI, fldpi_wrap),
    TRANS_FUNC_GEN(FLDZ, fldz_wrap),
    TRANS_FUNC_GEN(FMUL, fmul_wrap),
    TRANS_FUNC_GEN(FMULP, fmulp_wrap),
    TRANS_FUNC_GEN(FNCLEX, fnclex_wrap),
    TRANS_FUNC_GEN(FNINIT, fninit_wrap),
    TRANS_FUNC_GEN(FNOP, fnop_wrap),
    TRANS_FUNC_GEN(FNSAVE, fnsave_wrap),
    TRANS_FUNC_GEN(FNSTCW, fnstcw_wrap),
    TRANS_FUNC_GEN(FNSTENV, fnstenv_wrap),
    TRANS_FUNC_GEN(FNSTSW, fnstsw_wrap),
    TRANS_FUNC_GEN(FPATAN, fpatan_wrap),
    TRANS_FUNC_GEN(FPREM1, fprem1_wrap),
    TRANS_FUNC_GEN(FPREM, fprem_wrap),
    TRANS_FUNC_GEN(FPTAN, fptan_wrap),
    TRANS_FUNC_GEN(FRNDINT, frndint_wrap),
    TRANS_FUNC_GEN(FRSTOR, frstor_wrap),
    TRANS_FUNC_GEN(FSCALE, fscale_wrap),
    TRANS_FUNC_GEN(FSETPM, fsetpm_wrap),
    TRANS_FUNC_GEN(FSIN, fsin_wrap),
    TRANS_FUNC_GEN(FSINCOS, fsincos_wrap),
    TRANS_FUNC_GEN(FSQRT, fsqrt_wrap),
    TRANS_FUNC_GEN(FST, fst_wrap),
    TRANS_FUNC_GEN(FSTP, fstp_wrap),
    TRANS_FUNC_GEN(FSUB, fsub_wrap),
    TRANS_FUNC_GEN(FSUBP, fsubp_wrap),
    TRANS_FUNC_GEN(FSUBR, fsubr_wrap),
    TRANS_FUNC_GEN(FSUBRP, fsubrp_wrap),
    TRANS_FUNC_GEN(FTST, ftst_wrap),
    TRANS_FUNC_GEN(FUCOM, fucom_wrap),
    TRANS_FUNC_GEN(FUCOMI, fucomi_wrap),
    TRANS_FUNC_GEN(FUCOMIP, fucomip_wrap),
    TRANS_FUNC_GEN(FUCOMP, fucomp_wrap),
    TRANS_FUNC_GEN(FUCOMPP, fucompp_wrap),
    TRANS_FUNC_GEN(FXAM, fxam_wrap),
    TRANS_FUNC_GEN(FXCH, fxch_wrap),
    TRANS_FUNC_GEN(FXRSTOR, fxrstor_wrap),
    TRANS_FUNC_GEN(FXRSTOR64, fxrstor_wrap),
    TRANS_FUNC_GEN(FXSAVE, fxsave_wrap),
    TRANS_FUNC_GEN(FXSAVE64, fxsave_wrap),
    TRANS_FUNC_GEN(FXTRACT, fxtract_wrap),
    TRANS_FUNC_GEN(FYL2X, fyl2x_wrap),
    TRANS_FUNC_GEN(FYL2XP1, fyl2xp1_wrap),

    /* sha */
    TRANS_FUNC_GEN(SHA1MSG1, sha1msg1),
    TRANS_FUNC_GEN(SHA1MSG2, sha1msg2),
    TRANS_FUNC_GEN(SHA1NEXTE, sha1nexte),
    TRANS_FUNC_GEN(SHA1RNDS4, sha1rnds4),
    TRANS_FUNC_GEN(SHA256MSG1, sha256msg1),
    TRANS_FUNC_GEN(SHA256MSG2, sha256msg2),
    TRANS_FUNC_GEN(SHA256RNDS2, sha256rnds2),

#ifdef CONFIG_LATX_AVX_OPT
    /* f16c */
    TRANS_FUNC_GEN(VCVTPH2PS, vcvtph2ps),
    TRANS_FUNC_GEN(VCVTPS2PH, vcvtps2ph),

    /* avx */
    TRANS_FUNC_GEN(VPCLMULQDQ, vpclmulqdq),
    TRANS_FUNC_GEN(VTESTPD, vtestpd),
    TRANS_FUNC_GEN(VTESTPS, vtestps),
    TRANS_FUNC_GEN(VZEROALL, vzeroall),
    TRANS_FUNC_GEN(VPMULHRSW, vpmulhrsw),
    TRANS_FUNC_GEN(VPHMINPOSUW, vphminposuw),
    TRANS_FUNC_GEN(VPHSUBSW, vphsubsw),
    TRANS_FUNC_GEN(VPHSUBD, vphsubd),
    TRANS_FUNC_GEN(VPHSUBW, vphsubw),
    TRANS_FUNC_GEN(VPMADDUBSW, vpmaddubsw),
    TRANS_FUNC_GEN(VPMADDWD, vpmaddwd),
    TRANS_FUNC_GEN(VPMULHUW, vpmulhuw),
    TRANS_FUNC_GEN(VPMULHW, vpmulhw),
    TRANS_FUNC_GEN(VPSIGNW, vpsignw),
    TRANS_FUNC_GEN(VPSIGND, vpsignd),
    TRANS_FUNC_GEN(VPSIGNB, vpsignb),
    TRANS_FUNC_GEN(VRCPSS, vrcpss),
    TRANS_FUNC_GEN(VRCPPS, vrcpps),
    TRANS_FUNC_GEN(VRSQRTSS, vrsqrtss),
    TRANS_FUNC_GEN(VRSQRTPS, vrsqrtps),
    TRANS_FUNC_GEN(VPERMILPD, vpermilpd),
    TRANS_FUNC_GEN(VPERMILPS, vpermilps),
    TRANS_FUNC_GEN(VPERMPS, vpermpx),
    TRANS_FUNC_GEN(VPERMPD, vpermpx),
    TRANS_FUNC_GEN(VPSRAVD, vpsravd),
    TRANS_FUNC_GEN(VPSLLVQ, vpsllvq),
    TRANS_FUNC_GEN(VPSLLVD, vpsllvd),
    TRANS_FUNC_GEN(VPMASKMOVQ, vpmaskmovx),
    TRANS_FUNC_GEN(VPMASKMOVD, vpmaskmovx),
    TRANS_FUNC_GEN(VPSHUFD, vpshufd),
    TRANS_FUNC_GEN(VPMULDQ, vpmuldq),
	TRANS_FUNC_GEN(VPALIGNR, vpalignr),
    TRANS_FUNC_GEN(VPINSRB, vpinsrx),
    TRANS_FUNC_GEN(VPINSRW, vpinsrx),
    TRANS_FUNC_GEN(VPINSRD, vpinsrx),
    TRANS_FUNC_GEN(VPINSRQ, vpinsrx),
    TRANS_FUNC_GEN(VPMINSD, vpminxx),
    TRANS_FUNC_GEN(VPMINSW, vpminxx),
    TRANS_FUNC_GEN(VPMINSB, vpminxx),
    TRANS_FUNC_GEN(VPMAXUB, vpmaxxx),
    TRANS_FUNC_GEN(VPMAXUW, vpmaxxx),
    TRANS_FUNC_GEN(VPMAXUD, vpmaxxx),
    TRANS_FUNC_GEN(VPMAXSW, vpmaxxx),
    TRANS_FUNC_GEN(VPMAXSD, vpmaxxx),
    TRANS_FUNC_GEN(VPMAXSB, vpmaxxx),
    TRANS_FUNC_GEN(VMOVSD, vmovsd),
	TRANS_FUNC_GEN(VMOVNTDQA, vmovntdqa),
	TRANS_FUNC_GEN(VMOVLHPS, vmovlhps),
	TRANS_FUNC_GEN(VMOVHPD, vmovhpd),
	TRANS_FUNC_GEN(VMOVHPS, vmovhps),
	TRANS_FUNC_GEN(VMOVHLPS, vmovhlps),
	TRANS_FUNC_GEN(VMOVLPD, vmovlpd),
	TRANS_FUNC_GEN(VMOVLPS, vmovlps),
	TRANS_FUNC_GEN(VMOVQ, vmovq),
    TRANS_FUNC_GEN(VMOVAPD, vmovapd),
    TRANS_FUNC_GEN(VMOVAPS, vmovaps),
    TRANS_FUNC_GEN(VMOVDDUP, vmovddup),
    TRANS_FUNC_GEN(VMOVDQA, vmovdqa),
    TRANS_FUNC_GEN(VMOVDQU, vmovdqu),
    TRANS_FUNC_GEN(VMOVMSKPD, vmovmskpd),
    TRANS_FUNC_GEN(VMOVMSKPS, vmovmskps),
    TRANS_FUNC_GEN(VMOVNTDQ, vmovntdq),
    TRANS_FUNC_GEN(VMOVNTPD, vmovntpd),
    TRANS_FUNC_GEN(VMOVNTPS, vmovntps),
    TRANS_FUNC_GEN(VMOVSHDUP, vmovshdup),
    TRANS_FUNC_GEN(VMOVSLDUP, vmovsldup),
    //TRANS_FUNC_GEN(VMOVSD, vmovsd),
    TRANS_FUNC_GEN(VMOVSS, vmovss),
    TRANS_FUNC_GEN(VMOVD, vmovd),
    //TRANS_FUNC_GEN(VMOVQ, vmovq),
    TRANS_FUNC_GEN(VLDDQU, vlddqu),
    TRANS_FUNC_GEN(VPMOVMSKB, vpmovmskb),
    TRANS_FUNC_GEN(VMOVUPD, vmovupd),
    TRANS_FUNC_GEN(VMOVUPS, vmovups),
    TRANS_FUNC_GEN(VMASKMOVPD, vmaskmovpx),
    TRANS_FUNC_GEN(VMASKMOVPS, vmaskmovpx),
    TRANS_FUNC_GEN(VADDPD, vaddpd),
    TRANS_FUNC_GEN(VADDPS, vaddps),
    TRANS_FUNC_GEN(VADDSD, vaddsd),
    TRANS_FUNC_GEN(VADDSS, vaddss),
    TRANS_FUNC_GEN(VSUBPD, vsubpd),
    TRANS_FUNC_GEN(VSUBPS, vsubps),
    TRANS_FUNC_GEN(VSUBSD, vsubsd),
    TRANS_FUNC_GEN(VSUBSS, vsubss),
    TRANS_FUNC_GEN(VMULPD, vmulpd),
    TRANS_FUNC_GEN(VMULPS, vmulps),
    TRANS_FUNC_GEN(VMULSD, vmulsd),
    TRANS_FUNC_GEN(VMULSS, vmulss),
    TRANS_FUNC_GEN(VDIVPD, vdivpd),
    TRANS_FUNC_GEN(VDIVPS, vdivps),
    TRANS_FUNC_GEN(VDIVSD ,vdivsd),
    TRANS_FUNC_GEN(VDIVSS, vdivss),
    TRANS_FUNC_GEN(VSQRTPD, vsqrtpd),
    TRANS_FUNC_GEN(VSQRTPS, vsqrtps),
    TRANS_FUNC_GEN(VSQRTSD, vsqrtsd),
    TRANS_FUNC_GEN(VSQRTSS, vsqrtss),
    TRANS_FUNC_GEN(VADDSUBPD, vaddsubpd),
    TRANS_FUNC_GEN(VADDSUBPS, vaddsubps),
    TRANS_FUNC_GEN(VHADDPD, vhaddpd),
    TRANS_FUNC_GEN(VHADDPS, vhaddps),
    TRANS_FUNC_GEN(VHSUBPD, vhsubpd),
    TRANS_FUNC_GEN(VHSUBPS, vhsubps),
    TRANS_FUNC_GEN(VANDNPD, vandnpd),
    TRANS_FUNC_GEN(VANDNPS, vandnps),
    TRANS_FUNC_GEN(VANDPD, vandpd),
    TRANS_FUNC_GEN(VANDPS, vandps),
    TRANS_FUNC_GEN(VORPD, vorpd),
    TRANS_FUNC_GEN(VORPS, vorps),
    TRANS_FUNC_GEN(VXORPD, vxorpd),
    TRANS_FUNC_GEN(VXORPS, vxorps),
    TRANS_FUNC_GEN(VMAXPD, vmaxpx),
    TRANS_FUNC_GEN(VMAXPS, vmaxpx),
    TRANS_FUNC_GEN(VMAXSD, vmaxsx),
    TRANS_FUNC_GEN(VMAXSS, vmaxsx),
    TRANS_FUNC_GEN(VMINPD, vminpx),
    TRANS_FUNC_GEN(VMINPS, vminpx),
    TRANS_FUNC_GEN(VMINSD, vminsx),
    TRANS_FUNC_GEN(VMINSS, vminsx),
    TRANS_FUNC_GEN(VBLENDPD, vblendpd),
    TRANS_FUNC_GEN(VBLENDPS, vblendps),
    TRANS_FUNC_GEN(VBLENDVPD, vblendvpd),
    TRANS_FUNC_GEN(VBLENDVPS, vblendvps),
    TRANS_FUNC_GEN(VBROADCASTF128, vbroadcastf128),
    TRANS_FUNC_GEN(VBROADCASTI128, vbroadcasti128),
    TRANS_FUNC_GEN(VBROADCASTSD, vbroadcastsd),
    TRANS_FUNC_GEN(VBROADCASTSS, vbroadcastss),
    TRANS_FUNC_GEN(VEXTRACTF128, vextractf128),
    TRANS_FUNC_GEN(VEXTRACTI128, vextracti128),
    TRANS_FUNC_GEN(VEXTRACTPS, vextractps),
    TRANS_FUNC_GEN(VINSERTF128, vinsertf128),
    TRANS_FUNC_GEN(VINSERTI128, vinserti128),
    TRANS_FUNC_GEN(VINSERTPS, vinsertps),
    TRANS_FUNC_GEN(VSHUFPD, vshufpd),
    TRANS_FUNC_GEN(VSHUFPS, vshufps),
    TRANS_FUNC_GEN(VUNPCKHPD, vunpckhpd),
    TRANS_FUNC_GEN(VUNPCKHPS, vunpckhps),
    TRANS_FUNC_GEN(VUNPCKLPD, vunpcklpd),
    TRANS_FUNC_GEN(VUNPCKLPS, vunpcklps),
    TRANS_FUNC_GEN(VPSLLDQ, vpslldq),
    TRANS_FUNC_GEN(VPSLLD, vpsllx),
    TRANS_FUNC_GEN(VPSLLQ, vpsllx),
    TRANS_FUNC_GEN(VPSLLW, vpsllx),
    TRANS_FUNC_GEN(VPSRAD, vpsrax),
    TRANS_FUNC_GEN(VPSRAW, vpsrax),
    TRANS_FUNC_GEN(VPSRLDQ, vpsrldq),
    TRANS_FUNC_GEN(VPSRLD, vpsrlx),
    TRANS_FUNC_GEN(VPSRLQ, vpsrlx),
    TRANS_FUNC_GEN(VPSRLW, vpsrlx),
    TRANS_FUNC_GEN(VPCMPEQB, vpcmpeqx),
    TRANS_FUNC_GEN(VPCMPEQD, vpcmpeqx),
    TRANS_FUNC_GEN(VPCMPEQQ, vpcmpeqx),
    TRANS_FUNC_GEN(VPCMPEQW, vpcmpeqx),
    TRANS_FUNC_GEN(VPCMPGTB, vpcmpgtx),
    TRANS_FUNC_GEN(VPCMPGTD, vpcmpgtx),
    TRANS_FUNC_GEN(VPCMPGTQ, vpcmpgtx),
    TRANS_FUNC_GEN(VPCMPGTW, vpcmpgtx),
    TRANS_FUNC_GEN(VUCOMISD, vucomisd),
    TRANS_FUNC_GEN(VUCOMISS, vucomiss),
    TRANS_FUNC_GEN(VCOMISD, vcomisd),
    TRANS_FUNC_GEN(VCOMISS, vcomiss),
    TRANS_FUNC_GEN(VPABSB, vpabsx),
    TRANS_FUNC_GEN(VPABSD, vpabsx),
    TRANS_FUNC_GEN(VPABSW, vpabsx),
    TRANS_FUNC_GEN(VPACKUSDW, vpackusxx),
    TRANS_FUNC_GEN(VPACKUSWB, vpackusxx),
    TRANS_FUNC_GEN(VPADDB, vpaddx),
    TRANS_FUNC_GEN(VPADDD, vpaddx),
    TRANS_FUNC_GEN(VPADDQ, vpaddx),
    TRANS_FUNC_GEN(VPADDW, vpaddx),
    TRANS_FUNC_GEN(VPANDN, vpandn),
    TRANS_FUNC_GEN(VPAND, vpand),
    TRANS_FUNC_GEN(VPBLENDD, vpblendd),
    TRANS_FUNC_GEN(VPBLENDVB, vpblendvb),
    TRANS_FUNC_GEN(VPBLENDW, vpblendw),
    TRANS_FUNC_GEN(VPERM2F128, vperm2f128),
    TRANS_FUNC_GEN(VPERM2I128, vperm2i128),
    TRANS_FUNC_GEN(VPERMD, vpermd),
    TRANS_FUNC_GEN(VPERMQ, vpermq),
    TRANS_FUNC_GEN(VPEXTRB, vpextrx),
    TRANS_FUNC_GEN(VPEXTRD, vpextrx),
    TRANS_FUNC_GEN(VPEXTRQ, vpextrx),
    TRANS_FUNC_GEN(VPEXTRW, vpextrx),
    TRANS_FUNC_GEN(VPMINUB, vpminux),
    TRANS_FUNC_GEN(VPMINUD, vpminux),
    TRANS_FUNC_GEN(VPMINUW, vpminux),
    TRANS_FUNC_GEN(VPMOVSXBD, vpmovsxxx),
    TRANS_FUNC_GEN(VPMOVSXBQ, vpmovsxxx),
    TRANS_FUNC_GEN(VPMOVSXBW, vpmovsxxx),
    TRANS_FUNC_GEN(VPMOVSXDQ, vpmovsxxx),
    TRANS_FUNC_GEN(VPMOVSXWD, vpmovsxxx),
    TRANS_FUNC_GEN(VPMOVSXWQ, vpmovsxxx),
    TRANS_FUNC_GEN(VPMOVZXBD, vpmovzxxx),
    TRANS_FUNC_GEN(VPMOVZXBQ, vpmovzxxx),
    TRANS_FUNC_GEN(VPMOVZXBW, vpmovzxxx),
    TRANS_FUNC_GEN(VPMOVZXDQ, vpmovzxxx),
    TRANS_FUNC_GEN(VPMOVZXWD, vpmovzxxx),
    TRANS_FUNC_GEN(VPMOVZXWQ, vpmovzxxx),
    TRANS_FUNC_GEN(VPMULLD, vpmullx),
    TRANS_FUNC_GEN(VPMULLW, vpmullx),
    TRANS_FUNC_GEN(VPMULUDQ, vpmullx),
    TRANS_FUNC_GEN(VPOR, vpor),
    TRANS_FUNC_GEN(VPSHUFB, vpshufb),
    TRANS_FUNC_GEN(VPSUBB, vpsubx),
    TRANS_FUNC_GEN(VPSUBD, vpsubx),
    TRANS_FUNC_GEN(VPSUBQ, vpsubx),
    TRANS_FUNC_GEN(VPSUBSB, vpsubx),
    TRANS_FUNC_GEN(VPSUBSW, vpsubx),
    TRANS_FUNC_GEN(VPSUBUSB, vpsubx),
    TRANS_FUNC_GEN(VPSUBUSW, vpsubx),
    TRANS_FUNC_GEN(VPSUBW, vpsubx),
    TRANS_FUNC_GEN(VPTEST, vptest),
    TRANS_FUNC_GEN(VPUNPCKHBW, vpunpckhxx),
    TRANS_FUNC_GEN(VPUNPCKHDQ, vpunpckhxx),
    TRANS_FUNC_GEN(VPUNPCKHQDQ, vpunpckhxx),
    TRANS_FUNC_GEN(VPUNPCKHWD, vpunpckhxx),
    TRANS_FUNC_GEN(VPUNPCKLBW, vpunpcklxx),
    TRANS_FUNC_GEN(VPUNPCKLDQ, vpunpcklxx),
    TRANS_FUNC_GEN(VPUNPCKLQDQ, vpunpcklxx),
    TRANS_FUNC_GEN(VPUNPCKLWD, vpunpcklxx),
    TRANS_FUNC_GEN(VPXOR, vpxor),
    TRANS_FUNC_GEN(VCVTDQ2PD, vcvtdq2pd),
    TRANS_FUNC_GEN(VCVTDQ2PS, vcvtdq2ps),
    TRANS_FUNC_GEN(VCVTPD2DQ, vcvtpd2dq),
    TRANS_FUNC_GEN(VCVTPD2DQX, vcvtpd2dq),
    TRANS_FUNC_GEN(VCVTPD2PS, vcvtpd2ps),
    TRANS_FUNC_GEN(VCVTPD2PSX, vcvtpd2ps),
    TRANS_FUNC_GEN(VCVTPS2DQ, vcvtps2dq),
    TRANS_FUNC_GEN(VCVTPS2PD, vcvtps2pd),
    TRANS_FUNC_GEN(VCVTSD2SS, vcvtsd2ss),
    TRANS_FUNC_GEN(VCVTSI2SD, vcvtsi2sd),
    TRANS_FUNC_GEN(VCVTSI2SS, vcvtsi2ss),
    TRANS_FUNC_GEN(VCVTSS2SD, vcvtss2sd),
    TRANS_FUNC_GEN(VCVTTPD2DQ, vcvttpd2dq),
    TRANS_FUNC_GEN(VCVTTPD2DQX, vcvttpd2dq),
    TRANS_FUNC_GEN(VCVTTPS2DQ, vcvttps2dq),
    TRANS_FUNC_GEN(VCVTTSD2SI, cvttsx2si),
    TRANS_FUNC_GEN(VCVTTSS2SI, cvttsx2si),
    TRANS_FUNC_GEN(VFMADD132PD, vfmaddxxxpd),
    TRANS_FUNC_GEN(VFMADD132PS, vfmaddxxxps),
    TRANS_FUNC_GEN(VFMADD132SD, vfmaddxxxsd),
    TRANS_FUNC_GEN(VFMADD132SS, vfmaddxxxss),
    TRANS_FUNC_GEN(VFMADD213PD, vfmaddxxxpd),
    TRANS_FUNC_GEN(VFMADD213PS, vfmaddxxxps),
    TRANS_FUNC_GEN(VFMADD213SD, vfmaddxxxsd),
    TRANS_FUNC_GEN(VFMADD213SS, vfmaddxxxss),
    TRANS_FUNC_GEN(VFMADD231PD, vfmaddxxxpd),
    TRANS_FUNC_GEN(VFMADD231PS, vfmaddxxxps),
    TRANS_FUNC_GEN(VFMADD231SD, vfmaddxxxsd),
    TRANS_FUNC_GEN(VFMADD231SS, vfmaddxxxss),
    TRANS_FUNC_GEN(VFMADDSUB132PD, vfmaddsubxxxpd),
    TRANS_FUNC_GEN(VFMADDSUB132PS, vfmaddsubxxxps),
    TRANS_FUNC_GEN(VFMADDSUB213PD, vfmaddsubxxxpd),
    TRANS_FUNC_GEN(VFMADDSUB213PS, vfmaddsubxxxps),
    TRANS_FUNC_GEN(VFMADDSUB231PD, vfmaddsubxxxpd),
    TRANS_FUNC_GEN(VFMADDSUB231PS, vfmaddsubxxxps),
    TRANS_FUNC_GEN(VFMSUB132PD, vfmsubxxxpd),
    TRANS_FUNC_GEN(VFMSUB132PS, vfmsubxxxps),
    TRANS_FUNC_GEN(VFMSUB132SD, vfmsubxxxsd),
    TRANS_FUNC_GEN(VFMSUB132SS, vfmsubxxxss),
    TRANS_FUNC_GEN(VFMSUB213PD, vfmsubxxxpd),
    TRANS_FUNC_GEN(VFMSUB213PS, vfmsubxxxps),
    TRANS_FUNC_GEN(VFMSUB213SD, vfmsubxxxsd),
    TRANS_FUNC_GEN(VFMSUB213SS, vfmsubxxxss),
    TRANS_FUNC_GEN(VFMSUB231PD, vfmsubxxxpd),
    TRANS_FUNC_GEN(VFMSUB231PS, vfmsubxxxps),
    TRANS_FUNC_GEN(VFMSUB231SD, vfmsubxxxsd),
    TRANS_FUNC_GEN(VFMSUB231SS, vfmsubxxxss),
    TRANS_FUNC_GEN(VFMSUBADD132PD, vfmsubaddxxxpd),
    TRANS_FUNC_GEN(VFMSUBADD132PS, vfmsubaddxxxps),
    TRANS_FUNC_GEN(VFMSUBADD213PD, vfmsubaddxxxpd),
    TRANS_FUNC_GEN(VFMSUBADD213PS, vfmsubaddxxxps),
    TRANS_FUNC_GEN(VFMSUBADD231PD, vfmsubaddxxxpd),
    TRANS_FUNC_GEN(VFMSUBADD231PS, vfmsubaddxxxps),
    TRANS_FUNC_GEN(VFNMADD132PD, vfnmaddxxxpd),
    TRANS_FUNC_GEN(VFNMADD132PS, vfnmaddxxxps),
    TRANS_FUNC_GEN(VFNMADD132SD, vfnmaddxxxsd),
    TRANS_FUNC_GEN(VFNMADD132SS, vfnmaddxxxss),
    TRANS_FUNC_GEN(VFNMADD213PD, vfnmaddxxxpd),
    TRANS_FUNC_GEN(VFNMADD213PS, vfnmaddxxxps),
    TRANS_FUNC_GEN(VFNMADD213SD, vfnmaddxxxsd),
    TRANS_FUNC_GEN(VFNMADD213SS, vfnmaddxxxss),
    TRANS_FUNC_GEN(VFNMADD231PD, vfnmaddxxxpd),
    TRANS_FUNC_GEN(VFNMADD231PS, vfnmaddxxxps),
    TRANS_FUNC_GEN(VFNMADD231SD, vfnmaddxxxsd),
    TRANS_FUNC_GEN(VFNMADD231SS, vfnmaddxxxss),
    TRANS_FUNC_GEN(VFNMSUB132PD, vfnmsubxxxpd),
    TRANS_FUNC_GEN(VFNMSUB132PS, vfnmsubxxxps),
    TRANS_FUNC_GEN(VFNMSUB132SD, vfnmsubxxxsd),
    TRANS_FUNC_GEN(VFNMSUB132SS, vfnmsubxxxss),
    TRANS_FUNC_GEN(VFNMSUB213PD, vfnmsubxxxpd),
    TRANS_FUNC_GEN(VFNMSUB213PS, vfnmsubxxxps),
    TRANS_FUNC_GEN(VFNMSUB213SD, vfnmsubxxxsd),
    TRANS_FUNC_GEN(VFNMSUB213SS, vfnmsubxxxss),
    TRANS_FUNC_GEN(VFNMSUB231PD, vfnmsubxxxpd),
    TRANS_FUNC_GEN(VFNMSUB231PS, vfnmsubxxxps),
    TRANS_FUNC_GEN(VFNMSUB231SD, vfnmsubxxxsd),
    TRANS_FUNC_GEN(VFNMSUB231SS, vfnmsubxxxss),
    TRANS_FUNC_GEN(VCMPPD, vcmppd),
    TRANS_FUNC_GEN(VCMPEQPD, vcmpeqpd),
    TRANS_FUNC_GEN(VCMPLTPD, vcmpltpd),
    TRANS_FUNC_GEN(VCMPLEPD, vcmplepd),
    TRANS_FUNC_GEN(VCMPUNORDPD, vcmpunordpd),
    TRANS_FUNC_GEN(VCMPNEQPD, vcmpneqpd),
    TRANS_FUNC_GEN(VCMPNLTPD, vcmpnltpd),
    TRANS_FUNC_GEN(VCMPNLEPD, vcmpnlepd),
    TRANS_FUNC_GEN(VCMPORDPD, vcmpordpd),
    TRANS_FUNC_GEN(VCMPEQ_UQPD, vcmpeq_uqpd),
    TRANS_FUNC_GEN(VCMPNGEPD, vcmpngepd),
    TRANS_FUNC_GEN(VCMPNGTPD, vcmpngtpd),
    TRANS_FUNC_GEN(VCMPFALSEPD, vcmpfalsepd),
    TRANS_FUNC_GEN(VCMPNEQ_OQPD, vcmpneq_oqpd),
    TRANS_FUNC_GEN(VCMPGEPD, vcmpgepd),
    TRANS_FUNC_GEN(VCMPGTPD, vcmpgtpd),
    TRANS_FUNC_GEN(VCMPTRUEPD, vcmptruepd),
    TRANS_FUNC_GEN(VCMPEQ_OSPD, vcmpeq_ospd),
    TRANS_FUNC_GEN(VCMPLT_OQPD, vcmplt_oqpd),
    TRANS_FUNC_GEN(VCMPLE_OQPD, vcmple_oqpd),
    TRANS_FUNC_GEN(VCMPUNORD_SPD, vcmpunord_spd),
    TRANS_FUNC_GEN(VCMPNEQ_USPD, vcmpneq_uspd),
    TRANS_FUNC_GEN(VCMPNLT_UQPD, vcmpnlt_uqpd),
    TRANS_FUNC_GEN(VCMPNLE_UQPD, vcmpnle_uqpd),
    TRANS_FUNC_GEN(VCMPORD_SPD, vcmpord_spd),
    TRANS_FUNC_GEN(VCMPEQ_USPD, vcmpeq_uspd),
    TRANS_FUNC_GEN(VCMPNGE_UQPD, vcmpnge_uqpd),
    TRANS_FUNC_GEN(VCMPNGT_UQPD, vcmpngt_uqpd),
    TRANS_FUNC_GEN(VCMPFALSE_OSPD, vcmpfalse_ospd),
    TRANS_FUNC_GEN(VCMPNEQ_OSPD, vcmpneq_ospd),
    TRANS_FUNC_GEN(VCMPGE_OQPD, vcmpge_oqpd),
    TRANS_FUNC_GEN(VCMPGT_OQPD, vcmpgt_oqpd),
    TRANS_FUNC_GEN(VCMPTRUE_USPD, vcmptrue_uspd),
    TRANS_FUNC_GEN(VCMPPS, vcmpps),
    TRANS_FUNC_GEN(VCMPEQPS, vcmpeqps),
    TRANS_FUNC_GEN(VCMPLTPS, vcmpltps),
    TRANS_FUNC_GEN(VCMPLEPS, vcmpleps),
    TRANS_FUNC_GEN(VCMPUNORDPS, vcmpunordps),
    TRANS_FUNC_GEN(VCMPNEQPS, vcmpneqps),
    TRANS_FUNC_GEN(VCMPNLTPS, vcmpnltps),
    TRANS_FUNC_GEN(VCMPNLEPS, vcmpnleps),
    TRANS_FUNC_GEN(VCMPORDPS, vcmpordps),
    TRANS_FUNC_GEN(VCMPEQ_UQPS, vcmpeq_uqps),
    TRANS_FUNC_GEN(VCMPNGEPS, vcmpngeps),
    TRANS_FUNC_GEN(VCMPNGTPS, vcmpngtps),
    TRANS_FUNC_GEN(VCMPFALSEPS, vcmpfalseps),
    TRANS_FUNC_GEN(VCMPNEQ_OQPS, vcmpneq_oqps),
    TRANS_FUNC_GEN(VCMPGEPS, vcmpgeps),
    TRANS_FUNC_GEN(VCMPGTPS, vcmpgtps),
    TRANS_FUNC_GEN(VCMPTRUEPS, vcmptrueps),
    TRANS_FUNC_GEN(VCMPEQ_OSPS, vcmpeq_osps),
    TRANS_FUNC_GEN(VCMPLT_OQPS, vcmplt_oqps),
    TRANS_FUNC_GEN(VCMPLE_OQPS, vcmple_oqps),
    TRANS_FUNC_GEN(VCMPUNORD_SPS, vcmpunord_sps),
    TRANS_FUNC_GEN(VCMPNEQ_USPS, vcmpneq_usps),
    TRANS_FUNC_GEN(VCMPNLT_UQPS, vcmpnlt_uqps),
    TRANS_FUNC_GEN(VCMPNLE_UQPS, vcmpnle_uqps),
    TRANS_FUNC_GEN(VCMPORD_SPS, vcmpord_sps),
    TRANS_FUNC_GEN(VCMPEQ_USPS, vcmpeq_usps),
    TRANS_FUNC_GEN(VCMPNGE_UQPS, vcmpnge_uqps),
    TRANS_FUNC_GEN(VCMPNGT_UQPS, vcmpngt_uqps),
    TRANS_FUNC_GEN(VCMPFALSE_OSPS, vcmpfalse_osps),
    TRANS_FUNC_GEN(VCMPNEQ_OSPS, vcmpneq_osps),
    TRANS_FUNC_GEN(VCMPGE_OQPS, vcmpge_oqps),
    TRANS_FUNC_GEN(VCMPGT_OQPS, vcmpgt_oqps),
    TRANS_FUNC_GEN(VCMPTRUE_USPS, vcmptrue_usps),
    TRANS_FUNC_GEN(VCMPSD, vcmpsd),
    TRANS_FUNC_GEN(VCMPEQSD, vcmpeqsd),
    TRANS_FUNC_GEN(VCMPLTSD, vcmpltsd),
    TRANS_FUNC_GEN(VCMPLESD, vcmplesd),
    TRANS_FUNC_GEN(VCMPUNORDSD, vcmpunordsd),
    TRANS_FUNC_GEN(VCMPNEQSD, vcmpneqsd),
    TRANS_FUNC_GEN(VCMPNLTSD, vcmpnltsd),
    TRANS_FUNC_GEN(VCMPNLESD, vcmpnlesd),
    TRANS_FUNC_GEN(VCMPORDSD, vcmpordsd),
    TRANS_FUNC_GEN(VCMPEQ_UQSD, vcmpeq_uqsd),
    TRANS_FUNC_GEN(VCMPNGESD, vcmpngesd),
    TRANS_FUNC_GEN(VCMPNGTSD, vcmpngtsd),
    TRANS_FUNC_GEN(VCMPFALSESD, vcmpfalsesd),
    TRANS_FUNC_GEN(VCMPNEQ_OQSD, vcmpneq_oqsd),
    TRANS_FUNC_GEN(VCMPGESD, vcmpgesd),
    TRANS_FUNC_GEN(VCMPGTSD, vcmpgtsd),
    TRANS_FUNC_GEN(VCMPTRUESD, vcmptruesd),
    TRANS_FUNC_GEN(VCMPEQ_OSSD, vcmpeq_ossd),
    TRANS_FUNC_GEN(VCMPLT_OQSD, vcmplt_oqsd),
    TRANS_FUNC_GEN(VCMPLE_OQSD, vcmple_oqsd),
    TRANS_FUNC_GEN(VCMPUNORD_SSD, vcmpunord_ssd),
    TRANS_FUNC_GEN(VCMPNEQ_USSD, vcmpneq_ussd),
    TRANS_FUNC_GEN(VCMPNLT_UQSD, vcmpnlt_uqsd),
    TRANS_FUNC_GEN(VCMPNLE_UQSD, vcmpnle_uqsd),
    TRANS_FUNC_GEN(VCMPORD_SSD, vcmpord_ssd),
    TRANS_FUNC_GEN(VCMPEQ_USSD, vcmpeq_ussd),
    TRANS_FUNC_GEN(VCMPNGE_UQSD, vcmpnge_uqsd),
    TRANS_FUNC_GEN(VCMPNGT_UQSD, vcmpngt_uqsd),
    TRANS_FUNC_GEN(VCMPFALSE_OSSD, vcmpfalse_ossd),
    TRANS_FUNC_GEN(VCMPNEQ_OSSD, vcmpneq_ossd),
    TRANS_FUNC_GEN(VCMPGE_OQSD, vcmpge_oqsd),
    TRANS_FUNC_GEN(VCMPGT_OQSD, vcmpgt_oqsd),
    TRANS_FUNC_GEN(VCMPTRUE_USSD, vcmptrue_ussd),
    TRANS_FUNC_GEN(VCMPSS, vcmpss),
    TRANS_FUNC_GEN(VCMPEQSS, vcmpeqss),
    TRANS_FUNC_GEN(VCMPLTSS, vcmpltss),
    TRANS_FUNC_GEN(VCMPLESS, vcmpless),
    TRANS_FUNC_GEN(VCMPUNORDSS, vcmpunordss),
    TRANS_FUNC_GEN(VCMPNEQSS, vcmpneqss),
    TRANS_FUNC_GEN(VCMPNLTSS, vcmpnltss),
    TRANS_FUNC_GEN(VCMPNLESS, vcmpnless),
    TRANS_FUNC_GEN(VCMPORDSS, vcmpordss),
    TRANS_FUNC_GEN(VCMPEQ_UQSS, vcmpeq_uqss),
    TRANS_FUNC_GEN(VCMPNGESS, vcmpngess),
    TRANS_FUNC_GEN(VCMPNGTSS, vcmpngtss),
    TRANS_FUNC_GEN(VCMPFALSESS, vcmpfalsess),
    TRANS_FUNC_GEN(VCMPNEQ_OQSS, vcmpneq_oqss),
    TRANS_FUNC_GEN(VCMPGESS, vcmpgess),
    TRANS_FUNC_GEN(VCMPGTSS, vcmpgtss),
    TRANS_FUNC_GEN(VCMPTRUESS, vcmptruess),
    TRANS_FUNC_GEN(VCMPEQ_OSSS, vcmpeq_osss),
    TRANS_FUNC_GEN(VCMPLT_OQSS, vcmplt_oqss),
    TRANS_FUNC_GEN(VCMPLE_OQSS, vcmple_oqss),
    TRANS_FUNC_GEN(VCMPUNORD_SSS, vcmpunord_sss),
    TRANS_FUNC_GEN(VCMPNEQ_USSS, vcmpneq_usss),
    TRANS_FUNC_GEN(VCMPNLT_UQSS, vcmpnlt_uqss),
    TRANS_FUNC_GEN(VCMPNLE_UQSS, vcmpnle_uqss),
    TRANS_FUNC_GEN(VCMPORD_SSS, vcmpord_sss),
    TRANS_FUNC_GEN(VCMPEQ_USSS, vcmpeq_usss),
    TRANS_FUNC_GEN(VCMPNGE_UQSS, vcmpnge_uqss),
    TRANS_FUNC_GEN(VCMPNGT_UQSS, vcmpngt_uqss),
    TRANS_FUNC_GEN(VCMPFALSE_OSSS, vcmpfalse_osss),
    TRANS_FUNC_GEN(VCMPNEQ_OSSS, vcmpneq_osss),
    TRANS_FUNC_GEN(VCMPGE_OQSS, vcmpge_oqss),
    TRANS_FUNC_GEN(VCMPGT_OQSS, vcmpgt_oqss),
    TRANS_FUNC_GEN(VCMPTRUE_USSS, vcmptrue_usss),
    TRANS_FUNC_GEN(VPBROADCASTQ, vpbroadcastq),
    TRANS_FUNC_GEN(VPADDQ, vpaddq),
    TRANS_FUNC_GEN(VZEROUPPER, vzeroupper),
    //TRANS_FUNC_GEN(VPINSRB, vpinsrb),
    //TRANS_FUNC_GEN(VPINSRQ, vpinsrq),
    TRANS_FUNC_GEN(XGETBV, xgetbv_wrap),
    TRANS_FUNC_GEN(XSETBV, xsetbv_wrap),
    TRANS_FUNC_GEN(XSAVE, xsave_wrap),
    TRANS_FUNC_GEN(XSAVEOPT, xsaveopt_wrap),
    TRANS_FUNC_GEN(XRSTOR, xrstor_wrap),
    TRANS_FUNC_GEN(VPBROADCASTD, vpbroadcastd),
    //TRANS_FUNC_GEN(VPMAXSD, vpmaxsd),
    //TRANS_FUNC_GEN(VPMINSD, vpminsd),
#endif
    TRANS_FUNC_GEN(ANDN, andn),
    TRANS_FUNC_GEN(MOVBE, movbe),
    TRANS_FUNC_GEN(RORX, rorx),
    TRANS_FUNC_GEN(BLSI, blsi),
#ifdef CONFIG_LATX_AVX_OPT
    TRANS_FUNC_GEN(VPADDSB, vpaddx),
    TRANS_FUNC_GEN(VPADDSW, vpaddx),
    TRANS_FUNC_GEN(VPADDUSB, vpaddx),
    TRANS_FUNC_GEN(VPADDUSW, vpaddx),
    TRANS_FUNC_GEN(VCVTSS2SI, cvtsx2si),
    TRANS_FUNC_GEN(VCVTSD2SI, cvtsx2si),
    TRANS_FUNC_GEN(VPACKSSDW, vpackssxx),
    TRANS_FUNC_GEN(VPACKSSWB, vpackssxx),
    TRANS_FUNC_GEN(VPSHUFHW, vpshufhw),
    TRANS_FUNC_GEN(VPSHUFLW, vpshuflw),
    TRANS_FUNC_GEN(VPAVGB, vpavgb),
    TRANS_FUNC_GEN(VPAVGW, vpavgw),
    TRANS_FUNC_GEN(VDPPD, vdppd),
    TRANS_FUNC_GEN(VDPPS, vdpps),
    TRANS_FUNC_GEN(VMASKMOVDQU, maskmovdqu),
    TRANS_FUNC_GEN(VMPSADBW, vmpsadbw),
    TRANS_FUNC_GEN(VPHADDW, vphaddw),
    TRANS_FUNC_GEN(VPHADDD, vphaddd),
    TRANS_FUNC_GEN(VPHADDSW, vphaddsw),
    TRANS_FUNC_GEN(VPSADBW, vpsadbw),
    TRANS_FUNC_GEN(VROUNDPS, vroundps),
    TRANS_FUNC_GEN(VROUNDSS, vroundss),
    TRANS_FUNC_GEN(VROUNDPD, vroundpd),
    TRANS_FUNC_GEN(VROUNDSD, vroundsd),
#endif

#ifndef CONFIG_LATX_AVX_OPT
    TRANS_FUNC_GEN(PCMPESTRI, pcmpestri),
#else
    TRANS_FUNC_GEN(PCMPESTRI, vpcmpestri),
#endif
    TRANS_FUNC_GEN(PCMPESTRM, pcmpestrm),
#ifndef CONFIG_LATX_AVX_OPT
    TRANS_FUNC_GEN(PCMPISTRI, pcmpistri),
#else
    TRANS_FUNC_GEN(PCMPISTRI, vpcmpistri),
#endif
    TRANS_FUNC_GEN(PCMPISTRM, pcmpistrm),
#ifdef CONFIG_LATX_AVX_OPT
    TRANS_FUNC_GEN(VPCMPESTRI, vpcmpestri),
    TRANS_FUNC_GEN(VPCMPESTRM, vpcmpestrm),
    TRANS_FUNC_GEN(VPCMPISTRI, vpcmpistri),
    TRANS_FUNC_GEN(VPCMPISTRM, vpcmpistrm),
#endif

    TRANS_FUNC_GEN(AESDEC, aesdec),
    TRANS_FUNC_GEN(AESDECLAST, aesdeclast),
    TRANS_FUNC_GEN(AESENC, aesenc),
    TRANS_FUNC_GEN(AESENCLAST, aesenclast),
    TRANS_FUNC_GEN(AESIMC, aesimc),
    TRANS_FUNC_GEN(AESKEYGENASSIST, aeskeygenassist),

#ifdef CONFIG_LATX_AVX_OPT
    TRANS_FUNC_GEN(VAESDEC, vaesdec),
    TRANS_FUNC_GEN(VAESDECLAST, vaesdeclast),
    TRANS_FUNC_GEN(VAESENC, vaesenc),
    TRANS_FUNC_GEN(VAESENCLAST, vaesenclast),
    TRANS_FUNC_GEN(VAESIMC, vaesimc),
    TRANS_FUNC_GEN(VAESKEYGENASSIST, vaeskeygenassist),
    TRANS_FUNC_GEN(VPSRLVQ, vpsrlvq),
    TRANS_FUNC_GEN(VPSRLVD, vpsrlvd),
    TRANS_FUNC_GEN(VPGATHERDD, vpgatherdd),
    TRANS_FUNC_GEN(VPGATHERQD, vpgatherqd),
    TRANS_FUNC_GEN(VPGATHERDQ, vpgatherdq),
    TRANS_FUNC_GEN(VPGATHERQQ, vpgatherqq),
    TRANS_FUNC_GEN(VGATHERDPD, vpgatherdq),
    TRANS_FUNC_GEN(VGATHERQPD, vpgatherqq),
    TRANS_FUNC_GEN(VGATHERDPS, vpgatherdd),
    TRANS_FUNC_GEN(VGATHERQPS, vpgatherqd),
#endif

    TRANS_FUNC_GEN(PEXT, pext),
    TRANS_FUNC_GEN(PDEP, pdep),
    TRANS_FUNC_GEN(BEXTR, bextr),
    TRANS_FUNC_GEN(BLSMSK, blsmsk),
    TRANS_FUNC_GEN(BZHI, bzhi),
    TRANS_FUNC_GEN(LZCNT, lzcnt),
    TRANS_FUNC_GEN(ADCX, adcx),
    TRANS_FUNC_GEN(ADOX, adox),
#ifdef CONFIG_LATX_AVX_OPT
    TRANS_FUNC_GEN(VPBROADCASTB, vpbroadcastb),
    TRANS_FUNC_GEN(VPBROADCASTW, vpbroadcastw),
#endif
    TRANS_FUNC_GEN(CRC32, crc32),
    TRANS_FUNC_GEN(PCLMULQDQ, pclmulqdq),

    TRANS_FUNC_GEN_REAL(ENDING, NULL),
};

bool ir1_translate(IR1_INST *ir1)
{
#ifdef CONFIG_LATX_INSTS_PATTERN
    if (try_translate_instptn(ir1)) {
        ra_free_all();
        return true;
    }
#endif

    /* 2. call translate_xx function */
    int tr_func_idx = ir1_opcode(ir1) - dt_X86_INS_INVALID;

    bool translation_success = false;

#ifdef CONFIG_LATX_DEBUG
    if (unlikely(latx_trace_mem)) {
        lsenv->current_ir1 = ir1_addr(ir1);
    }
#endif

    if (ir1_opcode(ir1) == dt_X86_INS_CALL) {
        if (!ir1_is_indirect_call(ir1)) {
            if (ir1_addr_next(ir1) == ir1_target_addr(ir1)) {
                return translate_callnext(ir1);
            } else if (ht_pc_thunk_lookup(ir1_target_addr(ir1)) >= 0) {
                return translate_callthunk(ir1);
            }
        }
    }

    if (option_softfpu) {
        int opnd_num = ir1_get_opnd_num(ir1);
        bool mmx_ins = false;
        for (int i = 0; i < opnd_num; i++) {
            if (ir1_opnd_is_mmx(ir1_get_opnd(ir1, i))) {
                mmx_ins = true;
            }
        }
        if (mmx_ins) {
            la_st_w(zero_ir2_opnd, env_ir2_opnd,
                      lsenv_offset_of_top(lsenv));
            la_st_d(zero_ir2_opnd, env_ir2_opnd,
                      lsenv_offset_of_tag_word(lsenv));
        }
    }

    /*sse instructions need to reserve h128 bit of ymm\
        if support avx instructions*/
    IR2_OPND temp = ir2_opnd_new_none();
    if (option_lative && option_enable_lasx && ir1_need_reserve_h128(ir1)) {
        temp = save_h128_of_ymm(ir1);
    }

    // MOVSD means movsd(movs) or movsd(sse2) , diff opcode
    if (ir1_opcode(ir1) == dt_X86_INS_MOVSD) { 
        if (ir1->info->x86.opcode[0] == 0xa5) {
            translation_success = translate_movs(ir1);
        } else if (ir1->info->x86.opcode[0] == 0x0f) {
            translation_success = translate_movsd(ir1);
        } else {
            ir1_opcode_dump(ir1);
        }
    } else if (ir1_opcode(ir1) == dt_X86_INS_CMPSD) {
        if (ir1->info->x86.opcode[0] == 0x0f) {
            translation_success = translate_cmpsd(ir1);
        } else if (ir1->info->x86.opcode[0] == 0xa7) {
            translation_success = translate_cmps(ir1);
        } else {
            ir1_opcode_dump(ir1);
        }
    } else {
        if (translate_functions[tr_func_idx] == NULL) {
            ir1_opcode_dump(ir1);
#ifndef CONFIG_LATX_TU
            lsassertm(0, "%s %s %d error : this ins %d not implemented: %s\n",
                __FILE__, __func__, __LINE__, tr_func_idx, ir1->info->mnemonic);
#elif defined(CONFIG_LATX_DEBUG)
            fprintf(stderr, "\033[31m%s %s %d error : this ins %d not implemented: %s\n\033[m",
                __FILE__, __func__, __LINE__, tr_func_idx, ir1->info->mnemonic);
#endif
#if (defined CONFIG_LATX_TU || defined CONFIG_LATX_TS)
            return 0;
#endif
        } else {
            translation_success = translate_functions[tr_func_idx](ir1); /* TODO */
        }
    }

    /*restore h128 bit of ymm after translating sse instructions*/
    if (option_lative && option_enable_lasx && temp._type != IR2_OPND_NONE) {
        restore_h128_of_ymm(ir1, temp);
    }

#ifdef CONFIG_LATX_DEBUG

    if (unlikely(latx_trace_mem)) {
        if (ir1_opnd_num(ir1) && ir1_opnd_is_mem(ir1_get_opnd(ir1, 0))) {
            IR2_OPND label_ne = ra_alloc_label();
            IR2_OPND mem_opnd = ra_alloc_trace(TRACE);
            IR2_OPND trace_addr = ra_alloc_itemp();
            li_d(trace_addr, latx_trace_mem);
            la_bne(mem_opnd, trace_addr, label_ne);
            li_d(trace_addr, ir1_addr(ir1));
            la_st_d(trace_addr, env_ir2_opnd,
                    lsenv_offset_last_store_insn(lsenv));
            la_label(label_ne);
            ra_free_temp(trace_addr);
        }
    }

    if (unlikely(latx_break_insn) && ir1_addr(ir1) == latx_break_insn) {
        if (option_debug_lative) {
            if (ir1_opcode(ir1) != dt_X86_INS_RET) {
                la_ld_h(zero_ir2_opnd, zero_ir2_opnd, 0xde);
                latx_break_insn = ir1_addr_next(ir1);
            }
        } else {
            la_st_h(zero_ir2_opnd, zero_ir2_opnd, 0x1);
        }
    }
#endif

#ifdef DEBUG_LATX_PER_INSN_DBAR
    la_dbar(0);
#endif
    /* 3. postprocess */
    ra_free_all();
    /* For now the line number of first member of struct translate_functions is 476, remember to
     * change it if the line number is changed.
     */

#ifdef CONFIG_LATX_DEBUG
    if (!translation_success) {
        ir1_opcode_dump(ir1);

#ifdef CONFIG_LATX_TU
        fprintf(stderr, "%s %s %d error : this ins %d not implemented\n",
                    __FILE__, __func__, __LINE__, tr_func_idx);
#else
        lsassertm(0, "%s %s %d error : this ins %d not implemented: %s\n",
                    __FILE__, __func__, __LINE__, tr_func_idx, ir1->info->mnemonic);
#endif

    }
#endif
    return translation_success;
}

static inline void tr_init_for_each_ir1_in_tb(IR1_INST *pir1, int nr, int index)
 {
    lsenv->tr_data->curr_ir1_inst = pir1;
    lsenv->tr_data->curr_ir1_count = index;

    /* TODO: this addr only stored low 32 bits */
    IR2_OPND ir2_opnd_addr;
    if(pir1->decode_engine == OPT_DECODE_BY_CAPSTONE) {
        ir2_opnd_build(&ir2_opnd_addr, IR2_OPND_IMM, ir1_addr(pir1));
    } else {
        ir2_opnd_build(&ir2_opnd_addr, IR2_OPND_IMM, ir1_addr_bd(pir1));
    }
    la_x86_inst(ir2_opnd_addr);
}


#ifdef CONFIG_LATX_MONITOR_SHARED_MEM

static void tr_check_x86ins_change(struct TranslationBlock *tb)
{
    size_t checksum_len;
    IR1_INST *pir1 = tb_ir1_inst_last(tb);
    if(pir1->decode_engine == OPT_DECODE_BY_CAPSTONE) {
        checksum_len = ir1_addr_next(pir1) - tb->pc;
    } else {
        checksum_len = ir1_addr_next_bd(pir1) - tb->pc;
    }
    IR2_OPND checksum_tmp_d = ra_alloc_itemp();
    IR2_OPND checksum_tmp_sum = ra_alloc_itemp();
    IR2_OPND checksum_start = ra_alloc_itemp();
    IR2_OPND tb_opnd = ra_alloc_itemp();
    IR2_OPND checksum_tmp_len = ra_alloc_itemp();
    IR2_OPND sum_loop = ra_alloc_label();

    aot_load_host_addr(tb_opnd, (ADDR)tb, LOAD_TB_ADDR, 0);
    /* tb_checksum*/
    la_ld_d(checksum_start, tb_opnd, offsetof(struct TranslationBlock, pc));
    li_d(checksum_tmp_sum , 0);
    li_d(checksum_tmp_len, checksum_len - 1);
    la_label(sum_loop);
    la_ld_bu(checksum_tmp_d, checksum_start, 0);
    la_add(checksum_tmp_sum, checksum_tmp_sum, checksum_tmp_d);
    la_addi_d(checksum_tmp_len, checksum_tmp_len, -1);
    la_addi_d(checksum_start, checksum_start, 1);
    la_bge(checksum_tmp_len, zero_ir2_opnd,sum_loop);
    ra_free_temp(checksum_tmp_len);

    ra_free_temp(checksum_start);
    ra_free_temp(checksum_tmp_d);
    IR2_OPND checksum = ra_alloc_itemp();
    IR2_OPND check_suc = ra_alloc_label();
    li_d(checksum, tb_checksum((const uint8_t *)(uintptr_t)tb->pc, checksum_len));
    la_beq(checksum ,checksum_tmp_sum, check_suc);
    ra_free_temp(checksum_tmp_sum);
    ra_free_temp(checksum);
    //env->checksum_fail_tb = tb;
    la_st_d(tb_opnd, env_ir2_opnd,
          offsetof(CPUX86State, checksum_fail_tb));
    IR2_OPND base = ra_alloc_data();
    IR2_OPND target = ra_alloc_data();
    IR2_OPND eip_opnd = ra_alloc_dbt_arg2();
    /* set eip = tb->pc*/
    la_ld_d(eip_opnd, tb_opnd, offsetof(struct TranslationBlock, pc));
    IR2_OPND tb_ptr_opnd = a0_ir2_opnd;
    li_d(tb_ptr_opnd , 0);
    /* set base_address data */
    la_data_li(base, (ADDR)tb->tc.ptr);
    la_data_li(target, context_switch_native_to_bt);
    aot_la_append_ir2_jmp_far(target, base, B_EPILOGUE, 0);
    la_label(check_suc);
    ra_free_temp(tb_opnd);
}
#endif

int tr_ir2_generate(struct TranslationBlock *tb)
{
    int i;

    int ir1_nr = tb->icount;

    PER_TB_COUNT((void *)&((tb->profile).exec_times), 1);

#ifdef CONFIG_LATX_DEBUG
    ir2_dump_init();

    if (option_dump) {
        qemu_log("[LATX] translation : generate IR2 from IR1.\n");
        qemu_log("IR1 num = %d\n", ir1_nr);
    }
#endif

    IR1_INST *pir1 = tb_ir1_inst(tb, 0);

    bool reduce_proepo = false;
    int tr_func_idx;

#ifdef CONFIG_LATX_MONITOR_SHARED_MEM
    if (option_monitor_shared_mem && tb->checksum) {
        tr_check_x86ins_change(tb);
    }
#endif
#ifdef CONFIG_LATX_IMM_REG
    /**
     * 1.precache ir1 list before translate ir2
     * 2.temporary adjust imm_cache capacity to ir1_nr
     * 3.reset capacity to default 4 after precache
     */

    IMM_CACHE *imm_cache = lsenv->tr_data->imm_cache;
    if (option_imm_reg) {
        imm_log("======================================\n");
        imm_log("||[imm_cache init] TB:[0x" TARGET_FMT_lx "]||\n", tb->pc);
        imm_log("======================================\n");

        imm_cache->curr_pc = tb->pc;
        if (!option_imm_precache) {
            imm_cache_init(imm_cache, CACHE_DEFAULT_CAPACITY);
        } else {
            imm_cache_init(imm_cache, ir1_nr);
            IR1_INST *t_pir1 = tb_ir1_inst(tb, 0);
            // IR1_OPND opnd[2];
            // int mem_count = imm_cache_extract_ir1_mem_opnd(t_pir1, opnd);
            for (int i = 0; i < ir1_nr; i++) {
                imm_cache->curr_ir1_index = i;
                if(t_pir1->decode_engine == OPT_DECODE_BY_CAPSTONE) {
                    imm_cache_print_ir1(t_pir1);
                    IR1_OPND *opnd = NULL;
                    for (int j = 0; j < t_pir1->info->x86.op_count; j++) {
                        opnd = ir1_get_opnd(t_pir1, j);
                        longx offset;
                        if (ir1_opnd_type(opnd) == dt_X86_OP_MEM) {
                            offset = (longx)(opnd->mem.disp);
                        } else {
                            continue;
                        }
#ifdef TARGET_X86_64
                        if (ir1_opnd_base_reg(opnd) == dt_X86_REG_RIP) {
                            offset += ir1_addr_next(t_pir1);
                            imm_cache_precache_put(imm_cache, -100, -1,
                                            -1, offset);
                            continue;
                        }
#endif
                        /* convert_mem_helper will put si12 offset into host_off */
                        if (!si12_overflow(offset)) {
                            continue;
                        }
                        // record base and index
                        bool has_index = ir1_opnd_has_index(opnd);
                        bool has_base = ir1_opnd_has_base(opnd);
                        int base_op = -1;
                        int index_op = -1;
                        int scale = -1;
                        if (has_base) {
                            base_op = ir1_opnd_base_reg_num(opnd);
                        }
                        if (has_index) {
                            index_op = ir1_opnd_index_reg_num(opnd);
                            scale = ir1_opnd_scale(opnd);
                            if (scale != 1 && scale != 2 && scale != 4 &&
                                scale != 8) {
                                scale = -1;
                            }
                        }

                        if (base_op != -1 || index_op != -1) {
                            // qemu_log_mask(LAT_IMM_REG,"[imm_cache-]")
                            imm_cache_precache_put(imm_cache, base_op,
                                            index_op, scale, offset);
                        }
                    }
                } else {
                    imm_cache_print_ir1_bd(t_pir1);
                    IR1_OPND_BD *opnd = NULL;
                    for (int j = 0; j < ir1_get_opnd_num_bd(t_pir1); j++) {
                        opnd = ir1_get_opnd_bd(t_pir1, j);
                        longx offset;
                        if (ir1_opnd_type_bd(opnd) == ND_OP_MEM) {
                            offset = (longx)(opnd->Info.Memory.Disp);
                        } else {
                            continue;
                        }
#ifdef TARGET_X86_64
                        if (ir1_opnd_is_pc_relative_bd(opnd)) {
                            offset += ir1_addr_next_bd(t_pir1);
                            imm_cache_precache_put(imm_cache, -100, -1,
                                            -1, offset);
                            continue;
                        }
#endif
                        /* convert_mem_helper will put si12 offset into host_off */
                        if (!si12_overflow(offset)) {
                            continue;
                        }
                        // record base and index
                        bool has_index = ir1_opnd_has_index_bd(opnd);
                        bool has_base = ir1_opnd_has_base_bd(opnd);
                        int base_op = -1;
                        int index_op = -1;
                        int scale = -1;
                        if (has_base) {
                            base_op = ir1_opnd_base_reg_num_bd(opnd);
                        }
                        if (has_index) {
                            index_op = ir1_opnd_index_reg_num_bd(opnd);
                            scale = ir1_opnd_scale_bd(opnd);
                            if (scale != 1 && scale != 2 && scale != 4 &&
                                scale != 8) {
                                scale = -1;
                            }
                        }

                        if (base_op != -1 || index_op != -1) {
                            // qemu_log_mask(LAT_IMM_REG,"[imm_cache-]")
                            imm_cache_precache_put(imm_cache, base_op,
                                            index_op, scale, offset);
                        }
                    }
                }
                t_pir1++;
            }
            imm_cache_finish_precache(imm_cache);
        }
    }
#endif
    for (i = 0; i < ir1_nr; ++i) {
        if(pir1->decode_engine == OPT_DECODE_BY_CAPSTONE) {
            /*
            * handle segv scenario, store host pc to gen_insn_data and encode to a BYTE
            * at the end of TB translate cache.
            */
            tcg_ctx->gen_insn_data[i][0] = pir1->info->address;
            tcg_ctx->gen_insn_data[i][1] = 0;

#ifdef CONFIG_LATX_IMM_REG
            imm_cache->curr_ir1_index = i;
            imm_cache->curr_ir2_index = lsenv->tr_data->ir2_inst_num_current;
#endif
            tr_init_for_each_ir1_in_tb(pir1, ir1_nr, i);
#if defined(CONFIG_LATX_DEBUG) && defined(TARGET_X86_64) && \
    defined(CONFIG_LATX_RUNTIME_TRACE_RANGE)
            if (pir1->info->address == option_begin_trace_addr) {
                need_trace = true;
            } else if (pir1->info->address == option_end_trace_addr) {
                need_trace = false;
            }
            if (need_trace) {
                gen_ins_context_in_helper(pir1);
            }
#endif

            if (option_softfpu == 2 && !reduce_proepo) {
                tr_func_idx = ir1_opcode(pir1) - dt_X86_INS_INVALID;
                if (tr_func_idx <= dt_X86_INS_FYL2XP1) {
                    reduce_proepo = true;
                    gen_softfpu_helper_prologue(pir1);
                }
            }

            bool translation_success = ir1_translate(pir1);
            if (!translation_success) {
#ifdef CONFIG_LATX_TU
                tb->s_data->tu_tb_mode = BAD_TB;
#else
                lsassertm(0, "ir1_translate fail");
#endif
            }
        } else {
            /*
            * handle segv scenario, store host pc to gen_insn_data and encode to a BYTE
            * at the end of TB translate cache.
            */
            tcg_ctx->gen_insn_data[i][0] = ir1_addr_bd(pir1);
            tcg_ctx->gen_insn_data[i][1] = 0;

#ifdef CONFIG_LATX_IMM_REG
            imm_cache->curr_ir1_index = i;
            imm_cache->curr_ir2_index = lsenv->tr_data->ir2_inst_num_current;
#endif
            tr_init_for_each_ir1_in_tb(pir1, ir1_nr, i);
#if defined(CONFIG_LATX_DEBUG) && defined(TARGET_X86_64) && \
    defined(CONFIG_LATX_RUNTIME_TRACE_RANGE)
            if (ir1_addr_bd(pir1) == option_begin_trace_addr) {
                need_trace = true;
            } else if (ir1_addr_bd(pir1) == option_end_trace_addr) {
                need_trace = false;
            }
            if (need_trace) {
                gen_ins_context_in_helper(pir1);
            }
#endif

            if (option_softfpu == 2 && !reduce_proepo) {
                // tr_func_idx = ir1_opcode_bd(pir1) - ND_INS_INVALID;
                if (if_reduce_proepo(ir1_opcode_bd(pir1))) {
                    reduce_proepo = true;
                    gen_softfpu_helper_prologue_bd(pir1);
                }
            }

            bool translation_success = ir1_translate_bd(pir1);
            if (!translation_success) {
#ifdef CONFIG_LATX_TU
                tb->s_data->tu_tb_mode = BAD_TB;
#else
                lsassertm(0, "ir1_translate fail");
#endif
            }
        }
        if (option_softfpu == 2 && reduce_proepo) {
            if (i < ir1_nr - 1) {
                IR1_INST *pir1_next = pir1 + 1;
                if(pir1_next->decode_engine == OPT_DECODE_BY_CAPSTONE) {
                    tr_func_idx = ir1_opcode(pir1_next) - dt_X86_INS_INVALID;
                    if (tr_func_idx > dt_X86_INS_FYL2XP1) {
                        reduce_proepo = false;
                        if(pir1->decode_engine == OPT_DECODE_BY_CAPSTONE) {
                            gen_softfpu_helper_epilogue(pir1);
                        } else {
                            gen_softfpu_helper_epilogue_bd(pir1);
                        }
                    }
                } else {
                    // tr_func_idx = ir1_opcode_bd(pir1_next) - ND_INS_INVALID;
                    if (!if_reduce_proepo(ir1_opcode_bd(pir1_next))) {
                        reduce_proepo = false;
                        if(pir1->decode_engine == OPT_DECODE_BY_CAPSTONE) {
                            gen_softfpu_helper_epilogue(pir1);
                        } else {
                            gen_softfpu_helper_epilogue_bd(pir1);
                        }
                    }
                }
            }
        }

#ifdef CONFIG_LATX_IMM_REG
        if (option_imm_reg) {
            if(pir1->decode_engine == OPT_DECODE_BY_CAPSTONE) {
                imm_cache_update_ir1_usage(imm_cache, pir1, i);
            } else {
                imm_cache_update_ir1_usage_bd(imm_cache, pir1, i);
            }
            // log translated ir2 if ir1 is opted by imm_reg
            imm_cache_print_tr_ir2_if_opted();
        }
#endif
        pir1++;
    }
#ifdef CONFIG_LATX_DEBUG
    if (option_dump_ir1) {
        pir1 = tb_ir1_inst(tb, 0);
        for (i = 0; i < ir1_nr; ++i) {
            qemu_log("%llx IR1[%d] ", (unsigned long long)pthread_self(), i);
            if(pir1->decode_engine == OPT_DECODE_BY_CAPSTONE) {
                ir1_dump(pir1);
            } else {
                ir1_dump_bd(pir1);
            }
            pir1++;
        }
    }

    if (option_dump_ir2) {
        TRANSLATION_DATA *t = lsenv->tr_data;
        qemu_log("IR2 num = %d\n", t->ir2_inst_num_current);
        for (i = 0; i < t->ir2_inst_num_current; ++i) {
            IR2_INST *pir2 = &t->ir2_inst_array[i];
            ir2_dump(pir2, true);
        }
    }
#endif
        return 1;
}

static void generate_indirect_goto(void *code_buf, bool parallel)
{
    /*
     * WARNING!!!
     * 如果修改该函数，需要注意 unlink_indirect_jmp 中写 nop 的位置！！！
     */
    IR2_OPND next_x86_addr = ra_alloc_dbt_arg2();
    IR2_OPND base = ra_alloc_data();
    la_data_li(base, (ADDR)code_buf);

    /* indirect jmp */
    IR2_OPND jmp_entry = ra_alloc_itemp();
    IR2_OPND jmp_cache_addr = ra_alloc_static0();

    IR2_OPND next_tb = V0_RENAME_OPND;
    IR2_OPND target = ra_alloc_data();
    IR2_OPND label_miss = ra_alloc_label();
    /*
     * lookup HASH_JMP_CACHE
     * Step 1: calculate HASH = (x86_addr >> 12) ^ (x86_addr & 0xfff)
     * Step 2: load &HASH_JMP_CACHE[0]
     * Step 3: load tb = HASH_JMP_CACHE[HASH]
     * Step 4: if (tb == 0) {goto labal_miss}
     * Step 5: if (tb.pc == x86_addr) {goto fpu_rotate}
     *         else {goto labal_miss}
     */
    la_srli_d(next_tb, next_x86_addr, TB_JMP_CACHE_BITS);
    la_xor(next_tb, next_x86_addr, next_tb);
    la_bstrpick_d(next_tb, next_tb, TB_JMP_CACHE_BITS - 1, 0);

    if (!close_latx_parallel && !parallel) {
        la_alsl_d(next_tb, next_tb, jmp_cache_addr, 3);
        la_ld_d(jmp_entry, next_tb, 0);
        la_bne(jmp_entry, next_x86_addr, label_miss);
        la_ld_d(jmp_entry, next_tb, 8);
    } else {
        la_slli_d(next_tb, next_tb, 3);
        la_ldx_d(next_tb, next_tb, jmp_cache_addr);
        la_beq(next_tb, zero_ir2_opnd, label_miss);

        la_ld_d(jmp_entry, next_tb, offsetof(TranslationBlock, pc));
        la_bne(jmp_entry, next_x86_addr, label_miss);

        /*
         * if jmp_lock set, goto miss, else see if CF_INVALID is set.
         * This method is faster than spin-lock/unlock because the
         * locked case is rare.
         */
        la_ld_d(jmp_entry, next_tb, offsetof(TranslationBlock, cflags));
        /* CF_INVALID */
        la_bstrpick_w(jmp_entry, jmp_entry, 18, 18);
        la_bne(jmp_entry, zero_ir2_opnd, label_miss);
        la_ld_d(jmp_entry, next_tb,
                offsetof(TranslationBlock, tc) +
                offsetof(struct tb_tc, ptr));
    }

/* hit: */
    la_jirl(zero_ir2_opnd, jmp_entry, 0);

    ra_free_temp(jmp_entry);
/* miss: */
    la_label(label_miss);
    /*
     * jump to epilogue with 0 return
     * two args are pass-through:
     * ra_alloc_dbt_arg1: current tb (last tb)
     * ra_alloc_dbt_arg2: next x86 ip
     */

    la_data_li(target, context_switch_native_to_bt_ret_0);
    aot_la_append_ir2_jmp_far(target, base, B_EPILOGUE_RET_0, 0);

    return;
}

#ifdef CONFIG_LATX_XCOMISX_OPT
inline
void tr_generate_exit_tb(IR1_INST *branch, int succ_id)
{
    tr_generate_exit_stub_tb(branch, succ_id, NULL, NULL);
}

void tr_generate_exit_stub_tb(IR1_INST *branch, int succ_id, void *func, IR1_INST *stub)
#else
void tr_generate_exit_tb(IR1_INST *branch, int succ_id)
#endif
{
    TRANSLATION_DATA *t_data = lsenv->tr_data;
    TranslationBlock *tb = lsenv->tr_data->curr_tb;
    IR1_OPCODE opcode = ir1_opcode(branch);
    ADDR tb_addr = (ADDR)tb;
    ADDR succ_x86_addr = 0;
    /* The epilogue function needs to return the address of the tb */
    IR2_OPND tb_ptr_opnd = a0_ir2_opnd;
    IR2_OPND succ_x86_addr_opnd = ra_alloc_dbt_arg2();
    IR2_OPND goto_label_opnd = ra_alloc_label();
    IR2_OPND label_first_jmp_align = ra_alloc_label();

    IR2_OPND base = ra_alloc_data();
    IR2_OPND target = ra_alloc_data();
    /* set base_address data */
    la_data_li(base, (ADDR)tb->tc.ptr);

    switch (opcode) {
    case dt_X86_INS_CALL:
    case dt_X86_INS_LCALL:
    case dt_X86_INS_JMP:
    case dt_X86_INS_LJMP:
        /*
         * jmp_reset_offset[0] or [1] should be set when the instruction is
         * call or jmp. Because direct jmp or call only have one jmp target
         * address, jmp_reset_offset[n] may do not reset for right address.
         *
         * succ_x86_addr is set 1 for judging whether the instruction is
         * direct jmp or condition jmp.
         */
        if (ir1_is_indirect_jmp(branch)) {
            goto indirect_jmp;
        } else {
            succ_x86_addr = 1;
            goto direct_jmp;
        }
        break;
    case dt_X86_INS_JE:
    case dt_X86_INS_JNE:
    case dt_X86_INS_JS:
    case dt_X86_INS_JNS:
    case dt_X86_INS_JB:
    case dt_X86_INS_JAE:
    case dt_X86_INS_JO:
    case dt_X86_INS_JNO:
    case dt_X86_INS_JBE:
    case dt_X86_INS_JA:
    case dt_X86_INS_JP:
    case dt_X86_INS_JNP:
    case dt_X86_INS_JL:
    case dt_X86_INS_JGE:
    case dt_X86_INS_JLE:
    case dt_X86_INS_JG:
    case dt_X86_INS_JCXZ:
    case dt_X86_INS_JECXZ:
    case dt_X86_INS_JRCXZ:
    case dt_X86_INS_LOOP:
    case dt_X86_INS_LOOPE:
    case dt_X86_INS_LOOPNE:
direct_jmp:
        /*
         * If option_lsfpu is open, condition jmp will jmp to next tb diretly.
         * Therefore LATX do not always need to update last_tb, after tb link.
         *
         * When tb is not link, LATX should save next_x86_addr, succ_id and
         * tb_ptr. next_x86_addr will be used to find next TB, succ_id will
         * be used to TB_link and tb_ptr will be used by jmp_glue.
         * After saving those information, LATX switch to translate next_tb.
         *
         * Also, if we need to patch the offset, the patch place need to be
         * aligned. This nop is used to do that.
         */
        if (!succ_id) {
            la_label(label_first_jmp_align);
            tb->first_jmp_align = ir2_opnd_label_id(&label_first_jmp_align);
        }
        la_code_align(2, 0x03400000);

        if (!use_tu_jmp(tb)) {
#ifndef CONFIG_LATX_XCOMISX_OPT
            la_label(goto_label_opnd);
            if (!use_tu_jmp(tb)) {
                tb->jmp_reset_offset[succ_id] = ir2_opnd_label_id(&goto_label_opnd);
            }
#endif
            IR2_OPND ir2_opnd_addr;
            ir2_opnd_build(&ir2_opnd_addr, IR2_OPND_IMM, 0);
#ifdef CONFIG_LATX_XCOMISX_OPT
            /* Add stub for exit recover */
            if (func) {
                IR2_OPND stub_label = ra_alloc_label();
                la_label(stub_label);
                tb->jmp_stub_reset_offset[succ_id] = ir2_opnd_label_id(&stub_label);
                /* exit tb_link */
                la_nop();
#ifdef CONFIG_LATX_LARGE_CC
                la_nop();
#endif
                /* generate stub */
                ((bool (*)(IR1_INST *))func)(stub);

            }

            la_code_align(2, 0x03400000);

            la_label(goto_label_opnd);
            tb->jmp_reset_offset[succ_id] = ir2_opnd_label_id(&goto_label_opnd);
#endif
            la_b(ir2_opnd_addr);
#ifdef CONFIG_LATX_LARGE_CC
            la_nop();
#endif
        }

#ifdef CONFIG_LATX_PROFILER
        la_profile_begin();
#endif
        aot_load_host_addr(tb_ptr_opnd, tb_addr, LOAD_TB_ADDR, 0);
        if (succ_x86_addr) {
            succ_x86_addr = ir1_target_addr(branch);
        } else {
            succ_x86_addr = succ_id ? ir1_target_addr(branch) : ir1_addr_next(branch);
        }

        target_ulong call_offset __attribute__((unused)) =
                aot_get_call_offset(succ_x86_addr);
        aot_load_guest_addr(succ_x86_addr_opnd, succ_x86_addr,
                LOAD_CALL_TARGET, call_offset);

        bool self_jmp = ((opcode == dt_X86_INS_JMP) &&
                         (succ_x86_addr == ir1_addr(branch)) &&
                         (t_data->curr_ir1_count + 1 != MAX_IR1_NUM_PER_TB));

        if (!qemu_loglevel_mask(CPU_LOG_TB_NOCHAIN) && !self_jmp) {
            la_ori(tb_ptr_opnd, tb_ptr_opnd, succ_id);
        } else {
            la_ori(tb_ptr_opnd, zero_ir2_opnd, succ_id);
        }
        la_data_li(target, context_switch_native_to_bt);
        aot_la_append_ir2_jmp_far(target, base, B_EPILOGUE, 0);
        break;
    case dt_X86_INS_RET:
    case dt_X86_INS_RETF:
    case dt_X86_INS_IRET:
    case dt_X86_INS_IRETD:
    case dt_X86_INS_IRETQ:
indirect_jmp:
        tb->bool_flags |= IS_INDIRECT_JMP;
        /*
         * If option_lsfpu is open, LATX do not need to fpu_rotate, therefore
         * do not save the TB pointer.
         *
         * If using tb_link, LATX will jmp to jmp_glue finding the next TB.
         */
        if (!qemu_loglevel_mask(CPU_LOG_TB_NOCHAIN)) {
            CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
            CPUState *cpu = env_cpu(env);
            uint32_t parallel = cpu->tcg_cflags & CF_PARALLEL;
            if (!close_latx_parallel && !parallel) {
                IR2_OPND old_jmp_label = ra_alloc_label();
                la_label(old_jmp_label);
                tb->jmp_indirect = ir2_opnd_label_id(&old_jmp_label);
                generate_indirect_goto((void *)tb->tc.ptr, false);
                la_data_li(target, context_switch_native_to_bt_ret_0);
                aot_la_append_ir2_jmp_far(target, base, B_EPILOGUE_RET_0, 0);
            } else {
                la_data_li(target, parallel_indirect_jmp_glue);
                IR2_OPND old_jmp_label = ra_alloc_label();
                la_label(old_jmp_label);
                tb->jmp_indirect = ir2_opnd_label_id(&old_jmp_label);
                aot_la_append_ir2_jmp_far(target, base, B_NATIVE_JMP_GLUE2, 0);
                la_data_li(target, context_switch_native_to_bt_ret_0);
                aot_la_append_ir2_jmp_far(target, base, B_EPILOGUE_RET_0, 0);
            }
        } else {
            la_data_li(target, context_switch_native_to_bt_ret_0);
            aot_la_append_ir2_jmp_far(target, base, B_EPILOGUE_RET_0, 0);
        }
#ifdef CONFIG_LATX_PROFILER
        la_profile_begin();
#endif
        break;
    default:
        lsassertm(0, "not implement %d.\n", opcode);
    }

#ifdef CONFIG_LATX_PROFILER
    la_profile_end();
#endif
}

#if defined(CONFIG_LATX_DEBUG) && defined(CONFIG_LATX_INSTS_PATTERN)
__attribute__((unused))
void eflags_eliminate_debugger(TranslationBlock *tb, int n,
                                      TranslationBlock *next_tb)
{
    if (use_tu_jmp(tb)) {
        return;
    }
    if (tb->jmp_stub_reset_offset[n] == TB_JMP_RESET_OFFSET_INVALID
#ifdef CONFIG_LATX_INSTS_PATTERN
        &&tb->eflags_target_arg[n] == TB_JMP_RESET_OFFSET_INVALID
#endif
        )
        return;

#define ALL_FLAGS                                                    \
    (CF_USEDEF_BIT | PF_USEDEF_BIT | AF_USEDEF_BIT | ZF_USEDEF_BIT | \
     SF_USEDEF_BIT | OF_USEDEF_BIT)
    char stub[6] = "STUB ";
    char flags[6] = "FLAGS";
    char *type = (tb->jmp_stub_reset_offset[n] != TB_JMP_RESET_OFFSET_INVALID)
                     ? stub
                     : flags;
    qemu_log_mask(LAT_LOG_EFLAGS, "=EFLAGS=> %s [%c%c%c%c%c%c]\n", type,
                  (next_tb->eflag_use & CF_USEDEF_BIT) ? 'C' : '_',
                  (next_tb->eflag_use & PF_USEDEF_BIT) ? 'P' : '_',
                  (next_tb->eflag_use & AF_USEDEF_BIT) ? 'A' : '_',
                  (next_tb->eflag_use & ZF_USEDEF_BIT) ? 'Z' : '_',
                  (next_tb->eflag_use & SF_USEDEF_BIT) ? 'S' : '_',
                  (next_tb->eflag_use & OF_USEDEF_BIT) ? 'O' : '_');
    if (next_tb->eflag_use && qemu_loglevel_mask(LAT_LOG_EFLAGS)) {
        IR1_INST pir1;
        uint8_t inst_cache[64];
        uint8_t *pins;
        char info[IR1_INST_SIZE] = {0};
        ADDRX pc = next_tb->pc;
        int ir1_nr = next_tb->icount;
        for (int i = 0; i < ir1_nr; ++i) {
            pins = inst_cache;
            int max_insn_len = get_insn_len_readable(pc);
            for (int j = 0; j < max_insn_len; ++j) {
                *pins = cpu_read_code_via_qemu(lsenv->cpu_state, pc + j);
                pins++;
            }
            if(debug_with_dis((void *)inst_cache)) {
                pc = ir1_disasm_bd(&pir1, inst_cache, pc, 0, &info);
            } else {
                pc = ir1_disasm(&pir1, inst_cache, pc, 0, &info);
            }
            qemu_log("[EFLAGS] ");
            ir1_dump(&pir1);
        }
    }
#undef ALL_FLAGS
}
#endif

bool tr_opt_simm12(IR1_INST *ir1)
{
#ifdef CONFIG_LATX_FLAG_REDUCTION
    IR1_OPND *opnd1 = ir1_get_opnd(ir1, 1);
    return !ir1_need_calculate_any_flag(ir1) &&
            ir1_opnd_is_simm12(opnd1);
#else
    return false;
#endif
}

bool tr_opt_uimm12(IR1_INST *ir1)
{
#ifdef CONFIG_LATX_FLAG_REDUCTION
    IR1_OPND *opnd1 = ir1_get_opnd(ir1, 1);
    return !ir1_need_calculate_any_flag(ir1) &&
            ir1_opnd_is_uimm12(opnd1);
#else
    return false;
#endif
}

#ifdef CONFIG_LATX_AVX_OPT
void tr_gen_call_to_helper_xgetbv(void)
{
    /* aot relocation requires the tb struct */
    TranslationBlock *tb __attribute__((unused)) = NULL;
    if (option_aot) {
        tb = (TranslationBlock *)lsenv->tr_data->curr_tb;
    }

    /* prologue */
    tr_gen_call_to_helper_prologue(0);

    /* load func_addr and jmp */
    IR2_OPND func_addr_opnd = ra_alloc_dbt_arg2();
    li_d(func_addr_opnd, (ADDR)helper_xgetbv);

    /* prologue, jmp and epilogue */
    IR2_OPND eax_opnd = ra_alloc_gpr(eax_index);
    IR2_OPND ecx_opnd = ra_alloc_gpr(ecx_index);
    IR2_OPND edx_opnd = ra_alloc_gpr(edx_index);
    la_mov64(a0_ir2_opnd, env_ir2_opnd);
    la_mov64(a1_ir2_opnd, ecx_opnd);

    /* load func_addr and jmp */
    la_jirl(ra_ir2_opnd, func_addr_opnd, 0);

    /* CAUTION: make sure prologue/epilogue not saving/restore GPRs0-7 */
    /* Because we need eax/edx values used for insns after epliogue */
    la_bstrpick_d(eax_opnd, a0_ir2_opnd, 31, 0);
    la_bstrpick_d(edx_opnd, a0_ir2_opnd, 63, 32);

    tr_gen_call_to_helper_epilogue(0);
}
#endif
