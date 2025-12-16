#include "common.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "flag-lbt.h"
#include "translate.h"
#include "insts-pattern.h"
#include "tu.h"
#include "hbr.h"

#ifdef CONFIG_LATX_INSTS_PATTERN

#define WRAP(ins) (dt_X86_INS_##ins)
#define EFLAGS_CACULATE(opnd0, opnd1, inst, i)                       \
    do {                                                             \
        bool need_calc_flag = ir1_need_calculate_any_flag(inst);     \
        if (!need_calc_flag)                                         \
            break;                                                   \
        TranslationBlock *tb = lsenv->tr_data->curr_tb;              \
        IR2_OPND eflags = ra_alloc_label();                          \
        la_label(eflags);                                            \
        tb->eflags_target_arg[i] = ir2_opnd_label_id(&eflags);       \
        generate_eflag_calculation(opnd0, opnd0, opnd1, inst, true); \
    } while (0)

bool translate_cmp_jcc(IR1_INST *ir1)
{
    IR1_INST *curr = ir1;
    IR1_INST *next = ir1 + 1;
    lsassertm(curr->cflag & IR1_PATTERN_MASK, "%x", curr->cflag);
    lsassertm(next->cflag & IR1_PATTERN_MASK, "%x", next->cflag);
    lsassertm(next->cflag & IR1_INVALID_MASK, "%x", next->cflag);

    curr->info->id = WRAP(CMP);

    int em = ZERO_EXTENSION;
    switch (ir1_opcode(next)) {
    case WRAP(JL):
    case WRAP(JGE):
    case WRAP(JLE):
    case WRAP(JG):
        em = SIGN_EXTENSION;
        break;
    default:
        break;
    }

    IR2_OPND src_opnd_0 = load_ireg_from_ir1(ir1_get_opnd(curr, 0), em, false);
    IR2_OPND src_opnd_1 = load_ireg_from_ir1(ir1_get_opnd(curr, 1), em, false);

    IR2_OPND target_label_opnd = ra_alloc_label();
#ifdef CONFIG_LATX_TU
    IR2_OPND tu_reset_label_opnd = ra_alloc_label();
    la_label(tu_reset_label_opnd);
#endif

    switch (ir1_opcode(next)) {
    case WRAP(JB):
        la_bltu(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JAE):
        la_bgeu(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JE):
        la_beq(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JNE):
        la_bne(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JBE):
        la_bgeu(src_opnd_1, src_opnd_0, target_label_opnd);
        break;
    case WRAP(JA):
        la_bltu(src_opnd_1, src_opnd_0, target_label_opnd);
        break;
    case WRAP(JL):
        la_blt(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JGE):
        la_bge(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JLE):
        la_bge(src_opnd_1, src_opnd_0, target_label_opnd);
        break;
    case WRAP(JG):
        la_blt(src_opnd_1, src_opnd_0, target_label_opnd);
        break;
    default:
        lsassert(0);
        break;
    }

#ifdef CONFIG_LATX_TU
    if (judge_tu_eflag_gen(lsenv->tr_data->curr_tb)) {
        TranslationBlock *tb = lsenv->tr_data->curr_tb;
        tu_jcc_nop_gen(tb);
        la_label(target_label_opnd);
        /* tb->jmp_target_arg[0] = target_label_opnd._label_id; */
        tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_reset_label_opnd._label_id;
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            IR2_OPND translated_label_opnd = ra_alloc_label();
            /* la_code_align(2, 0x03400000); */
            la_label(translated_label_opnd);
            la_b(imm_zero_ir2_opnd);
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
        }

        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);

        IR2_OPND target_label_opnd2 = ra_alloc_label();
        switch (ir1_opcode(next)) {
        case WRAP(JB):
            la_bltu(src_opnd_0, src_opnd_1, target_label_opnd2);
            break;
        case WRAP(JAE):
            la_bgeu(src_opnd_0, src_opnd_1, target_label_opnd2);
            break;
        case WRAP(JE):
            la_beq(src_opnd_0, src_opnd_1, target_label_opnd2);
            break;
        case WRAP(JNE):
            la_bne(src_opnd_0, src_opnd_1, target_label_opnd2);
            break;
        case WRAP(JBE):
            la_bgeu(src_opnd_1, src_opnd_0, target_label_opnd2);
            break;
        case WRAP(JA):
            la_bltu(src_opnd_1, src_opnd_0, target_label_opnd2);
            break;
        case WRAP(JL):
            la_blt(src_opnd_0, src_opnd_1, target_label_opnd2);
            break;
        case WRAP(JGE):
            la_bge(src_opnd_0, src_opnd_1, target_label_opnd2);
            break;
        case WRAP(JLE):
            la_bge(src_opnd_1, src_opnd_0, target_label_opnd2);
            break;
        case WRAP(JG):
            la_blt(src_opnd_1, src_opnd_0, target_label_opnd2);
            break;
        default:
            lsassert(0);
            break;
        }
        /* not taken */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 0); */
        tr_generate_exit_tb(next, 0);

        la_label(target_label_opnd2);
        /* taken */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 1); */
        tr_generate_exit_tb(next, 1);

        /*
         * the backup of the eflags instruction, which is used
         * to recover the eflags instruction when unlink a tb.
         */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, EFLAG_BACKUP); */
        return true;
    }
