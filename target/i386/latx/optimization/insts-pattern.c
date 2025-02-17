/**
 * @file insts-pattern.c
 * @author huqi <spcreply@outlook.com>
 *         liuchaoyi <lcy285183897@gmail.com>
 * @brief insts-ptn optimization
 */
#include "lsenv.h"
#include "reg-alloc.h"
#include "translate.h"
#include "insts-pattern.h"

#ifdef CONFIG_LATX_INSTS_PATTERN

#define WRAP(ins) (dt_X86_INS_##ins)
#define BUF_OP(buf, i) (ir1_opcode(buf[i]))
#define BUF_CHK(buf, i, ret) \
    do {                     \
        if (!buf[i])         \
            return ret;      \
    } while (0)

static inline bool in_pattern_list(IR1_INST *ir1)
{
    IR1_OPCODE opcode = ir1_opcode(ir1);
    switch (opcode) {
#define PATTERN_TAIL
#define PATTERN_HEADER
#include "insts_pattern_table.h"
#undef PATTERN_HEADER
#undef PATTERN_TAIL
        return true;
        break;

    default:
        break;
    }
    return false;
}

static inline bool in_pattern_header(IR1_INST *ir1)
{
    IR1_OPCODE opcode = ir1_opcode(ir1);
    switch (opcode) {
#define PATTERN_HEADER
#include "insts_pattern_table.h"
#undef PATTERN_HEADER
        return true;
        break;

    default:
        break;
    }
    return false;
}

static inline void pattern_push(IR1_INST *ir1, IR1_INST *scan_buf[PTN_BUF_SIZE])
{
    /* shift right, drop the last one */
    for (int i = PTN_BUF_SIZE - 1; i > 0; --i) {
        scan_buf[i] = scan_buf[i - 1];
    }
    scan_buf[0] = ir1;
}

static inline void pattern_clear(IR1_INST *scan_buf[PTN_BUF_SIZE])
{
    memset(scan_buf, 0, sizeof(IR1_INST *) * PTN_BUF_SIZE);
}

static inline void pattern_invalid(IR1_INST *scan_buf[PTN_BUF_SIZE], int num)
{
    assert(num < PTN_BUF_SIZE);
    for (int i = 0; i <= num; ++i) {
        scan_buf[i]->cflag |= IR1_INVALID_MASK | IR1_PATTERN_MASK;
    }
}

static inline void pattern_modify(IR1_INST *ir1, IR1_OPCODE opcode)
{
    ir1->info->id = opcode;
    ir1->cflag |= IR1_PATTERN_MASK;
}

static bool is_contain_edx(IR1_OPND *opnd)
{
    if (ir1_opnd_is_gpr(opnd)) {
        switch (opnd->reg) {
        case dt_X86_REG_DL:    case dt_X86_REG_DH:
        case dt_X86_REG_DX:    case dt_X86_REG_EDX:
        case dt_X86_REG_RDX:
            return true;
        default:
            break;
        }
    } else if (ir1_opnd_is_mem(opnd)) {
        switch (opnd->mem.base) {
        case dt_X86_REG_DL:    case dt_X86_REG_DH:
        case dt_X86_REG_DX:    case dt_X86_REG_EDX:
        case dt_X86_REG_RDX:
            return true;
        default:
            break;
        }
        switch (opnd->mem.index) {
        case dt_X86_REG_DL:    case dt_X86_REG_DH:
        case dt_X86_REG_DX:    case dt_X86_REG_EDX:
        case dt_X86_REG_RDX:
            return true;
        default:
            break;
        }
    }
    return false;
}

