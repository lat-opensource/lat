#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "ir1.h"
#include "latx-config.h"
#include "latx-options.h"
#include "latx-disassemble-trace.h"
#include "lsenv.h"

IR1_OPND al_ir1_opnd;
IR1_OPND ah_ir1_opnd;
IR1_OPND ax_ir1_opnd;
IR1_OPND eax_ir1_opnd;
IR1_OPND rax_ir1_opnd;

IR1_OPND cl_ir1_opnd;
IR1_OPND ch_ir1_opnd;
IR1_OPND cx_ir1_opnd;
IR1_OPND ecx_ir1_opnd;
IR1_OPND rcx_ir1_opnd;

IR1_OPND dl_ir1_opnd;
IR1_OPND dh_ir1_opnd;
IR1_OPND dx_ir1_opnd;
IR1_OPND edx_ir1_opnd;
IR1_OPND rdx_ir1_opnd;

IR1_OPND bl_ir1_opnd;
IR1_OPND bh_ir1_opnd;
IR1_OPND bx_ir1_opnd;
IR1_OPND ebx_ir1_opnd;
IR1_OPND rbx_ir1_opnd;

IR1_OPND esp_ir1_opnd;
IR1_OPND rsp_ir1_opnd;

IR1_OPND ebp_ir1_opnd;
IR1_OPND rbp_ir1_opnd;

IR1_OPND si_ir1_opnd;
IR1_OPND esi_ir1_opnd;
IR1_OPND rsi_ir1_opnd;

IR1_OPND di_ir1_opnd;
IR1_OPND edi_ir1_opnd;
IR1_OPND rdi_ir1_opnd;

IR1_OPND eax_mem8_ir1_opnd;
IR1_OPND ecx_mem8_ir1_opnd;
IR1_OPND edx_mem8_ir1_opnd;
IR1_OPND ebx_mem8_ir1_opnd;
IR1_OPND esp_mem8_ir1_opnd;
IR1_OPND ebp_mem8_ir1_opnd;
IR1_OPND esi_mem8_ir1_opnd;
IR1_OPND edi_mem8_ir1_opnd;
IR1_OPND di_mem8_ir1_opnd;
IR1_OPND si_mem8_ir1_opnd;

IR1_OPND eax_mem16_ir1_opnd;
IR1_OPND ecx_mem16_ir1_opnd;
IR1_OPND edx_mem16_ir1_opnd;
IR1_OPND ebx_mem16_ir1_opnd;
IR1_OPND esp_mem16_ir1_opnd;
IR1_OPND ebp_mem16_ir1_opnd;
IR1_OPND esi_mem16_ir1_opnd;
IR1_OPND edi_mem16_ir1_opnd;

IR1_OPND eax_mem32_ir1_opnd;
IR1_OPND ecx_mem32_ir1_opnd;
IR1_OPND edx_mem32_ir1_opnd;
IR1_OPND ebx_mem32_ir1_opnd;
IR1_OPND esp_mem32_ir1_opnd;
IR1_OPND esp_mem64_ir1_opnd;
IR1_OPND ebp_mem32_ir1_opnd;
IR1_OPND esi_mem32_ir1_opnd;
IR1_OPND edi_mem32_ir1_opnd;
IR1_OPND si_mem16_ir1_opnd;
IR1_OPND di_mem16_ir1_opnd;
IR1_OPND si_mem32_ir1_opnd;
IR1_OPND di_mem32_ir1_opnd;

#ifdef TARGET_X86_64
IR1_OPND rax_mem64_ir1_opnd;
IR1_OPND rcx_mem64_ir1_opnd;
IR1_OPND rdx_mem64_ir1_opnd;
IR1_OPND rbx_mem64_ir1_opnd;
IR1_OPND rsp_mem64_ir1_opnd;
IR1_OPND rbp_mem64_ir1_opnd;
IR1_OPND rsi_mem64_ir1_opnd;
IR1_OPND rdi_mem64_ir1_opnd;
IR1_OPND rsi_mem8_ir1_opnd;
IR1_OPND rdi_mem8_ir1_opnd;
IR1_OPND rsi_mem32_ir1_opnd;
IR1_OPND rdi_mem32_ir1_opnd;
IR1_OPND rsi_mem16_ir1_opnd;
IR1_OPND rdi_mem16_ir1_opnd;
IR1_OPND esi_mem64_ir1_opnd;
IR1_OPND edi_mem64_ir1_opnd;
extern int GPR_USEDEF_TO_SAVE;
extern int FPR_USEDEF_TO_SAVE;
extern int XMM_USEDEF_TO_SAVE;
#endif
csh handle[2];

#ifdef CONFIG_LATX_CAPSTONE_GIT
int (*la_disa_v1)(const uint8_t *code, size_t code_size,
        uint64_t address,
        size_t count, struct la_dt_insn **insn,
        //int ir1_num, void *pir1_base) = &nextcapstone_get;
        int ir1_num, void *pir1_base, int mode) = &gitcapstone_get;
#else
int (*la_disa_v1)(const uint8_t *code, size_t code_size,
        uint64_t address,
        size_t count, struct la_dt_insn **insn,
        int ir1_num, void *pir1_base, int mode) = &lacapstone_get;
#endif
void (*disassemble_trace_cmp)(const uint8_t *code, size_t code_size,
        uint64_t address,
        size_t count,
        struct la_dt_insn *inputinsn, int mode) = disassemble_trace_cmp_nop;
static IR1_OPND ir1_opnd_new_static_reg(IR1_OPND_TYPE opnd_type, int size,
                                        dt_x86_reg reg)
{
    IR1_OPND ir1_opnd;
    IR1_OPND *opnd = &ir1_opnd;

    opnd->type = opnd_type;
    lsassert(size % 8 == 0);
    opnd->size = size / 8;
    opnd->reg = reg;
    opnd->avx_bcast = dt_X86_AVX_BCAST_INVALID;

    return ir1_opnd;
}

