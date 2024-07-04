#include "laxed.h"
#undef XED_ICLASS_

la_name_enum_t la_xed_insn_tr[XED_ICLASS_LAST + 1];
la_name_enum_t la_xed_insn_reg[XED_REG_LAST + 1];

xed_state_t dstate[2];
xed_decoded_inst_t xedd;
uint8_t dt_laxed_mode;
static void handle_prefix(struct la_dt_insn *ret, xed_decoded_inst_t *inputinfo)
{
    /*
     *first prefix f3, f2 or f0
     */
    if (xed_operand_values_has_rep_prefix(inputinfo)) {
        ret->x86.prefix[0] = 0xf3;
    } else if (xed_operand_values_has_repne_prefix(inputinfo)) {
        ret->x86.prefix[0] = 0xf2;
    } else if (xed_operand_values_has_lock_prefix(inputinfo)) {
        ret->x86.prefix[0] = 0xf0;
    }
    /*
     *second prefix 2E, 36, 3E, 26, 64 or 65H
     */
    ret->x86.prefix[1] = xed3_operand_get_ild_seg(inputinfo);
    /*
     *third 66
     */
    if (xed_operand_values_has_66_prefix(inputinfo)) {
        ret->x86.prefix[2] = 0x66;
    }
    /*
     *last 67
     */
    if (xed_operand_values_has_address_size_prefix(inputinfo)) {
        ret->x86.prefix[3] = 0x67;
    }
}
static void handle_opcode(struct la_dt_insn *ret, xed_decoded_inst_t *inputinfo)
{
    uint8_t opcode_end = xed3_operand_get_nominal_opcode(inputinfo);
    char *op_str = (char *)(intptr_t)ret->address;
    xed_bits_t op_ops = xed3_operand_get_pos_nominal_opcode(inputinfo);
    int find_0f = 0;
    /*
     *0x0f is XED_ILD_LEGACY_MAP0, capstone not set.
     */
    while ((op_ops > 0) && (op_str[op_ops - 1] == 0x0f)) {
        op_ops--;
        find_0f = 1;
    }
    if (find_0f) {
        for (int i = op_ops, j = 0; i < 24; i++) {
            ret->x86.opcode[j++] = op_str[i];
            if (op_str[i] == opcode_end) {
                break;
            }
        }
    } else {
        ret->x86.opcode[0] = opcode_end;
    }

}

#ifdef LATX_DISASSEMBLE_TRACE_DEBUG
static char xed_tolower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 'a';
    }
    return c;
}

static int xed_strncat_lower(char *dst, const char *src, int len)
{
    xed_uint_t dst_len = xed_strlen(dst);
    xed_uint_t orig_max = dst_len + XED_STATIC_CAST(xed_uint_t, len);
    xed_uint_t i;
    xed_uint_t src_len = xed_strlen(src);
    xed_uint_t copy_max = src_len;
    xed_uint_t ulen = (xed_uint_t)len - 1;
    if (len <= 0) {
        return 0;
    }

    /*
     * do not copy more bytes than fit in the buffer including the null
     */

    if (src_len > ulen) {
        copy_max = ulen;
    }

    for (i = 0; i < copy_max; i++) {
        dst[dst_len + i] = xed_tolower(src[i]);
    }

    dst[dst_len + copy_max] = 0;
    /*
     * should never go negative
     */
    return XED_STATIC_CAST(int, orig_max - xed_strlen(dst));
}

static void handle_dis_str(struct la_dt_insn *ret,
    xed_decoded_inst_t *inputinfo)
{
#define XED_TMP_BUF_LEN (160 + 32)
    char instruction_name[32] = {0};
    char buf[XED_TMP_BUF_LEN] = {0};
    char *op_start;
    xed_iform_enum_t iform = xed_decoded_inst_get_iform_enum(inputinfo);
    xed_strncat_lower(instruction_name,
        xed_iform_to_iclass_string_intel(iform), 32);
    xed_format_context(XED_SYNTAX_INTEL, inputinfo, buf,
        XED_TMP_BUF_LEN, ret->address, 0, 0);
    strncpy(ret->mnemonic, instruction_name, 32);
    op_start = strstr(buf, instruction_name);
    if (op_start) {
        strncpy(ret->op_str, op_start + strlen(instruction_name) + 1, 160);
    } else {
        dtassert(0);
    }
}
#endif

static xed_bool_t  stringop_memop(const xed_decoded_inst_t *p,
    const xed_operand_t *o)
{
    xed_bool_t stringop =
        (xed_decoded_inst_get_category(p) == XED_CATEGORY_STRINGOP);
    if (stringop)  {
        xed_operand_enum_t   op_name = xed_operand_name(o);
        if (op_name == XED_OPERAND_MEM0 || op_name == XED_OPERAND_MEM1) {
            return 1;
        }
    }
    return 0;
}

static void seg_prefix_for_suppressed_operands(
    const xed_operand_values_t *ov,
    const xed_operand_t *op)
{
    xed_uint_t i;
    xed_operand_enum_t op_name = xed_operand_name(op);
    /*
     * suppressed memops with nondefault segments get their segment printed
     */
    const xed_operand_enum_t names[] = { XED_OPERAND_MEM0, XED_OPERAND_MEM1};
    for (i = 0; i < 2; i++) {
        if (op_name == names[i]) {
            xed_reg_enum_t seg = XED_REG_INVALID;
            switch (i) {
            case 0:
                seg = xed3_operand_get_seg0(ov);
                break;
            case 1:
                seg = xed3_operand_get_seg1(ov);
                break;
            }
            if (seg != XED_REG_INVALID &&
                xed_operand_values_using_default_segment(ov, i) == 0) {
                dtassert(0);
            }
        }
    }
}

