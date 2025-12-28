#include "common.h"
#include "reg-alloc.h"
#include "flag-lbt.h"
#include "latx-options.h"
#include "translate.h"
#include "runtime-trace.h"
#include "linux-user/qemu.h"
#include <string.h>
#include <stdlib.h>
#include "ir1.h"
#include "lsenv.h"

#define TRACE_REG_STACK_OFFSET 0x10

size_t trace_save_regs[] = {
   /* helper inside could destroy a0-a7
    * and t0-t8, don't save s0-s8, because they are callee save*/
   la_a0, la_a1, la_a2, la_a3,
   la_a4, la_a5, la_a6, la_a7,
   la_t0, la_t1, la_t2, la_t3, la_t4,
   la_t5, la_t6, la_t7, la_t8,
   la_s0, la_s1, la_s2, la_s3, la_s4,
   la_s5, la_s6, la_s7, la_s8,
};

__thread size_t last_x86_info_size;
__thread struct store_x86_infos last_x86_info;

static void diff_printf(const char *format, uint64_t current, uint64_t last)
{
    if (current != last) {
        /* red highlight */
        /* printf("\033[1;31m"); */
        printf(format, current);
        /* printf("\033[0m"); */
    } else {
        /* printf(format, current); */
    }
}

static void print_disass(const uint8_t *bin, size_t length)
{
    cs_insn *insn;
    IR1_INST ir1;
#ifdef TARGET_X86_64
#ifdef CONFIG_LATX_CAPSTONE_GIT
    int count = latx_cs_disasm(handle[CODEIS64], bin, length,
        (uint64_t)0, 1, &insn, 1, &ir1);
#else
    int count = la_cs_disasm(handle[CODEIS64], bin, length,
        (uint64_t)0, 1, &insn, 1, &ir1);
#endif
#else
#ifdef CONFIG_LATX_CAPSTONE_GIT
    int count = latx_cs_disasm(handle[CODEIS64], bin, length,
        (uint64_t)0, 1, &insn, 1, &ir1);
#else
    int count = la_cs_disasm(handle[CODEIS64], bin, length,
        (uint64_t)0, 1, &insn, 1, &ir1);
#endif
#endif
    if (count) {
        printf("[");
        for (int i = 0; i < length; i++) {
            printf(" %02x", bin[i]);
        }
        printf("]\n %s\t%s\n", insn[0].mnemonic, insn[0].op_str);
    }
}

static bool is_branch(const uint8_t *bin, size_t length)
{
    cs_insn *insn;
    IR1_INST ir1 = {0};
#ifdef TARGET_X86_64
#ifdef CONFIG_LATX_CAPSTONE_GIT
    int count = latx_cs_disasm(handle[CODEIS64], bin, length,
        (uint64_t)0, 1, &insn, 1, &ir1);
#else
    int count = la_cs_disasm(handle[CODEIS64], bin, length,
        (uint64_t)0, 1, &insn, 1, &ir1);
#endif
#else
#ifdef CONFIG_LATX_CAPSTONE_GIT
    int count = latx_cs_disasm(handle[CODEIS64], bin, length,
        (uint64_t)0, 1, &insn, 1, &ir1);
#else
    int count = la_cs_disasm(handle[CODEIS64], bin, length,
        (uint64_t)0, 1, &insn, 1, &ir1);
#endif
#endif
    if (count && ir1.info && (ir1_is_jump(&ir1) || ir1_is_branch(&ir1)
        || ir1_is_call(&ir1) || ir1_is_return(&ir1))) {
        return true;
    }
    return false;
}