#endif

    /* not taken */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 0);
    tr_generate_exit_tb(next, 0);

    la_label(target_label_opnd);
    /* taken */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 1);
    tr_generate_exit_tb(next, 1);

    /*
     * the backup of the eflags instruction, which is used
     * to recover the eflags instruction when unlink a tb.
     */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, EFLAG_BACKUP);
    return true;
}

static inline void gen_sub(IR2_OPND dest, IR2_OPND src_opnd_0, IR2_OPND src_opnd_1,
        IR2_OPND mem_opnd, int imm, IR1_INST *ir1, IR1_OPND *opnd0, int opnd0_size)
{
    la_sub_d(dest, src_opnd_0, src_opnd_1);
#ifdef TARGET_X86_64
    if (!GHBR_ON(ir1) && CODEIS64 && ir1_opnd_is_gpr(opnd0) && opnd0_size == 32) {
        la_mov32_zx(dest, dest);
    }
#endif
    /* write back */
    if (ir1_opnd_is_gpr(opnd0)) {
        /* r16/r8 */
        if (opnd0_size < 32) {
            store_ireg_to_ir1(dest, opnd0, false);
        }
    } else {
        la_st_by_op_size(dest, mem_opnd, imm, opnd0_size);
    }
}

static IR2_OPND load_opnd_from_opnd(IR2_OPND src_opnd, EXTENSION_MODE em, int opnd_size)
{
    lsassert(em == SIGN_EXTENSION || em == ZERO_EXTENSION ||
             em == UNKNOWN_EXTENSION);
    IR2_OPND ret_opnd = ra_alloc_itemp_internal();
    if (opnd_size == 64 || em == UNKNOWN_EXTENSION) {
#ifdef CONFIG_LATX_TU
        if (judge_tu_eflag_gen(lsenv->tr_data->curr_tb)) {
            la_mov64(ret_opnd, src_opnd);
            return ret_opnd;
        }
#endif
        return src_opnd;
    }
    if (opnd_size == 32) {
        if (em == SIGN_EXTENSION ) {
            la_mov32_sx(ret_opnd, src_opnd);
        } else if (em == ZERO_EXTENSION ) {
            la_mov32_zx(ret_opnd, src_opnd);
        }
    } else if (opnd_size == 16) {
        if (em == SIGN_EXTENSION) {
            la_ext_w_h(ret_opnd, src_opnd);
        }
        else if (em == ZERO_EXTENSION ) {
            la_bstrpick_d(ret_opnd, src_opnd, 15, 0);
        }
    } else {
        if (em == SIGN_EXTENSION) {
            la_ext_w_b(ret_opnd, src_opnd);
        } else if (em == ZERO_EXTENSION ) {
            la_andi(ret_opnd, src_opnd, 0xff);
        }
    }
    return ret_opnd;
}

static inline void jcc_gen_bcc(IR2_OPND src_opnd_0, IR2_OPND src_opnd_1,
        IR2_OPND target_label_opnd, IR1_INST *jcc_inst) {
    switch (ir1_opcode(jcc_inst)) {
    case WRAP(JB):
        la_bltu(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JAE):
        la_bgeu(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JE):
        la_beq(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JNE):
        la_bne(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JBE):
        la_bgeu(src_opnd_1, src_opnd_0, target_label_opnd);
        break;
    case WRAP(JA):
        la_bltu(src_opnd_1, src_opnd_0, target_label_opnd);
        break;
    case WRAP(JL):
        la_blt(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JGE):
        la_bge(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JLE):
        la_bge(src_opnd_1, src_opnd_0, target_label_opnd);
        break;
    case WRAP(JG):
        la_blt(src_opnd_1, src_opnd_0, target_label_opnd);
        break;
    default:
        lsassert(0);
        break;
    }
}

