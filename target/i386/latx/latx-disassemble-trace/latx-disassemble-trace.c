#include "latx-disassemble-trace.h"
#ifdef LATX_DISASSEMBLE_TRACE_DEBUG
int dt_debug_enable = 1;
#endif
uint8_t dt_mode;
int (*la_disa_v2)(const uint8_t *code, size_t code_size,
        uint64_t address,
        size_t count, struct la_dt_insn **insn,
        int ir1_num, void *pir1_base, int mode) = NULL;
void disassemble_trace_init(int abi_bits, int args)
{
    dt_mode = abi_bits;
    switch (args & 0xffff) {
    case 0:
        break;
    case OPT_V1NEXTCAPSTONE:
        la_disa_v1 = &gitcapstone_get;
        break;
    case OPT_V1LACAPSTONE:
        la_disa_v1 = &gitcapstone_get;
        break;
    case OPT_V1LAXED:
        la_disa_v1 = &laxed_get;
        break;
    case OPT_V1LAZYDIS:
        la_disa_v1 = &lazydis_get;
        break;
    default:
        lsassertm(0, "invalid V1.");
    }
    switch (args & 0xffff0000) {
    case 0:
        break;
    case OPT_V2LACAPSTONE:
        la_disa_v2 = &gitcapstone_get;
        break;
    case OPT_V2NEXTCAPSTONE:
        la_disa_v2 = &gitcapstone_get;
        break;
    case OPT_V2LAXED:
        la_disa_v2 = &laxed_get;
        break;
    case OPT_V2LAZYDIS:
        la_disa_v2 = &lazydis_get;
        break;
    default:
        dtassert(0);
    }
    if (la_disa_v2) {
        disassemble_trace_cmp = &disassemble_trace_loop;
    }
    gitcapstone_init(abi_bits);
    laxed_init(abi_bits);
    lazydis_init(abi_bits);
}
static int64_t imm_cast(int64_t imm, uint32_t size)
{
    switch (size) {
    case 0:
        return 0;
    case 1:
        return LA_STATIC_CAST(int8_t, imm);
    case 2:
        return LA_STATIC_CAST(int16_t, imm);
    case 4:
        return LA_STATIC_CAST(int32_t, imm);
    case 8:
        return LA_STATIC_CAST(int64_t, imm);
    default:
        dtassert(0);
    }
    return imm;
}