static int64_t mask_value_bybit(int64_t value, uint8_t ibits)
{
    switch (ibits) {
    case 0:
        break;
    case 4:
        value &= 0xf;
        break;
    case 8:
        value &= 0xff;
        break;
    case 16:
        value &= 0xffff;
        break;
    case 32:
        value &= 0xffffffff;
        break;
    case 64:
        value &= 0xffffffffffffffff;
        break;
    default:
        dtassert(0);
    }
    return value;
}
static int64_t xed_imm_cast(int32_t imm, uint32_t size)
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
struct la_dt_insn *laxed_get_from_insn(int64_t address,
    xed_decoded_inst_t *inputinfo,
    int ir1_num, void *pir1_base)
{
    if (inputinfo == NULL) {
        return NULL;
    }
    struct la_dt_insn *ret;
    if (pir1_base) {
        uint64_t current_address =  (uint64_t)pir1_base +
            (ir1_num * sizeof(struct la_dt_insn));
        ret = (void *)current_address;
        memset(ret, 0, sizeof(struct la_dt_insn));
    } else {
        ret = calloc(1, sizeof(struct la_dt_insn));
    }
    ret->address = address;
#ifdef LATX_DISASSEMBLE_TRACE_DEBUG
    handle_dis_str(ret, inputinfo);
#endif
    xed_iclass_enum_t iclass = XED_ICLASS_INVALID;
    const xed_operand_values_t *ov = xed_decoded_inst_operands_const(inputinfo);
    if (xed_operand_values_has_real_rep(ov)) {
        iclass =
            xed_rep_remove(xed_decoded_inst_get_iclass(inputinfo));
    } else {
        iclass = xed_decoded_inst_get_iclass(inputinfo);
    }
    dtassert((iclass <= XED_ICLASS_LAST) &&
        (la_xed_insn_tr[iclass].id != dt_X86_INS_INVALID));
    ret->id = la_xed_insn_tr[iclass].id;
        handle_prefix(ret, inputinfo);
    handle_opcode(ret, inputinfo);
    ret->size = xed_decoded_inst_get_length(inputinfo);
    for (int i = 0; i < ret->size; i++) {
        ret->bytes[i] = xed_decoded_inst_get_byte(inputinfo, i);
    }
    ret->x86.addr_size = xed_operand_values_get_effective_address_width(
        xed_decoded_inst_operands_const(inputinfo)) / 8;
    if (dt_laxed_mode == 64) {
        ret->x86.rex = xed3_operand_get_rex(inputinfo) << 6;
        ret->x86.rex |= xed3_operand_get_rexb(inputinfo) << 0;
        ret->x86.rex |= xed3_operand_get_rexx(inputinfo) << 1;
        ret->x86.rex |= xed3_operand_get_rexr(inputinfo) << 2;
        ret->x86.rex |= xed3_operand_get_rexw(inputinfo) << 3;
    }
    const xed_inst_t *xi = xed_decoded_inst_inst(inputinfo);
    int xed_ocount = xed_inst_noperands(xi);
    int opcount = 0;
    int findmem[2] = {0}, memc = 0;
#define XED_GRAMMAR_MODE_32 1
#define XED_GRAMMAR_MODE_64 2
    /*cpuid skip opnd*/
    if (iclass == XED_ICLASS_CPUID) {
        goto foroperand;
    }
    for (int i = 0; i < xed_ocount; i++) {
        const xed_operand_t *op = xed_inst_operand(xi, i);
        if (xed_operand_operand_visibility(op) == XED_OPVIS_SUPPRESSED) {
            if (!stringop_memop(inputinfo, op)) {
                seg_prefix_for_suppressed_operands(ov, op);
                continue;
            }
        }
        xed_operand_enum_t op_name = xed_operand_name(op);
        ret->x86.operands[opcount].size =
            xed_decoded_inst_operand_length_bits(inputinfo, i) / 8;
        switch (op_name) {
        case XED_OPERAND_AGEN:
        case XED_OPERAND_MEM0:
        case XED_OPERAND_MEM1: {
            findmem[memc++] = opcount;
            ret->x86.operands[opcount++].type = dt_X86_OP_MEM;
            break;
        }
        case XED_OPERAND_PTR:  /* pointer (always in conjunction with a IMM0)*/
        case XED_OPERAND_RELBR: { /* branch displacements*/
            lsassert(xed_decoded_inst_get_branch_displacement_width(inputinfo));
            if (xed_decoded_inst_get_branch_displacement_width(inputinfo)) {
                ret->x86.operands[opcount].imm =
                    ret->address +
                    xed_decoded_inst_get_branch_displacement(inputinfo);
                if (op_name == XED_OPERAND_RELBR) {
                    ret->x86.operands[opcount].imm += ret->size;
                    switch (dt_laxed_mode) {
                    case 32:
                        ret->x86.operands[opcount].size = 4;
                        break;
                    case 64:
                        ret->x86.operands[opcount].size = 8;
                        break;
                    default:
                        dtassert(0);
                    }
                }
                ret->x86.operands[opcount++].type = dt_X86_OP_IMM;
            }
            break;
        }
        case XED_OPERAND_IMM0: {
            xed_uint_t ibits = xed_decoded_inst_get_operand_width(inputinfo);
            if (xed3_operand_get_imm0signed(inputinfo)) {
                ret->x86.operands[opcount].imm = XED_STATIC_CAST(xed_uint64_t,
                xed_sign_extend_arbitrary_to_64(
                xed_operand_values_get_immediate_int64(ov),
                ibits));
                ret->x86.operands[opcount].imm =
                    mask_value_bybit(ret->x86.operands[opcount].imm, ibits);
            } else {
                ret->x86.operands[opcount].imm =
                    xed_operand_values_get_immediate_uint64(ov);
            }
            ret->x86.operands[opcount].size = ibits / 8;
            ret->x86.operands[opcount++].type = dt_X86_OP_IMM;
            break;
        }
        case XED_OPERAND_IMM1: { /* 2nd immediate is always 1 byte.*/
            ret->x86.operands[opcount].imm = xed3_operand_get_uimm1(inputinfo);
            ret->x86.operands[opcount++].type = dt_X86_OP_IMM;
            break;
        }
        case XED_OPERAND_REG0:
        case XED_OPERAND_REG1:
        case XED_OPERAND_REG2:
        case XED_OPERAND_REG3:
        case XED_OPERAND_REG4:
        case XED_OPERAND_REG5:
        case XED_OPERAND_REG6:
        case XED_OPERAND_REG7:
        case XED_OPERAND_REG8:
        case XED_OPERAND_REG9: {
            xed_reg_enum_t r = xed_decoded_inst_get_reg(inputinfo, op_name);
            if (((r == XED_REG_STACKPOP) || (r == XED_REG_STACKPUSH))) {
                ret->x86.op_count = opcount;
                goto foroperand;
            }
            /* skip eflag or eip */
            if (r == XED_REG_EFLAGS || r == XED_REG_EIP) {
                continue;
            }
            ret->x86.operands[opcount].reg = la_xed_insn_reg[r].id;
            ret->x86.operands[opcount++].type = dt_X86_OP_REG;
            break;
        }
        case XED_OPERAND_BASE0:
        case XED_OPERAND_BASE1:
            continue;
        default:
            lsassert(0);
        }
    }
foroperand:
    ret->x86.op_count = opcount;
    dtassert(xed_decoded_inst_number_of_memory_operands(inputinfo) >= memc);
    for (int i = 0; i < memc; i++) {
        int memindex = findmem[i];
        unsigned int seg;
        lsassert(memindex <= ret->x86.op_count);
        lsassert(ret->x86.operands[memindex].type == dt_X86_OP_MEM);
        ret->x86.operands[memindex].access = dt_CS_AC_INVALID;
        if (xed_decoded_inst_mem_read(inputinfo, i)) {
            ret->x86.operands[memindex].access |= dt_CS_AC_READ;
        }
        if (xed_decoded_inst_mem_written(inputinfo, i)) {
            ret->x86.operands[memindex].access |= dt_CS_AC_WRITE;
        }
        seg = xed_decoded_inst_get_seg_reg(inputinfo, i);
        /* skip default segment */
        if (((i == 0) &&
                (!xed3_operand_get_using_default_segment0(inputinfo))) ||
            ((i == 1) &&
                (!xed3_operand_get_using_default_segment1(inputinfo)))) {
                ret->x86.operands[memindex].mem.segment =
                    la_xed_insn_reg[seg].id;
                ret->x86.operands[memindex].mem.default_segment =
                    dt_X86_INS_INVALID;
        } else {
            ret->x86.operands[memindex].mem.segment =
                dt_X86_INS_INVALID;
            ret->x86.operands[memindex].mem.default_segment =
                la_xed_insn_reg[seg].id;
        }
        ret->x86.operands[memindex].mem.base =
            la_xed_insn_reg[xed_decoded_inst_get_base_reg(inputinfo, i)].id;
        if (i == 0) {
            ret->x86.operands[memindex].mem.index =
              la_xed_insn_reg[xed_decoded_inst_get_index_reg(inputinfo, i)].id;
            ret->x86.operands[memindex].mem.scale =
              xed_decoded_inst_get_scale(inputinfo, i);
        }
        ret->x86.operands[memindex].mem.disp =
            xed_decoded_inst_get_memory_displacement(inputinfo, i);
        if (!((ret->x86.operands[memindex].mem.base != dt_X86_REG_INVALID) &&
            ret->x86.operands[memindex].mem.disp < 0)) {
            xed_int_t disbit =
                xed_decoded_inst_get_memory_displacement_width_bits(
                inputinfo, i);
            ret->x86.operands[memindex].mem.disp =
                mask_value_bybit(ret->x86.operands[memindex].mem.disp, disbit);
        }
        ret->x86.operands[memindex].mem.disp =
            xed_imm_cast(ret->x86.operands[memindex].mem.disp,
            dt_laxed_mode / 8);
    }
    return ret;
}

