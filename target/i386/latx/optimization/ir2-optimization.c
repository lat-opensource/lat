/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "common.h"
#include "translate.h"
#include "la-append.h"
#include "lsenv.h"
#include "cpu.h"

#if 0
static bool has_dependency(IR2_INST *p1, IR2_INST *p2)
{
    IR2_OPND empty_opnd = ir2_opnd_new_none();

    for (int i = 0; i < 3; ++i) {
        IR2_OPND *opnd1 = p1->_opnd + i;
        if (ir2_opnd_cmp(opnd1, &empty_opnd)) {
            break;
        }

        for (int j = 0; j < 3; ++j) {
            IR2_OPND *opnd2 = p2->_opnd + j;
            if (ir2_opnd_cmp(opnd2, &empty_opnd)) {
                break;
            }

            if (opnd1->_reg_num == opnd2->_reg_num) {
                IR2_OPND_TYPE t1 = ir2_opnd_type(opnd1);
                IR2_OPND_TYPE t2 = ir2_opnd_type(opnd2);
                if (t1 == IR2_OPND_GPR &&
                    (t2 == IR2_OPND_GPR || t2 == IR2_OPND_MEM))
                    return true;
                else if (t2 == IR2_OPND_GPR &&
                         (t1 == IR2_OPND_GPR || t1 == IR2_OPND_MEM)) {
                    return true;
                } else if (t1 == IR2_OPND_FPR && t2 == IR2_OPND_FPR) {
                    return true;
                }
            }
        }
    }
    return false;
}
#endif

#if 0
static void tri_schedule_one_ir2(IR2_INST *p, int max_distance)
{
    /* max_distance default is 10 */
    int distance = 0;
    /* 1. scan backward to find a proper position */
    IR2_INST *ir2_first = lsenv->tr_data->first_ir2;
    IR2_INST *ir2_block = ir2_prev(p);
    while (ir2_block != ir2_first) {
        IR2_OPCODE opc = ir2_opcode(ir2_block);
        /* 1.1 if we meet a load/store/label, stop here. */
        if (ir2_opcode_is_load(opc) || ir2_opcode_is_store(opc) ||
            opc == mips_label) {
            break;
        }
        /* 1.2. if we meet a branch/jump, stop after the delay slot */
        else if (ir2_opcode_is_branch(opc) || ir2_opcode_is_f_branch(opc) ||
                 opc == mips_j || opc == mips_jal || opc == mips_jr ||
                 opc == mips_jalr) {
            ir2_block = ir2_next(ir2_block);
            break;
        }
        /* 1.3 if the operands has any dependency, stop here */
        else if (has_dependency(p, ir2_block)) {
            break;
        }
        /* 1.4 in other case, check the previous ir2 */
        else {
            ir2_block = ir2_prev(ir2_block);
            distance++;
        }
        if (distance >= max_distance) {
            break;
        }
    }

    while (ir2_opcode(ir2_next(ir2_block)) == LISA_X86_INST) {
        ir2_block = ir2_next(ir2_block);
    }

    /* 2. now we have a proper ir2_block, so insert p after ir2_block */
    if (p != ir2_block && p != ir2_next(ir2_block)) {
        ir2_remove(p);
        ir2_insert_after(p, ir2_block);
    }
}
#endif

