#include "lazydis.h"
#undef ZYDIS_MNEMONIC_

la_name_enum_t la_zydis_insn_tr[ZYDIS_MNEMONIC_MAX_VALUE + 1];
la_name_enum_t la_zydis_insn_reg[ZYDIS_REGISTER_MAX_VALUE + 1];
ZydisDecoder decoder[2];
uint8_t dt_lazydis_mode;
#ifdef LATX_DISASSEMBLE_TRACE_DEBUG
ZydisFormatter formatter[2];
char lazydis_buffer[256];
ZydisFormatterFunc default_pre_opnd[2];
static ZyanStatus ZydisFormatterFormatFindOpndIndex(
    const ZydisFormatter *formatter,
    ZydisFormatterBuffer *buffer, ZydisFormatterContext *context)
{
    uint32_t *user_data = (uint32_t *)context->user_data;
    if (*user_data == 0) {
        *user_data = strlen(buffer->string.vector.data);
    }
    return ZYAN_STATUS_SUCCESS;
}
#endif
static void handle_prefix(struct la_dt_insn *ret,
    const ZydisDecodedInstruction *instruction) {
    for (ZyanU8 i = 0; i < instruction->raw.prefix_count; ++i) {
        const ZyanU8 value = instruction->raw.prefixes[i].value;
        if ((value & 0xF0) == 0x40) {
            ret->x86.rex = value;
        } else {
            switch (value) {
            case 0xF0:
            case 0xF2:
            case 0xF3:
                ret->x86.prefix[0] = value;
                break;
            case 0x2E:
            case 0x36:
            case 0x3E:
            case 0x26:
            case 0x64:
            case 0x65:
                ret->x86.prefix[1] = value;
                break;
            case 0x66:
                ret->x86.prefix[2] = value;
                break;
            case 0x67:
                ret->x86.prefix[3] = value;
                break;
            default:
                dtassert(0);
            }
        }
    }
}
static int lazydis_operand_type[] = {
    dt_X86_OP_INVALID,
    dt_X86_OP_REG,
    dt_X86_OP_MEM,
    dt_X86_OP_IMM,
    dt_X86_OP_IMM
};
static void handle_Operands(struct la_dt_insn *ret,
    const ZydisDecodedInstruction *instruction,
    const ZydisDecodedOperand *operands)
{
    ZyanU8 imm_id = 0;
    int opcount = 0;
    uint8_t iopc = instruction->operand_count > 5 ?
         5 : instruction->operand_count;
    for (ZyanU8 i = 0; i < iopc; ++i) {
        if (operands[i].visibility == ZYDIS_OPERAND_VISIBILITY_HIDDEN) {
            continue;
        }
        ret->x86.operands[opcount].type =
            lazydis_operand_type[operands[i].type];
        ret->x86.operands[opcount].access = operands[i].actions&0x3;
        ret->x86.operands[opcount].size= operands[i].size / 8;
        switch (operands[i].type) {
        case ZYDIS_OPERAND_TYPE_REGISTER:
            ret->x86.operands[opcount++].reg =
                la_zydis_insn_reg[operands[i].reg.value].id;
            break;
        case ZYDIS_OPERAND_TYPE_MEMORY:
            ret->x86.operands[opcount].mem.segment =
                la_zydis_insn_reg[operands[i].mem.segment].id;
            ret->x86.operands[opcount].mem.base =
                la_zydis_insn_reg[operands[i].mem.base].id;
            ret->x86.operands[opcount].mem.index =
                la_zydis_insn_reg[operands[i].mem.index].id;
            ret->x86.operands[opcount].mem.scale = operands[i].mem.scale;
            if (operands[i].mem.disp.has_displacement) {
                ret->x86.operands[opcount].mem.disp =
                    operands[i].mem.disp.value;
            } else {
                ret->x86.operands[opcount].mem.disp = 0;
            }
            opcount++;
            break;
        case ZYDIS_OPERAND_TYPE_POINTER:
            dtassert(0);
            break;
        case ZYDIS_OPERAND_TYPE_IMMEDIATE:
            if (operands[i].imm.is_signed) {
                ret->x86.operands[opcount].imm = operands[i].imm.value.s;
            } else {
                ret->x86.operands[opcount].imm = operands[i].imm.value.u;
            }
            if (operands[i].imm.is_relative) {
                ret->x86.operands[opcount].imm += ret->address + ret->size;
                ret->x86.operands[opcount].size = dt_lazydis_mode / 8;
            }
            opcount++;
            ++imm_id;
            break;
        case ZYDIS_OPERAND_TYPE_UNUSED:
            continue;
        default:
            dtassert(0);
        }
    }
    ret->x86.op_count = opcount;
    ret->x86.addr_size = instruction->address_width / 8;
}