static inline bool get_pattern_list(IR1_INST *ir1,
                                    IR1_INST *scan_buf[PTN_BUF_SIZE], int *ptr,
                                    IR1_OPCODE *opcode)
{
    assert(PTN_BUF_SIZE > 0);
    IR1_OPND *opnd0;
    IR1_OPND *opnd1;
    switch (ir1_opcode(ir1)) {
    case WRAP(CMP):
        BUF_CHK(scan_buf, 0, false);
        switch (BUF_OP(scan_buf, 0)) {
        case WRAP(JB):
        case WRAP(JAE):
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JBE):
        case WRAP(JA):
        case WRAP(JL):
        case WRAP(JGE):
        case WRAP(JLE):
        case WRAP(JG):
            *ptr = 0;
            *opcode = WRAP(CMP_JCC);
            return true;
            break;
        case WRAP(SBB):
            opnd0 = ir1_get_opnd(scan_buf[0], 0);
            opnd1 = ir1_get_opnd(scan_buf[0], 1);
            /* sbb check */
            if (!ir1_opnd_is_same_reg(opnd0, opnd1)) {
                break;
            }
            *ptr = 0;
            *opcode = WRAP(CMP_SBB);
            return true;
            break;
        default:
            break;
        }
        break;
    case WRAP(SUB):
        BUF_CHK(scan_buf, 0, false);
        switch (BUF_OP(scan_buf, 0)) {
        case WRAP(JB):
        case WRAP(JAE):
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JBE):
        case WRAP(JA):
        case WRAP(JL):
        case WRAP(JGE):
        case WRAP(JLE):
        case WRAP(JG):
            *ptr = 0;
            *opcode = WRAP(SUB_JCC);
            return true;
            break;
        default:
            break;
        }
        break;
    case WRAP(TEST):
        BUF_CHK(scan_buf, 0, false);
        opnd0 = ir1_get_opnd(ir1, 0);
        opnd1 = ir1_get_opnd(ir1, 1);
        if (ir1_opnd_is_same_reg(opnd0, opnd1)) {
            switch (BUF_OP(scan_buf, 0)) {
            case WRAP(JE):
            case WRAP(JNE):
            case WRAP(JS):
            case WRAP(JNS):
            case WRAP(JLE):
            case WRAP(JG):
            case WRAP(JNO):
            case WRAP(JO):
            case WRAP(JB):
            case WRAP(JBE):
            case WRAP(JA):
            case WRAP(JAE):
                *ptr = 0;
                *opcode = WRAP(TEST_JCC);
                return true;
                break;

            default:
                break;
            }
        }
        break;
#ifdef CONFIG_LATX_XCOMISX_OPT
    case WRAP(COMISD):
        BUF_CHK(scan_buf, 0, false);
        switch (BUF_OP(scan_buf, 0)) {
        case WRAP(JA):
        case WRAP(JAE):
        case WRAP(JB):
        case WRAP(JBE):
        case WRAP(JNE):
        case WRAP(JE):
        case WRAP(JL):
        case WRAP(JGE):
        case WRAP(JLE):
        case WRAP(JG):
            *ptr = 0;
            *opcode = WRAP(COMISD_JCC);
            return true;
            break;
        default:
            break;
        }
        break;
    case WRAP(COMISS):
        BUF_CHK(scan_buf, 0, false);
        switch (BUF_OP(scan_buf, 0)) {
        case WRAP(JA):
        case WRAP(JAE):
        case WRAP(JB):
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JBE):
        case WRAP(JL):
        case WRAP(JGE):
        case WRAP(JLE):
        case WRAP(JG):
            *ptr = 0;
            *opcode = WRAP(COMISS_JCC);
            return true;
            break;
        default:
            break;
        }
        break;
    case WRAP(UCOMISD):
        BUF_CHK(scan_buf, 0, false);
        switch (BUF_OP(scan_buf, 0)) {
        case WRAP(JA):
        case WRAP(JAE):
        case WRAP(JB):
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JBE):
        case WRAP(JL):
        case WRAP(JGE):
        case WRAP(JLE):
        case WRAP(JG):
            *ptr = 0;
            *opcode = WRAP(UCOMISD_JCC);
            return true;
            break;
        default:
            break;
        }
        break;
    case WRAP(UCOMISS):
        BUF_CHK(scan_buf, 0, false);
        switch (BUF_OP(scan_buf, 0)) {
        case WRAP(JA):
        case WRAP(JAE):
        case WRAP(JB):
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JBE):
        case WRAP(JL):
        case WRAP(JGE):
        case WRAP(JLE):
        case WRAP(JG):
            *ptr = 0;
            *opcode = WRAP(UCOMISS_JCC);
            return true;
            break;
        default:
            break;
        }
        break;