static void dump_store_x86_infos(struct store_x86_infos *x86_info)
{
    assert(x86_info->ins_size);
    /* this is last diff */
    diff_printf(" r8 :   0x%016lx ", x86_info->r8,    last_x86_info.r8);
    diff_printf(" r9 :   0x%016lx ", x86_info->r9,    last_x86_info.r9);
    diff_printf(" r10:   0x%016lx ", x86_info->r10,   last_x86_info.r10);
    diff_printf(" r11:   0x%016lx ""\n", x86_info->r11, last_x86_info.r11);
    diff_printf(" r12:   0x%016lx ", x86_info->r12,   last_x86_info.r12);
    diff_printf(" r13:   0x%016lx ", x86_info->r13,   last_x86_info.r13);
    diff_printf(" r14:   0x%016lx ", x86_info->r14,   last_x86_info.r14);
    diff_printf(" r15:   0x%016lx ""\n", x86_info->r15, last_x86_info.r15);
    diff_printf(" eax:   0x%016lx ", x86_info->eax,   last_x86_info.eax);
    diff_printf(" ecx:   0x%016lx ", x86_info->ecx,   last_x86_info.ecx);
    diff_printf(" edx:   0x%016lx ", x86_info->edx,   last_x86_info.edx);
    diff_printf(" ebx:   0x%016lx ""\n", x86_info->ebx, last_x86_info.ebx);
    diff_printf(" esp:   0x%016lx ", x86_info->esp,   last_x86_info.esp);
    diff_printf(" ebp:   0x%016lx ", x86_info->ebp,   last_x86_info.ebp);
    diff_printf(" esi:   0x%016lx ", x86_info->esi,   last_x86_info.esi);
    diff_printf(" edi:   0x%016lx ""\n", x86_info->edi, last_x86_info.edi);
    diff_printf(" xmm0_l:0x%016lx", x86_info->xmm0[0], last_x86_info.xmm0[0]);
    diff_printf("  xmm0_h:0x%016lx ",  x86_info->xmm0[1], last_x86_info.xmm0[1]);
    diff_printf(" xmm1_l:0x%016lx", x86_info->xmm1[0], last_x86_info.xmm1[0]);
    diff_printf("  xmm1_h:0x%016lx\n", x86_info->xmm1[1], last_x86_info.xmm1[1]);
    diff_printf(" xmm2_l:0x%016lx", x86_info->xmm2[0] , last_x86_info.xmm2[0]);
    diff_printf("  xmm2_h:0x%016lx ",  x86_info->xmm2[1], last_x86_info.xmm2[1]);
    diff_printf(" xmm3_l:0x%016lx", x86_info->xmm3[0], last_x86_info.xmm3[0]);
    diff_printf("  xmm3_h:0x%016lx\n", x86_info->xmm3[1], last_x86_info.xmm3[1]);
    diff_printf(" xmm4_l:0x%016lx", x86_info->xmm4[0] , last_x86_info.xmm4[0]);
    diff_printf("  xmm4_h:0x%016lx ",  x86_info->xmm4[1], last_x86_info.xmm4[1]);
    diff_printf(" xmm5_l:0x%016lx", x86_info->xmm5[0], last_x86_info.xmm5[0]);
    diff_printf("  xmm5_h:0x%016lx" "\n", x86_info->xmm5[1], last_x86_info.xmm5[1]);
    diff_printf(" xmm6_l:0x%016lx", x86_info->xmm6[0] , last_x86_info.xmm6[0]);
    diff_printf("  xmm6_h:0x%016lx ",  x86_info->xmm6[1], last_x86_info.xmm6[1]);
    diff_printf(" xmm7_l:0x%016lx", x86_info->xmm7[0], last_x86_info.xmm7[0]);
    diff_printf("  xmm7_h:0x%016lx" "\n", x86_info->xmm7[1], last_x86_info.xmm7[1]);
    diff_printf(" mm0:   0x%016lx ",  x86_info->mm0, last_x86_info.mm0);
    diff_printf(" mm1:   0x%016lx ",  x86_info->mm1, last_x86_info.mm1);
    diff_printf(" mm2:   0x%016lx ",  x86_info->mm2, last_x86_info.mm2);
    diff_printf(" mm3:   0x%016lx\n", x86_info->mm3, last_x86_info.mm3);
    diff_printf(" mm4:   0x%016lx ",  x86_info->mm4, last_x86_info.mm4);
    diff_printf(" mm5:   0x%016lx ",  x86_info->mm5, last_x86_info.mm5);
    diff_printf(" mm6:   0x%016lx ",  x86_info->mm6, last_x86_info.mm6);
    diff_printf(" mm7:   0x%016lx\n", x86_info->mm7, last_x86_info.mm7);
    diff_printf(" eflags:0x%lx" "\n",   x86_info->eflag, last_x86_info.eflag);
    printf("\n--------------------------------------------------------------------\n");
    if (is_branch((unsigned char *)x86_info->ins_buf, x86_info->ins_size)) {
        return;
    }
    printf(" eip:   0x%08lx: ins_size %lx", x86_info->eip, x86_info->ins_size);
    print_disass((unsigned char *)x86_info->ins_buf, x86_info->ins_size);
}