static struct la_dt_insn *lazydis_get_from_insn(int64_t address,
    const ZydisDecodedInstruction *instruction,
    const ZydisDecodedOperand *operands,
    int ir1_num, void *pir1_base, int mode)
{
    dtassert((instruction != NULL) && (operands != NULL));
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
    uint32_t opnd_index = 0;
    ZydisFormatterFormatInstructionEx(&formatter[mode], instruction, operands,
        instruction->operand_count_visible, &lazydis_buffer[0],
        sizeof(lazydis_buffer), address, &opnd_index);
    if (opnd_index > 0) {
        lazydis_buffer[opnd_index - 1] = '\0';/*skip space*/
        strncpy(ret->op_str, lazydis_buffer + opnd_index, 160);
    }
    strncpy(ret->mnemonic, lazydis_buffer, 32);
#endif
    lsassert(instruction->mnemonic <= ZYDIS_MNEMONIC_MAX_VALUE);
    ret->size = instruction->length;
    ret->id = la_zydis_insn_tr[instruction->mnemonic].id;
    char *mybytes = (char *)(intptr_t)ret->address;
    for (int i = 0; i < ret->size; i++) {
        ret->bytes[i] = mybytes[i];
    }
    if (unlikely(instruction->opcode_map == ZYDIS_OPCODE_MAP_0F)) {
        ret->x86.opcode[0] = 0x0f;
        ret->x86.opcode[1] = instruction->opcode;
    } else {
        ret->x86.opcode[0] = instruction->opcode;
    }
    handle_prefix(ret, instruction);
    handle_Operands(ret, instruction, operands);
    return ret; 
}

int lazydis_get(const uint8_t *code, size_t code_size,
        uint64_t address,
        size_t count, struct la_dt_insn **insn,
        int ir1_num, void *pir1_base, int mode)
{
    struct la_dt_insn *ret;
    ZydisDecodedInstruction instruction = { 0 };
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE] = { { 0 } };
    dtassert(ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder[mode],
        code, code_size, &instruction, operands,
        ZYDIS_MAX_OPERAND_COUNT_VISIBLE, ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY)));
    ret = lazydis_get_from_insn(address,
        &instruction, operands, ir1_num, pir1_base, mode);
    *insn = ret;
    return 1;
}

static void init_insn_id(void);
static void init_insn_reg(void);

void lazydis_init(int abi_bits)
{
    ZydisMachineMode machine_mode[2];
    ZydisStackWidth stack_width[2];
    dt_lazydis_mode = abi_bits;
    machine_mode[0] = ZYDIS_MACHINE_MODE_LONG_COMPAT_32;
    stack_width[0] = ZYDIS_STACK_WIDTH_32;
    machine_mode[1] = ZYDIS_MACHINE_MODE_LONG_64;
    stack_width[1] = ZYDIS_STACK_WIDTH_64;
    ZydisDecoderInit(&decoder[0], machine_mode[0], stack_width[0]);
#ifdef LATX_DISASSEMBLE_TRACE_DEBUG
    ZydisFormatterInit(&formatter[0], ZYDIS_FORMATTER_STYLE_INTEL);
    ZydisFormatterSetProperty(&formatter[0],
        ZYDIS_FORMATTER_PROP_FORCE_SEGMENT, ZYAN_TRUE);
    ZydisFormatterSetProperty(&formatter[0],
        ZYDIS_FORMATTER_PROP_FORCE_SIZE, ZYAN_TRUE);
    default_pre_opnd[0] = (ZydisFormatterFunc)&ZydisFormatterFormatFindOpndIndex;
    ZydisFormatterSetHook(&formatter[0], ZYDIS_FORMATTER_FUNC_PRE_OPERAND,
            (const void **)&default_pre_opnd[0]);
#endif
    ZydisDecoderInit(&decoder[1], machine_mode[1], stack_width[1]);
#ifdef LATX_DISASSEMBLE_TRACE_DEBUG
    ZydisFormatterInit(&formatter[1], ZYDIS_FORMATTER_STYLE_INTEL);
    ZydisFormatterSetProperty(&formatter[1],
        ZYDIS_FORMATTER_PROP_FORCE_SEGMENT, ZYAN_TRUE);
    ZydisFormatterSetProperty(&formatter[1],
        ZYDIS_FORMATTER_PROP_FORCE_SIZE, ZYAN_TRUE);
    default_pre_opnd[1] = (ZydisFormatterFunc)&ZydisFormatterFormatFindOpndIndex;
    ZydisFormatterSetHook(&formatter[1], ZYDIS_FORMATTER_FUNC_PRE_OPERAND,
            (const void **)&default_pre_opnd[1]);
#endif

    init_insn_id();
    init_insn_reg();
}
static void init_insn_reg(void)
{
    LA_X86_INSN_REENUM(la_zydis_insn_reg,
        ZYDIS_REGISTER_NONE, dt_X86_REG_INVALID);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, AH);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, AL);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, AX);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, BH);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, BL);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, BP);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, BPL);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, BX);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CH);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CL);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CS);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CX);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DH);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DI);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DIL);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DL);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DS);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DX);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, EAX);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, EBP);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, EBX);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ECX);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, EDI);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, EDX);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, EFLAGS);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, EIP);
