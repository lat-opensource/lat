/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_IR1_DECODE_H
#define LATX_IR1_DECODE_H

#define IR1_DECODE_TEMPLATE_CACHE_SIZE 16384

/*
 * Translation decodes repeated position-independent instruction encodings.
 * Keep exact decoded instructions as templates after the hot decoder declines
 * them, and retain the full decoder for unsupported forms and cache misses.
 */
typedef struct Ir1DecodeTemplate {
    uint8_t mode;
    uint8_t size;
    uint8_t bytes[15];
    struct la_dt_insn decoded;
} Ir1DecodeTemplate;

static __thread Ir1DecodeTemplate *ir1_decode_templates;
static __thread uint8_t *ir1_decode_template_valid;
static GPrivate ir1_decode_templates_owner = G_PRIVATE_INIT(g_free);
static GPrivate ir1_decode_template_valid_owner = G_PRIVATE_INIT(g_free);

static uint32_t ir1_decode_template_hash(const uint8_t *code, size_t size,
                                         int mode)
{
    uint32_t hash = 2166136261u ^ (uint32_t)mode;
    for (size_t i = 0; i < size; i++) {
        hash = (hash ^ code[i]) * 16777619u;
    }
    return hash & (IR1_DECODE_TEMPLATE_CACHE_SIZE - 1);
}

static struct la_dt_insn *ir1_decode_template_output(int ir1_num,
                                                     void *pir1_base)
{
    return (void *)((uintptr_t)pir1_base +
                    ir1_num * sizeof(struct la_dt_insn));
}

/*
 * Decode the common, regular x86-64 forms directly.  This path runs before the
 * cache, has no lookup or warm-up cost, and rejects prefixes and forms that it
 * does not model exactly.  Rejected instructions continue through the exact
 * cache and then the full decoder.
 */
static const dt_x86_reg ir1_hot_gpr64[16] = {
    dt_X86_REG_RAX, dt_X86_REG_RCX, dt_X86_REG_RDX, dt_X86_REG_RBX,
    dt_X86_REG_RSP, dt_X86_REG_RBP, dt_X86_REG_RSI, dt_X86_REG_RDI,
    dt_X86_REG_R8, dt_X86_REG_R9, dt_X86_REG_R10, dt_X86_REG_R11,
    dt_X86_REG_R12, dt_X86_REG_R13, dt_X86_REG_R14, dt_X86_REG_R15,
};

static const dt_x86_reg ir1_hot_gpr32[16] = {
    dt_X86_REG_EAX, dt_X86_REG_ECX, dt_X86_REG_EDX, dt_X86_REG_EBX,
    dt_X86_REG_ESP, dt_X86_REG_EBP, dt_X86_REG_ESI, dt_X86_REG_EDI,
    dt_X86_REG_R8D, dt_X86_REG_R9D, dt_X86_REG_R10D, dt_X86_REG_R11D,
    dt_X86_REG_R12D, dt_X86_REG_R13D, dt_X86_REG_R14D, dt_X86_REG_R15D,
};

static int64_t ir1_hot_read_simm(const uint8_t *code, uint8_t size)
{
    if (size == 1) {
        return *(const int8_t *)code;
    }
    int32_t value;
    memcpy(&value, code, sizeof(value));
    return value;
}