bool translate_sub_jcc(IR1_INST *ir1)
{
    IR1_INST *curr = ir1;
    IR1_INST *next = ir1 + 1;

    lsassertm(curr->cflag & IR1_PATTERN_MASK, "%x", curr->cflag);
    lsassertm(next->cflag & IR1_PATTERN_MASK, "%x", next->cflag);
    lsassertm(next->cflag & IR1_INVALID_MASK, "%x", next->cflag);
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    IR1_OPND *opnd0 = ir1_get_opnd(ir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(ir1, 1);
    IR2_OPND src_opnd_0, src_opnd_1;
    IR2_OPND bcc_src0, bcc_src1;
    IR2_OPND dest, mem_opnd;
    int imm, opnd0_size;

    curr->info->id = WRAP(SUB);
    opnd0_size = ir1_opnd_size(opnd0);

    bool is_lock = ir1_is_prefix_lock(ir1) && ir1_opnd_is_mem(opnd0);
    if (!close_latx_parallel) {
        is_lock = is_lock && (cpu->tcg_cflags & CF_PARALLEL);
    }
    if (is_lock) {
        translate_sub(ir1);
        translate_jcc(ir1 + 1);
        return true;
    }

    /* int em = SIGN_EXTENSION; */
    int em = ZERO_EXTENSION;
    switch (ir1_opcode(next)) {
    case WRAP(JL):
    case WRAP(JGE):
    case WRAP(JLE):
    case WRAP(JG):
        em = SIGN_EXTENSION;
        break;
    default:
        break;
    }

    src_opnd_1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    if (ir1_opnd_is_gpr(opnd0)) {
        src_opnd_0 = convert_gpr_opnd(opnd0, UNKNOWN_EXTENSION);
        if (opnd0_size >= 32) {
            dest = src_opnd_0;
        } else {
            dest = ra_alloc_itemp();
        }
    } else {
        src_opnd_0 = ra_alloc_itemp();
        dest = src_opnd_0;
        mem_opnd = convert_mem(opnd0, &imm);
        la_ld_by_op_size(src_opnd_0, mem_opnd, imm, opnd0_size);
    }

    bcc_src0 = load_opnd_from_opnd(src_opnd_0, em, opnd0_size);
    bcc_src1 = load_opnd_from_opnd(src_opnd_1, em, ir1_opnd_size(opnd1));

    /* bcc_src0 = load_ireg_from_ir1(opnd0, em, false); */
    /* bcc_src1 = load_ireg_from_ir1(opnd1, em, false); */

    IR2_OPND target_label_opnd = ra_alloc_label();
#ifdef CONFIG_LATX_TU
    IR2_OPND tu_reset_label_opnd = ra_alloc_label();
    if (judge_tu_eflag_gen(lsenv->tr_data->curr_tb)) {
        gen_sub(dest, src_opnd_0, src_opnd_1, mem_opnd, imm,
                ir1, opnd0, opnd0_size);
        la_label(tu_reset_label_opnd);
        jcc_gen_bcc(bcc_src0, bcc_src1, target_label_opnd, next);
        TranslationBlock *tb = lsenv->tr_data->curr_tb;
        tu_jcc_nop_gen(tb);
        la_label(target_label_opnd);
        /* tb->jmp_target_arg[0] = target_label_opnd._label_id; */
        tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_reset_label_opnd._label_id;
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            IR2_OPND translated_label_opnd = ra_alloc_label();
            /* la_code_align(2, 0x03400000); */
            la_label(translated_label_opnd);
            la_b(imm_zero_ir2_opnd);
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
        }
        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);
        IR2_OPND target_label_opnd2 = ra_alloc_label();
        jcc_gen_bcc(bcc_src0, bcc_src1, target_label_opnd2, next);
        tr_generate_exit_tb(next, 0);
        la_label(target_label_opnd2);
        tr_generate_exit_tb(next, 1);
        /* ra_free_temp(bcc_src0); */
        /* ra_free_temp(bcc_src1); */
        return true;
    } else
#endif
    {
        jcc_gen_bcc(bcc_src0, bcc_src1, target_label_opnd, next);
    }
    /* not taken */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 0);
    gen_sub(dest, src_opnd_0, src_opnd_1, mem_opnd, imm,
            ir1, opnd0, opnd0_size);
    tr_generate_exit_tb(next, 0);

    la_label(target_label_opnd);
    /* taken */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 1);
    gen_sub(dest, src_opnd_0, src_opnd_1, mem_opnd, imm,
            ir1, opnd0, opnd0_size);
    tr_generate_exit_tb(next, 1);
    /*
     * the backup of the eflags instruction, which is used
     * to recover the eflags instruction when unlink a tb.
     */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, EFLAG_BACKUP);
    /* ra_free_temp(bcc_src0); */
    /* ra_free_temp(bcc_src1); */

    return true;
}

#ifdef CONFIG_LATX_XCOMISX_OPT
static inline bool xcomisx_jcc(IR1_INST *ir1, bool is_double, bool qnan_exp)
{
    IR1_INST *curr = ir1;
    IR1_INST *next = ir1 + 1;
    bool (*trans)(IR1_INST *) = translate_xcomisx;
    IR2_INST* (*la_fcmp)(IR2_OPND, IR2_OPND, IR2_OPND, int);


    lsassertm(curr->cflag & IR1_PATTERN_MASK, "%x", curr->cflag);
    lsassertm(next->cflag & IR1_PATTERN_MASK, "%x", next->cflag);
    lsassertm(next->cflag & IR1_INVALID_MASK, "%x", next->cflag);

    if (is_double) {
        la_fcmp = la_fcmp_cond_d;
        if (qnan_exp) {
            curr->info->id = WRAP(COMISD);
        } else {
            curr->info->id = WRAP(UCOMISD);
        }
    } else {
        la_fcmp = la_fcmp_cond_s;
        if (qnan_exp) {
            curr->info->id = WRAP(COMISS);
        } else {
            curr->info->id = WRAP(UCOMISS);
        }
    }

    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(ir1, 0));
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(ir1, 1));
    IR2_OPND target_label_opnd = ra_alloc_label();

    switch (ir1_opcode(next)) {
    case WRAP(JA):
        la_fcmp(fcc7_ir2_opnd, src, dest, FCMP_COND_CLT + qnan_exp);
        break;
    case WRAP(JAE):
        la_fcmp(fcc7_ir2_opnd, src, dest, FCMP_COND_CLE + qnan_exp);
        break;
    case WRAP(JB):
	/* below or NAN, x86 special define */
        la_fcmp(fcc7_ir2_opnd, dest, src, FCMP_COND_CULT + qnan_exp);
        break;
    case WRAP(JBE):
	/* below or equal or NAN, x86 special define */
        la_fcmp(fcc7_ir2_opnd, dest, src, FCMP_COND_CULE + qnan_exp);
        break;
    case WRAP(JE):
    case WRAP(JLE):
	/* equal or NAN, x86 special define */
        la_fcmp(fcc7_ir2_opnd, dest, src, FCMP_COND_CUEQ + qnan_exp);
        break;
    case WRAP(JNE):
    case WRAP(JG):
        la_fcmp(fcc7_ir2_opnd, dest, src, FCMP_COND_CNE + qnan_exp);
        break;
    case WRAP(JL):
        break;
    case WRAP(JGE):
        la_b(target_label_opnd);
        break;
    default:
        lsassert(0);
        break;
    }