static void cmp_insn(struct la_dt_insn* v1,struct la_dt_insn* v2)
{
    int logmask = 0;
    int logindex = 0;
    const char *line = NULL;
    dtcmpmask(v1->id == v2->id, logmask , line);
    if (v1->id == dt_X86_INS_NOP) {
        return;
    }
    for (int i = 0; i < 4; i++) {
        dtcmpmask5(v1->x86.prefix[i] ==
            v2->x86.prefix[i], logmask , line, logindex, i);
    }
    /*only opcode[0] be used so far.*/
    for (int i = 0; i < 1; i++) {
        dtcmpmask5(v1->x86.opcode[i] ==
            v2->x86.opcode[i], logmask , line, logindex, i);
    }
    for (int i = 0; i < v1->size; i++) {
        dtcmpmask5(v1->bytes[i] == v2->bytes[i], logmask , line, logindex, i);
    }
    dtcmpmask(v1->x86.addr_size == v2->x86.addr_size, logmask , line);
    dtassert((dt_mode == 32) || (v1->x86.rex == v2->x86.rex));
    dtcmpmask(v1->x86.op_count == v2->x86.op_count, logmask , line);
    dtassert(v1->x86.op_count <= 8);
     for (int i = 0; i < v1->x86.op_count; i++) {
        int otype = v1->x86.operands[i].type;
        dtcmpmask5(v1->x86.operands[i].type ==
            v2->x86.operands[i].type, logmask , line, logindex, i);
        if (otype == dt_X86_OP_IMM) {
            dtcmpmask5(imm_cast(v1->x86.operands[i].imm,
                v1->x86.operands[i].size) ==
                    imm_cast(v2->x86.operands[i].imm,
                        v2->x86.operands[i].size), logmask , line, logindex, i);
        } else if (otype == dt_X86_OP_REG) {
                dtcmpmask5(v1->x86.operands[i].reg ==
                    v2->x86.operands[i].reg, logmask , line, logindex, i);
        } else if (otype == dt_X86_OP_MEM) {
            int seg = v1->x86.operands[i].mem.segment ==
                dt_X86_REG_INVALID ? v1->x86.operands[i].mem.default_segment :
                    v1->x86.operands[i].mem.segment;
            dtcmpmask5((seg == v1->x86.operands[i].mem.segment)
                || (seg == v1->x86.operands[i].mem.default_segment),
                    logmask , line, logindex, i);
            dtcmpmask5(v1->x86.operands[i].mem.base ==
                v2->x86.operands[i].mem.base, logmask , line, logindex, i);
            dtcmpmask5(v1->x86.operands[i].mem.index ==
                v2->x86.operands[i].mem.index, logmask , line, logindex, i);
            if (v1->x86.operands[i].mem.index != dt_X86_REG_INVALID) {
                dtcmpmask5(v1->x86.operands[i].mem.scale ==
                    v2->x86.operands[i].mem.scale, logmask , line, logindex, i);
            }
            dtcmpmask5(imm_cast(v1->x86.operands[i].mem.disp, dt_mode / 8) ==
                imm_cast(v2->x86.operands[i].mem.disp, dt_mode / 8),
                    logmask , line, logindex, i);
        }
        /*IMM only care value instead of size. */
        if (otype != dt_X86_OP_IMM) {
            dtcmpmask5(v1->x86.operands[i].size ==
                v2->x86.operands[i].size, logmask , line, logindex, i);
        }
    }
    dtcmpmask(v1->size == v2->size, logmask , line);
    dtcmpmask(v1->address == v2->address, logmask , line);
#ifdef LATX_DISASSEMBLE_TRACE_DEBUG
    if (unlikely(logmask)) {
        dtassert(v1->address == v2->address);
        dtassert(v1->size == v2->size);
        fprintf(stderr , "[%d] 0x%" PRIx64":", getpid(), v1->address);
        unsigned char *c = (unsigned char *)(intptr_t)v1->address;
        for (int i = 0; i < v1->size; i++) {
            fprintf(stderr , " %02x", c[i]);
        }
        fprintf(stderr, "\tv1:%s\t%s\tv2:%s\t%s\ti=%d\terr:%s\n",
            v1->mnemonic, v1->op_str, v2->mnemonic,
            v2->op_str,
            logindex, line);
    }
#endif
}
static void *disassemble_trace_vscapstone(const uint8_t *code, size_t code_size,
		uint64_t address,
		size_t count,
        struct la_dt_insn *inputinsn, int mode)
{
    struct la_dt_insn *v1 = inputinsn;
    struct la_dt_insn *v2 = NULL;
    int cmp_count = la_disa_v2(code, code_size ,
        address, count, &v2, 0, NULL, mode);
    if (cmp_count < 0) {
        return v2;
    }
    cmp_insn(v1, v2);
    if (v2 != NULL) {
        free(v2);
    }
    return NULL;
}
//todo
static void disassemble_trace_dump(struct la_dt_insn *inputinsn, void *dt_insn)
{
    fprintf(stderr, "%s :\n", __func__);
}
void disassemble_trace_loop(const uint8_t *code, size_t code_size,
    uint64_t address,
    size_t count,
    struct la_dt_insn *inputinsn, int mode)
{
    void *ret;
    ret = disassemble_trace_vscapstone(code, code_size,
        address, count, inputinsn, mode);
    if (ret != NULL)
    {
        disassemble_trace_dump(inputinsn, ret);
    }
}