static IR1_OPND ir1_opnd_new_static_mem(IR1_OPND_TYPE opnd_type, int size,
                                        dt_x86_reg reg, uint64_t imm)
{
    switch (reg) {
    case dt_X86_REG_EAX:
    case dt_X86_REG_ECX:
    case dt_X86_REG_EBX:
    case dt_X86_REG_EDX:
    case dt_X86_REG_ESI:
    case dt_X86_REG_EDI:
    case dt_X86_REG_ESP:
    case dt_X86_REG_EBP:
    case dt_X86_REG_SI:
    case dt_X86_REG_DI:
#ifdef TARGET_X86_64
    case dt_X86_REG_RAX:
    case dt_X86_REG_RBX:
    case dt_X86_REG_RCX:
    case dt_X86_REG_RDX:
    case dt_X86_REG_RBP:
    case dt_X86_REG_RSP:
    case dt_X86_REG_RDI:
    case dt_X86_REG_RSI:
#endif
        break;
    default:
        lsassert(0);
    }

    IR1_OPND ir1_opnd;
    IR1_OPND *opnd = &ir1_opnd;

    opnd->type = opnd_type;
    lsassert(size % 8 == 0);
    opnd->size = size / 8;
    opnd->size = size / 8;
    opnd->mem.base = reg;

    opnd->mem.index = dt_X86_REG_INVALID;
    opnd->mem.segment = dt_X86_REG_INVALID;
    opnd->mem.scale = 0;
    opnd->mem.disp = imm;

    opnd->avx_bcast = dt_X86_AVX_BCAST_INVALID;

    return ir1_opnd;
}
static void __attribute__((__constructor__)) x86tomisp_ir1_init(void)
{
    al_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 8, dt_X86_REG_AL);
    ah_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 8, dt_X86_REG_AH);
    ax_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 16, dt_X86_REG_AX);
    eax_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 32, dt_X86_REG_EAX);
    rax_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 64, dt_X86_REG_RAX);

    cl_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 8, dt_X86_REG_CL);
    ch_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 8, dt_X86_REG_CH);
    cx_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 16, dt_X86_REG_CX);
    ecx_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 32, dt_X86_REG_ECX);
    rcx_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 64, dt_X86_REG_RCX);

    dl_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 8, dt_X86_REG_DL);
    dh_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 8, dt_X86_REG_DH);
    dx_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 16, dt_X86_REG_DX);
    edx_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 32, dt_X86_REG_EDX);
    rdx_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 64, dt_X86_REG_RDX);

    bl_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 8, dt_X86_REG_BL);
    bh_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 8, dt_X86_REG_BH);
    bx_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 16, dt_X86_REG_BX);
    ebx_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 32, dt_X86_REG_EBX);
    rbx_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 64, dt_X86_REG_RBX);

    esp_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 32, dt_X86_REG_ESP);
    rsp_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 64, dt_X86_REG_RSP);

    ebp_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 32, dt_X86_REG_EBP);
    rbp_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 64, dt_X86_REG_RBP);

    si_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 16, dt_X86_REG_SI);
    esi_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 32, dt_X86_REG_ESI);
    rsi_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 64, dt_X86_REG_RSI);

    di_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 16, dt_X86_REG_DI);
    edi_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 32, dt_X86_REG_EDI);
    rdi_ir1_opnd = ir1_opnd_new_static_reg(dt_X86_OP_REG, 64, dt_X86_REG_RDI);

    eax_mem8_ir1_opnd = ir1_opnd_new_static_mem(dt_X86_OP_MEM,
        8, dt_X86_REG_EAX, 0);
    ecx_mem8_ir1_opnd = ir1_opnd_new_static_mem(dt_X86_OP_MEM,
        8, dt_X86_REG_ECX, 0);
    edx_mem8_ir1_opnd = ir1_opnd_new_static_mem(dt_X86_OP_MEM,
        8, dt_X86_REG_EDX, 0);
    ebx_mem8_ir1_opnd = ir1_opnd_new_static_mem(dt_X86_OP_MEM,
        8, dt_X86_REG_EBX, 0);
    esp_mem8_ir1_opnd = ir1_opnd_new_static_mem(dt_X86_OP_MEM,
        8, dt_X86_REG_ESP, 0);
    ebp_mem8_ir1_opnd = ir1_opnd_new_static_mem(dt_X86_OP_MEM,
        8, dt_X86_REG_EBP, 0);
    esi_mem8_ir1_opnd = ir1_opnd_new_static_mem(dt_X86_OP_MEM,
        8, dt_X86_REG_ESI, 0);
    edi_mem8_ir1_opnd = ir1_opnd_new_static_mem(dt_X86_OP_MEM,
        8, dt_X86_REG_EDI, 0);
    di_mem8_ir1_opnd = ir1_opnd_new_static_mem(dt_X86_OP_MEM,
        8, dt_X86_REG_DI, 0);
    si_mem8_ir1_opnd = ir1_opnd_new_static_mem(dt_X86_OP_MEM,
        8, dt_X86_REG_SI, 0);
    eax_mem16_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 16, dt_X86_REG_EAX, 0);
    ecx_mem16_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 16, dt_X86_REG_ECX, 0);
    edx_mem16_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 16, dt_X86_REG_EDX, 0);
    ebx_mem16_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 16, dt_X86_REG_EBX, 0);
    esp_mem16_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 16, dt_X86_REG_ESP, 0);
    ebp_mem16_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 16, dt_X86_REG_EBP, 0);
    esi_mem16_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 16, dt_X86_REG_ESI, 0);
    edi_mem16_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 16, dt_X86_REG_EDI, 0);

    eax_mem32_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 32, dt_X86_REG_EAX, 0);
    ecx_mem32_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 32, dt_X86_REG_ECX, 0);
    edx_mem32_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 32, dt_X86_REG_EDX, 0);
    ebx_mem32_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 32, dt_X86_REG_EBX, 0);
    esp_mem32_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 32, dt_X86_REG_ESP, 0);
    esp_mem64_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 64, dt_X86_REG_ESP, 0);
    ebp_mem32_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 32, dt_X86_REG_EBP, 0);
    esi_mem32_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 32, dt_X86_REG_ESI, 0);
    edi_mem32_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 32, dt_X86_REG_EDI, 0);
    si_mem16_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 16, dt_X86_REG_SI, 0);
    di_mem16_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 16, dt_X86_REG_DI, 0);
    si_mem32_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 32, dt_X86_REG_SI, 0);
    di_mem32_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 32, dt_X86_REG_DI, 0);
#ifdef TARGET_X86_64
    rax_mem64_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 64, dt_X86_REG_RAX, 0);
    rcx_mem64_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 64, dt_X86_REG_RCX, 0);
    rdx_mem64_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 64, dt_X86_REG_RDX, 0);
    rbx_mem64_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 64, dt_X86_REG_RBX, 0);
    rsp_mem64_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 64, dt_X86_REG_RSP, 0);
    rbp_mem64_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 64, dt_X86_REG_RBP, 0);
    rsi_mem64_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 64, dt_X86_REG_RSI, 0);
    rdi_mem64_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 64, dt_X86_REG_RDI, 0);
    rsi_mem8_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 8, dt_X86_REG_RSI, 0);
    rdi_mem8_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 8, dt_X86_REG_RDI, 0);
    rsi_mem16_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 16, dt_X86_REG_RSI, 0);
    rdi_mem16_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 16, dt_X86_REG_RDI, 0);
    rsi_mem32_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 32, dt_X86_REG_RSI, 0);
    rdi_mem32_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 32, dt_X86_REG_RDI, 0);
    esi_mem64_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 64, dt_X86_REG_ESI, 0);
    edi_mem64_ir1_opnd =
        ir1_opnd_new_static_mem(dt_X86_OP_MEM, 64, dt_X86_REG_EDI, 0);
#endif
};

ADDRX ir1_disasm(IR1_INST *ir1, uint8_t *addr, ADDRX t_pc, int ir1_num, void *pir1_base)
{
    struct la_dt_insn *info;
    uint32_t nop = 0x401f0f;
    uint64_t nop_5 = 0x441f0f;
    if (((*((uint32_t *)addr)) & 0xf8ffffff) == 0xc81e0ff3) {
        //repleace endbr32/rdsspd with 4 bytes nop, just a temporary solution
        addr = (uint8_t *)&nop;
    }

    if (((*((uint64_t *)addr)) & 0xfffffaff) == 0x1e0f48f3) {
        /* repleace rdsspq with 5 bytes nop, just a temporary solution */
        addr = (uint8_t *)&nop_5;
    }
    /* FIXME:the count parameter in cs_disasm is 1, it means we translte 1 insn at a time,
     * there should be a performance improvement if we increase the number, but
     * for now there are some problems if we change it. It will be settled later.
     */
    int count = la_disa_v1(addr, 15, (uint64_t)t_pc,
        1, &info, ir1_num, pir1_base, CODEIS64);

    ir1->info = info;
    ir1->cflag = 0;

    /* Invalid Insn */
    if (info == NULL) {
        return t_pc;
    }

    if (count != 1) {
        fprintf(stderr, "ERROR : disasm, ADDR : 0x%" PRIx64 "\n", (uint64_t)t_pc);
        exit(-1);
    }

    disassemble_trace_cmp(addr, 15, (uint64_t)t_pc, 1, info, CODEIS64);

    ir1->_eflag = 0;

    // xtm treat opnd with default seg(without segment-override prefix) as a mem
    // opnd so, we make it invalid
    if (info->x86.prefix[1] != dt_X86_PREFIX_CS &&
        info->x86.prefix[1] != dt_X86_PREFIX_DS &&
        info->x86.prefix[1] != dt_X86_PREFIX_SS &&
        info->x86.prefix[1] != dt_X86_PREFIX_ES &&
        info->x86.prefix[1] != dt_X86_PREFIX_FS &&
        info->x86.prefix[1] != dt_X86_PREFIX_GS) {
        for (int i = 0; i < info->x86.op_count; i++) {
            if (info->x86.operands[i].type == dt_X86_OP_MEM) {
                info->x86.operands[i].mem.segment = dt_X86_REG_INVALID;
            }
        }
    }

    return (ADDRX)(ir1->info->address + ir1->info->size);
}