#ifdef CONFIG_LATX_TU
    IR2_OPND tu_reset_label_opnd = ra_alloc_label();
    la_label(tu_reset_label_opnd);
#endif

    la_bcnez(fcc7_ir2_opnd, target_label_opnd);

#ifdef CONFIG_LATX_TU
    if (judge_tu_eflag_gen(lsenv->tr_data->curr_tb)) {
        TranslationBlock *tb = lsenv->tr_data->curr_tb;
        tu_jcc_nop_gen(tb);
        la_label(target_label_opnd);
        /* tb->jmp_target_arg[0] = target_label_opnd._label_id; */
        tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_reset_label_opnd._label_id;
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            IR2_OPND translated_label_opnd = ra_alloc_label();
            /* la_code_align(2, 0x03400000); */
            la_label(translated_label_opnd);
            la_b(imm_zero_ir2_opnd);
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
        }

        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);

        IR2_OPND target_label_opnd2 = ra_alloc_label();
        switch (ir1_opcode(next)) {
        case WRAP(JA):
            la_fcmp(fcc7_ir2_opnd, src, dest, FCMP_COND_CLT + qnan_exp);
            break;
        case WRAP(JAE):
            la_fcmp(fcc7_ir2_opnd, src, dest, FCMP_COND_CLE + qnan_exp);
            break;
        case WRAP(JB):
    	/* below or NAN, x86 special define */
            la_fcmp(fcc7_ir2_opnd, dest, src, FCMP_COND_CULT + qnan_exp);
            break;
        case WRAP(JBE):
    	/* below or equal or NAN, x86 special define */
            la_fcmp(fcc7_ir2_opnd, dest, src, FCMP_COND_CULE + qnan_exp);
            break;
        case WRAP(JE):
        case WRAP(JLE):
    	/* equal or NAN, x86 special define */
            la_fcmp(fcc7_ir2_opnd, dest, src, FCMP_COND_CUEQ + qnan_exp);
            break;
        case WRAP(JNE):
        case WRAP(JG):
            la_fcmp(fcc7_ir2_opnd, dest, src, FCMP_COND_CNE + qnan_exp);
            break;
        case WRAP(JL):
            break;
        case WRAP(JGE):
            la_b(target_label_opnd);
        break;
        default:
            lsassert(0);
            break;
        }

        la_bcnez(fcc7_ir2_opnd, target_label_opnd2);
        /* not taken */
        tr_generate_exit_stub_tb(next, 0, trans, curr);

        la_label(target_label_opnd2);
        /* taken */
        tr_generate_exit_stub_tb(next, 1, trans, curr);

        return true;
    }
#endif


    /* not taken */
    tr_generate_exit_stub_tb(next, 0, trans, curr);

    la_label(target_label_opnd);
    /* taken */
    tr_generate_exit_stub_tb(next, 1, trans, curr);

    return true;
}

bool translate_comisd_jcc(IR1_INST *ir1)
{
    return xcomisx_jcc(ir1, true, true);
}

bool translate_comiss_jcc(IR1_INST *ir1)
{
    return xcomisx_jcc(ir1, false, true);
}

bool translate_ucomisd_jcc(IR1_INST *ir1)
{
    return xcomisx_jcc(ir1, true, false);
}

bool translate_ucomiss_jcc(IR1_INST *ir1)
{
    return xcomisx_jcc(ir1, false, false);
}
#endif