int laxed_get(const uint8_t *code, size_t code_size,
        uint64_t address,
        size_t count, struct la_dt_insn **insn,
        int ir1_num, void *pir1_base, int mode)
{
    xed_error_enum_t xed_error;
    struct la_dt_insn *ret;
    memset(&xedd, 0, sizeof(xed_operand_values_t));
    xed_operand_values_set_mode(&xedd, &dstate[mode]);
    xed_error = xed_decode(&xedd, code, code_size);
    switch (xed_error) {
    case XED_ERROR_NONE:
        break;
    case XED_ERROR_BUFFER_TOO_SHORT:
        printf("Not enough bytes provided\n");
        exit(1);
    case XED_ERROR_INVALID_FOR_CHIP:
        printf("The instruction was not valid for the specified chip.\n");
        exit(1);
    case XED_ERROR_GENERAL_ERROR:
        printf("Could not decode given input.\n");
        exit(1);
    default:
        printf("Unhandled error code %s\n",
           xed_error_enum_t2str(xed_error));
        exit(1);
    }
    ret = laxed_get_from_insn(address, &xedd, ir1_num, pir1_base);
    *insn = ret;
    return 1;
}

static void init_insn_id(void);
static void init_insn_reg(void);
static void dt_xed_init(int abi_bits)
{
    xed_tables_init();
    xed_state_zero(&dstate[0]);
    dstate[0].mmode = XED_MACHINE_MODE_LEGACY_32;
    dstate[0].stack_addr_width = XED_ADDRESS_WIDTH_32b;
    xed_tables_init();
    xed_state_zero(&dstate[1]);
    dstate[0].mmode = XED_MACHINE_MODE_LONG_64;
    
}