static void ir1_hot_finish(struct la_dt_insn *info, const uint8_t *code,
                           uint8_t size, uint64_t address, uint8_t rex,
                           uint8_t opcode0, uint8_t opcode1)
{
    info->size = size;
    info->address = address;
    info->x86.addr_size = 8;
    info->x86.rex = rex;
    info->x86.opcode[0] = opcode0;
    info->x86.opcode[1] = opcode1;
    memcpy(info->bytes, code, size);
#ifdef CONFIG_LATX_CAPSTONE_OP_ACCESS
    for (int i = 0; i < info->x86.op_count; i++) {
        dt_cs_x86_op *operand = &info->x86.operands[i];
        if (operand->type != dt_X86_OP_IMM) {
            operand->access = dt_CS_AC_READ;
        }
    }
    switch (info->id) {
    case dt_X86_INS_MOV:
    case dt_X86_INS_LEA:
    case dt_X86_INS_POP:
        info->x86.operands[0].access = dt_CS_AC_WRITE;
        break;
    case dt_X86_INS_ADD:
    case dt_X86_INS_OR:
    case dt_X86_INS_ADC:
    case dt_X86_INS_SBB:
    case dt_X86_INS_AND:
    case dt_X86_INS_SUB:
    case dt_X86_INS_XOR:
        info->x86.operands[0].access = dt_CS_AC_READ | dt_CS_AC_WRITE;
        break;
    default:
        break;
    }
#endif
}

static uint8_t ir1_hot_decode_rm(const uint8_t *code, uint8_t offset,
                                 uint8_t rex, uint8_t width,
                                 dt_cs_x86_op *operand)
{
    uint8_t modrm = code[offset++];
    uint8_t mod = modrm >> 6;
    uint8_t rm = modrm & 7;
    if (mod == 3) {
        const dt_x86_reg *regs = width == 8 ? ir1_hot_gpr64 : ir1_hot_gpr32;
        operand->type = dt_X86_OP_REG;
        operand->size = width;
        operand->reg = regs[rm | ((rex & 1) << 3)];
        return offset;
    }

    operand->type = dt_X86_OP_MEM;
    operand->size = width;
    operand->mem.segment = dt_X86_REG_INVALID;
    operand->mem.default_segment = dt_X86_REG_INVALID;
    operand->mem.base = dt_X86_REG_INVALID;
    operand->mem.index = dt_X86_REG_INVALID;
    operand->mem.scale = 1;
    operand->mem.disp = 0;

    bool disp32 = false;
    if (rm == 4) {
        uint8_t sib = code[offset++];
        uint8_t index = (sib >> 3) & 7;
        uint8_t base = sib & 7;
        operand->mem.scale = 1 << (sib >> 6);
        if (index != 4 || (rex & 2)) {
            operand->mem.index = ir1_hot_gpr64[index | ((rex & 2) << 2)];
        }
        if (mod == 0 && base == 5) {
            disp32 = true;
        } else {
            operand->mem.base = ir1_hot_gpr64[base | ((rex & 1) << 3)];
        }
        /* Match Capstone's explicit zero-index marker for redundant SIBs. */
        if (operand->mem.index == dt_X86_REG_INVALID &&
            (operand->mem.scale != 1 ||
             (operand->mem.base != dt_X86_REG_INVALID &&
              operand->mem.base != dt_X86_REG_RSP &&
              operand->mem.base != dt_X86_REG_R12))) {
            operand->mem.index = dt_X86_REG_RIZ;
        }
    } else if (mod == 0 && rm == 5) {
        operand->mem.base = dt_X86_REG_RIP;
        disp32 = true;
    } else {
        operand->mem.base = ir1_hot_gpr64[rm | ((rex & 1) << 3)];
    }
    if (mod == 1) {
        operand->mem.disp = ir1_hot_read_simm(code + offset, 1);
        offset++;
    } else if (mod == 2 || disp32) {
        operand->mem.disp = ir1_hot_read_simm(code + offset, 4);
        offset += 4;
    }
    return offset;
}