static __attribute__((unused))
void ir2_schedule(TranslationBlock *tb)
{
#if 0
    /* 1. scan forward, from the second ir2 */
    IR2_INST *ir2_current = ir2_next(lsenv->tr_data->first_ir2);
    while (ir2_current != NULL) {
        /* 1.1 schedule every load/store */
        if (ir2_opcode_is_load(ir2_opcode(ir2_current)) ||
            ir2_opcode_is_store(ir2_opcode(ir2_current))) {
            tri_schedule_one_ir2(ir2_current, 10);
        }
        /* 1.2 we should stop before the linkage code */
        else if (ir2_opcode(ir2_current) == LISA_LABEL) {
            int label_id = ir2_opnd_addr(ir2_current->_opnd);
            if (tb->jmp_target_arg[0] == label_id ||
                tb->jmp_target_arg[1] == label_id)
                break;
        }

        ir2_current = ir2_next(ir2_current);
    }
#endif
}
static __attribute__((unused))
void tri_separate_branch_from_ldst(TranslationBlock *tb)
{
#if 0
    IR2_INST *ir2_current = lsenv->tr_data->first_ir2;
    IR2_INST *ir2_prev = NULL;
    bool is_prev_ldst_not_rl = false;
    bool is_prev_branch = false;
    while (ir2_current != NULL) {
        if (ir2_opcode(ir2_current) == LISA_LABEL) {
            int label_id = ir2_opnd_addr(ir2_current->_opnd);
            if (tb->jmp_target_arg[0] == label_id ||
                tb->jmp_target_arg[1] == label_id)
                break;
            ir2_current = ir2_next(ir2_current);
            continue;
        }

        if (ir2_opcode(ir2_current) == LISA_X86_INST) {
            ir2_current = ir2_next(ir2_current);
            continue;
        }

        if (ir2_opcode_is_branch(ir2_opcode(ir2_current))) {
            if (is_prev_ldst_not_rl) {
                IR2_INST *extra = la_andi(zero_ir2_opnd, zero_ir2_opnd, 0);
                ir2_remove(extra);
                ir2_insert_after(extra, ir2_prev);
            }
            ir2_prev = ir2_current;
            is_prev_branch = true;
        } else {
            ir2_prev = ir2_current;
            is_prev_branch = false;
        }

        if (ir2_opcode_is_load_not_rl(ir2_opcode(ir2_current)) ||
            ir2_opcode_is_store_not_rl(ir2_opcode(ir2_current))) {
            if (is_prev_branch) {
                /* env->tr_data->dump(); */
                lsassertm(0,
                          "load/store instruction cannot be in delay slot\n");
            }
            ir2_prev = ir2_current;
            is_prev_ldst_not_rl = true;
        } else {
            ir2_prev = ir2_current;
            is_prev_ldst_not_rl = false;
        }

        ir2_current = ir2_next(ir2_current);
    }
#endif
}

static __attribute__((unused))
void tri_avoid_last_ir2_is_ldst_not_rl(void)
{
#if 0
    IR2_INST *ir2_curr = lsenv->tr_data->last_ir2;
    while (ir2_opcode(ir2_curr) == LISA_LABEL ||
           ir2_opcode(ir2_curr) == LISA_X86_INST) {
        if (ir2_curr == lsenv->tr_data->first_ir2) {
            return;
        }
        ir2_curr = ir2_prev(ir2_curr);
    }

    if (ir2_opcode_is_load_not_rl(ir2_opcode(ir2_curr)) ||
        ir2_opcode_is_store_not_rl(ir2_opcode(ir2_curr))) {
        append_ir2_opnd0(mips_nop);
        /* fprintf(stderr, "last meaningful ir2 is ldst not rl!\n"); */
    }
#endif
}

static __attribute__((unused))
void tri_avoid_leading_label(void)
{
#if 0
    IR2_INST *ir2_current = lsenv->tr_data->first_ir2;
    while (ir2_current != NULL) {
        if (ir2_opcode(ir2_current) == LISA_X86_INST) {
            ir2_current = ir2_next(ir2_current);
            continue;
        } else if (ir2_opcode(ir2_current) == LISA_LABEL) { /* leading label */
            IR2_INST *extra = append_ir2_opnd0(mips_nop);
            ir2_remove(extra);
            ir2_insert_before(extra, ir2_current);
            return;
        } else { /* other instruction, so leading label is impossible */
            return;
        }
    }
#endif
}

static int ir2_get_addi_rsp_offs(IR2_INST *ir2)
{
    int rsp = reg_gpr_map[esp_index];
    if (ir2->_opnd[0]._reg_num == rsp && ir2->_opnd[1]._reg_num == rsp) {
        return ir2->_opnd[2]._imm32;
    }
    return 0;
}

static bool ir2_opndn_is_rsp(IR2_INST *ir2, int index)
{
    if (ir2->_opnd[index]._type == IR2_OPND_GPR &&
        ir2->_opnd[index]._reg_num == reg_gpr_map[esp_index]) {
        return true;
    }
    return false;
}