bool translate_bt_jcc(IR1_INST *ir1)
{
    IR1_INST *curr = ir1;
    IR1_INST *next = ir1 + 1;
    lsassertm(curr->cflag & IR1_PATTERN_MASK, "%x", curr->cflag);
    lsassertm(next->cflag & IR1_PATTERN_MASK, "%x", next->cflag);
    lsassertm(next->cflag & IR1_INVALID_MASK, "%x", next->cflag);

    curr->info->id = WRAP(BT);
    IR1_OPND *bt_opnd0 = ir1_get_opnd(curr, 0);
    IR1_OPND *bt_opnd1 = ir1_get_opnd(curr, 1);
    IR2_OPND src_opnd_0, src_opnd_1, bit_offset;
    int imm;

    src_opnd_1 = load_ireg_from_ir1(bt_opnd1, ZERO_EXTENSION, false);

    bit_offset = ra_alloc_itemp();
    la_bstrpick_d(bit_offset, src_opnd_1,
        __builtin_ctz(ir1_opnd_size(bt_opnd0)) - 1, 0);
    if (ir1_opnd_is_gpr(bt_opnd0)) {
        /* r16/r32/r64 */
        src_opnd_0 = convert_gpr_opnd(bt_opnd0, UNKNOWN_EXTENSION);
    } else {
        src_opnd_0 = ra_alloc_itemp();
        IR2_OPND tmp_mem_op = convert_mem(bt_opnd0, &imm);
        IR2_OPND mem_opnd = ra_alloc_itemp();
        la_or(mem_opnd, tmp_mem_op, zero_ir2_opnd);
#ifdef CONFIG_LATX_IMM_REG
        imm_cache_free_temp_helper(tmp_mem_op);
#else
        ra_free_temp_auto(tmp_mem_op);
#endif

        if (ir1_opnd_is_gpr(bt_opnd1)) {
            IR2_OPND tmp = ra_alloc_itemp();
            IR2_OPND src1 = convert_gpr_opnd(bt_opnd1, UNKNOWN_EXTENSION);
            int opnd_size = ir1_opnd_size(bt_opnd0);
            int ctz_opnd_size = __builtin_ctz(opnd_size);
            int ctz_align_size = __builtin_ctz(opnd_size / 8);
            lsassertm((opnd_size == 16) || (opnd_size == 32) ||
                (opnd_size == 64), "%s opnd_size error!", __func__);
            la_srai_d(tmp, src1, ctz_opnd_size);
            la_alsl_d(mem_opnd, tmp, mem_opnd, ctz_align_size - 1);
            ra_free_temp(tmp);
        }

        if (ir1_opnd_size(bt_opnd0) == 64) {
            /* m64 */
            la_ld_d(src_opnd_0, mem_opnd, imm);
        } else {
            /* m16/m32 */
            la_ld_w(src_opnd_0, mem_opnd, imm);
        }
        ra_free_temp(mem_opnd);
    }

    IR2_OPND temp_opnd = ra_alloc_itemp();
    la_srl_d(temp_opnd, src_opnd_0, bit_offset);
    la_andi(temp_opnd, temp_opnd, 1);

    IR2_OPND target_label_opnd = ra_alloc_label();
#ifdef CONFIG_LATX_TU
    IR2_OPND tu_reset_label_opnd = ra_alloc_label();
    la_label(tu_reset_label_opnd);
#endif

    switch (ir1_opcode(next)) {
    case WRAP(JB):   /*CF=1*/
        la_bne(temp_opnd, zero_ir2_opnd, target_label_opnd);
        break;
    case WRAP(JAE):  /*CF=0*/
        la_beq(temp_opnd, zero_ir2_opnd, target_label_opnd);
        break;
    default:
        lsassert(0);
        break;
    }

#ifdef CONFIG_LATX_TU
    if (judge_tu_eflag_gen(lsenv->tr_data->curr_tb)) {
        TranslationBlock *tb = lsenv->tr_data->curr_tb;
        tu_jcc_nop_gen(tb);
        la_label(target_label_opnd);
        /* tb->jmp_target_arg[0] = target_label_opnd._label_id; */
        tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_reset_label_opnd._label_id;
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            IR2_OPND translated_label_opnd = ra_alloc_label();
            /* la_code_align(2, 0x03400000); */
            la_label(translated_label_opnd);
            la_b(imm_zero_ir2_opnd);
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
        }

        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);

        IR2_OPND target_label_opnd2 = ra_alloc_label();
        switch (ir1_opcode(next)) {
            case WRAP(JB):   /*CF=1*/
                la_bne(temp_opnd, zero_ir2_opnd, target_label_opnd2);
                break;
            case WRAP(JAE):  /*CF=0*/
                la_beq(temp_opnd, zero_ir2_opnd, target_label_opnd2);
                break;
            default:
                lsassert(0);
                break;
        }
        /* not taken */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 0); */
        tr_generate_exit_tb(next, 0);

        la_label(target_label_opnd2);
        /* taken */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 1); */
        tr_generate_exit_tb(next, 1);

        /*
         * the backup of the eflags instruction, which is used
         * to recover the eflags instruction when unlink a tb.
         */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, EFLAG_BACKUP); */
        return true;
    }
#endif


    /* not taken */
    EFLAGS_CACULATE(src_opnd_0, bit_offset, curr, 0);
    tr_generate_exit_tb(next, 0);

    la_label(target_label_opnd);
    /* taken */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 1);
    tr_generate_exit_tb(next, 1);

    /*
     * the backup of the eflags instruction, which is used
     * to recover the eflags instruction when unlink a tb.
     */
    EFLAGS_CACULATE(src_opnd_0, bit_offset, curr, EFLAG_BACKUP);

    ra_free_temp_auto(src_opnd_0);
    ra_free_temp(bit_offset);
    if (ir2_opnd_is_itemp(&src_opnd_1)) {
        ra_free_temp(src_opnd_1);
    }
    return true;
}