void ir1_opnd_build_reg(IR1_OPND *opnd, int size, dt_x86_reg reg)
{
    opnd->type = dt_X86_OP_REG;
    lsassert(size % 8 == 0);
    opnd->size = size / 8;
    opnd->reg = reg;
    opnd->avx_bcast = dt_X86_AVX_BCAST_INVALID;
}

void ir1_opnd_build_imm(IR1_OPND *opnd, int size, int64_t imm)
{
    opnd->type = dt_X86_OP_IMM;
    lsassert(size % 8 == 0);
    opnd->size = size / 8;
    opnd->imm = imm;
    opnd->avx_bcast = dt_X86_AVX_BCAST_INVALID;
}

void ir1_opnd_build_mem(IR1_OPND *opnd, int size, dt_x86_reg base, int64_t disp)
{
    opnd->type = dt_X86_OP_MEM;
    lsassert(size % 8 == 0);
    opnd->size = size / 8;
    opnd->mem.base = base;
    opnd->mem.disp = disp;
    opnd->mem.index = dt_X86_REG_INVALID;
    opnd->mem.scale = 0;
    opnd->mem.segment = dt_X86_REG_INVALID;
    opnd->avx_bcast = dt_X86_AVX_BCAST_INVALID;
}

void ir1_opnd_check(IR1_OPND *opnd) { lsassert(0); }

IR1_OPND_TYPE ir1_opnd_type(IR1_OPND *opnd) { return opnd->type; }

int ir1_opnd_size(IR1_OPND *opnd) { return opnd->size << 3; }

int ir1_addr_size(IR1_INST *inst)
{
    return inst->info->x86.addr_size << 3;
}

int ir1_get_opnd_size(IR1_INST *inst)
{
    switch (ir1_opcode(inst)) {
    case dt_X86_INS_CALL:
    case dt_X86_INS_LCALL:
    case dt_X86_INS_RET:
    case dt_X86_INS_RETF:
    case dt_X86_INS_ENTER:
    case dt_X86_INS_LEAVE: {
#ifndef TARGET_X86_64
        if (ir1_prefix_opnd_size(inst) != 0) {
            return 2 << 3;
        } else {
            return 4 << 3;
        }
#else
        if (!CODEIS64) {
            if (ir1_prefix_opnd_size(inst) != 0) {
                return 2 << 3;
            } else {
                return 4 << 3;
            }
        }
        /* call/... will use only 64 bit opnd-size in AMD64 */
        return 8 << 3;
#endif
        break;
    }
    default:
        lsassertm(0, "Not alloc the function,"
                    "if you need, you can insert your process function in this part");
        break;
    }
    return 0;
}

int ir1_opnd_base_reg_bits_start(IR1_OPND *opnd)
{
    lsassert(opnd->type == dt_X86_OP_REG && opnd->reg > dt_X86_REG_INVALID &&
             opnd->reg < dt_X86_REG_ENDING);
    switch (opnd->reg) {
    case dt_X86_REG_AH:
    case dt_X86_REG_BH:
    case dt_X86_REG_CH:
    case dt_X86_REG_DH:
        return 8;
    default:
        return 0;
    }
}

dt_x86_reg ir1_opnd_index_reg(IR1_OPND *opnd) { return opnd->mem.index; }

dt_x86_reg ir1_opnd_base_reg(IR1_OPND *opnd) { return opnd->mem.base; }

int ir1_opnd_scale(IR1_OPND *opnd) { return opnd->mem.scale; }

int ir1_opnd_vsib_index_reg_num(IR1_OPND *opnd)
{
    dt_x86_reg index = opnd->mem.index;
    lsassert((dt_X86_REG_XMM0 <= index && index <= dt_X86_REG_XMM15) ||
             (dt_X86_REG_YMM0 <= index && index <= dt_X86_REG_YMM15));

    if(index <= dt_X86_REG_XMM15){
        return index - dt_X86_REG_XMM0;
    }
    return index - dt_X86_REG_YMM0;
}

int ir1_index_reg_is_ymm(IR1_OPND *opnd)
{
    lsassert(ir1_opnd_is_mem(opnd) && ir1_opnd_has_index(opnd));
    dt_x86_reg index = opnd->mem.index;

    return dt_X86_REG_YMM0 <= index && index <= dt_X86_REG_YMM15;
}


int ir1_opnd_index_reg_num(IR1_OPND *opnd)
{
    switch (opnd->mem.index) {
    case dt_X86_REG_AL:
    case dt_X86_REG_AH:
    case dt_X86_REG_AX:
    case dt_X86_REG_EAX:
#ifdef TARGET_X86_64
    case dt_X86_REG_RAX:
#endif
        return eax_index;
    case dt_X86_REG_BL:
    case dt_X86_REG_BH:
    case dt_X86_REG_BX:
    case dt_X86_REG_EBX:
#ifdef TARGET_X86_64
    case dt_X86_REG_RBX:
#endif
        return ebx_index;
    case dt_X86_REG_CL:
    case dt_X86_REG_CH:
    case dt_X86_REG_CX:
    case dt_X86_REG_ECX:
#ifdef TARGET_X86_64
    case dt_X86_REG_RCX:
#endif
        return ecx_index;
    case dt_X86_REG_DL:
    case dt_X86_REG_DH:
    case dt_X86_REG_DX:
    case dt_X86_REG_EDX:
#ifdef TARGET_X86_64
    case dt_X86_REG_RDX:
#endif
        return edx_index;
    case dt_X86_REG_BP:
    case dt_X86_REG_EBP:
#ifdef TARGET_X86_64
    case dt_X86_REG_RBP:
    case dt_X86_REG_BPL:
#endif
        return ebp_index;
    case dt_X86_REG_SI:
    case dt_X86_REG_ESI:
#ifdef TARGET_X86_64
    case dt_X86_REG_RSI:
    case dt_X86_REG_SIL:
#endif
        return esi_index;
    case dt_X86_REG_DI:
    case dt_X86_REG_EDI:
#ifdef TARGET_X86_64
    case dt_X86_REG_RDI:
    case dt_X86_REG_DIL:
#endif
        return edi_index;
    case dt_X86_REG_SP:
    case dt_X86_REG_ESP:
#ifdef TARGET_X86_64
    case dt_X86_REG_RSP:
    case dt_X86_REG_SPL:
#endif
        return esp_index;
    case dt_X86_REG_CS:
        return cs_index;
    case dt_X86_REG_DS:
        return ds_index;
    case dt_X86_REG_SS:
        return ss_index;
    case dt_X86_REG_ES:
        return es_index;
    case dt_X86_REG_GS:
        return gs_index;
    case dt_X86_REG_FS:
        return fs_index;
#ifdef TARGET_X86_64
    case dt_X86_REG_R8B:
    case dt_X86_REG_R8W:
    case dt_X86_REG_R8D:
    case dt_X86_REG_R8:
        return r8_index;
    case dt_X86_REG_R9B:
    case dt_X86_REG_R9W:
    case dt_X86_REG_R9D:
    case dt_X86_REG_R9:
        return r9_index;
    case dt_X86_REG_R10B:
    case dt_X86_REG_R10W:
    case dt_X86_REG_R10D:
    case dt_X86_REG_R10:
        return r10_index;
    case dt_X86_REG_R11B:
    case dt_X86_REG_R11W:
    case dt_X86_REG_R11D:
    case dt_X86_REG_R11:
        return r11_index;
    case dt_X86_REG_R12B:
    case dt_X86_REG_R12W:
    case dt_X86_REG_R12D:
    case dt_X86_REG_R12:
        return r12_index;
    case dt_X86_REG_R13B:
    case dt_X86_REG_R13W:
    case dt_X86_REG_R13D:
    case dt_X86_REG_R13:
        return r13_index;
    case dt_X86_REG_R14B:
    case dt_X86_REG_R14W:
    case dt_X86_REG_R14D:
    case dt_X86_REG_R14:
        return r14_index;
    case dt_X86_REG_R15B:
    case dt_X86_REG_R15W:
    case dt_X86_REG_R15D:
    case dt_X86_REG_R15:
        return r15_index;
    case dt_X86_REG_RIZ:
        return riz_index;
#endif
    default:
        lsassert(0);
        // may fpu?
        return -1;
    }
}