static struct la_dt_insn *ir1_hot_decode(const uint8_t *code,
                                         uint64_t address, int ir1_num,
                                         void *pir1_base, int mode)
{
    static const unsigned int jcc[16] = {
        dt_X86_INS_JO, dt_X86_INS_JNO, dt_X86_INS_JB, dt_X86_INS_JAE,
        dt_X86_INS_JE, dt_X86_INS_JNE, dt_X86_INS_JBE, dt_X86_INS_JA,
        dt_X86_INS_JS, dt_X86_INS_JNS, dt_X86_INS_JP, dt_X86_INS_JNP,
        dt_X86_INS_JL, dt_X86_INS_JGE, dt_X86_INS_JLE, dt_X86_INS_JG,
    };
    /* Other backends and debug disassembly retain their complete metadata. */
#if !defined(CONFIG_LATX_CAPSTONE_GIT) || defined(CONFIG_LATX_DEBUG)
    return NULL;
#endif
    if (mode != 1 || !pir1_base) {
        return NULL;
    }
    uint8_t offset = 0;
    uint8_t rex = 0;
    if (code[offset] >= 0x40 && code[offset] <= 0x4f) {
        rex = code[offset++];
    }
    uint8_t opcode = code[offset++];
    struct la_dt_insn *info;

    if (!rex && opcode >= 0x70 && opcode <= 0x7f) {
        info = ir1_decode_template_output(ir1_num, pir1_base);
        memset(info, 0, sizeof(*info));
        info->id = jcc[opcode & 0xf];
        info->x86.op_count = 1;
        info->x86.operands[0].type = dt_X86_OP_IMM;
        info->x86.operands[0].size = 8;
        info->x86.operands[0].imm = address + 2 +
                                     ir1_hot_read_simm(code + 1, 1);
        ir1_hot_finish(info, code, 2, address, 0, opcode, 0);
        return info;
    }
    if (!rex && opcode == 0x0f && code[1] >= 0x80 && code[1] <= 0x8f) {
        info = ir1_decode_template_output(ir1_num, pir1_base);
        memset(info, 0, sizeof(*info));
        info->id = jcc[code[1] & 0xf];
        info->x86.op_count = 1;
        info->x86.operands[0].type = dt_X86_OP_IMM;
        info->x86.operands[0].size = 8;
        info->x86.operands[0].imm = address + 6 +
                                     ir1_hot_read_simm(code + 2, 4);
        ir1_hot_finish(info, code, 6, address, 0, 0x0f, code[1]);
        return info;
    }
    if (!rex && (opcode == 0xe8 || opcode == 0xe9 || opcode == 0xeb)) {
        uint8_t imm_size = opcode == 0xeb ? 1 : 4;
        uint8_t size = 1 + imm_size;
        info = ir1_decode_template_output(ir1_num, pir1_base);
        memset(info, 0, sizeof(*info));
        info->id = opcode == 0xe8 ? dt_X86_INS_CALL : dt_X86_INS_JMP;
        info->x86.op_count = 1;
        info->x86.operands[0].type = dt_X86_OP_IMM;
        info->x86.operands[0].size = 8;
        info->x86.operands[0].imm = address + size +
                                     ir1_hot_read_simm(code + 1, imm_size);
        ir1_hot_finish(info, code, size, address, 0, opcode, 0);
        return info;
    }
    if (!rex && (opcode == 0x90 || opcode == 0xc3)) {
        info = ir1_decode_template_output(ir1_num, pir1_base);
        memset(info, 0, sizeof(*info));
        info->id = opcode == 0x90 ? dt_X86_INS_NOP : dt_X86_INS_RET;
        ir1_hot_finish(info, code, 1, address, 0, opcode, 0);
        return info;
    }
    if (opcode >= 0x50 && opcode <= 0x5f && !(rex & 0x0e)) {
        unsigned int reg = (opcode & 7) | ((rex & 1) << 3);
        info = ir1_decode_template_output(ir1_num, pir1_base);
        memset(info, 0, sizeof(*info));
        info->id = opcode < 0x58 ? dt_X86_INS_PUSH : dt_X86_INS_POP;
        info->x86.op_count = 1;
        info->x86.operands[0].type = dt_X86_OP_REG;
        info->x86.operands[0].size = 8;
        info->x86.operands[0].reg = ir1_hot_gpr64[reg];
        ir1_hot_finish(info, code, offset, address, rex, opcode, 0);
        return info;
    }
    if (opcode >= 0xb8 && opcode <= 0xbf) {
        uint8_t width = rex & 8 ? 8 : 4;
        uint64_t imm = 0;
        memcpy(&imm, code + offset, width);
        unsigned int reg = (opcode & 7) | ((rex & 1) << 3);
        const dt_x86_reg *regs = width == 8 ? ir1_hot_gpr64 : ir1_hot_gpr32;
        info = ir1_decode_template_output(ir1_num, pir1_base);
        memset(info, 0, sizeof(*info));
        info->id = dt_X86_INS_MOV;
        info->x86.op_count = 2;
        info->x86.operands[0].type = dt_X86_OP_REG;
        info->x86.operands[0].size = width;
        info->x86.operands[0].reg = regs[reg];
        info->x86.operands[1].type = dt_X86_OP_IMM;
        info->x86.operands[1].size = width;
        info->x86.operands[1].imm = imm;
        ir1_hot_finish(info, code, offset + width, address, rex, opcode, 0);
        return info;
    }

    if (opcode == 0x83) {
        uint8_t modrm = code[offset];
        uint8_t group = (modrm >> 3) & 7;
        static const unsigned int group_ids[8] = {
            dt_X86_INS_ADD, dt_X86_INS_OR, dt_X86_INS_ADC, dt_X86_INS_SBB,
            dt_X86_INS_AND, dt_X86_INS_SUB, dt_X86_INS_XOR, dt_X86_INS_CMP,
        };
        uint8_t width = rex & 8 ? 8 : 4;
        dt_cs_x86_op rm_operand = { 0 };
        uint8_t end = ir1_hot_decode_rm(code, offset, rex, width, &rm_operand);
        info = ir1_decode_template_output(ir1_num, pir1_base);
        memset(info, 0, sizeof(*info));
        info->id = group_ids[group];
        info->x86.modrm = modrm;
        info->x86.op_count = 2;
        info->x86.operands[0] = rm_operand;
        info->x86.operands[1].type = dt_X86_OP_IMM;
        info->x86.operands[1].size = width;
        int64_t imm = ir1_hot_read_simm(code + end, 1);
        bool logical = group == 1 || group == 4 || group == 6;
        info->x86.operands[1].imm = width == 4 && logical ?
                                     (uint32_t)imm : imm;
        ir1_hot_finish(info, code, end + 1, address, rex, opcode, 0);
        return info;
    }

    uint8_t modrm = code[offset];
    bool reverse = false;
    bool lea = false;
    unsigned int id;
    switch (opcode) {
    case 0x89:
        id = dt_X86_INS_MOV;
        break;
    case 0x8b:
        id = dt_X86_INS_MOV;
        reverse = true;
        break;
    case 0x8d:
        id = dt_X86_INS_LEA;
        reverse = true;
        lea = true;
        break;
    case 0x85:
        id = dt_X86_INS_TEST;
        break;
    case 0x31:
        id = dt_X86_INS_XOR;
        break;
    case 0x33:
        id = dt_X86_INS_XOR;
        reverse = true;
        break;
    case 0x39:
        id = dt_X86_INS_CMP;
        break;
    case 0x3b:
        id = dt_X86_INS_CMP;
        reverse = true;
        break;
    case 0x01:
        id = dt_X86_INS_ADD;
        break;
    case 0x03:
        id = dt_X86_INS_ADD;
        reverse = true;
        break;
    case 0x29:
        id = dt_X86_INS_SUB;
        break;
    case 0x2b:
        id = dt_X86_INS_SUB;
        reverse = true;
        break;
    case 0x21:
        id = dt_X86_INS_AND;
        break;
    case 0x23:
        id = dt_X86_INS_AND;
        reverse = true;
        break;
    case 0x09:
        id = dt_X86_INS_OR;
        break;
    case 0x0b:
        id = dt_X86_INS_OR;
        reverse = true;
        break;
    default: return NULL;
    }
    unsigned int reg_index = ((modrm >> 3) & 7) | ((rex & 4) << 1);
    uint8_t width = rex & 8 ? 8 : 4;
    const dt_x86_reg *regs = width == 8 ? ir1_hot_gpr64 : ir1_hot_gpr32;
    dt_cs_x86_op rm_operand = { 0 };
    uint8_t end = ir1_hot_decode_rm(code, offset, rex, width, &rm_operand);
    if (lea && rm_operand.type != dt_X86_OP_MEM) {
        return NULL;
    }
    /* Leave memory TEST's backend-specific access metadata to Capstone. */
    if (id == dt_X86_INS_TEST && rm_operand.type == dt_X86_OP_MEM) {
        return NULL;
    }
    dt_cs_x86_op reg_operand = { 0 };
    reg_operand.type = dt_X86_OP_REG;
    reg_operand.size = width;
    reg_operand.reg = regs[reg_index];
    info = ir1_decode_template_output(ir1_num, pir1_base);
    memset(info, 0, sizeof(*info));
    info->id = id;
    info->x86.modrm = modrm;
    info->x86.op_count = 2;
    info->x86.operands[0] = reverse ? reg_operand : rm_operand;
    info->x86.operands[1] = reverse ? rm_operand : reg_operand;
    ir1_hot_finish(info, code, end, address, rex, opcode, 0);
    return info;
}