/*
 *    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, EIZ);
 */
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ES);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ESI);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ESP);
/*
 *    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, FPSW);
 */
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, FS);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, GS);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, IP);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, RAX);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, RBP);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, RBX);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, RCX);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, RDI);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, RDX);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, RIP);
/*
 *    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, RIZ);
 */
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, RSI);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, RSP);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, SI);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, SIL);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, SP);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, SPL);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, SS);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CR0);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CR1);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CR2);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CR3);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CR4);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CR5);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CR6);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CR7);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CR8);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CR9);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CR10);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CR11);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CR12);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CR13);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CR14);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, CR15);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DR0);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DR1);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DR2);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DR3);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DR4);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DR5);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DR6);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DR7);
/*
 *    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DR8);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DR9);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DR10);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DR11);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DR12);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DR13);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DR14);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, DR15);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, FP0);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, FP1);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, FP2);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, FP3);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, FP4);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, FP5);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, FP6);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, FP7);
 */
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, K0);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, K1);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, K2);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, K3);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, K4);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, K5);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, K6);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, K7);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, MM0);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, MM1);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, MM2);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, MM3);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, MM4);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, MM5);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, MM6);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, MM7);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R8);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R9);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R10);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R11);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R12);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R13);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R14);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R15);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ST0);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ST1);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ST2);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ST3);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ST4);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ST5);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ST6);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ST7);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM0);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM1);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM2);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM3);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM4);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM5);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM6);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM7);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM8);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM9);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM10);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM11);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM12);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM13);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM14);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM15);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM16);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM17);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM18);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM19);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM20);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM21);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM22);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM23);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM24);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM25);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM26);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM27);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM28);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM29);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM30);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, XMM31);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM0);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM1);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM2);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM3);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM4);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM5);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM6);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM7);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM8);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM9);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM10);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM11);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM12);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM13);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM14);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM15);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM16);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM17);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM18);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM19);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM20);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM21);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM22);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM23);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM24);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM25);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM26);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM27);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM28);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM29);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM30);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, YMM31);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM0);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM1);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM2);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM3);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM4);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM5);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM6);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM7);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM8);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM9);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM10);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM11);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM12);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM13);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM14);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM15);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM16);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM17);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM18);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM19);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM20);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM21);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM22);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM23);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM24);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM25);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM26);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM27);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM28);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM29);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM30);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, ZMM31);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R8B);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R9B);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R10B);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R11B);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R12B);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R13B);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R14B);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R15B);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R8D);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R9D);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R10D);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R11D);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R12D);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R13D);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R14D);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R15D);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R8W);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R9W);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R10W);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R11W);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R12W);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R13W);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R14W);
    LA_X86_INSN_ENUM(la_zydis_insn_reg, ZYDIS_REGISTER_, X86_REG_, R15W);
    LA_X86_INSN_REENUM(la_zydis_insn_reg,
        ZYDIS_REGISTER_MAX_VALUE, dt_X86_REG_ENDING);

}