int ir1_opnd_base_reg_num(const IR1_OPND *opnd)
{
    // xtm : base may not a mem opnd
    if (opnd->type == dt_X86_OP_MEM && opnd->mem.base == dt_X86_REG_INVALID) {
        return -1;
    }
    switch (opnd->type == dt_X86_OP_REG ? opnd->reg : opnd->mem.base) {
    case dt_X86_REG_AL:
    case dt_X86_REG_AH:
    case dt_X86_REG_AX:
    case dt_X86_REG_EAX:
#ifdef TARGET_X86_64
    case dt_X86_REG_RAX:
#endif
        return eax_index;
    case dt_X86_REG_BL:
    case dt_X86_REG_BH:
    case dt_X86_REG_BX:
    case dt_X86_REG_EBX:
#ifdef TARGET_X86_64
    case dt_X86_REG_RBX:
#endif
        return ebx_index;
    case dt_X86_REG_CL:
    case dt_X86_REG_CH:
    case dt_X86_REG_CX:
    case dt_X86_REG_ECX:
#ifdef TARGET_X86_64
    case dt_X86_REG_RCX:
#endif
        return ecx_index;
    case dt_X86_REG_DL:
    case dt_X86_REG_DH:
    case dt_X86_REG_DX:
    case dt_X86_REG_EDX:
#ifdef TARGET_X86_64
    case dt_X86_REG_RDX:
#endif
        return edx_index;
    case dt_X86_REG_BP:
    case dt_X86_REG_EBP:
#ifdef TARGET_X86_64
    case dt_X86_REG_RBP:
    case dt_X86_REG_BPL:
#endif
        return ebp_index;
    case dt_X86_REG_SI:
    case dt_X86_REG_ESI:
#ifdef TARGET_X86_64
    case dt_X86_REG_RSI:
    case dt_X86_REG_SIL:
#endif
        return esi_index;
    case dt_X86_REG_DI:
    case dt_X86_REG_EDI:
#ifdef TARGET_X86_64
    case dt_X86_REG_RDI:
    case dt_X86_REG_DIL:
#endif
        return edi_index;
    case dt_X86_REG_SP:
    case dt_X86_REG_ESP:
#ifdef TARGET_X86_64
    case dt_X86_REG_RSP:
    case dt_X86_REG_SPL:
#endif
        return esp_index;
#ifdef TARGET_X86_64
    case dt_X86_REG_R8B:
    case dt_X86_REG_R8W:
    case dt_X86_REG_R8D:
    case dt_X86_REG_R8:
        return r8_index;
    case dt_X86_REG_R9B:
    case dt_X86_REG_R9W:
    case dt_X86_REG_R9D:
    case dt_X86_REG_R9:
        return r9_index;
    case dt_X86_REG_R10B:
    case dt_X86_REG_R10W:
    case dt_X86_REG_R10D:
    case dt_X86_REG_R10:
        return r10_index;
    case dt_X86_REG_R11B:
    case dt_X86_REG_R11W:
    case dt_X86_REG_R11D:
    case dt_X86_REG_R11:
        return r11_index;
    case dt_X86_REG_R12B:
    case dt_X86_REG_R12W:
    case dt_X86_REG_R12D:
    case dt_X86_REG_R12:
        return r12_index;
    case dt_X86_REG_R13B:
    case dt_X86_REG_R13W:
    case dt_X86_REG_R13D:
    case dt_X86_REG_R13:
        return r13_index;
    case dt_X86_REG_R14B:
    case dt_X86_REG_R14W:
    case dt_X86_REG_R14D:
    case dt_X86_REG_R14:
        return r14_index;
    case dt_X86_REG_R15B:
    case dt_X86_REG_R15W:
    case dt_X86_REG_R15D:
    case dt_X86_REG_R15:
        return r15_index;
#endif
    case dt_X86_REG_CS:
        return cs_index;
    case dt_X86_REG_DS:
        return ds_index;
    case dt_X86_REG_SS:
        return ss_index;
    case dt_X86_REG_ES:
        return es_index;
    case dt_X86_REG_GS:
        return gs_index;
    case dt_X86_REG_FS:
        return fs_index;
    case dt_X86_REG_MM0:
        return 0;
    case dt_X86_REG_MM1:
        return 1;
    case dt_X86_REG_MM2:
        return 2;
    case dt_X86_REG_MM3:
        return 3;
    case dt_X86_REG_MM4:
        return 4;
    case dt_X86_REG_MM5:
        return 5;
    case dt_X86_REG_MM6:
        return 6;
    case dt_X86_REG_MM7:
        return 7;
    case dt_X86_REG_XMM0:
        return 0;
    case dt_X86_REG_XMM1:
        return 1;
    case dt_X86_REG_XMM2:
        return 2;
    case dt_X86_REG_XMM3:
        return 3;
    case dt_X86_REG_XMM4:
        return 4;
    case dt_X86_REG_XMM5:
        return 5;
    case dt_X86_REG_XMM6:
        return 6;
    case dt_X86_REG_XMM7:
        return 7;
    case dt_X86_REG_YMM0:
        return 0;
    case dt_X86_REG_YMM1:
        return 1;
    case dt_X86_REG_YMM2:
        return 2;
    case dt_X86_REG_YMM3:
        return 3;
    case dt_X86_REG_YMM4:
        return 4;
    case dt_X86_REG_YMM5:
        return 5;
    case dt_X86_REG_YMM6:
        return 6;
    case dt_X86_REG_YMM7:
        return 7;
#ifdef TARGET_X86_64
    case dt_X86_REG_XMM8:
        return 8;
    case dt_X86_REG_XMM9:
        return 9;
    case dt_X86_REG_XMM10:
        return 10;
    case dt_X86_REG_XMM11:
        return 11;
    case dt_X86_REG_XMM12:
        return 12;
    case dt_X86_REG_XMM13:
        return 13;
    case dt_X86_REG_XMM14:
        return 14;
    case dt_X86_REG_XMM15:
        return 15;
    case dt_X86_REG_YMM8:
        return 8;
    case dt_X86_REG_YMM9:
        return 9;
    case dt_X86_REG_YMM10:
        return 10;
    case dt_X86_REG_YMM11:
        return 11;
    case dt_X86_REG_YMM12:
        return 12;
    case dt_X86_REG_YMM13:
        return 13;
    case dt_X86_REG_YMM14:
        return 14;
    case dt_X86_REG_YMM15:
        return 15;
#endif
    case dt_X86_REG_ST0:
        return 0;
    case dt_X86_REG_ST1:
        return 1;
    case dt_X86_REG_ST2:
        return 2;
    case dt_X86_REG_ST3:
        return 3;
    case dt_X86_REG_ST4:
        return 4;
    case dt_X86_REG_ST5:
        return 5;
    case dt_X86_REG_ST6:
        return 6;
    case dt_X86_REG_ST7:
        return 7;
    default:
        return -1;
    }
}