bool translate_cqo_idiv(IR1_INST *ir1)
{
    IR1_INST *next = ir1 + 1;
    lsassertm(next->cflag & IR1_PATTERN_MASK, "%x", next->cflag);
    lsassertm(next->cflag & IR1_INVALID_MASK, "%x", next->cflag);

    IR2_OPND src_opnd_0 =
        load_ireg_from_ir1(ir1_get_opnd(next, 0), SIGN_EXTENSION, false);

    IR2_OPND label_z = ra_alloc_label();
    la_bne(src_opnd_0, zero_ir2_opnd, label_z);
    la_break(0x7);
    la_label(label_z);

    if (ir1_opnd_size(ir1_get_opnd(next, 0)) != 64) {
        lsassert(0);
    } else {
        IR2_OPND src_opnd_1 =
            load_ireg_from_ir1(&rax_ir1_opnd, SIGN_EXTENSION, false);
        IR2_OPND temp_src = ra_alloc_itemp();
        IR2_OPND temp1_opnd = ra_alloc_itemp();

        la_or(temp_src, zero_ir2_opnd, src_opnd_1);

        la_mod_d(temp1_opnd, temp_src, src_opnd_0);
        la_div_d(temp_src, temp_src, src_opnd_0);

        store_ireg_to_ir1(temp_src, &rax_ir1_opnd, false);
        store_ireg_to_ir1(temp1_opnd, &rdx_ir1_opnd, false);

        ra_free_temp(temp_src);
        ra_free_temp(temp1_opnd);
    }

    return true;
}

bool translate_cmp_sbb(IR1_INST *ir1)
{
    IR1_INST *curr = ir1;
    IR1_INST *next = ir1 + 1;
    lsassertm(curr->cflag & IR1_PATTERN_MASK, "%x", curr->cflag);
    lsassertm(next->cflag & IR1_PATTERN_MASK, "%x", next->cflag);
    lsassertm(next->cflag & IR1_INVALID_MASK, "%x", next->cflag);

    /* cmp */
    IR1_OPND *cmp_opnd0 = ir1_get_opnd(curr, 0);
    IR1_OPND *cmp_opnd1 = ir1_get_opnd(curr, 1);

    bool cmp_opnd1_is_imm = ir1_opnd_is_simm12(cmp_opnd1);

    IR2_OPND cmp_opnd_0 = load_ireg_from_ir1(cmp_opnd0, SIGN_EXTENSION, false);
    IR2_OPND cmp_opnd_1;

    /* sbb */
    IR1_OPND *sbb_opnd0 = ir1_get_opnd(next, 0);
    lsassert(ir1_opnd_is_same_reg(sbb_opnd0, ir1_get_opnd(next, 1)));
    bool opnd_clobber = ir1_opnd_size(sbb_opnd0) != 64;

    IR2_OPND cond = opnd_clobber
                        ? ra_alloc_itemp()
                        : ra_alloc_gpr(ir1_opnd_base_reg_num(sbb_opnd0));
    /* caculate cmp */
    if (cmp_opnd1_is_imm) {
        la_sltui(cond, cmp_opnd_0, ir1_opnd_simm(cmp_opnd1));
    } else {
        cmp_opnd_1 = load_ireg_from_ir1(cmp_opnd1, SIGN_EXTENSION, false);
        la_sltu(cond, cmp_opnd_0, cmp_opnd_1);
    }

    /* we need change to sub because sbb uses CF (not calculate) */
    next->info->id = WRAP(SUB);
    generate_eflag_calculation(zero_ir2_opnd, zero_ir2_opnd, cond, next, true);

    la_sub_d(cond, zero_ir2_opnd, cond);
    if (opnd_clobber) {
        store_ireg_to_ir1(cond, sbb_opnd0, false);
        ra_free_temp(cond);
    }

    return true;
}