static void gen_store_x86_infos(
    uint64_t *regs, struct store_x86_infos *x86_info,
    uint64_t mem_access_count, uint64_t ins_length)
{
    x86_info->r8  = regs[la_a2_idx];
    x86_info->r9  = regs[la_a3_idx];
    x86_info->r10 = regs[la_a4_idx];
    x86_info->r11 = regs[la_a5_idx];
    x86_info->r12 = regs[la_t5_idx];
    x86_info->r13 = regs[la_t6_idx];
    x86_info->r14 = regs[la_t7_idx];
    x86_info->r15 = regs[la_t8_idx];
    x86_info->eax = regs[la_s0_idx];
    x86_info->ecx = regs[la_s1_idx];
    x86_info->edx = regs[la_s2_idx];
    x86_info->ebx = regs[la_s3_idx];
    x86_info->esp = regs[la_s4_idx];
    x86_info->ebp = regs[la_s5_idx];
    x86_info->esi = regs[la_s6_idx];
    x86_info->edi = regs[la_s7_idx];
    x86_info->xmm0[0] = regs[la_xmm0_l_idx];
    x86_info->xmm0[1] = regs[la_xmm0_h_idx];
    x86_info->xmm1[0] = regs[la_xmm1_l_idx];
    x86_info->xmm1[1] = regs[la_xmm1_h_idx];
    x86_info->xmm2[0] = regs[la_xmm2_l_idx];
    x86_info->xmm2[1] = regs[la_xmm2_h_idx];
    x86_info->xmm3[0] = regs[la_xmm3_l_idx];
    x86_info->xmm3[1] = regs[la_xmm3_h_idx];
    x86_info->xmm4[0] = regs[la_xmm4_l_idx];
    x86_info->xmm4[1] = regs[la_xmm4_h_idx];
    x86_info->xmm5[0] = regs[la_xmm5_l_idx];
    x86_info->xmm5[1] = regs[la_xmm5_h_idx];
    x86_info->xmm6[0] = regs[la_xmm6_l_idx];
    x86_info->xmm6[1] = regs[la_xmm6_h_idx];
    x86_info->xmm7[0] = regs[la_xmm7_l_idx];
    x86_info->xmm7[1] = regs[la_xmm7_h_idx];
    x86_info->mm0 = regs[la_mm0_idx];
    x86_info->mm1 = regs[la_mm1_idx];
    x86_info->mm2 = regs[la_mm2_idx];
    x86_info->mm3 = regs[la_mm3_idx];
    x86_info->mm4 = regs[la_mm4_idx];
    x86_info->mm5 = regs[la_mm5_idx];
    x86_info->mm6 = regs[la_mm6_idx];
    x86_info->mm7 = regs[la_mm7_idx];
    x86_info->eflag = regs[la_eflag_idx];
    x86_info->ins = regs[la_ins_idx];
    x86_info->ins_size = ins_length;
    memcpy(x86_info->ins_buf, (void *)x86_info->eip, ins_length);
    x86_info->ins_buf[1] = regs[la_ins_buf_hi_idx];
    x86_info->opnd_size = mem_access_count;
}

static void trace_session(uint64_t ins_length, uint64_t reg_store_addr,
    uint64_t eip, uint64_t mem_access_count, uint64_t begin)
{
    uint64_t *regs = (uint64_t *)(reg_store_addr + TRACE_REG_STACK_OFFSET);
    size_t buf_size =  sizeof(struct store_x86_infos);
    struct store_x86_infos *x86_info = (struct store_x86_infos *)malloc(buf_size);
    x86_info->eip = eip;
    x86_info->ctx_type = begin;
    gen_store_x86_infos(regs, x86_info, mem_access_count, ins_length);
    dump_store_x86_infos(x86_info);
    memcpy(&last_x86_info, x86_info, buf_size);
    free(x86_info);
}

void trace_session_begin(uint64_t ins_length, uint64_t reg_store_addr,
    uint64_t eip, uint64_t mem_access_count)
{
    trace_session(ins_length, reg_store_addr, eip, mem_access_count, CONTEXT_IN);
}