longx ir1_opnd_simm(IR1_OPND *opnd)
{
    if (ir1_opnd_is_imm(opnd)) {
        switch (ir1_opnd_size(opnd)) {
        case 8:
            return (longx)((int8_t)(opnd->imm));
        case 16:
            return (longx)((int16_t)(opnd->imm));
        case 32:
            return (longx)((int32_t)(opnd->imm));
        case 64:
            return (longx)((int64_t)(opnd->imm));
        default:
            lsassert(0);
        }
    } else if (ir1_opnd_is_mem(opnd)) {
        return (longx)(opnd->mem.disp);
    } else {
        lsassertm(0, "REG opnd has no imm\n");
    }
    abort();
}

ulongx ir1_opnd_uimm_addr(IR1_OPND *opnd)
{
    if (ir1_opnd_is_imm(opnd)) {
        return (ulongx)(opnd->imm);
    } else if (ir1_opnd_is_mem(opnd)) {
        return (ulongx)(opnd->mem.disp);
    } else {
        lsassertm(0, "REG opnd has no imm\n");
    }
    abort();
}

ulongx ir1_opnd_uimm(IR1_OPND *opnd)
{
    if (ir1_opnd_is_imm(opnd)) {
        switch (ir1_opnd_size(opnd)) {
        case 8:
            return (ulongx)((uint8_t)(opnd->imm));
        case 16:
            return (ulongx)((uint16_t)(opnd->imm));
        case 32:
            return (ulongx)((uint32_t)(opnd->imm));
        case 64:
            return (ulongx)((uint64_t)(opnd->imm));
        default:
            lsassert(0);
        }
    } else if (ir1_opnd_is_mem(opnd)) {
        return (ulongx)(opnd->mem.disp);
    } else {
        lsassertm(0, "REG opnd has no imm\n");
    }
    abort();
}

longx ir1_opnd_u2simm(IR1_OPND *opnd)
{
    lsassert(ir1_opnd_is_imm(opnd));
    switch (ir1_opnd_size(opnd)) {
    case 8:
        return (longx)((uint8_t)(opnd->imm));
    case 16:
        return (longx)((uint16_t)(opnd->imm));
    case 32:
        return (longx)((uint32_t)(opnd->imm));
    case 64:
        return (longx)((uint64_t)(opnd->imm));
    default:
        lsassert(0);
    }
    abort();
}

ulongx ir1_opnd_s2uimm(IR1_OPND *opnd)
{
    lsassert(ir1_opnd_is_imm(opnd));
    switch (ir1_opnd_size(opnd)) {
    case 8:
        return (ulongx)((int8_t)(opnd->imm));
    case 16:
        return (ulongx)((int16_t)(opnd->imm));
    case 32:
        return (ulongx)((int32_t)(opnd->imm));
    case 64:
        return (ulongx)((int64_t)(opnd->imm));
    default:
        lsassert(0);
    }
    abort();
}

dt_x86_reg ir1_opnd_seg_reg(IR1_OPND *opnd) { return opnd->mem.segment; }

int ir1_opnd_is_imm(IR1_OPND *opnd) { return opnd->type == dt_X86_OP_IMM; }

int ir1_opnd_is_8h(const IR1_OPND *opnd)
{
    return opnd->type == dt_X86_OP_REG && opnd->size == 1 &&
           (opnd->reg == dt_X86_REG_AH || opnd->reg == dt_X86_REG_BH ||
            opnd->reg == dt_X86_REG_CH || opnd->reg == dt_X86_REG_DH);
}

int ir1_opnd_is_8l(const IR1_OPND *opnd)
{
#ifndef TARGET_X86_64
    return opnd->type == dt_X86_OP_REG && opnd->size == 1 &&
           (opnd->reg == dt_X86_REG_AL || opnd->reg == dt_X86_REG_BL ||
            opnd->reg == dt_X86_REG_CL || opnd->reg == dt_X86_REG_DL);
#else
    if (!CODEIS64) {
        return opnd->type == dt_X86_OP_REG && opnd->size == 1 &&
           (opnd->reg == dt_X86_REG_AL || opnd->reg == dt_X86_REG_BL ||
            opnd->reg == dt_X86_REG_CL || opnd->reg == dt_X86_REG_DL);
    }
    return opnd->type == dt_X86_OP_REG && opnd->size == 1 &&
           (opnd->reg == dt_X86_REG_AL   || opnd->reg == dt_X86_REG_BL   ||
            opnd->reg == dt_X86_REG_CL   || opnd->reg == dt_X86_REG_DL   ||
            opnd->reg == dt_X86_REG_R8B  || opnd->reg == dt_X86_REG_R9B  ||
            opnd->reg == dt_X86_REG_R10B || opnd->reg == dt_X86_REG_R11B ||
            opnd->reg == dt_X86_REG_R12B || opnd->reg == dt_X86_REG_R13B ||
            opnd->reg == dt_X86_REG_R14B || opnd->reg == dt_X86_REG_R15B ||
            opnd->reg == dt_X86_REG_SPL  || opnd->reg == dt_X86_REG_BPL  ||
            opnd->reg == dt_X86_REG_SIL  || opnd->reg == dt_X86_REG_DIL);
#endif
}

int ir1_opnd_is_16bit(const IR1_OPND *opnd)
{
#ifndef TARGET_X86_64
    return opnd->type == dt_X86_OP_REG && opnd->size == 2 &&
           (opnd->reg == dt_X86_REG_AX || opnd->reg == dt_X86_REG_BX ||
            opnd->reg == dt_X86_REG_CX || opnd->reg == dt_X86_REG_DX ||
            opnd->reg == dt_X86_REG_SP || opnd->reg == dt_X86_REG_BP ||
            opnd->reg == dt_X86_REG_SI || opnd->reg == dt_X86_REG_DI);
#else
     if (!CODEIS64) {
        return opnd->type == dt_X86_OP_REG && opnd->size == 2 &&
           (opnd->reg == dt_X86_REG_AX || opnd->reg == dt_X86_REG_BX ||
            opnd->reg == dt_X86_REG_CX || opnd->reg == dt_X86_REG_DX ||
            opnd->reg == dt_X86_REG_SP || opnd->reg == dt_X86_REG_BP ||
            opnd->reg == dt_X86_REG_SI || opnd->reg == dt_X86_REG_DI);
     }
    return opnd->type == dt_X86_OP_REG && opnd->size == 2 &&
           (opnd->reg == dt_X86_REG_AX   || opnd->reg == dt_X86_REG_BX   ||
            opnd->reg == dt_X86_REG_CX   || opnd->reg == dt_X86_REG_DX   ||
            opnd->reg == dt_X86_REG_R8W  || opnd->reg == dt_X86_REG_R9W  ||
            opnd->reg == dt_X86_REG_R10W || opnd->reg == dt_X86_REG_R11W ||
            opnd->reg == dt_X86_REG_R12W || opnd->reg == dt_X86_REG_R13W ||
            opnd->reg == dt_X86_REG_R14W || opnd->reg == dt_X86_REG_R15W ||
            opnd->reg == dt_X86_REG_SP   || opnd->reg == dt_X86_REG_BP   ||
            opnd->reg == dt_X86_REG_SI   || opnd->reg == dt_X86_REG_DI);
#endif
}