static void init_insn_id(void)
{
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INVALID);

    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, AAA);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, AAD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, AAM);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, AAS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FABS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ADC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ADCX);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ADD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ADDPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ADDPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ADDSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ADDSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ADDSUBPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ADDSUBPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FADD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FIADD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FADDP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ADOX);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, AESDECLAST);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, AESDEC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, AESENCLAST);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, AESENC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, AESIMC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, AESKEYGENASSIST);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, AND);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ANDN);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ANDNPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ANDNPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ANDPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ANDPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ARPL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BEXTR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BLCFILL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BLCI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BLCIC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BLCMSK);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BLCS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BLENDPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BLENDPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BLENDVPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BLENDVPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BLSFILL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BLSI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BLSIC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BLSMSK);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BLSR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BOUND);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BSF);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BSR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BSWAP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BTC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BTR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BTS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, BZHI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CALL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CDQE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FCHS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CLAC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CLC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CLD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CLFLUSH);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CLFLUSHOPT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CLGI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CLI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CLTS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CLWB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMC);
    LA_X86_INSN_REDEF(la_zydis_insn_tr,
        ZYDIS_MNEMONIC_CMOVNBE, dt_X86_INS_CMOVA);
    LA_X86_INSN_REDEF(la_zydis_insn_tr,
        ZYDIS_MNEMONIC_CMOVNB, dt_X86_INS_CMOVAE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMOVB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMOVBE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FCMOVBE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FCMOVB);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_CMOVZ, dt_X86_INS_CMOVE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FCMOVE);
    LA_X86_INSN_REDEF(la_zydis_insn_tr,
        ZYDIS_MNEMONIC_CMOVNLE, dt_X86_INS_CMOVG);
    LA_X86_INSN_REDEF(la_zydis_insn_tr,
        ZYDIS_MNEMONIC_CMOVNL, dt_X86_INS_CMOVGE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMOVL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMOVLE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FCMOVNBE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FCMOVNB);
    LA_X86_INSN_REDEF(la_zydis_insn_tr,
        ZYDIS_MNEMONIC_CMOVNZ, dt_X86_INS_CMOVNE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FCMOVNE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMOVNO);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMOVNP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FCMOVNU);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMOVNS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMOVO);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMOVP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FCMOVU);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMOVS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPSQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPXCHG16B);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPXCHG);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPXCHG8B);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, COMISD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, COMISS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FCOMP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FCOMIP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FCOMI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FCOM);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FCOS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CPUID);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CQO);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CRC32);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTDQ2PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTDQ2PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTPD2DQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTPD2PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTPS2DQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTPS2PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTSD2SI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTSD2SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTSI2SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTSI2SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTSS2SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTSS2SI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTTPD2DQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTTPS2DQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTTSD2SI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTTSS2SI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CWDE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, DAA);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, DAS);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, DATA16);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, DEC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, DIV);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, DIVPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, DIVPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FDIVR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FIDIVR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FDIVRP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, DIVSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, DIVSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FDIV);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FIDIV);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FDIVP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, DPPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, DPPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RET);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ENCLS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ENCLU);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ENTER);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, EXTRACTPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, EXTRQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, F2XM1);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LCALL);
 */
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LJMP);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FBLD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FBSTP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FCOMPP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FDECSTP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FEMMS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FFREE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FICOM);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FICOMP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FINCSTP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FLDCW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FLDENV);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FLDL2E);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FLDL2T);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FLDLG2);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FLDLN2);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FLDPI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FNCLEX);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FNINIT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FNOP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FNSTCW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FNSTSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FPATAN);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FPREM);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FPREM1);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FPTAN);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FFREEP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FRNDINT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FRSTOR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FNSAVE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FSCALE);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FSETPM);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FSINCOS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FNSTENV);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FXAM);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FXRSTOR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FXRSTOR64);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FXSAVE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FXSAVE64);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FXTRACT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FYL2X);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FYL2XP1);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVAPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVAPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ORPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ORPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVAPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVAPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XORPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XORPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, GETSEC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, HADDPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, HADDPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, HLT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, HSUBPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, HSUBPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, IDIV);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FILD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, IMUL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, IN);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INSERTPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INSERTQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INT1);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INT3);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INTO);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INVD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INVEPT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INVLPG);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INVLPGA);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INVPCID);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, INVVPID);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, IRET);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, IRETD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, IRETQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FISTTP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FIST);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FISTP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, UCOMISD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, UCOMISS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCOMISD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCOMISS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTSD2SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTSI2SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTSI2SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTSS2SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTTSD2SI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTTSD2USI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTTSS2SI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTTSS2USI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTUSI2SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTUSI2SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VUCOMISD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VUCOMISS);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_JNB, dt_X86_INS_JAE);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_JNBE, dt_X86_INS_JA);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, JBE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, JB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, JCXZ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, JECXZ);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_JZ, dt_X86_INS_JE);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_JNL, dt_X86_INS_JGE);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_JNLE, dt_X86_INS_JG);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, JLE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, JL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, JMP);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_JNZ, dt_X86_INS_JNE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, JNO);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, JNP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, JNS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, JO);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, JP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, JRCXZ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, JS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KANDB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KANDD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KANDNB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KANDND);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KANDNQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KANDNW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KANDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KANDW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KMOVB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KMOVD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KMOVQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KMOVW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KNOTB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KNOTD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KNOTQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KNOTW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KORB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KORD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KORQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KORTESTB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KORTESTD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KORTESTQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KORTESTW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KORW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KSHIFTLB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KSHIFTLD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KSHIFTLQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KSHIFTLW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KSHIFTRB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KSHIFTRD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KSHIFTRQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KSHIFTRW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KUNPCKBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KXNORB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KXNORD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KXNORQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KXNORW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KXORB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KXORD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KXORQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, KXORW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LAHF);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LAR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LDDQU);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LDMXCSR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LDS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FLDZ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FLD1);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FLD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LEA);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LEAVE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LES);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LFENCE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LFS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LGDT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LGS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LIDT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LLDT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LMSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, OR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SUB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XOR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LODSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LODSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LODSQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LODSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LOOP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LOOPE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LOOPNE);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RETF);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RETFQ);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LSL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LTR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XADD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, LZCNT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MASKMOVDQU);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MAXPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MAXPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MAXSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MAXSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MFENCE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MINPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MINPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MINSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MINSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTPD2PI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTPI2PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTPI2PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTPS2PI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTTPD2PI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CVTTPS2PI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, EMMS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MASKMOVQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVDQ2Q);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVNTQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVQ2DQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PABSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PABSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PABSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PACKSSDW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PACKSSWB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PACKUSWB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PADDB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PADDD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PADDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PADDSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PADDSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PADDUSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PADDUSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PADDW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PALIGNR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PANDN);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PAND);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PAVGB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PAVGW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PCMPEQB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PCMPEQD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PCMPEQW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PCMPGTB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PCMPGTD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PCMPGTW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PEXTRW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PHADDSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PHADDW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PHADDD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PHSUBD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PHSUBSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PHSUBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PINSRW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMADDUBSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMADDWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMAXSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMAXUB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMINSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMINUB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMOVMSKB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMULHRSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMULHUW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMULHW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMULLW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMULUDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, POR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSADBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSHUFB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSHUFW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSIGNB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSIGND);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSIGNW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSLLD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSLLQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSLLW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSRAD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSRAW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSRLD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSRLQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSRLW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSUBB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSUBD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSUBQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSUBSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSUBSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSUBUSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSUBUSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSUBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PUNPCKHBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PUNPCKHDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PUNPCKHWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PUNPCKLBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PUNPCKLDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PUNPCKLWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PXOR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MONITOR);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MONTMUL);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOV);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVABS);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVBE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVDDUP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVDQA);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVDQU);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVHLPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVHPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVHPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVLHPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVLPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVLPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVMSKPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVMSKPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVNTDQA);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVNTDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVNTI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVNTPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVNTPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVNTSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVNTSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVSHDUP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVSLDUP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVSQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVSX);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVSXD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVUPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVUPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MOVZX);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MPSADBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MUL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MULPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MULPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MULSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MULSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MULX);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FMUL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FIMUL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FMULP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, MWAIT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, NEG);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, NOP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, NOT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, OUT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, OUTSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, OUTSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, OUTSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PACKUSDW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PAUSE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PAVGUSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PBLENDVB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PBLENDW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PCLMULQDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PCMPEQQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PCMPESTRI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PCMPESTRM);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PCMPGTQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PCMPISTRI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PCMPISTRM);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PCOMMIT);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PDEP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PEXT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PEXTRB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PEXTRD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PEXTRQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PF2ID);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PF2IW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFACC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFADD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFCMPEQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFCMPGE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFCMPGT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFMAX);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFMIN);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFMUL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFNACC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFPNACC);
    /*
     *LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFRCPIT1);
     */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFRCPIT2);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFRCP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFRSQIT1);
    /*
     *LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFRSQRT);
     */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFSUBR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PFSUB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PHMINPOSUW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PI2FD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PI2FW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PINSRB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PINSRD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PINSRQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMAXSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMAXSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMAXUD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMAXUW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMINSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMINSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMINUD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMINUW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMOVSXBD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMOVSXBQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMOVSXBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMOVSXDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMOVSXWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMOVSXWQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMOVZXBD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMOVZXBQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMOVZXBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMOVZXDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMOVZXWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMOVZXWQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMULDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMULHRW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PMULLD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, POP);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, POPAW);
 */
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_POPAD, dt_X86_INS_POPAL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, POPCNT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, POPF);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, POPFD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, POPFQ);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PREFETCH);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PREFETCHNTA);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PREFETCHT0);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PREFETCHT1);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PREFETCHT2);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PREFETCHW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSHUFD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSHUFHW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSHUFLW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSLLDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSRLDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PSWAPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PTEST);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PUNPCKHQDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PUNPCKLQDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PUSH);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PUSHAW);
 */
    LA_X86_INSN_REDEF(la_zydis_insn_tr,
        ZYDIS_MNEMONIC_PUSHAD, dt_X86_INS_PUSHAL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PUSHF);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PUSHFD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, PUSHFQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RCL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RCPPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RCPSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RCR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RDFSBASE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RDGSBASE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RDMSR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RDPMC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RDRAND);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RDSEED);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RDTSC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RDTSCP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ROL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ROR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RORX);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ROUNDPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ROUNDPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ROUNDSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ROUNDSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RSM);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RSQRTPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, RSQRTSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SAHF);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SAL);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SALC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SAR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SARX);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SBB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SCASB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SCASD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SCASQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SCASW);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_SETNB, dt_X86_INS_SETAE);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_SETNBE, dt_X86_INS_SETA);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SETBE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SETB);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_SETZ, dt_X86_INS_SETE);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_SETNL, dt_X86_INS_SETGE);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_SETNLE, dt_X86_INS_SETG);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SETLE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SETL);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_SETNZ, dt_X86_INS_SETNE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SETNO);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SETNP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SETNS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SETO);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SETP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SETS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SFENCE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SGDT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SHA1MSG1);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SHA1MSG2);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SHA1NEXTE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SHA1RNDS4);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SHA256MSG1);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SHA256MSG2);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SHA256RNDS2);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SHL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SHLD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SHLX);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SHR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SHRD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SHRX);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SHUFPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SHUFPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SIDT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FSIN);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SKINIT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SLDT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SMSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SQRTPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SQRTPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SQRTSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SQRTSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FSQRT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, STAC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, STC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, STD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, STGI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, STI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, STMXCSR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, STOSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, STOSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, STOSQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, STOSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, STR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FST);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FSTP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FSTPNCE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FXCH);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SUBPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SUBPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FSUBR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FISUBR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FSUBRP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SUBSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SUBSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FSUB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FISUB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FSUBP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SWAPGS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SYSCALL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SYSENTER);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SYSEXIT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, SYSRET);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, T1MSKC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, TEST);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, UD2);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FTST);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, TZCNT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, TZMSK);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FUCOMIP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FUCOMI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FUCOMPP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FUCOMP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FUCOM);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, UD2B);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, UNPCKHPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, UNPCKHPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, UNPCKLPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, UNPCKLPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VADDPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VADDPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VADDSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VADDSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VADDSUBPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VADDSUBPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VAESDECLAST);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VAESDEC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VAESENCLAST);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VAESENC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VAESIMC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VAESKEYGENASSIST);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VALIGND);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VALIGNQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VANDNPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VANDNPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VANDPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VANDPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VBLENDMPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VBLENDMPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VBLENDPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VBLENDPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VBLENDVPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VBLENDVPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VBROADCASTF128);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VBROADCASTI32X4);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VBROADCASTI64X4);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VBROADCASTSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VBROADCASTSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCOMPRESSPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCOMPRESSPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTDQ2PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTDQ2PS);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTPD2DQX);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTPD2DQ);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTPD2PSX);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTPD2PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTPD2UDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTPH2PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTPS2DQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTPS2PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTPS2PH);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTPS2UDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTSD2SI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTSD2USI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTSS2SI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTSS2USI);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTTPD2DQX);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTTPD2DQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTTPD2UDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTTPS2DQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTTPS2UDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTUDQ2PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCVTUDQ2PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VDIVPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VDIVPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VDIVSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VDIVSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VDPPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VDPPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VERR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VERW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VEXP2PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VEXP2PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VEXPANDPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VEXPANDPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VEXTRACTF128);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VEXTRACTF32X4);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VEXTRACTF64X4);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VEXTRACTI128);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VEXTRACTI32X4);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VEXTRACTI64X4);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VEXTRACTPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADD132PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADD132PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADDPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADD213PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADD231PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADDPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADD213PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADD231PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADDSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADD213SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADD132SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADD231SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADDSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADD213SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADD132SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADD231SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADDSUB132PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADDSUB132PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADDSUBPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADDSUB213PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADDSUB231PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADDSUBPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADDSUB213PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMADDSUB231PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUB132PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUB132PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUBADD132PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUBADD132PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUBADDPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUBADD213PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUBADD231PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUBADDPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUBADD213PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUBADD231PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUBPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUB213PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUB231PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUBPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUB213PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUB231PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUBSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUB213SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUB132SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUB231SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUBSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUB213SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUB132SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFMSUB231SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMADD132PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMADD132PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMADDPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMADD213PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMADD231PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMADDPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMADD213PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMADD231PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMADDSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMADD213SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMADD132SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMADD231SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMADDSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMADD213SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMADD132SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMADD231SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMSUB132PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMSUB132PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMSUBPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMSUB213PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMSUB231PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMSUBPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMSUB213PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMSUB231PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMSUBSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMSUB213SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMSUB132SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMSUB231SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMSUBSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMSUB213SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMSUB132SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFNMSUB231SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFRCZPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFRCZPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFRCZSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VFRCZSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VORPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VORPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VXORPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VXORPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VGATHERDPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VGATHERDPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VGATHERPF0DPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VGATHERPF0DPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VGATHERPF0QPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VGATHERPF0QPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VGATHERPF1DPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VGATHERPF1DPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VGATHERPF1QPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VGATHERPF1QPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VGATHERQPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VGATHERQPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VHADDPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VHADDPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VHSUBPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VHSUBPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VINSERTF128);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VINSERTF32X4);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VINSERTF32X8);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VINSERTF64X2);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VINSERTF64X4);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VINSERTI128);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VINSERTI32X4);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VINSERTI32X8);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VINSERTI64X2);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VINSERTI64X4);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VINSERTPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VLDDQU);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VLDMXCSR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMASKMOVDQU);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMASKMOVPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMASKMOVPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMAXPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMAXPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMAXSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMAXSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMCALL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMCLEAR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMFUNC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMINPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMINPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMINSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMINSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMLAUNCH);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMLOAD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMMCALL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVDDUP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVDQA32);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVDQA64);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVDQA);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVDQU16);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVDQU32);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVDQU64);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVDQU8);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVDQU);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVHLPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVHPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVHPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVLHPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVLPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVLPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVMSKPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVMSKPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVNTDQA);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVNTDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVNTPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVNTPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVSHDUP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVSLDUP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVUPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMOVUPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMPSADBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMPTRLD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMPTRST);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMREAD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMRESUME);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMRUN);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMSAVE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMULPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMULPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMULSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMULSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMWRITE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMXOFF);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VMXON);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPABSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPABSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPABSQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPABSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPACKSSDW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPACKSSWB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPACKUSDW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPACKUSWB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPADDB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPADDD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPADDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPADDSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPADDSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPADDUSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPADDUSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPADDW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPALIGNR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPANDD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPANDND);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPANDNQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPANDN);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPANDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPAND);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPAVGB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPAVGW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPBLENDD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPBLENDMB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPBLENDMD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPBLENDMQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPBLENDMW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPBLENDVB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPBLENDW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPBROADCASTB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPBROADCASTD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPBROADCASTMB2Q);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPBROADCASTMW2D);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPBROADCASTQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPBROADCASTW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCLMULQDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMOV);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPEQB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPEQD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPEQQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPEQW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPESTRI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPESTRM);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPGTB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPGTD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPGTQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPGTW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPISTRI);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPISTRM);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPUB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPUD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPUQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPUW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCMPW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCOMB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCOMD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCOMPRESSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCOMPRESSQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCOMQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCOMUB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCOMUD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCOMUQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCOMUW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCOMW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCONFLICTD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPCONFLICTQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERM2F128);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERM2I128);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERMD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERMI2D);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERMI2PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERMI2PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERMI2Q);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERMIL2PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERMIL2PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERMILPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERMILPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERMPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERMPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERMQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERMT2D);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERMT2PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERMT2PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPERMT2Q);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPEXPANDD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPEXPANDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPEXTRB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPEXTRD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPEXTRQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPEXTRW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPGATHERDD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPGATHERDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPGATHERQD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPGATHERQQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHADDBD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHADDBQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHADDBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHADDDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHADDD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHADDSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHADDUBD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHADDUBQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHADDUBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHADDUDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHADDUWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHADDUWQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHADDWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHADDWQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHADDW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHMINPOSUW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHSUBBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHSUBDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHSUBD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHSUBSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHSUBWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPHSUBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPINSRB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPINSRD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPINSRQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPINSRW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPLZCNTD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPLZCNTQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMACSDD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMACSDQH);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMACSDQL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMACSSDD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMACSSDQH);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMACSSDQL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMACSSWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMACSSWW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMACSWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMACSWW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMADCSSWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMADCSWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMADDUBSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMADDWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMASKMOVD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMASKMOVQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMAXSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMAXSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMAXSQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMAXSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMAXUB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMAXUD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMAXUQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMAXUW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMINSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMINSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMINSQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMINSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMINUB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMINUD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMINUQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMINUW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVDB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVDW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVM2B);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVM2D);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVM2Q);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVM2W);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVMSKB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVQB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVQD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVQW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVSDB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVSDW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVSQB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVSQD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVSQW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVSXBD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVSXBQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVSXBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVSXDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVSXWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVSXWQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVUSDB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVUSDW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVUSQB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVUSQD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVUSQW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVZXBD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVZXBQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVZXBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVZXDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVZXWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMOVZXWQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMULDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMULHRSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMULHUW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMULHW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMULLD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMULLQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMULLW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPMULUDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPORD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPORQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPOR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPPERM);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPROTB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPROTD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPROTQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPROTW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSADBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSCATTERDD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSCATTERDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSCATTERQD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSCATTERQQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSHAB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSHAD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSHAQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSHAW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSHLB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSHLD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSHLQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSHLW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSHUFB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSHUFD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSHUFHW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSHUFLW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSIGNB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSIGND);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSIGNW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSLLDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSLLD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSLLQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSLLVD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSLLVQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSLLW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSRAD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSRAQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSRAVD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSRAVQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSRAW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSRLDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSRLD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSRLQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSRLVD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSRLVQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSRLW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSUBB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSUBD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSUBQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSUBSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSUBSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSUBUSB);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSUBUSW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPSUBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPTESTMD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPTESTMQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPTESTNMD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPTESTNMQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPTEST);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPUNPCKHBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPUNPCKHDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPUNPCKHQDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPUNPCKHWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPUNPCKLBW);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPUNPCKLDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPUNPCKLQDQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPUNPCKLWD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPXORD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPXORQ);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VPXOR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRCP14PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRCP14PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRCP14SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRCP14SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRCP28PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRCP28PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRCP28SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRCP28SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRCPPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRCPSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRNDSCALEPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRNDSCALEPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRNDSCALESD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRNDSCALESS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VROUNDPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VROUNDPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VROUNDSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VROUNDSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRSQRT14PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRSQRT14PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRSQRT14SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRSQRT14SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRSQRT28PD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRSQRT28PS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRSQRT28SD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRSQRT28SS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRSQRTPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VRSQRTSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSCATTERDPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSCATTERDPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSCATTERPF0DPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSCATTERPF0DPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSCATTERPF0QPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSCATTERPF0QPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSCATTERPF1DPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSCATTERPF1DPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSCATTERPF1QPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSCATTERPF1QPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSCATTERQPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSCATTERQPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSHUFPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSHUFPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSQRTPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSQRTPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSQRTSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSQRTSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSTMXCSR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSUBPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSUBPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSUBSD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VSUBSS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VTESTPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VTESTPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VUNPCKHPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VUNPCKHPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VUNPCKLPD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VUNPCKLPS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VZEROALL);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VZEROUPPER);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_FWAIT, dt_X86_INS_WAIT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, WBINVD);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, WRFSBASE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, WRGSBASE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, WRMSR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XABORT);