bool translate_test_jcc(IR1_INST *ir1)
{
    IR1_INST *curr = ir1;
    IR1_INST *next = ir1 + 1;
#ifdef CONFIG_LATX_TU
    bool is_branch = true;
#endif
    lsassertm(curr->cflag & IR1_PATTERN_MASK, "%x", curr->cflag);
    lsassertm(next->cflag & IR1_PATTERN_MASK, "%x", next->cflag);
    lsassertm(next->cflag & IR1_INVALID_MASK, "%x", next->cflag);
    lsassert(
        ir1_opnd_is_same_reg(ir1_get_opnd(curr, 0), ir1_get_opnd(curr, 1)));

    curr->info->id = WRAP(TEST);

    IR2_OPND src_opnd_0 =
        load_ireg_from_ir1(ir1_get_opnd(curr, 0), SIGN_EXTENSION, false);

    IR2_OPND target_label_opnd = ra_alloc_label();

#ifdef CONFIG_LATX_TU
    IR2_OPND tu_reset_label_opnd = ra_alloc_label();
    la_label(tu_reset_label_opnd);
#endif


    switch (ir1_opcode(next)) {
    case WRAP(JE):
        la_beq(src_opnd_0, zero_ir2_opnd, target_label_opnd);
        // la_beqz(src_opnd_0, target_label_opnd);
        break;
    case WRAP(JNE):
        // la_bnez(src_opnd_0, target_label_opnd);
        la_bne(src_opnd_0, zero_ir2_opnd, target_label_opnd);
        break;
    case WRAP(JS):
        la_blt(src_opnd_0, zero_ir2_opnd, target_label_opnd);
        break;
    case WRAP(JNS):
        la_bge(src_opnd_0, zero_ir2_opnd, target_label_opnd);
        break;
    case WRAP(JLE):
        la_bge(zero_ir2_opnd, src_opnd_0, target_label_opnd);
        break;
    case WRAP(JG):
        la_blt(zero_ir2_opnd, src_opnd_0, target_label_opnd);
        break;
    case WRAP(JNO):
        /*
         * OF = 0
         * For compatibility with bcc+b.
         */
        la_beq(zero_ir2_opnd, zero_ir2_opnd, target_label_opnd);
        // la_beqz(zero_ir2_opnd, target_label_opnd);
        break;
    case WRAP(JO):
        /* OF = 1 */
#ifdef CONFIG_LATX_TU
        is_branch = false;
#endif
        break;
    case WRAP(JB):
        /* CF = 1 */
#ifdef CONFIG_LATX_TU
        is_branch = false;
#endif
        break;
    case WRAP(JBE):
        /* CF = 1 or ZF = 1 */
        la_beq(src_opnd_0, zero_ir2_opnd, target_label_opnd);
        // la_beqz(src_opnd_0, target_label_opnd);
        break;
    case WRAP(JA):
        la_bne(src_opnd_0, zero_ir2_opnd, target_label_opnd);
        /* CF = 0 and ZF = 0 */
        // la_bnez(src_opnd_0, target_label_opnd);
        break;
    case WRAP(JAE):
        /*
         * CF = 0
         * For compatibility with bcc+b.
         */
        la_beq(zero_ir2_opnd, zero_ir2_opnd, target_label_opnd);
        // la_beqz(zero_ir2_opnd, target_label_opnd);
        break;
    default:
        lsassert(0);
        break;
    }
#ifdef CONFIG_LATX_TU
    if (judge_tu_eflag_gen(lsenv->tr_data->curr_tb)) {
        TranslationBlock *tb = lsenv->tr_data->curr_tb;
        tu_jcc_nop_gen(tb);
        la_label(target_label_opnd);
        if (is_branch) {
            /* tb->jmp_target_arg[0] = target_label_opnd._label_id; */
            tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_reset_label_opnd._label_id;
        } else {
            /* tb->jmp_target_arg[0] = TB_JMP_RESET_OFFSET_INVALID; */
            tb->tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
        }
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            IR2_OPND translated_label_opnd = ra_alloc_label();
            /* la_code_align(2, 0x03400000); */
            la_label(translated_label_opnd);
            la_b(imm_zero_ir2_opnd);
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
        }

        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);

        IR2_OPND target_label_opnd2 = ra_alloc_label();
        switch (ir1_opcode(next)) {
        case WRAP(JE):
            la_beq(src_opnd_0, zero_ir2_opnd, target_label_opnd2);
            break;
        case WRAP(JNE):
            la_bne(src_opnd_0, zero_ir2_opnd, target_label_opnd2);
            break;
        case WRAP(JS):
            la_blt(src_opnd_0, zero_ir2_opnd, target_label_opnd2);
            break;
        case WRAP(JNS):
            la_bge(src_opnd_0, zero_ir2_opnd, target_label_opnd2);
            break;
        case WRAP(JLE):
            la_bge(zero_ir2_opnd, src_opnd_0, target_label_opnd2);
            break;
        case WRAP(JG):
            la_blt(zero_ir2_opnd, src_opnd_0, target_label_opnd2);
            break;
        case WRAP(JNO):
            /*
             * OF = 0
             * For compatibility with bcc+b.
             */
            la_beq(zero_ir2_opnd, zero_ir2_opnd, target_label_opnd2);
            break;
        case WRAP(JO):
            /* OF = 1 */
#ifdef CONFIG_LATX_TU
            is_branch = false;
#endif
            break;
        case WRAP(JB):
            /* CF = 1 */
#ifdef CONFIG_LATX_TU
            is_branch = false;
#endif
            break;
        case WRAP(JBE):
            /* CF = 1 or ZF = 1 */
            la_beq(src_opnd_0, zero_ir2_opnd, target_label_opnd2);
            break;
        case WRAP(JA):
            la_bne(src_opnd_0, zero_ir2_opnd, target_label_opnd2);
            /* CF = 0 and ZF = 0 */
            break;
        case WRAP(JAE):
            /*
             * CF = 0
             * For compatibility with bcc+b.
             */
            la_beq(zero_ir2_opnd, zero_ir2_opnd, target_label_opnd2);
            break;
        default:
            lsassert(0);
            break;
        }
        /* not taken */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_0, curr, 0); */
        tr_generate_exit_tb(next, 0);

        la_label(target_label_opnd2);
        /* taken */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_0, curr, 1); */
        tr_generate_exit_tb(next, 1);

        /*
         * the backup of the eflags instruction, which is used
         * to recover the eflags instruction when unlink a tb.
         */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_0, curr, EFLAG_BACKUP); */

        return true;
    }