int ir1_opnd_is_32bit(const IR1_OPND *opnd)
{
#ifndef TARGET_X86_64
    return opnd->type == dt_X86_OP_REG && opnd->size == 4 &&
           (opnd->reg == dt_X86_REG_EAX || opnd->reg == dt_X86_REG_EBX ||
            opnd->reg == dt_X86_REG_ECX || opnd->reg == dt_X86_REG_EDX ||
            opnd->reg == dt_X86_REG_ESP || opnd->reg == dt_X86_REG_EBP ||
            opnd->reg == dt_X86_REG_ESI || opnd->reg == dt_X86_REG_EDI);
#else
    if (!CODEIS64) {
        return opnd->type == dt_X86_OP_REG && opnd->size == 4 &&
           (opnd->reg == dt_X86_REG_EAX || opnd->reg == dt_X86_REG_EBX ||
            opnd->reg == dt_X86_REG_ECX || opnd->reg == dt_X86_REG_EDX ||
            opnd->reg == dt_X86_REG_ESP || opnd->reg == dt_X86_REG_EBP ||
            opnd->reg == dt_X86_REG_ESI || opnd->reg == dt_X86_REG_EDI);
    }
    return opnd->type == dt_X86_OP_REG && opnd->size == 4 &&
           (opnd->reg == dt_X86_REG_EAX || opnd->reg == dt_X86_REG_EBX ||
            opnd->reg == dt_X86_REG_ECX || opnd->reg == dt_X86_REG_EDX ||
            opnd->reg == dt_X86_REG_R8D || opnd->reg == dt_X86_REG_R9D ||
            opnd->reg == dt_X86_REG_R10D || opnd->reg == dt_X86_REG_R11D ||
            opnd->reg == dt_X86_REG_R12D || opnd->reg == dt_X86_REG_R13D ||
            opnd->reg == dt_X86_REG_R14D || opnd->reg == dt_X86_REG_R15D ||
            opnd->reg == dt_X86_REG_ESP || opnd->reg == dt_X86_REG_EBP ||
            opnd->reg == dt_X86_REG_ESI || opnd->reg == dt_X86_REG_EDI);
#endif
}

#ifdef TARGET_X86_64
int ir1_opnd_is_64bit(const IR1_OPND *opnd)
{
    return opnd->type == dt_X86_OP_REG && opnd->size == 8 &&
           (opnd->reg == dt_X86_REG_RAX || opnd->reg == dt_X86_REG_RBX ||
            opnd->reg == dt_X86_REG_RCX || opnd->reg == dt_X86_REG_RDX ||
            opnd->reg == dt_X86_REG_R8 || opnd->reg == dt_X86_REG_R9 ||
            opnd->reg == dt_X86_REG_R10 || opnd->reg == dt_X86_REG_R11 ||
            opnd->reg == dt_X86_REG_R12 || opnd->reg == dt_X86_REG_R13 ||
            opnd->reg == dt_X86_REG_R14 || opnd->reg == dt_X86_REG_R15 ||
            opnd->reg == dt_X86_REG_RSP || opnd->reg == dt_X86_REG_RBP ||
            opnd->reg == dt_X86_REG_RSI || opnd->reg == dt_X86_REG_RDI);
}
#endif

int ir1_opnd_is_reg(const IR1_OPND *opnd)
{
    return ir1_opnd_is_gpr(opnd) || ir1_opnd_is_fpr(opnd) ||
           ir1_opnd_is_seg(opnd) || ir1_opnd_is_mmx(opnd) ||
           ir1_opnd_is_xmm(opnd) || ir1_opnd_is_ymm(opnd);
}

int ir1_opnd_is_gpr(const IR1_OPND *opnd)
{
    if (opnd->type != dt_X86_OP_REG) {
        return 0;
    }
    switch (opnd->reg) {
    case dt_X86_REG_AL:
    case dt_X86_REG_AH:
    case dt_X86_REG_AX:
    case dt_X86_REG_EAX:
    case dt_X86_REG_BL:
    case dt_X86_REG_BH:
    case dt_X86_REG_BX:
    case dt_X86_REG_EBX:
    case dt_X86_REG_CL:
    case dt_X86_REG_CH:
    case dt_X86_REG_CX:
    case dt_X86_REG_ECX:
    case dt_X86_REG_DL:
    case dt_X86_REG_DH:
    case dt_X86_REG_DX:
    case dt_X86_REG_EDX:

    case dt_X86_REG_BP:
    case dt_X86_REG_EBP:
    case dt_X86_REG_SI:
    case dt_X86_REG_ESI:
    case dt_X86_REG_DI:
    case dt_X86_REG_EDI:
    case dt_X86_REG_SP:
    case dt_X86_REG_ESP:
#ifdef TARGET_X86_64
    case dt_X86_REG_RAX:
    case dt_X86_REG_RBX:
    case dt_X86_REG_RCX:
    case dt_X86_REG_RDX:

    case dt_X86_REG_R8 ... dt_X86_REG_R15:
    case dt_X86_REG_R8D ... dt_X86_REG_R15D:
    case dt_X86_REG_R8W ... dt_X86_REG_R15W:
    case dt_X86_REG_R8B ... dt_X86_REG_R15B:

    case dt_X86_REG_RBP:
    case dt_X86_REG_RSI:
    case dt_X86_REG_RDI:
    case dt_X86_REG_RSP:
    case dt_X86_REG_BPL:
    case dt_X86_REG_SIL:
    case dt_X86_REG_DIL:
    case dt_X86_REG_SPL:
#endif
        return 1;
    default:
        return 0;
    }
}

int ir1_opnd_is_same_reg(const IR1_OPND *opnd0, const IR1_OPND *opnd1)
{
    return ir1_opnd_is_reg(opnd0) && ir1_opnd_is_reg(opnd1) &&
         opnd0->reg == opnd1->reg;
}

bool ir1_opnd_is_uimm12(IR1_OPND *opnd)
{
    return ir1_opnd_is_imm(opnd) && ir1_opnd_uimm(opnd) < 4096;
}

bool ir1_opnd_is_simm12(IR1_OPND *opnd)
{
    return ir1_opnd_is_imm(opnd) && ir1_opnd_simm(opnd) >= -2048 &&
           ir1_opnd_simm(opnd) < 2047;
}

bool ir1_opnd_is_s2uimm12(IR1_OPND *opnd)
{
    return ir1_opnd_is_imm(opnd) && ir1_opnd_s2uimm(opnd) < 4096;
}

bool ir1_opnd_is_u2simm12(IR1_OPND *opnd)
{
    lsassert(ir1_opnd_is_imm(opnd) && ir1_opnd_u2simm(opnd) >= 0);
    return ir1_opnd_is_imm(opnd) && ir1_opnd_u2simm(opnd) < 2047;
}

int ir1_opnd_is_gpr_used(IR1_OPND *opnd, uint8_t gpr_index)
{
    if (ir1_opnd_is_gpr(opnd)) {
        return ir1_opnd_base_reg_num(opnd) == gpr_index;
    } else if (ir1_opnd_is_mem(opnd)) {
        if (ir1_opnd_has_base(opnd)) {
#ifdef TARGET_X86_64
            if (!CODEIS64 && ir1_opnd_base_reg(opnd) == dt_X86_REG_RIP) {
                return 0;
            }
#endif
            return ir1_opnd_base_reg_num(opnd) == gpr_index;
        }
        if (ir1_opnd_has_index(opnd)) {
            return ir1_opnd_index_reg_num(opnd) == gpr_index;
        }
    }
    return 0;
}