/*
 *  LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XACQUIRE);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XBEGIN);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XCHG);
/*
 *   LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XCRYPTCBC);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XCRYPTCFB);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XCRYPTCTR);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XCRYPTECB);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XCRYPTOFB);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XEND);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XGETBV);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_XLAT, dt_X86_INS_XLATB);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XRELEASE);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XRSTOR);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XRSTOR64);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XRSTORS);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XRSTORS64);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XSAVE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XSAVE64);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XSAVEC);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XSAVEC64);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XSAVEOPT);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XSAVEOPT64);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XSAVES);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XSAVES64);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XSETBV);
/*
 *   LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XSHA1);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XSHA256);
 */
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XSTORE);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, XTEST);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FDISI8087_NOP);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, FENI8087_NOP);

    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPSS);
/*
 *   LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPEQSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPLTSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPLESS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPUNORDSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPNEQSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPNLTSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPNLESS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPORDSS);
 */

    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPSD);
/*
 *   LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPLTSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPLESD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPUNORDSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPNEQSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPNLTSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPNLESD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPORDSD);
 */

    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPPS);
/*
 *   LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPEQPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPLTPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPLEPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPUNORDPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPNEQPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPNLTPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPNLEPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPORDPS);
 */

    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPPD);
/*
 *   LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPEQPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPLTPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPLEPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPUNORDPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPNEQPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPNLTPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPNLEPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, CMPORDPD);
 */

    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPSS);