void laxed_init(int abi_bits)
{
    dt_laxed_mode = abi_bits;
    dt_xed_init(abi_bits);
    init_insn_id();
    init_insn_reg();
}
static void init_insn_reg(void)
{
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, INVALID);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, AH);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, AL);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, AX);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, BH);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, BL);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, BP);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, BPL);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, BX);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CH);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CL);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CS);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CX);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DH);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DI);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DIL);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DL);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DS);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DX);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, EAX);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, EBP);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, EBX);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ECX);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, EDI);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, EDX);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, EFLAGS);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, EIP);
/*
 *    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, EIZ);
 */
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ES);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ESI);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ESP);
/*
 *    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, FPSW);
 */
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, FS);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, GS);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, IP);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, RAX);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, RBP);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, RBX);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, RCX);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, RDI);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, RDX);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, RIP);
/*
 *    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, RIZ);
 */
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, RSI);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, RSP);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, SI);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, SIL);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, SP);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, SPL);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, SS);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CR0);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CR1);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CR2);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CR3);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CR4);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CR5);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CR6);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CR7);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CR8);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CR9);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CR10);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CR11);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CR12);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CR13);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CR14);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, CR15);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DR0);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DR1);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DR2);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DR3);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DR4);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DR5);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DR6);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DR7);
/*
 *    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DR8);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DR9);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DR10);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DR11);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DR12);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DR13);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DR14);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, DR15);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, FP0);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, FP1);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, FP2);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, FP3);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, FP4);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, FP5);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, FP6);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, FP7);
 */
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, K0);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, K1);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, K2);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, K3);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, K4);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, K5);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, K6);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, K7);
    LA_X86_INSN_REENUM(la_xed_insn_reg, XED_REG_MMX0, dt_X86_REG_MM0);
    LA_X86_INSN_REENUM(la_xed_insn_reg, XED_REG_MMX1, dt_X86_REG_MM1);
    LA_X86_INSN_REENUM(la_xed_insn_reg, XED_REG_MMX2, dt_X86_REG_MM2);
    LA_X86_INSN_REENUM(la_xed_insn_reg, XED_REG_MMX3, dt_X86_REG_MM3);
    LA_X86_INSN_REENUM(la_xed_insn_reg, XED_REG_MMX4, dt_X86_REG_MM4);
    LA_X86_INSN_REENUM(la_xed_insn_reg, XED_REG_MMX5, dt_X86_REG_MM5);
    LA_X86_INSN_REENUM(la_xed_insn_reg, XED_REG_MMX6, dt_X86_REG_MM6);
    LA_X86_INSN_REENUM(la_xed_insn_reg, XED_REG_MMX7, dt_X86_REG_MM7);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R8);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R9);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R10);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R11);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R12);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R13);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R14);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R15);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ST0);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ST1);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ST2);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ST3);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ST4);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ST5);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ST6);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ST7);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM0);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM1);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM2);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM3);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM4);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM5);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM6);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM7);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM8);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM9);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM10);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM11);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM12);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM13);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM14);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM15);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM16);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM17);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM18);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM19);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM20);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM21);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM22);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM23);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM24);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM25);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM26);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM27);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM28);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM29);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM30);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, XMM31);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM0);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM1);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM2);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM3);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM4);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM5);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM6);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM7);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM8);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM9);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM10);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM11);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM12);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM13);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM14);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM15);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM16);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM17);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM18);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM19);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM20);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM21);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM22);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM23);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM24);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM25);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM26);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM27);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM28);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM29);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM30);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, YMM31);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM0);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM1);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM2);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM3);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM4);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM5);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM6);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM7);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM8);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM9);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM10);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM11);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM12);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM13);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM14);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM15);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM16);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM17);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM18);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM19);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM20);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM21);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM22);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM23);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM24);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM25);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM26);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM27);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM28);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM29);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM30);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, ZMM31);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R8B);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R9B);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R10B);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R11B);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R12B);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R13B);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R14B);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R15B);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R8D);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R9D);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R10D);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R11D);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R12D);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R13D);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R14D);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R15D);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R8W);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R9W);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R10W);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R11W);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R12W);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R13W);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R14W);
    LA_X86_INSN_ENUM(la_xed_insn_reg, XED_REG_, X86_REG_, R15W);
    LA_X86_INSN_REENUM(la_xed_insn_reg, XED_REG_LAST, dt_X86_REG_ENDING);

}