static struct la_dt_insn *ir1_decode_template_lookup(const uint8_t *code,
                                                     size_t code_size,
                                                     uint64_t address,
                                                     int ir1_num,
                                                     void *pir1_base,
                                                     int mode)
{
    if (!ir1_decode_templates || !pir1_base) {
        return NULL;
    }
    size_t prefixes = MIN(code_size, 4);
    for (size_t size = 1; size <= prefixes; size++) {
        size_t index = ir1_decode_template_hash(code, size, mode);
        Ir1DecodeTemplate *entry = &ir1_decode_templates[index];
        if (ir1_decode_template_valid[index] && entry->mode == mode &&
            entry->size <= code_size &&
            !memcmp(entry->bytes, code, entry->size)) {
            struct la_dt_insn *output = ir1_decode_template_output(
                ir1_num, pir1_base);
            memcpy(output, &entry->decoded, sizeof(*output));
            output->address = address;
            return output;
        }
    }
    return NULL;
}

static bool ir1_decode_template_is_address_independent(
        const struct la_dt_insn *info)
{
    /* Relative immediates and RIP-relative memory change with the guest PC. */
    for (int i = 0; i < info->x86.op_count; i++) {
        if (info->x86.operands[i].type == dt_X86_OP_IMM ||
            (info->x86.operands[i].type == dt_X86_OP_MEM &&
             (info->x86.operands[i].mem.base == dt_X86_REG_RIP ||
              info->x86.operands[i].mem.index == dt_X86_REG_RIP))) {
            return false;
        }
    }
    return true;
}