/*
 *   LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPEQSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPLTSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPLESS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPUNORDSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNEQSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNLTSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNLESS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPORDSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPEQ_UQSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNGESS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNGTSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPFALSESS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNEQ_OQSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPGESS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPGTSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPTRUESS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPEQ_OSSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPLT_OQSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPLE_OQSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPUNORD_SSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNEQ_USSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNLT_UQSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNLE_UQSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPORD_SSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPEQ_USSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNGE_UQSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNGT_UQSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPFALSE_OSSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNEQ_OSSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPGE_OQSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPGT_OQSS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPTRUE_USSS);
 */

    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPSD);
/*
 *   LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPEQSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPLTSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPLESD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPUNORDSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNEQSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNLTSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNLESD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPORDSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPEQ_UQSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNGESD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNGTSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPFALSESD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNEQ_OQSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPGESD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPGTSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPTRUESD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPEQ_OSSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPLT_OQSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPLE_OQSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPUNORD_SSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNEQ_USSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNLT_UQSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNLE_UQSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPORD_SSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPEQ_USSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNGE_UQSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNGT_UQSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPFALSE_OSSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNEQ_OSSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPGE_OQSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPGT_OQSD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPTRUE_USSD);
 */

    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPPS);
/*
 *   LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPEQPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPLTPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPLEPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPUNORDPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNEQPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNLTPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNLEPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPORDPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPEQ_UQPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNGEPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNGTPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPFALSEPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNEQ_OQPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPGEPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPGTPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPTRUEPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPEQ_OSPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPLT_OQPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPLE_OQPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPUNORD_SPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNEQ_USPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNLT_UQPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNLE_UQPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPORD_SPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPEQ_USPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNGE_UQPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNGT_UQPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPFALSE_OSPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNEQ_OSPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPGE_OQPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPGT_OQPS);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPTRUE_USPS);
 */

    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPPD);
/*
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPEQPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPLTPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPLEPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPUNORDPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNEQPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNLTPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNLEPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPORDPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPEQ_UQPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNGEPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNGTPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPFALSEPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNEQ_OQPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPGEPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPGTPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPTRUEPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPEQ_OSPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPLT_OQPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPLE_OQPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPUNORD_SPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNEQ_USPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNLT_UQPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNLE_UQPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPORD_SPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPEQ_USPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNGE_UQPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNGT_UQPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPFALSE_OSPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPNEQ_OSPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPGE_OQPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPGT_OQPD);
 *    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, VCMPTRUE_USPD);
 */

    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, UD0);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ENDBR32);
    LA_X86_INSN_DEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_, ENDBR64);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_ENDBR32, dt_X86_INS_NOP);
    LA_X86_INSN_REDEF(la_zydis_insn_tr, ZYDIS_MNEMONIC_ENDBR64, dt_X86_INS_NOP);

    LA_X86_INSN_REDEF(la_zydis_insn_tr,
        ZYDIS_MNEMONIC_MAX_VALUE, dt_X86_INS_ENDING);
}