static void init_insn_id(void)
{
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, INVALID);

    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, AAA);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, AAD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, AAM);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, AAS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FABS);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, ADC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ADCX);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, ADD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ADDPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ADDPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ADDSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ADDSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ADDSUBPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ADDSUBPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FADD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FIADD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FADDP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ADOX);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, AESDECLAST);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, AESDEC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, AESENCLAST);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, AESENC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, AESIMC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, AESKEYGENASSIST);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, AND);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ANDN);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ANDNPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ANDNPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ANDPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ANDPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ARPL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BEXTR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BLCFILL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BLCI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BLCIC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BLCMSK);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BLCS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BLENDPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BLENDPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BLENDVPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BLENDVPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BLSFILL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BLSI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BLSIC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BLSMSK);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BLSR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BOUND);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BSF);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BSR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BSWAP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BT);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, BTC);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, BTR);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, BTS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, BZHI);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_CALL_NEAR, dt_X86_INS_CALL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CDQE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FCHS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CLAC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CLC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CLD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CLFLUSH);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CLFLUSHOPT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CLGI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CLI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CLTS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CLWB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMC);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_CMOVNBE, dt_X86_INS_CMOVA);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_CMOVNB, dt_X86_INS_CMOVAE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMOVB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMOVBE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FCMOVBE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FCMOVB);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_CMOVZ, dt_X86_INS_CMOVE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FCMOVE);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_CMOVNLE, dt_X86_INS_CMOVG);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_CMOVNL, dt_X86_INS_CMOVGE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMOVL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMOVLE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FCMOVNBE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FCMOVNB);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_CMOVNZ, dt_X86_INS_CMOVNE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FCMOVNE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMOVNO);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMOVNP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FCMOVNU);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMOVNS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMOVO);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMOVP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FCMOVU);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMOVS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPSQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPSW);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, CMPXCHG16B);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, CMPXCHG);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, CMPXCHG8B);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, COMISD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, COMISS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FCOMP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FCOMIP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FCOMI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FCOM);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FCOS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CPUID);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CQO);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CRC32);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTDQ2PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTDQ2PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTPD2DQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTPD2PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTPS2DQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTPS2PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTSD2SI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTSD2SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTSI2SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTSI2SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTSS2SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTSS2SI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTTPD2DQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTTPS2DQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTTSD2SI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTTSS2SI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CWDE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, DAA);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, DAS);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, DATA16);
 */
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, DEC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, DIV);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, DIVPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, DIVPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FDIVR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FIDIVR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FDIVRP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, DIVSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, DIVSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FDIV);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FIDIV);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FDIVP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, DPPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, DPPS);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_RET_NEAR, dt_X86_INS_RET);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ENCLS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ENCLU);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ENTER);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, EXTRACTPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, EXTRQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, F2XM1);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LCALL);
 */
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LJMP);
 */
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FBLD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FBSTP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FCOMPP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FDECSTP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FEMMS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FFREE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FICOM);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FICOMP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FINCSTP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FLDCW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FLDENV);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FLDL2E);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FLDL2T);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FLDLG2);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FLDLN2);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FLDPI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FNCLEX);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FNINIT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FNOP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FNSTCW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FNSTSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FPATAN);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FPREM);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FPREM1);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FPTAN);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FFREEP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FRNDINT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FRSTOR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FNSAVE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FSCALE);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FSETPM);
 */
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FSINCOS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FNSTENV);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FXAM);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FXRSTOR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FXRSTOR64);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FXSAVE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FXSAVE64);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FXTRACT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FYL2X);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FYL2XP1);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVAPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVAPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ORPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ORPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVAPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVAPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XORPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XORPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, GETSEC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, HADDPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, HADDPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, HLT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, HSUBPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, HSUBPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, IDIV);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FILD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, IMUL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, IN);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, INC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, INSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, INSERTPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, INSERTQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, INSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, INSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, INT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, INT1);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, INT3);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, INTO);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, INVD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, INVEPT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, INVLPG);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, INVLPGA);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, INVPCID);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, INVVPID);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, IRET);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, IRETD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, IRETQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FISTTP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FIST);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FISTP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, UCOMISD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, UCOMISS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCOMISD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCOMISS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTSD2SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTSI2SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTSI2SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTSS2SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTTSD2SI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTTSD2USI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTTSS2SI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTTSS2USI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTUSI2SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTUSI2SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VUCOMISD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VUCOMISS);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_JNB, dt_X86_INS_JAE);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_JNBE, dt_X86_INS_JA);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, JBE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, JB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, JCXZ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, JECXZ);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_JZ, dt_X86_INS_JE);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_JNL, dt_X86_INS_JGE);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_JNLE, dt_X86_INS_JG);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, JLE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, JL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, JMP);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_JNZ, dt_X86_INS_JNE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, JNO);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, JNP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, JNS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, JO);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, JP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, JRCXZ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, JS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KANDB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KANDD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KANDNB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KANDND);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KANDNQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KANDNW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KANDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KANDW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KMOVB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KMOVD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KMOVQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KMOVW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KNOTB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KNOTD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KNOTQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KNOTW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KORB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KORD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KORQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KORTESTB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KORTESTD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KORTESTQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KORTESTW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KORW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KSHIFTLB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KSHIFTLD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KSHIFTLQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KSHIFTLW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KSHIFTRB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KSHIFTRD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KSHIFTRQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KSHIFTRW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KUNPCKBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KXNORB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KXNORD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KXNORQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KXNORW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KXORB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KXORD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KXORQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, KXORW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LAHF);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LAR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LDDQU);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LDMXCSR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LDS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FLDZ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FLD1);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FLD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LEA);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LEAVE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LES);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LFENCE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LFS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LGDT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LGS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LIDT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LLDT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LMSW);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, OR);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, SUB);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, XOR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LODSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LODSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LODSQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LODSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LOOP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LOOPE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LOOPNE);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RETF);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RETFQ);
 */
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LSL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LTR);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, XADD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, LZCNT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MASKMOVDQU);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MAXPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MAXPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MAXSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MAXSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MFENCE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MINPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MINPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MINSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MINSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTPD2PI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTPI2PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTPI2PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTPS2PI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTTPD2PI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CVTTPS2PI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, EMMS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MASKMOVQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVDQ2Q);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVNTQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVQ2DQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PABSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PABSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PABSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PACKSSDW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PACKSSWB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PACKUSWB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PADDB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PADDD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PADDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PADDSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PADDSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PADDUSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PADDUSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PADDW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PALIGNR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PANDN);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PAND);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PAVGB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PAVGW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PCMPEQB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PCMPEQD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PCMPEQW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PCMPGTB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PCMPGTD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PCMPGTW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PEXTRW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PHADDSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PHADDW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PHADDD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PHSUBD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PHSUBSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PHSUBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PINSRW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMADDUBSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMADDWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMAXSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMAXUB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMINSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMINUB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMOVMSKB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMULHRSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMULHUW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMULHW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMULLW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMULUDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, POR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSADBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSHUFB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSHUFW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSIGNB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSIGND);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSIGNW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSLLD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSLLQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSLLW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSRAD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSRAW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSRLD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSRLQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSRLW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSUBB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSUBD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSUBQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSUBSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSUBSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSUBUSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSUBUSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSUBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PUNPCKHBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PUNPCKHDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PUNPCKHWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PUNPCKLBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PUNPCKLDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PUNPCKLWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PXOR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MONITOR);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MONTMUL);
 */
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOV);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVABS);
 */
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVBE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVDDUP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVDQA);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVDQU);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVHLPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVHPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVHPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVLHPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVLPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVLPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVMSKPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVMSKPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVNTDQA);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVNTDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVNTI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVNTPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVNTPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVNTSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVNTSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVSB);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_MOVSD_XMM, dt_X86_INS_MOVSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVSHDUP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVSLDUP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVSQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVSX);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVSXD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVUPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVUPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MOVZX);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MPSADBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MUL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MULPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MULPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MULSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MULSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MULX);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FMUL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FIMUL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FMULP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, MWAIT);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, NEG);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, NOP);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, NOT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, OUT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, OUTSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, OUTSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, OUTSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PACKUSDW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PAUSE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PAVGUSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PBLENDVB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PBLENDW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PCLMULQDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PCMPEQQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PCMPESTRI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PCMPESTRM);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PCMPGTQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PCMPISTRI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PCMPISTRM);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PCOMMIT);
 */
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PDEP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PEXT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PEXTRB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PEXTRD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PEXTRQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PF2ID);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PF2IW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFACC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFADD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFCMPEQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFCMPGE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFCMPGT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFMAX);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFMIN);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFMUL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFNACC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFPNACC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFRCPIT1);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFRCPIT2);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFRCP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFRSQIT1);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFRSQRT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFSUBR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PFSUB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PHMINPOSUW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PI2FD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PI2FW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PINSRB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PINSRD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PINSRQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMAXSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMAXSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMAXUD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMAXUW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMINSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMINSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMINUD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMINUW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMOVSXBD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMOVSXBQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMOVSXBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMOVSXDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMOVSXWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMOVSXWQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMOVZXBD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMOVZXBQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMOVZXBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMOVZXDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMOVZXWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMOVZXWQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMULDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMULHRW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PMULLD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, POP);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, POPAW);
 */
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_POPAD, dt_X86_INS_POPAL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, POPCNT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, POPF);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, POPFD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, POPFQ);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PREFETCH);
 */
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PREFETCHNTA);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PREFETCHT0);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PREFETCHT1);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PREFETCHT2);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PREFETCHW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSHUFD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSHUFHW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSHUFLW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSLLDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSRLDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PSWAPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PTEST);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PUNPCKHQDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PUNPCKLQDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PUSH);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PUSHAW);
 */
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_PUSHAD, dt_X86_INS_PUSHAL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PUSHF);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PUSHFD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, PUSHFQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RCL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RCPPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RCPSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RCR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RDFSBASE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RDGSBASE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RDMSR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RDPMC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RDRAND);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RDSEED);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RDTSC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RDTSCP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ROL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ROR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RORX);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ROUNDPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ROUNDPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ROUNDSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ROUNDSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RSM);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RSQRTPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, RSQRTSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SAHF);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SAL);
 */
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SALC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SAR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SARX);
    LA_X86_INSN_DEF_LOCK(la_xed_insn_tr, XED_ICLASS_, SBB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SCASB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SCASD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SCASQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SCASW);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_SETNB, dt_X86_INS_SETAE);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_SETNBE, dt_X86_INS_SETA);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SETBE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SETB);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_SETZ, dt_X86_INS_SETE);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_SETNL, dt_X86_INS_SETGE);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_SETNLE, dt_X86_INS_SETG);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SETLE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SETL);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_SETNZ, dt_X86_INS_SETNE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SETNO);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SETNP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SETNS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SETO);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SETP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SETS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SFENCE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SGDT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SHA1MSG1);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SHA1MSG2);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SHA1NEXTE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SHA1RNDS4);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SHA256MSG1);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SHA256MSG2);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SHA256RNDS2);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SHL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SHLD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SHLX);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SHR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SHRD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SHRX);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SHUFPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SHUFPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SIDT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FSIN);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SKINIT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SLDT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SMSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SQRTPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SQRTPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SQRTSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SQRTSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FSQRT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, STAC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, STC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, STD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, STGI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, STI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, STMXCSR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, STOSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, STOSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, STOSQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, STOSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, STR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FST);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FSTP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FSTPNCE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FXCH);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SUBPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SUBPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FSUBR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FISUBR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FSUBRP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SUBSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SUBSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FSUB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FISUB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FSUBP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SWAPGS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SYSCALL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SYSENTER);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SYSEXIT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, SYSRET);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, T1MSKC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, TEST);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, UD2);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FTST);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, TZCNT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, TZMSK);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FUCOMIP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FUCOMI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FUCOMPP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FUCOMP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FUCOM);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, UD2B);
 */
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, UNPCKHPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, UNPCKHPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, UNPCKLPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, UNPCKLPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VADDPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VADDPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VADDSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VADDSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VADDSUBPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VADDSUBPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VAESDECLAST);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VAESDEC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VAESENCLAST);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VAESENC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VAESIMC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VAESKEYGENASSIST);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VALIGND);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VALIGNQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VANDNPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VANDNPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VANDPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VANDPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VBLENDMPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VBLENDMPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VBLENDPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VBLENDPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VBLENDVPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VBLENDVPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VBROADCASTF128);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VBROADCASTI32X4);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VBROADCASTI64X4);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VBROADCASTSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VBROADCASTSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCOMPRESSPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCOMPRESSPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTDQ2PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTDQ2PS);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTPD2DQX);
 */
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTPD2DQ);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTPD2PSX);
 */
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTPD2PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTPD2UDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTPH2PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTPS2DQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTPS2PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTPS2PH);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTPS2UDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTSD2SI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTSD2USI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTSS2SI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTSS2USI);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTTPD2DQX);
 */
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTTPD2DQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTTPD2UDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTTPS2DQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTTPS2UDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTUDQ2PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCVTUDQ2PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VDIVPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VDIVPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VDIVSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VDIVSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VDPPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VDPPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VERR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VERW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VEXP2PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VEXP2PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VEXPANDPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VEXPANDPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VEXTRACTF128);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VEXTRACTF32X4);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VEXTRACTF64X4);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VEXTRACTI128);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VEXTRACTI32X4);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VEXTRACTI64X4);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VEXTRACTPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADD132PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADD132PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADDPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADD213PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADD231PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADDPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADD213PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADD231PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADDSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADD213SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADD132SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADD231SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADDSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADD213SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADD132SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADD231SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADDSUB132PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADDSUB132PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADDSUBPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADDSUB213PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADDSUB231PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADDSUBPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADDSUB213PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMADDSUB231PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUB132PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUB132PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUBADD132PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUBADD132PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUBADDPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUBADD213PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUBADD231PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUBADDPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUBADD213PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUBADD231PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUBPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUB213PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUB231PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUBPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUB213PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUB231PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUBSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUB213SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUB132SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUB231SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUBSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUB213SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUB132SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFMSUB231SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMADD132PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMADD132PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMADDPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMADD213PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMADD231PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMADDPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMADD213PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMADD231PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMADDSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMADD213SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMADD132SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMADD231SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMADDSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMADD213SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMADD132SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMADD231SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMSUB132PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMSUB132PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMSUBPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMSUB213PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMSUB231PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMSUBPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMSUB213PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMSUB231PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMSUBSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMSUB213SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMSUB132SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMSUB231SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMSUBSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMSUB213SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMSUB132SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFNMSUB231SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFRCZPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFRCZPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFRCZSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VFRCZSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VORPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VORPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VXORPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VXORPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VGATHERDPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VGATHERDPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VGATHERPF0DPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VGATHERPF0DPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VGATHERPF0QPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VGATHERPF0QPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VGATHERPF1DPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VGATHERPF1DPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VGATHERPF1QPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VGATHERPF1QPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VGATHERQPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VGATHERQPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VHADDPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VHADDPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VHSUBPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VHSUBPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VINSERTF128);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VINSERTF32X4);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VINSERTF32X8);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VINSERTF64X2);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VINSERTF64X4);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VINSERTI128);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VINSERTI32X4);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VINSERTI32X8);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VINSERTI64X2);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VINSERTI64X4);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VINSERTPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VLDDQU);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VLDMXCSR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMASKMOVDQU);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMASKMOVPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMASKMOVPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMAXPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMAXPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMAXSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMAXSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMCALL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMCLEAR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMFUNC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMINPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMINPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMINSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMINSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMLAUNCH);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMLOAD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMMCALL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVDDUP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVDQA32);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVDQA64);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVDQA);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVDQU16);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVDQU32);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVDQU64);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVDQU8);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVDQU);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVHLPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVHPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVHPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVLHPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVLPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVLPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVMSKPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVMSKPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVNTDQA);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVNTDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVNTPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVNTPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVSHDUP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVSLDUP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVUPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMOVUPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMPSADBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMPTRLD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMPTRST);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMREAD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMRESUME);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMRUN);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMSAVE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMULPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMULPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMULSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMULSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMWRITE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMXOFF);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VMXON);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPABSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPABSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPABSQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPABSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPACKSSDW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPACKSSWB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPACKUSDW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPACKUSWB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPADDB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPADDD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPADDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPADDSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPADDSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPADDUSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPADDUSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPADDW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPALIGNR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPANDD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPANDND);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPANDNQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPANDN);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPANDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPAND);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPAVGB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPAVGW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPBLENDD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPBLENDMB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPBLENDMD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPBLENDMQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPBLENDMW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPBLENDVB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPBLENDW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPBROADCASTB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPBROADCASTD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPBROADCASTMB2Q);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPBROADCASTMW2D);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPBROADCASTQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPBROADCASTW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCLMULQDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMOV);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPEQB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPEQD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPEQQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPEQW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPESTRI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPESTRM);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPGTB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPGTD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPGTQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPGTW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPISTRI);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPISTRM);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPUB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPUD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPUQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPUW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCMPW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCOMB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCOMD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCOMPRESSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCOMPRESSQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCOMQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCOMUB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCOMUD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCOMUQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCOMUW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCOMW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCONFLICTD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPCONFLICTQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERM2F128);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERM2I128);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERMD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERMI2D);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERMI2PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERMI2PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERMI2Q);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERMIL2PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERMIL2PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERMILPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERMILPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERMPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERMPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERMQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERMT2D);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERMT2PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERMT2PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPERMT2Q);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPEXPANDD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPEXPANDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPEXTRB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPEXTRD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPEXTRQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPEXTRW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPGATHERDD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPGATHERDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPGATHERQD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPGATHERQQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHADDBD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHADDBQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHADDBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHADDDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHADDD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHADDSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHADDUBD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHADDUBQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHADDUBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHADDUDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHADDUWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHADDUWQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHADDWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHADDWQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHADDW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHMINPOSUW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHSUBBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHSUBDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHSUBD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHSUBSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHSUBWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPHSUBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPINSRB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPINSRD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPINSRQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPINSRW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPLZCNTD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPLZCNTQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMACSDD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMACSDQH);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMACSDQL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMACSSDD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMACSSDQH);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMACSSDQL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMACSSWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMACSSWW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMACSWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMACSWW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMADCSSWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMADCSWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMADDUBSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMADDWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMASKMOVD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMASKMOVQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMAXSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMAXSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMAXSQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMAXSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMAXUB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMAXUD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMAXUQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMAXUW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMINSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMINSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMINSQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMINSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMINUB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMINUD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMINUQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMINUW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVDB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVDW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVM2B);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVM2D);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVM2Q);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVM2W);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVMSKB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVQB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVQD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVQW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVSDB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVSDW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVSQB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVSQD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVSQW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVSXBD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVSXBQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVSXBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVSXDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVSXWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVSXWQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVUSDB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVUSDW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVUSQB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVUSQD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVUSQW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVZXBD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVZXBQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVZXBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVZXDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVZXWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMOVZXWQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMULDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMULHRSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMULHUW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMULHW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMULLD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMULLQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMULLW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPMULUDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPORD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPORQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPOR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPPERM);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPROTB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPROTD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPROTQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPROTW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSADBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSCATTERDD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSCATTERDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSCATTERQD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSCATTERQQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSHAB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSHAD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSHAQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSHAW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSHLB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSHLD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSHLQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSHLW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSHUFB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSHUFD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSHUFHW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSHUFLW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSIGNB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSIGND);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSIGNW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSLLDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSLLD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSLLQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSLLVD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSLLVQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSLLW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSRAD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSRAQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSRAVD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSRAVQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSRAW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSRLDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSRLD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSRLQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSRLVD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSRLVQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSRLW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSUBB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSUBD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSUBQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSUBSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSUBSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSUBUSB);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSUBUSW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPSUBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPTESTMD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPTESTMQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPTESTNMD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPTESTNMQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPTEST);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPUNPCKHBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPUNPCKHDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPUNPCKHQDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPUNPCKHWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPUNPCKLBW);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPUNPCKLDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPUNPCKLQDQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPUNPCKLWD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPXORD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPXORQ);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VPXOR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRCP14PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRCP14PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRCP14SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRCP14SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRCP28PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRCP28PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRCP28SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRCP28SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRCPPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRCPSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRNDSCALEPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRNDSCALEPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRNDSCALESD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRNDSCALESS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VROUNDPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VROUNDPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VROUNDSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VROUNDSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRSQRT14PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRSQRT14PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRSQRT14SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRSQRT14SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRSQRT28PD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRSQRT28PS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRSQRT28SD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRSQRT28SS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRSQRTPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VRSQRTSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSCATTERDPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSCATTERDPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSCATTERPF0DPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSCATTERPF0DPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSCATTERPF0QPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSCATTERPF0QPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSCATTERPF1DPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSCATTERPF1DPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSCATTERPF1QPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSCATTERPF1QPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSCATTERQPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSCATTERQPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSHUFPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSHUFPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSQRTPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSQRTPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSQRTSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSQRTSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSTMXCSR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSUBPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSUBPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSUBSD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VSUBSS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VTESTPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VTESTPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VUNPCKHPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VUNPCKHPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VUNPCKLPD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VUNPCKLPS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VZEROALL);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VZEROUPPER);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_FWAIT, dt_X86_INS_WAIT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, WBINVD);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, WRFSBASE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, WRGSBASE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, WRMSR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XABORT);
/*
 *  LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XACQUIRE);
 */
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XBEGIN);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XCHG);
/*
 *   LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XCRYPTCBC);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XCRYPTCFB);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XCRYPTCTR);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XCRYPTECB);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XCRYPTOFB);
 */
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XEND);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XGETBV);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_XLAT, dt_X86_INS_XLATB);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XRELEASE);
 */
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XRSTOR);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XRSTOR64);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XRSTORS);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XRSTORS64);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XSAVE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XSAVE64);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XSAVEC);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XSAVEC64);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XSAVEOPT);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XSAVEOPT64);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XSAVES);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XSAVES64);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XSETBV);
/*
 *   LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XSHA1);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XSHA256);
 */
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XSTORE);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, XTEST);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FDISI8087_NOP);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, FENI8087_NOP);

        // pseudo instructions
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPSS);
/*
 *   LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPEQSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPLTSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPLESS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPUNORDSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPNEQSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPNLTSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPNLESS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPORDSS);
 */

    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPSD);
    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_CMPSD_XMM, dt_X86_INS_CMPEQSD);