int ir1_opnd_is_mem(const IR1_OPND *opnd) { return opnd->type == dt_X86_OP_MEM; }

int ir1_opnd_is_fpr(const IR1_OPND *opnd)
{
    if (opnd->type != dt_X86_OP_REG) {
        return 0;
    }
    switch (opnd->reg) {
    case dt_X86_REG_ST0:
    case dt_X86_REG_ST1:
    case dt_X86_REG_ST2:
    case dt_X86_REG_ST3:
    case dt_X86_REG_ST4:
    case dt_X86_REG_ST5:
    case dt_X86_REG_ST6:
    case dt_X86_REG_ST7:
        return 1;
    default:
        return 0;
    }
}

int ir1_opnd_is_seg(const IR1_OPND *opnd)
{
    if (opnd->type != dt_X86_OP_REG) {
        return 0;
    }
    switch (opnd->reg) {
    case dt_X86_REG_CS:
    case dt_X86_REG_DS:
    case dt_X86_REG_SS:
    case dt_X86_REG_ES:
    case dt_X86_REG_FS:
    case dt_X86_REG_GS:
        return 1;
    default:
        return 0;
    }
}

int ir1_opnd_is_mmx(const IR1_OPND *opnd)
{
    if (opnd->type != dt_X86_OP_REG) {
        return 0;
    }
    switch (opnd->reg) {
    case dt_X86_REG_MM0:
    case dt_X86_REG_MM1:
    case dt_X86_REG_MM2:
    case dt_X86_REG_MM3:
    case dt_X86_REG_MM4:
    case dt_X86_REG_MM5:
    case dt_X86_REG_MM6:
    case dt_X86_REG_MM7:
        return 1;
    default:
        return 0;
    }
}

int ir1_opnd_is_xmm(const IR1_OPND *opnd)
{
    if (opnd->type != dt_X86_OP_REG) {
        return 0;
    }
    switch (opnd->reg) {
    case dt_X86_REG_XMM0:
    case dt_X86_REG_XMM1:
    case dt_X86_REG_XMM2:
    case dt_X86_REG_XMM3:
    case dt_X86_REG_XMM4:
    case dt_X86_REG_XMM5:
    case dt_X86_REG_XMM6:
    case dt_X86_REG_XMM7:
#ifdef TARGET_X86_64
    case dt_X86_REG_XMM8:
    case dt_X86_REG_XMM9:
    case dt_X86_REG_XMM10:
    case dt_X86_REG_XMM11:
    case dt_X86_REG_XMM12:
    case dt_X86_REG_XMM13:
    case dt_X86_REG_XMM14:
    case dt_X86_REG_XMM15:
#endif
        return 1;
    default:
        return 0;
    }
}

int ir1_opnd_is_ymm(const IR1_OPND *opnd)
{
    if (opnd->type != dt_X86_OP_REG) {
        return 0;
    }
    switch (opnd->reg) {
    case dt_X86_REG_YMM0:
    case dt_X86_REG_YMM1:
    case dt_X86_REG_YMM2:
    case dt_X86_REG_YMM3:
    case dt_X86_REG_YMM4:
    case dt_X86_REG_YMM5:
    case dt_X86_REG_YMM6:
    case dt_X86_REG_YMM7:
#ifdef TARGET_X86_64
    case dt_X86_REG_YMM8:
    case dt_X86_REG_YMM9:
    case dt_X86_REG_YMM10:
    case dt_X86_REG_YMM11:
    case dt_X86_REG_YMM12:
    case dt_X86_REG_YMM13:
    case dt_X86_REG_YMM14:
    case dt_X86_REG_YMM15:
#endif
        return 1;
    default:
        return 0;
    }
}

int ir1_opnd_has_base(IR1_OPND *opnd)
{
    // may unnecessary to judge mem opnd
    return opnd->mem.base != dt_X86_REG_INVALID;
}

int ir1_opnd_has_index(IR1_OPND *opnd)
{
    return opnd->mem.index != dt_X86_REG_INVALID;
}

int ir1_opnd_has_seg(IR1_OPND *opnd)
{
    return opnd->mem.segment != dt_X86_REG_INVALID;
}

int ir1_opnd_get_seg_index(IR1_OPND *opnd)
{
#ifdef TARGET_X86_64
    if (CODEIS64) {
        lsassert(ir1_opnd_type(opnd) == dt_X86_OP_MEM);
    } else {
        lsassert(ir1_opnd_type(opnd) == dt_X86_OP_MEM ||
                 ir1_opnd_type(opnd) == dt_X86_OP_REG);
    }
#else
    /* in x64, seg reg may can be read by push/pop */
    /* so sometimes it might be a reg */
    lsassert(ir1_opnd_type(opnd) == dt_X86_OP_MEM ||
             ir1_opnd_type(opnd) == dt_X86_OP_REG);
#endif
    dt_x86_reg seg = opnd->mem.segment;
    if (seg == dt_X86_REG_ES) {
        return es_index;
    }
    if (seg == dt_X86_REG_CS) {
        return cs_index;
    }
    if (seg == dt_X86_REG_SS) {
        return ss_index;
    }
    if (seg == dt_X86_REG_DS) {
        return ds_index;
    }
    if (seg == dt_X86_REG_FS) {
        return fs_index;
    }
    if (seg == dt_X86_REG_GS) {
        return gs_index;
    }
    abort();
}

IR1_PREFIX ir1_prefix(IR1_INST *ir1)
{
    // TODO: one ins may has two or more PREFIX (rarely)
    // only support rep now
    return ir1->info->x86.prefix[0];  /* only lock rep*/
}

IR1_PREFIX ir1_prefix_opnd_size(IR1_INST *ir1)
{
    return ir1->info->x86.prefix[2];
}

#ifdef TARGET_X86_64
uint8_t ir1_rex(IR1_INST *ir1)
{
    return ir1->info->x86.rex;
}
    /*
    7                               0
    +---+---+---+---+---+---+---+---+
    | 0   1   0   0 | W | R | X | B |
    +---+---+---+---+---+---+---+---+
    */
uint8_t ir1_rex_w(IR1_INST *ir1)
{
    return ir1->info->x86.rex & 0x8;
}
#endif

int ir1_opnd_num(IR1_INST *ir1) { return ir1->info->x86.op_count; }

ADDRX ir1_addr(IR1_INST *ir1) { return ir1->info->address; }

ADDRX ir1_addr_next(IR1_INST *ir1)
{
    return ir1->info->address + ir1->info->size;
}

ADDRX ir1_target_addr(IR1_INST *ir1)
{
    lsassert(ir1_opnd_type(&(ir1->info->x86.operands[0])) ==
             dt_X86_OP_IMM);
    return (ADDRX)ir1_opnd_uimm_addr(&(ir1->info->x86.operands[0]));
}

IR1_OPCODE ir1_opcode(const IR1_INST *ir1) { return ir1->info->id; }

int ir1_is_branch(IR1_INST *ir1)
{
    switch (ir1->info->id) {
    case dt_X86_INS_JO:
    case dt_X86_INS_JNO:
    case dt_X86_INS_JB:
    case dt_X86_INS_JAE:
    case dt_X86_INS_JE:
    case dt_X86_INS_JNE:
    case dt_X86_INS_JBE:
    case dt_X86_INS_JA:
    case dt_X86_INS_JS:
    case dt_X86_INS_JNS:
    case dt_X86_INS_JP:
    case dt_X86_INS_JNP:
    case dt_X86_INS_JL:
    case dt_X86_INS_JGE:
    case dt_X86_INS_JLE:
    case dt_X86_INS_JG:

    case dt_X86_INS_LOOPNE:
    case dt_X86_INS_LOOPE:
    case dt_X86_INS_LOOP:
    case dt_X86_INS_JCXZ:
    case dt_X86_INS_JECXZ:
    case dt_X86_INS_JRCXZ:

        return 1;

    default:
        return 0;
    }
}