static bool ir2_src_contain_rsp(IR2_INST *ir2)
{
    IR2_OPCODE op = ir2_opcode(ir2);
    switch (op) {
    case LISA_X86MTTOP:
    case LISA_X86MFTOP:
    case LISA_X86LOOPE:
    case LISA_X86LOOPNE:
    case LISA_X86INC_B:
    case LISA_X86INC_H:
    case LISA_X86INC_W:
    case LISA_X86INC_D:
    case LISA_X86DEC_B:
    case LISA_X86DEC_H:
    case LISA_X86DEC_W:
    case LISA_X86DEC_D:
    case LISA_X86SETTM:
    case LISA_X86CLRTM:
    case LISA_X86INCTOP:
    case LISA_X86DECTOP:
    case LISA_SETX86J:
    case LISA_X86MUL_B:
    case LISA_X86MUL_H:
    case LISA_X86MUL_W:
    case LISA_X86MUL_D:
    case LISA_X86MUL_BU:
    case LISA_X86MUL_HU:
    case LISA_X86MUL_WU:
    case LISA_X86MUL_DU:
    case LISA_X86ADD_WU:
    case LISA_X86ADD_DU:
    case LISA_X86SUB_WU:
    case LISA_X86SUB_DU:
    case LISA_X86ADD_B:
    case LISA_X86ADD_H:
    case LISA_X86ADD_W:
    case LISA_X86ADD_D:
    case LISA_X86SUB_B:
    case LISA_X86SUB_H:
    case LISA_X86SUB_W:
    case LISA_X86SUB_D:
    case LISA_X86ADC_B:
    case LISA_X86ADC_H:
    case LISA_X86ADC_W:
    case LISA_X86ADC_D:
    case LISA_X86SBC_B:
    case LISA_X86SBC_H:
    case LISA_X86SBC_W:
    case LISA_X86SBC_D:
    case LISA_X86SLL_B:
    case LISA_X86SLL_H:
    case LISA_X86SLL_W:
    case LISA_X86SLL_D:
    case LISA_X86SRL_B:
    case LISA_X86SRL_H:
    case LISA_X86SRL_W:
    case LISA_X86SRL_D:
    case LISA_X86SRA_B:
    case LISA_X86SRA_H:
    case LISA_X86SRA_W:
    case LISA_X86SRA_D:
    case LISA_X86ROTR_B:
    case LISA_X86ROTR_H:
    case LISA_X86ROTR_D:
    case LISA_X86ROTR_W:
    case LISA_X86ROTL_B:
    case LISA_X86ROTL_H:
    case LISA_X86ROTL_W:
    case LISA_X86ROTL_D:
    case LISA_X86RCR_B:
    case LISA_X86RCR_H:
    case LISA_X86RCR_W:
    case LISA_X86RCR_D:
    case LISA_X86RCL_B:
    case LISA_X86RCL_H:
    case LISA_X86RCL_W:
    case LISA_X86RCL_D:
    case LISA_X86AND_B:
    case LISA_X86AND_H:
    case LISA_X86AND_W:
    case LISA_X86AND_D:
    case LISA_X86OR_B:
    case LISA_X86OR_H:
    case LISA_X86OR_W:
    case LISA_X86OR_D:
    case LISA_X86XOR_B:
    case LISA_X86XOR_H:
    case LISA_X86XOR_W:
    case LISA_X86XOR_D:
    case LISA_X86SLLI_B:
    case LISA_X86SLLI_H:
    case LISA_X86SLLI_W:
    case LISA_X86SLLI_D:
    case LISA_X86SRLI_B:
    case LISA_X86SRLI_H:
    case LISA_X86SRLI_W:
    case LISA_X86SRLI_D:
    case LISA_X86SRAI_B:
    case LISA_X86SRAI_H:
    case LISA_X86SRAI_W:
    case LISA_X86SRAI_D:
    case LISA_X86ROTRI_B:
    case LISA_X86ROTRI_H:
    case LISA_X86ROTRI_W:
    case LISA_X86ROTRI_D:
    case LISA_X86RCRI_B:
    case LISA_X86RCRI_H:
    case LISA_X86RCRI_W:
    case LISA_X86RCRI_D:
    case LISA_X86ROTLI_B:
    case LISA_X86ROTLI_H:
    case LISA_X86ROTLI_W:
    case LISA_X86ROTLI_D:
    case LISA_X86RCLI_B:
    case LISA_X86RCLI_H:
    case LISA_X86RCLI_W:
    case LISA_X86RCLI_D:
    case LISA_X86SETTAG:
    case LISA_X86MFFLAG:
    case LISA_X86MTFLAG: {
        for (int i = 0; i < ir2->op_count; i++) {
            if (ir2->_opnd[i]._type == IR2_OPND_GPR &&
                ir2->_opnd[i]._reg_num == reg_gpr_map[esp_index]) {
                return true;
            }
        }
        break;
    }
    case LISA_SC_W:
    case LISA_SC_D:
    case LISA_STPTR_W:
    case LISA_STPTR_D:
    case LISA_ST_B:
    case LISA_ST_H:
    case LISA_ST_W:
    case LISA_ST_D:
    case LISA_STL_W:
    case LISA_STR_W:
    case LISA_STL_D:
    case LISA_STR_D:
    case LISA_STX_B:
    case LISA_STX_H:
    case LISA_STX_W:
    case LISA_STX_D:
    case LISA_STGT_B:
    case LISA_STGT_H:
    case LISA_STGT_W:
    case LISA_STGT_D:
    case LISA_STLE_B:
    case LISA_STLE_H:
    case LISA_STLE_W:
    case LISA_STLE_D: {
        for (int i = 0; i < ir2->op_count; i++) {
            if (ir2->_opnd[i]._type == IR2_OPND_GPR &&
                ir2->_opnd[i]._reg_num == reg_gpr_map[esp_index]) {
                return true;
            }
        }
        break;
    }
    case LISA_BSTRINS_W:
    case LISA_BSTRINS_D: {
        if ((ir2->_opnd[0]._type == IR2_OPND_GPR &&
            ir2->_opnd[0]._reg_num == reg_gpr_map[esp_index]) ||
            (ir2->_opnd[1]._type == IR2_OPND_GPR &&
            ir2->_opnd[1]._reg_num == reg_gpr_map[esp_index])) {
            return true;
        }
        break;
    }
    default:
        for (int i = 1; i < ir2->op_count; i++) {
            if (ir2->_opnd[i]._type == IR2_OPND_GPR &&
                ir2->_opnd[i]._reg_num == reg_gpr_map[esp_index]) {
                return true;
            }
        }
        break;
    }
    return false;
}