#endif
    case WRAP(BT):
        BUF_CHK(scan_buf, 0, false);
        switch (BUF_OP(scan_buf, 0)) {
        case WRAP(JB):
        case WRAP(JAE):
            *ptr = 0;
            *opcode = WRAP(BT_JCC);
            return true;
            break;
        default:
            break;
        }
        break;
    case WRAP(CQO):
        BUF_CHK(scan_buf, 0, false);
        switch (BUF_OP(scan_buf, 0)) {
        case WRAP(IDIV):
            opnd0 = ir1_get_opnd(scan_buf[0], 0);
            if (!ir1_opnd_is_gpr(opnd0))
                return false;
            if (ir1_opnd_size(opnd0) != 64)
                return false;
            if (is_contain_edx(opnd0))
                return false;
            *ptr = 0;
            *opcode = WRAP(CQO_IDIV);
            return true;
            break;
        default:
            break;
        }
        break;
    case WRAP(XOR):
        BUF_CHK(scan_buf, 0, false);
        opnd0 = ir1_get_opnd(ir1, 0);
        opnd1 = ir1_get_opnd(ir1, 1);
        if (BUF_OP(scan_buf, 0) == WRAP(DIV)) {
            if ((opnd0->reg == dt_X86_REG_EDX && opnd1->reg == dt_X86_REG_EDX &&
                ir1_opnd_size(ir1_get_opnd(scan_buf[0], 0)) == 32 &&
                ir1_opnd_is_gpr(ir1_get_opnd(scan_buf[0], 0))) ||
                (opnd0->reg == dt_X86_REG_RDX && opnd1->reg == dt_X86_REG_RDX &&
                ir1_opnd_size(ir1_get_opnd(scan_buf[0], 0)) == 64 &&
                ir1_opnd_is_gpr(ir1_get_opnd(scan_buf[0], 0)))) {
                    if (is_contain_edx(ir1_get_opnd(scan_buf[0], 0)))
                        return false;
                    *ptr = 0;
                    *opcode = WRAP(XOR_DIV);
                    return true;
                }
        }
        break;
    case WRAP(CDQ):
        BUF_CHK(scan_buf, 0, false);
        switch (BUF_OP(scan_buf, 0)) {
        case WRAP(IDIV):
            opnd0 = ir1_get_opnd(scan_buf[0], 0);
            if (ir1_opnd_size(opnd0) != 32)
                return false;
            if (is_contain_edx(opnd0))
                return false;
            *ptr = 0;
            *opcode = WRAP(CDQ_IDIV);
            return true;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return false;
}

void insts_pattern_combine(IR1_INST *ir1, IR1_INST *scan_buf[PTN_BUF_SIZE])
{
    /* check current is in list */
    if (!in_pattern_list(ir1)) {
        pattern_clear(scan_buf);
        return;
    }
    /* if current is not the header */
    if (!in_pattern_header(ir1)) {
        pattern_push(ir1, scan_buf);
        return;
    }
    /* if current is in list, check the pattern */
    int i = 0;
    IR1_OPCODE opcode = dt_X86_INS_INVALID;
    if (get_pattern_list(ir1, scan_buf, &i, &opcode)) {
        pattern_invalid(scan_buf, i);
        pattern_modify(ir1, opcode);
        pattern_clear(scan_buf);
    } else {
        pattern_push(ir1, scan_buf);
    }
}

#undef WRAP

#endif