/*
 *   LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPLTSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPLESD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPUNORDSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPNEQSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPNLTSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPNLESD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPORDSD);
 */

    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPPS);
/*
 *   LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPEQPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPLTPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPLEPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPUNORDPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPNEQPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPNLTPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPNLEPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPORDPS);
 */

    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPPD);
/*
 *   LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPEQPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPLTPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPLEPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPUNORDPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPNEQPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPNLTPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPNLEPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, CMPORDPD);
 */

    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPSS);
/*
 *   LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPEQSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPLTSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPLESS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPUNORDSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNEQSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNLTSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNLESS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPORDSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPEQ_UQSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNGESS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNGTSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPFALSESS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNEQ_OQSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPGESS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPGTSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPTRUESS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPEQ_OSSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPLT_OQSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPLE_OQSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPUNORD_SSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNEQ_USSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNLT_UQSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNLE_UQSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPORD_SSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPEQ_USSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNGE_UQSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNGT_UQSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPFALSE_OSSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNEQ_OSSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPGE_OQSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPGT_OQSS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPTRUE_USSS);
 */

    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPSD);
/*
 *   LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPEQSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPLTSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPLESD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPUNORDSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNEQSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNLTSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNLESD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPORDSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPEQ_UQSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNGESD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNGTSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPFALSESD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNEQ_OQSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPGESD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPGTSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPTRUESD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPEQ_OSSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPLT_OQSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPLE_OQSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPUNORD_SSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNEQ_USSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNLT_UQSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNLE_UQSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPORD_SSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPEQ_USSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNGE_UQSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNGT_UQSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPFALSE_OSSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNEQ_OSSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPGE_OQSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPGT_OQSD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPTRUE_USSD);
 */

    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPPS);