static IR2_INST *ir2_find_next_x86_inst(IR2_INST *start, int end_id)
{
    IR2_INST *curr = start;
    IR2_OPCODE op = ir2_opcode(curr);

    while (!la_ir2_opcode_is_x86_inst(op) && ir2_get_id(curr) < end_id) {
        curr = ir2_next(curr);
        op = ir2_opcode(curr);
    }

    return curr;
}

static void ir2_opt_push_pop(TranslationBlock *tb)
{
    if (PAGE_ANON & page_get_flags(tb->pc)) {
        return;
    }
    IR2_INST *curr;
    int curr_id, end_id, off, patch_off;
    //int branch_id = 0;
    bool insert;
    IR2_OPCODE op;

    IR2_OPND esp;
    ir2_opnd_build(&esp, IR2_OPND_GPR, reg_gpr_map[esp_index]);

    /* There is no need to analyze the last x86 inst or inst-pattern */
    curr = lsenv->tr_data->last_ir2;
    if (!curr) {
        return;
    }
    while (curr) {
        curr = ir2_prev(curr);
        if (!curr) {
            return;
        }
        op = ir2_opcode(curr);
        if (la_ir2_opcode_is_x86_inst(op)) {
            break;
        }
    }
    end_id = ir2_get_id(curr);

    /* get start (skip the first LISA_X86_INST) */
    curr = ir2_next(lsenv->tr_data->first_ir2);
    curr_id = ir2_get_id(curr);

    qemu_log_mask(LAT_IR2_SCHED, "\n[LAT_PUSH] TB = 0x" TARGET_FMT_lx
                " end_id %d\n", tb->pc, end_id);
    /* optimize */
    patch_off = 0;
    while (curr_id < end_id) {
        op = ir2_opcode(curr);
        insert = false;

        if ((ir2_opcode_is_branch(op) || ir2_opcode_is_jirl(op) ||
            la_ir2_opcode_is_label(op)) && patch_off) {
            curr = ir2_insert_before(generate_addi_d(esp, esp, patch_off), curr_id);
            patch_off = 0;
            curr = ir2_find_next_x86_inst(curr, end_id);
            curr_id = ir2_get_id(curr);
            qemu_log_mask(LAT_IR2_SCHED, "[LAT_PUSH] [insert] branch id = %-3d "
                        "patch_off = %-5d\n", curr_id, patch_off);
            continue;
        }

        if (la_ir2_opcode_is_addi(op) && (off = ir2_get_addi_rsp_offs(curr))) {
            if (!si12_overflow(patch_off + off)) {
                patch_off += off;
                ir2_remove(curr_id);
                qemu_log_mask(LAT_IR2_SCHED, "[LAT_PUSH] [remove] id = %-3d "
                        "patch_off = %-5d off = %-5d\n", curr_id, patch_off, off);
            } else {
                qemu_log_mask(LAT_IR2_SCHED, "[LAT_PUSH] [ flow ] id = %-3d "
                        "patch_off = %-5d off = %-5d\n", curr_id, patch_off, off);
            }
        } else if (patch_off) {
            if (ir2_src_contain_rsp(curr)) {
                if (la_ir2_can_patch_imm12(op) && ir2_opndn_is_rsp(curr, 1) &&
                    !si12_overflow((int)(curr->_opnd[2]._imm32) + patch_off)) {
                    if (ir2_opndn_is_rsp(curr, 0)) {
                        insert = true;
                    } else {
                        curr->_opnd[2]._imm32 += patch_off;
                    }
                } else {
                    insert = true;
                }
            } else if (ir2_opndn_is_rsp(curr, 0)) {
                patch_off = 0;
            }
        }

        if (insert) {
            curr = ir2_insert_before(generate_addi_d(esp, esp, patch_off), curr_id);
            patch_off = 0;
            qemu_log_mask(LAT_IR2_SCHED, "[LAT_PUSH] [insert] id = %-3d "
                        "patch_off = %-5d\n", curr_id, patch_off);
        }
        curr = ir2_next(curr);
        curr_id = ir2_get_id(curr);
    }

    if (patch_off) {
        ir2_insert_before(generate_addi_d(esp, esp, patch_off), curr_id);
        qemu_log_mask(LAT_IR2_SCHED, "[LAT_PUSH] [insert] id = %-3d "
                    "patch_off = %-5d\n", curr_id, 0);
    }
}