#endif

    /* not taken */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_0, curr, 0);
    tr_generate_exit_tb(next, 0);

    la_label(target_label_opnd);
    /* taken */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_0, curr, 1);
    tr_generate_exit_tb(next, 1);

    /*
     * the backup of the eflags instruction, which is used
     * to recover the eflags instruction when unlink a tb.
     */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_0, curr, EFLAG_BACKUP);
    return true;
}

#undef WRAP

bool translate_xor_div(IR1_INST *ir1)
{
    IR1_INST *next = ir1 + 1;
    lsassertm(next->cflag & IR1_PATTERN_MASK, "%x", next->cflag);
    lsassertm(next->cflag & IR1_INVALID_MASK, "%x", next->cflag);

    IR2_OPND src_opnd_0 =
        load_ireg_from_ir1(ir1_get_opnd(next, 0), ZERO_EXTENSION, false);

    IR2_OPND label_z = ra_alloc_label();
    la_bne(src_opnd_0, zero_ir2_opnd, label_z);
    la_break(0x7);
    la_label(label_z);

    IR2_OPND temp_src = ra_alloc_itemp();
    IR2_OPND temp1_opnd = ra_alloc_itemp();
    if (ir1_opnd_size(ir1_get_opnd(next, 0)) == 32) {
        IR2_OPND src_opnd_1 =
            load_ireg_from_ir1(&eax_ir1_opnd, ZERO_EXTENSION, false);
        la_or(temp_src, zero_ir2_opnd, zero_ir2_opnd);
        la_or(temp_src, zero_ir2_opnd, src_opnd_1);

        la_mod_du(temp1_opnd, temp_src, src_opnd_0);
        la_div_du(temp_src, temp_src, src_opnd_0);

        store_ireg_to_ir1(temp_src, &eax_ir1_opnd, false);
        store_ireg_to_ir1(temp1_opnd, &edx_ir1_opnd, false);
    } else if (ir1_opnd_size(ir1_get_opnd(next, 0)) == 64) {
        IR2_OPND src_opnd_1 =
            load_ireg_from_ir1(&rax_ir1_opnd, ZERO_EXTENSION, false);

        la_or(temp_src, zero_ir2_opnd, src_opnd_1);

        la_mod_du(temp1_opnd, temp_src, src_opnd_0);
        la_div_du(temp_src, temp_src, src_opnd_0);

        store_ireg_to_ir1(temp_src, &rax_ir1_opnd, false);
        store_ireg_to_ir1(temp1_opnd, &rdx_ir1_opnd, false);
    } else {
        lsassert(0);
    }
    ra_free_temp(temp_src);
    ra_free_temp(temp1_opnd);

    return true;
}

bool translate_cdq_idiv(IR1_INST *ir1)
{
    IR1_INST *next = ir1 + 1;
    lsassertm(next->cflag & IR1_PATTERN_MASK, "%x", next->cflag);
    lsassertm(next->cflag & IR1_INVALID_MASK, "%x", next->cflag);

    IR2_OPND src_opnd_0 =
        load_ireg_from_ir1(ir1_get_opnd(next, 0), SIGN_EXTENSION, false);

    IR2_OPND label_z = ra_alloc_label();
    la_bne(src_opnd_0, zero_ir2_opnd, label_z);
    la_break(0x7);
    la_label(label_z);

    if (ir1_opnd_size(ir1_get_opnd(next, 0)) != 32) {
        lsassert(0);
    } else {
        IR2_OPND src_opnd_1 =
            load_ireg_from_ir1(&eax_ir1_opnd, SIGN_EXTENSION, false);
        IR2_OPND temp_src = ra_alloc_itemp();
        IR2_OPND temp1_opnd = ra_alloc_itemp();

        la_or(temp_src, zero_ir2_opnd, src_opnd_1);

        la_mod_d(temp1_opnd, temp_src, src_opnd_0);
        la_div_d(temp_src, temp_src, src_opnd_0);

        store_ireg_to_ir1(temp_src, &eax_ir1_opnd, false);
        store_ireg_to_ir1(temp1_opnd, &edx_ir1_opnd, false);

        ra_free_temp(temp_src);
        ra_free_temp(temp1_opnd);
    }
    return true;
}
#else

bool translate_cmp_jcc(IR1_INST *ir1)     { return false; }
bool translate_sub_jcc(IR1_INST *ir1)     { return false; }
bool translate_cmp_sbb(IR1_INST *ir1)     { return false; }
bool translate_bt_jcc(IR1_INST *ir1)      { return false; }
bool translate_cqo_idiv(IR1_INST *ir1)  { return false; }
bool translate_test_jcc(IR1_INST *ir1)    { return false; }
bool translate_xor_div(IR1_INST *ir1)  { return false; }
bool translate_cdq_idiv(IR1_INST *ir1)  { return false; }
#ifdef CONFIG_LATX_XCOMISX_OPT
bool translate_comisd_jcc(IR1_INST *ir1)  { return false; }
bool translate_comiss_jcc(IR1_INST *ir1)  { return false; }
bool translate_ucomisd_jcc(IR1_INST *ir1) { return false; }
bool translate_ucomiss_jcc(IR1_INST *ir1) { return false; }
#endif
#endif