static void gen_trace_helper(ADDR helper_method, IR1_INST *pir1)
{
    int i = 0;
#ifdef TARGET_X86_64
    /* because we reuse x86 curr_ir1_inst info when gen call helper code
     * consider when we trace a 32bit addr size x86_64 instr, ST_D will gen
     * code as 32bit */
    uint32_t org_x86_addr_size =
         lsenv->tr_data->curr_ir1_inst->info->x86.addr_size;
    lsenv->tr_data->curr_ir1_inst->info->x86.addr_size = 64 >> 3;
#endif
    /* 不知道此处 sp_ir2_opnd 表示 host 还是 guest，如有使用请小心 */
    la_addi_d(sp_ir2_opnd, sp_ir2_opnd, -0x300);
    for (i = 0; i < sizeof(trace_save_regs) / sizeof(trace_save_regs[0]); i++) {
        la_st_d(ir2_opnd_new(IR2_OPND_GPR, trace_save_regs[i]),
                             sp_ir2_opnd, TRACE_REG_STACK_OFFSET + i * 8);
    }
    for (int j = 0; j < 8; j++) {
        la_vst(ra_alloc_xmm(j), sp_ir2_opnd,
                             TRACE_REG_STACK_OFFSET + i * 8);
        i += 2;
    }

    /* current lsfpu default enable */
    for (int j = 0; j < 8; j++) {
        la_fst_d(ra_alloc_mmx(j),
                             sp_ir2_opnd, TRACE_REG_STACK_OFFSET + i * 8);
        i += 1;
    }

    uint64_t mem_access_count = 0;
    for (int j = 0; j < pir1->info->x86.op_count; j++) {
        IR1_OPND *opnd = ir1_get_opnd(pir1, j);
        if (ir1_opnd_is_mem(opnd)) {
            mem_access_count++;
        }
    }
    /* store eflags as last reg */
    {
        IR2_OPND eflags_opnd = ra_alloc_eflags();
        IR2_OPND eflags_temp = ir2_opnd_new(IR2_OPND_GPR, 11);
        la_x86mfflag(eflags_temp, 0x3f);
        la_or(eflags_opnd, eflags_opnd, eflags_temp);
        la_st_d(eflags_opnd, sp_ir2_opnd,
                             TRACE_REG_STACK_OFFSET + i * 8);
        i += 1;
    }

    /* this is ins */
    {
       IR2_OPND tmp_opnd1 = ir2_opnd_new(IR2_OPND_GPR, 11);
       li_d(tmp_opnd1, (ADDR)pir1->info->x86.opcode);
       la_st_d(tmp_opnd1, sp_ir2_opnd,
                            TRACE_REG_STACK_OFFSET + i * 8);
       i += 1;
    }
    /* this is ins_len and ins_buf[2] */
    i += 3;

    /* set la_a0 by inst size,
     * set la_a1 sp value to indicate stored register value
     * set la_a2 by eip == pir1->info-address */
    IR2_OPND a0_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a0);
    IR2_OPND a1_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a1);
    IR2_OPND a2_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a2);
    IR2_OPND a3_opnd = ir2_opnd_new(IR2_OPND_GPR, la_a3);
    la_andi(a0_opnd, a0_opnd, 0);
    la_ori(a0_opnd, a0_opnd, pir1->info->size);
    la_andi(a1_opnd, a1_opnd, 0);
    la_or(a1_opnd, a1_opnd, sp_ir2_opnd);
    la_andi(a2_opnd, a2_opnd, 0);
    li_d(a2_opnd, (ADDR)pir1->info->address);
    li_d(a3_opnd, (ADDR)mem_access_count);

    tr_gen_call_to_helper(helper_method, LOAD_HELPER_TRACE_SESSION_BEGIN);
    for (i = 0; i < sizeof(trace_save_regs) / sizeof(trace_save_regs[0]); i++) {
        la_ld_d(ir2_opnd_new(IR2_OPND_GPR, trace_save_regs[i]),
            sp_ir2_opnd, TRACE_REG_STACK_OFFSET + i * 8);
    }
    /* xmm is 128bit */
    for (int j = 0; j < 8; j++) {
        la_vld(ra_alloc_xmm(j), sp_ir2_opnd,
                             TRACE_REG_STACK_OFFSET + i * 8);
        i += 2;
    }
    /* mmx is 128bit
     * current we ignore float because limit to ins size*/
    for (int j = 0; j < 8; j++) {
        la_fld_d(ir2_opnd_new(IR2_OPND_FPR, j),
                             sp_ir2_opnd, TRACE_REG_STACK_OFFSET + i * 16);
        i++;
    }
    la_addi_d(sp_ir2_opnd, sp_ir2_opnd, 0x300);
    ra_free_all();
#ifdef TARGET_X86_64
    if(CODEIS64) {
        lsenv->tr_data->curr_ir1_inst->info->x86.addr_size = org_x86_addr_size;
    }
#endif
}

void gen_ins_context_in_helper(IR1_INST *pir1)
{
    gen_trace_helper((ADDR)trace_session_begin, pir1);
}