/*
 *   LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPEQPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPLTPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPLEPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPUNORDPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNEQPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNLTPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNLEPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPORDPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPEQ_UQPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNGEPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNGTPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPFALSEPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNEQ_OQPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPGEPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPGTPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPTRUEPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPEQ_OSPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPLT_OQPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPLE_OQPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPUNORD_SPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNEQ_USPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNLT_UQPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNLE_UQPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPORD_SPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPEQ_USPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNGE_UQPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNGT_UQPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPFALSE_OSPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNEQ_OSPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPGE_OQPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPGT_OQPS);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPTRUE_USPS);
 */

    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPPD);
/*
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPEQPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPLTPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPLEPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPUNORDPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNEQPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNLTPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNLEPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPORDPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPEQ_UQPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNGEPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNGTPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPFALSEPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNEQ_OQPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPGEPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPGTPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPTRUEPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPEQ_OSPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPLT_OQPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPLE_OQPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPUNORD_SPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNEQ_USPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNLT_UQPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNLE_UQPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPORD_SPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPEQ_USPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNGE_UQPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNGT_UQPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPFALSE_OSPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPNEQ_OSPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPGE_OQPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPGT_OQPD);
 *    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, VCMPTRUE_USPD);
 */

    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, UD0);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ENDBR32);
    LA_X86_INSN_DEF(la_xed_insn_tr, XED_ICLASS_, ENDBR64);

    LA_X86_INSN_REDEF(la_xed_insn_tr, XED_ICLASS_LAST, dt_X86_INS_ENDING);
}