int ir1_is_jump(IR1_INST *ir1) { return ir1->info->id == dt_X86_INS_JMP ||ir1->info->id == dt_X86_INS_LJMP; }

int ir1_is_call(IR1_INST *ir1)
{
    // lsassert(0);
    // TODO : only direct?
    return ir1->info->id == dt_X86_INS_CALL;
}

int ir1_is_return(IR1_INST *ir1)
{
    return ir1->info->id == dt_X86_INS_RET ||
           ir1->info->id == dt_X86_INS_IRET ||
           ir1->info->id == dt_X86_INS_RETF;
}

int ir1_is_indirect(IR1_INST *ir1)
{
    lsassert(0);
    // TODO : some indirect call/jmp
    return ir1->info->id == dt_X86_INS_LJMP ||
           ir1->info->id == dt_X86_INS_LCALL;
}

int ir1_is_syscall(IR1_INST *ir1)
{
    // TODO : 0x80?
    switch (ir1->info->id) {
    case dt_X86_INS_INT:
#ifdef TARGET_X86_64
    case dt_X86_INS_SYSCALL:
    case dt_X86_INS_SYSENTER:
    case dt_X86_INS_SYSEXIT:
    case dt_X86_INS_SYSRET:
#endif
        return true;
    default:
        return false;
    }
}

bool ir1_is_tb_ending(IR1_INST *ir1)
{
#if defined(CONFIG_LATX_KZT)
    if(option_kzt && ir1_opcode(ir1) == dt_X86_INS_INT3) return true;
#endif
    if (ir1_opcode(ir1) == dt_X86_INS_CALL && !ir1_is_indirect_call(ir1) &&
        ir1_addr_next(ir1) == ir1_target_addr(ir1)) {
        return false;
    }

    if (ir1_is_call(ir1) && !ir1_is_indirect_call(ir1) &&
        (ht_pc_thunk_lookup(ir1_target_addr(ir1)) >= 0)) {
        return false;
    }

#ifdef CONFIG_LATX_SYSCALL_TUNNEL
    return ir1_is_branch(ir1) || ir1_is_jump(ir1) || ir1_is_call(ir1) ||
           ir1_is_return(ir1);
#else
    return ir1_is_branch(ir1) || ir1_is_jump(ir1) || ir1_is_call(ir1) ||
           ir1_is_return(ir1) || ir1_is_syscall(ir1);
#endif
}

bool ir1_is_cf_use(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_use, 1 << 0);
}
bool ir1_is_pf_use(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_use, 1 << 1);
}
bool ir1_is_af_use(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_use, 1 << 2);
}
bool ir1_is_zf_use(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_use, 1 << 3);
}
bool ir1_is_sf_use(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_use, 1 << 4);
}
bool ir1_is_of_use(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_use, 1 << 5);
}

bool ir1_is_cf_def(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_def, 1 << 0);
}
bool ir1_is_pf_def(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_def, 1 << 1);
}
bool ir1_is_af_def(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_def, 1 << 2);
}
bool ir1_is_zf_def(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_def, 1 << 3);
}
bool ir1_is_sf_def(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_def, 1 << 4);
}
bool ir1_is_of_def(IR1_INST *ir1)
{
    return BITS_ARE_SET(ir1->_eflag_def, 1 << 5);
}

uint8_t ir1_get_eflag_use(IR1_INST *ir1) { return ir1->_eflag_use; }
uint8_t ir1_get_eflag_def(IR1_INST *ir1) { return ir1->_eflag_def; }
void ir1_set_eflag_use(IR1_INST *ir1, uint8_t use) { ir1->_eflag_use = use; }
void ir1_set_eflag_def(IR1_INST *ir1, uint8_t def) { ir1->_eflag_def = def; }

void ir1_make_ins_JMP(IR1_INST *ir1, ADDRX addr, int32 off)
{
    struct la_dt_insn *info = ir1->info;

    info->bytes[0] = 0x69;
    *(int32_t *)(info->bytes + 1) = off;
    info->address = (uint64_t)addr;
    info->size = 5;
    info->x86.opcode[0] = 0xe9;
    info->x86.op_count = 1;
    info->x86.operands[0].type = dt_X86_OP_IMM;
    info->x86.operands[0].imm = (int64_t)off + (int64_t)addr;
#ifndef TARGET_X86_64
    info->x86.operands[0].size = 4;
    info->x86.addr_size = 4;
#else
    if (!CODEIS64) {
        info->x86.operands[0].size = 4;
        info->x86.addr_size = 4;
    } else {
        info->x86.operands[0].size = 8;
        info->x86.addr_size = 8;
    }
#endif
    info->id = dt_X86_INS_JMP;
    // TODO : other field in ir1 and detail->x86
    // another way : use capstone to disasm 0xe9 | off
}

void ir1_make_ins_NOP(IR1_INST *ir1, ADDRX addr) { lsassert(0); }
void ir1_make_ins_RET(IR1_INST *ir1, ADDRX addr) { lsassert(0); }
void ir1_make_ins_LIBFUNC(IR1_INST *ir1, ADDRX addr) { lsassert(0); }

int ir1_dump(IR1_INST *ir1)
{
#ifdef CONFIG_LATX_DEBUG
    qemu_log("0x%" PRIx64 ":\t%s\t\t%s\n", ir1->info->address,
            ir1->info->mnemonic, ir1->info->op_str);
#endif
    return 0;
}

int ir1_opcode_dump(IR1_INST *ir1)
{
#ifdef CONFIG_LATX_DEBUG
    qemu_log("0x%" PRIx64 ":\t%s\n", ir1->info->address,
            ir1->info->mnemonic);
#endif
    return 0;
}

const char * ir1_name(IR1_OPCODE op){
    return la_cs_insn_name(handle[CODEIS64], op);
}

const char *ir1_group_name(dt_x86_insn_group grp)
{
    return la_cs_group_name(handle[CODEIS64], grp);
}

uint8_t ir1_get_opnd_num(const IR1_INST *ir1)
{
    return ir1->info->x86.op_count;
}

IR1_OPND *ir1_get_opnd(const IR1_INST *ir1, int i)
{
    lsassert(i < ir1->info->x86.op_count);
    return &(ir1->info->x86.operands[i]);
}

bool ir1_is_indirect_call(IR1_INST *ir1)
{
    return !ir1_opnd_is_imm(ir1_get_opnd(ir1, 0));
}

bool ir1_is_indirect_jmp(IR1_INST *ir1)
{
    return !ir1_opnd_is_imm(ir1_get_opnd(ir1, 0));
}

bool ir1_is_prefix_lock(IR1_INST *ir1)
{
    return ir1->info->x86.prefix[0] == dt_X86_PREFIX_LOCK;
}

const char *ir1_reg_name(dt_x86_reg reg)
{
    return la_cs_reg_name(handle[CODEIS64], reg);
}

IR1_INST *tb_ir1_inst(TranslationBlock *tb, const int i)
{
    return (IR1_INST *)(tb->s_data->ir1) + i;
}

IR1_INST *tb_ir1_inst_last(TranslationBlock *tb)
{
    return (IR1_INST *)(tb->s_data->ir1) + tb->icount - 1;
}

int tb_ir1_num(TranslationBlock *tb)
{
    return tb->icount;
}