void tr_ir2_optimize(TranslationBlock *tb)
{
#ifdef CONFIG_LATX_OPT_PUSH_POP
    CPUX86State *env = (CPUX86State*)lsenv->cpu_state;
    CPUState *cpu = env_cpu(env);
    uint32_t parallel = cpu->tcg_cflags & CF_PARALLEL;
    if (!parallel) {
        ir2_opt_push_pop(tb);
    }
#endif
    /* temporarily disabled to prevent reschedule ldc1 before dectop/inctop
    if (0) {
        ir2_schedule(lsenv->tr_data->curr_tb);
        tri_avoid_last_ir2_is_ldst_not_rl();
        tri_separate_branch_from_ldst(lsenv->tr_data->curr_tb);
        tri_avoid_leading_label();
    } */
}

extern void ir1_optimization(TranslationBlock *tb);

void ir2_opt_push_pop_fix(TranslationBlock *tb, CPUState *cpu, int i)
{
    if (PAGE_ANON & page_get_flags(tb->pc)) {
        return;
    }
    TranslationBlock *ntb = tcg_tb_alloc(tcg_ctx);
    memcpy(ntb, tb, sizeof(TranslationBlock));

    tr_disasm(ntb, TCG_MAX_INSNS);
    ir1_optimization(ntb);
    tr_init(ntb);
    tr_ir2_generate(ntb);

    IR2_INST *curr;
    int curr_id, end_id, off, patch_off, last_id;
    bool insert;
    IR2_OPCODE op;
    IR2_OPND esp;
    ir2_opnd_build(&esp, IR2_OPND_GPR, reg_gpr_map[esp_index]);

    /* There is no need to analyze the last x86 inst or inst-pattern */
    curr = lsenv->tr_data->last_ir2;
    if(!curr){
        return;
    }
    while (curr) {
        curr = ir2_prev(curr);
        if(!curr){
            return;
        }
        op = ir2_opcode(curr);
        if (la_ir2_opcode_is_x86_inst(op)) {
            last_id = ir2_get_id(curr);
            break;
        }
    }

    if (ir2_get_id(lsenv->tr_data->first_ir2) == last_id) {
        return;
    }

    end_id = i;

    /* get start (skip the first LISA_X86_INST) */
    curr = ir2_next(lsenv->tr_data->first_ir2);
    curr_id = ir2_get_id(curr);
    end_id++;

    /* optimize */
    patch_off = 0;
    int skip_count = 0;
    qemu_log_mask(LAT_IR2_SCHED, "\n[LAT_PUSH_FIX] TB = 0x" TARGET_FMT_lx
                " end_id %d\n", tb->pc, end_id);
    while (curr_id <= end_id) {
        op = ir2_opcode(curr);

        switch (op) {
            case LISA_X86_INST:
            case LISA_LABEL:
            case LISA_DATA_LI:
            case LISA_DATA_ADD:
            case LISA_INST_DIFF:
            case LISA_DATA_ST:
            case LISA_DATA_ST_REL_TABLE:
            case LISA_PROFILE:
                skip_count++;
                end_id++;
                qemu_log_mask(LAT_IR2_SCHED,
                    "\n[LAT_PUSH_FIX] op %d curr_id %d end_id %d skip_count %d\n",
                    op, curr_id, end_id, skip_count);
                break;
            case LISA_ALIGN: {
                if (!((curr_id - skip_count) & 0x1)) {
                    skip_count++;
                    end_id++;
                    qemu_log_mask(LAT_IR2_SCHED,
                        "\n[LAT_PUSH_FIX] op %d curr_id %d end_id %d skip_count %d\n",
                        op, curr_id, end_id, skip_count);
                }
                break;
            }
            case LISA_FAR_JUMP:
                end_id--;
                qemu_log_mask(LAT_IR2_SCHED,
                    "\n[LAT_PUSH_FIX] op %d curr_id %d end_id %d\n",
                    op, curr_id, end_id);
                break;
            case LISA_MOV64:
            case LISA_MOV32_SX:
            case LISA_MOV32_ZX:
            case LISA_ADD:
            case LISA_SUB:
            case LISA_ADDI_ADDRX:
            case LISA_LOAD_ADDRX:
            case LISA_STORE_ADDRX:
            case LISA_NOP:
            case LISA_CODE:
            default:
                qemu_log_mask(LAT_IR2_SCHED, "\n[LAT_PUSH_FIX] op %d curr_id %d\n",
                    op, curr_id);
                break;
        }

        insert = false;

        if ((ir2_opcode_is_branch(op) || ir2_opcode_is_jirl(op) ||
            la_ir2_opcode_is_label(op)) && patch_off) {
            patch_off = 0;
            end_id--;
            curr = ir2_next(curr);
            curr_id = ir2_get_id(curr);
            qemu_log_mask(LAT_IR2_SCHED, "[LAT_PUSH_FIX] [insert] branch id = %-3d "
                        "patch_off = %-5d curr_id %d end_id %d\n",
                        curr_id, patch_off, curr_id, end_id);
            continue;
        }

        if (la_ir2_opcode_is_addi(op) && (off = ir2_get_addi_rsp_offs(curr))) {
            if (!si12_overflow(patch_off + off)) {
                patch_off += off;
                end_id++;
                qemu_log_mask(LAT_IR2_SCHED, "[LAT_PUSH_FIX] [remove] id = %-3d "
                        "patch_off = %-5d off = %-5d curr_id %d end_id %d\n",
                        curr_id, patch_off, off, curr_id, end_id);
            }
        } else if (patch_off) {
            if (ir2_src_contain_rsp(curr)) {
                if (la_ir2_can_patch_imm12(op) &&
                    !si12_overflow((int)(curr->_opnd[2]._imm32) + patch_off)) {
                    curr->_opnd[2]._imm32 += patch_off;
                    if (ir2_opndn_is_rsp(curr, 0)) {
                        patch_off = 0;
                    }
                } else {
                    insert = true;
                }
            } else if (ir2_opndn_is_rsp(curr, 0)) {
                patch_off = 0;
            }
        }

        if (insert) {
            patch_off = 0;
            end_id--;
            qemu_log_mask(LAT_IR2_SCHED, "[LAT_PUSH_FIX] [insert] id = %-3d "
                        "patch_off = %-5d curr_id %d end_id %d\n",
                        curr_id, patch_off, curr_id, end_id);
        }

        curr = ir2_next(curr);
        curr_id = ir2_get_id(curr);
    }

    CPUArchState *env = cpu->env_ptr;
    env->regs[R_ESP] += patch_off;
    tr_fini(false);
}