static void ir1_decode_template_insert(const uint8_t *code, size_t code_size,
                                       int mode,
                                       const struct la_dt_insn *info)
{
    if (!info->size || info->size > code_size || info->size > 15 ||
        !ir1_decode_template_is_address_independent(info)) {
        return;
    }
    if (!ir1_decode_templates) {
        ir1_decode_templates = g_new(Ir1DecodeTemplate,
                                     IR1_DECODE_TEMPLATE_CACHE_SIZE);
        ir1_decode_template_valid = g_new0(uint8_t,
                                           IR1_DECODE_TEMPLATE_CACHE_SIZE);
        g_private_set(&ir1_decode_templates_owner, ir1_decode_templates);
        g_private_set(&ir1_decode_template_valid_owner,
                      ir1_decode_template_valid);
    }
    size_t prefix_size = MIN((size_t)info->size, 4);
    size_t index = ir1_decode_template_hash(code, prefix_size, mode);
    Ir1DecodeTemplate *entry = &ir1_decode_templates[index];
    entry->mode = mode;
    entry->size = info->size;
    memcpy(entry->bytes, code, info->size);
    memcpy(&entry->decoded, info, sizeof(entry->decoded));
    ir1_decode_template_valid[index] = true;
}

#endif /* LATX_IR1_DECODE_H */
