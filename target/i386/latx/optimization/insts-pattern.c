/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

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
#define SCAN_CHECK(buf, i) do { \
    if (buf[i] == -1) return false; \
} while (0)
#define SCAN_IDX(buf, i)        (buf[i])
#define SCAN_IR1(tb, buf, i)    (tb_ir1_inst(tb, SCAN_IDX(buf, i)))
#define INSTPTN_REJECT_AND_RETURN(option, reason, value) do { \
    instptn_stats_record_reject((option), (reason)); \
    return (value); \
} while (0)

InstPtnStats instptn_stats[INSTPTN_OPT_COUNT];

static const char *const instptn_option_names[INSTPTN_OPT_COUNT] = {
    [INSTPTN_OPT_CMP_JCC] = "cmp-jcc",
    [INSTPTN_OPT_TEST_JCC] = "test-jcc",
    [INSTPTN_OPT_BT_JCC] = "bt-jcc",
    [INSTPTN_OPT_CQO_IDIV] = "cqo-idiv",
    [INSTPTN_OPT_CMP_SBB] = "cmp-sbb",
    [INSTPTN_OPT_COMISD_JCC] = "comisd-jcc",
    [INSTPTN_OPT_COMISS_JCC] = "comiss-jcc",
    [INSTPTN_OPT_UCOMISD_JCC] = "ucomisd-jcc",
    [INSTPTN_OPT_UCOMISS_JCC] = "ucomiss-jcc",
    [INSTPTN_OPT_XOR_DIV] = "xor-div",
    [INSTPTN_OPT_CDQ_IDIV] = "cdq-idiv",
    [INSTPTN_OPT_CMP_XX_JCC] = "cmp-xx-jcc",
    [INSTPTN_OPT_TEST_XX_JCC] = "test-xx-jcc",
    [INSTPTN_OPT_BT_XX_JCC] = "bt-xx-jcc",
    [INSTPTN_OPT_COMISD_XX_JCC] = "comisd-xx-jcc",
    [INSTPTN_OPT_COMISS_XX_JCC] = "comiss-xx-jcc",
    [INSTPTN_OPT_UCOMISD_XX_JCC] = "ucomisd-xx-jcc",
    [INSTPTN_OPT_UCOMISS_XX_JCC] = "ucomiss-xx-jcc",
    [INSTPTN_OPT_CMP_XXCC] = "cmp-xxcc",
    [INSTPTN_OPT_TEST_XXCC] = "test-xxcc",
    [INSTPTN_OPT_UCOMISD_SETA] = "ucomisd-seta",
    [INSTPTN_OPT_SUB_JCC] = "sub-jcc",
    [INSTPTN_OPT_MOVAPS_VST_X4] = "movaps-vst-x4",
    [INSTPTN_OPT_NEG_CMOVCC] = "neg-cmovcc",
    [INSTPTN_OPT_SHR_JCC] = "shr-jcc",
    [INSTPTN_OPT_AND_JCC] = "and-jcc",
    [INSTPTN_OPT_ADD_JCC] = "add-jcc",
    [INSTPTN_OPT_OR_JCC] = "or-jcc",
    [INSTPTN_OPT_XOR_JCC] = "xor-jcc",
    [INSTPTN_OPT_SAR_JCC] = "sar-jcc",
    [INSTPTN_OPT_DEC_JCC] = "dec-jcc",
    [INSTPTN_OPT_CMP_JS_JNS] = "cmp-js-jns",
    [INSTPTN_OPT_SUB_JS_JNS] = "sub-js-jns",
    [INSTPTN_OPT_AND_JE] = "and-je",
    [INSTPTN_OPT_SHR_JE] = "shr-je",
    [INSTPTN_OPT_OR_XX_JCC] = "or-xx-jcc",
};

static const char *const instptn_reject_names[INSTPTN_REJECT_COUNT] = {
    [INSTPTN_REJECT_DISABLED] = "disabled",
    [INSTPTN_REJECT_NON_ADJACENT] = "non-adjacent",
    [INSTPTN_REJECT_UNSUPPORTED_CC] = "unsupported-cc",
    [INSTPTN_REJECT_UNSUPPORTED_OPERAND] = "unsupported-operand",
    [INSTPTN_REJECT_ZERO_SHIFT_COUNT] = "zero-shift-count",
    [INSTPTN_REJECT_FLAGS_LIVE] = "flags-live",
    [INSTPTN_REJECT_CLOBBER] = "clobber",
    [INSTPTN_REJECT_FAULT_OR_HELPER] = "fault-or-helper",
    [INSTPTN_REJECT_TEMP_LIFETIME] = "temp-lifetime",
    [INSTPTN_REJECT_OTHER] = "other",
};

static bool instptn_opcode_to_option(InstPtnOpcode opcode,
                                     InstPtnOption *option)
{
    switch (opcode) {
    case INSTPTN_OPC_CMP_JCC:
        *option = INSTPTN_OPT_CMP_JCC;
        break;
    case INSTPTN_OPC_TEST_JCC:
        *option = INSTPTN_OPT_TEST_JCC;
        break;
    case INSTPTN_OPC_BT_JCC:
        *option = INSTPTN_OPT_BT_JCC;
        break;
    case INSTPTN_OPC_CQO_IDIV:
        *option = INSTPTN_OPT_CQO_IDIV;
        break;
    case INSTPTN_OPC_CMP_SBB:
        *option = INSTPTN_OPT_CMP_SBB;
        break;
    case INSTPTN_OPC_COMISD_JCC:
        *option = INSTPTN_OPT_COMISD_JCC;
        break;
    case INSTPTN_OPC_COMISS_JCC:
        *option = INSTPTN_OPT_COMISS_JCC;
        break;
    case INSTPTN_OPC_UCOMISD_JCC:
        *option = INSTPTN_OPT_UCOMISD_JCC;
        break;
    case INSTPTN_OPC_UCOMISS_JCC:
        *option = INSTPTN_OPT_UCOMISS_JCC;
        break;
    case INSTPTN_OPC_XOR_DIV:
        *option = INSTPTN_OPT_XOR_DIV;
        break;
    case INSTPTN_OPC_CDQ_IDIV:
        *option = INSTPTN_OPT_CDQ_IDIV;
        break;
    case INSTPTN_OPC_CMP_XX_JCC:
        *option = INSTPTN_OPT_CMP_XX_JCC;
        break;
    case INSTPTN_OPC_TEST_XX_JCC:
        *option = INSTPTN_OPT_TEST_XX_JCC;
        break;
    case INSTPTN_OPC_BT_XX_JCC:
        *option = INSTPTN_OPT_BT_XX_JCC;
        break;
    case INSTPTN_OPC_COMISD_XX_JCC:
        *option = INSTPTN_OPT_COMISD_XX_JCC;
        break;
    case INSTPTN_OPC_COMISS_XX_JCC:
        *option = INSTPTN_OPT_COMISS_XX_JCC;
        break;
    case INSTPTN_OPC_UCOMISD_XX_JCC:
        *option = INSTPTN_OPT_UCOMISD_XX_JCC;
        break;
    case INSTPTN_OPC_UCOMISS_XX_JCC:
        *option = INSTPTN_OPT_UCOMISS_XX_JCC;
        break;
    case INSTPTN_OPC_CMP_XXCC:
        *option = INSTPTN_OPT_CMP_XXCC;
        break;
    case INSTPTN_OPC_TEST_XXCC:
        *option = INSTPTN_OPT_TEST_XXCC;
        break;
    case INSTPTN_OPC_UCOMISD_SETA:
        *option = INSTPTN_OPT_UCOMISD_SETA;
        break;
    case INSTPTN_OPC_SUB_JCC:
        *option = INSTPTN_OPT_SUB_JCC;
        break;
    case INSTPTN_OPC_MOVAPS_VST_X4:
        *option = INSTPTN_OPT_MOVAPS_VST_X4;
        break;
    case INSTPTN_OPC_NEG_CMOVCC:
        *option = INSTPTN_OPT_NEG_CMOVCC;
        break;
    case INSTPTN_OPC_SHR_JCC:
        *option = INSTPTN_OPT_SHR_JCC;
        break;
    case INSTPTN_OPC_AND_JCC:
        *option = INSTPTN_OPT_AND_JCC;
        break;
    case INSTPTN_OPC_ADD_JCC:
        *option = INSTPTN_OPT_ADD_JCC;
        break;
    case INSTPTN_OPC_SAR_JCC:
        *option = INSTPTN_OPT_SAR_JCC;
        break;
    case INSTPTN_OPC_SHR_JE:
        *option = INSTPTN_OPT_SHR_JE;
        break;
    case INSTPTN_OPC_OR_JCC:
        *option = INSTPTN_OPT_OR_JCC;
        break;
    case INSTPTN_OPC_XOR_JCC:
        *option = INSTPTN_OPT_XOR_JCC;
        break;
    case INSTPTN_OPC_DEC_JCC:
        *option = INSTPTN_OPT_DEC_JCC;
        break;
    case INSTPTN_OPC_OR_XX_JCC:
        *option = INSTPTN_OPT_OR_XX_JCC;
        break;
    default:
        return false;
    }
    return true;
}

void instptn_stats_reset(void)
{
    memset(instptn_stats, 0, sizeof(instptn_stats));
}

void instptn_stats_record_match_opcode(InstPtnOpcode opcode)
{
    InstPtnOption option;

    if (!option_instptn_stats ||
        !instptn_opcode_to_option(opcode, &option)) {
        return;
    }
    qatomic_inc(&instptn_stats[option].matches);
}

void instptn_stats_record_reject(InstPtnOption option,
                                 InstPtnRejectReason reason)
{
    if (!option_instptn_stats || option < 0 ||
        option >= INSTPTN_OPT_COUNT || reason < 0 ||
        reason >= INSTPTN_REJECT_COUNT) {
        return;
    }
    qatomic_inc(&instptn_stats[option].rejects[reason]);
}

void instptn_stats_record_eflags_eliminated(InstPtnOption option)
{
    if (!option_instptn_stats || option < 0 ||
        option >= INSTPTN_OPT_COUNT) {
        return;
    }
    qatomic_inc(&instptn_stats[option].eflags_eliminations);
}

void instptn_stats_record_eflags_fallback(InstPtnOption option)
{
    if (!option_instptn_stats || option < 0 ||
        option >= INSTPTN_OPT_COUNT) {
        return;
    }
    qatomic_inc(&instptn_stats[option].eflags_fallbacks);
}

void instptn_stats_record_eflags_fallback_opcode(InstPtnOpcode opcode)
{
    InstPtnOption option;

    if (!instptn_opcode_to_option(opcode, &option)) {
        return;
    }
    instptn_stats_record_eflags_fallback(option);
}

void instptn_stats_record_codegen_opcode(InstPtnOpcode opcode,
                                         uint64_t ir2_emitted,
                                         uint64_t host_emitted)
{
    InstPtnOption option;

    if (!option_instptn_stats ||
        !instptn_opcode_to_option(opcode, &option)) {
        return;
    }
    qatomic_add(&instptn_stats[option].ir2_emitted, ir2_emitted);
    qatomic_add(&instptn_stats[option].host_emitted, host_emitted);
}

void instptn_stats_dump(void)
{
    if (!option_instptn_stats) {
        return;
    }

    fprintf(stderr, "[LATX][instptn] pid=%d mask=0x%" PRIx64 "\n",
            getpid(), option_instptn);
    for (int i = 0; i < INSTPTN_OPT_COUNT; i++) {
        InstPtnStats *stats = &instptn_stats[i];
        uint64_t matches = qatomic_read(&stats->matches);
        uint64_t eliminated = qatomic_read(&stats->eflags_eliminations);
        uint64_t eflags = qatomic_read(&stats->eflags_fallbacks);
        uint64_t ir2 = qatomic_read(&stats->ir2_emitted);
        uint64_t host = qatomic_read(&stats->host_emitted);
        bool populated = matches || eliminated || eflags || ir2 || host;

        for (int reason = 0; reason < INSTPTN_REJECT_COUNT; reason++) {
            populated |= qatomic_read(&stats->rejects[reason]) != 0;
        }
        if (!populated) {
            continue;
        }

        fprintf(stderr,
                "[LATX][instptn] %s match=%" PRIu64
                " eflags-eliminated=%" PRIu64
                " eflags-fallback=%" PRIu64
                " ir2=%" PRIu64 " host=%" PRIu64 "\n",
                instptn_option_names[i], matches, eliminated, eflags,
                ir2, host);
        for (int reason = 0; reason < INSTPTN_REJECT_COUNT; reason++) {
            uint64_t rejects = qatomic_read(&stats->rejects[reason]);

            if (rejects) {
                fprintf(stderr,
                        "[LATX][instptn]   reject.%s=%" PRIu64 "\n",
                        instptn_reject_names[reason], rejects);
            }
        }
    }
}

// static inline void pattern_invalid(IR1_INST *scan_buf[PTN_BUF_SIZE], int num)
// {
//     assert(num < PTN_BUF_SIZE);
//     for (int i = 0; i <= num; ++i) {
//         scan_buf[i]->cflag |= IR1_INVALID_MASK | IR1_PATTERN_MASK;
//     }
// }

// static inline void pattern_modify(IR1_INST *ir1, IR1_OPCODE opcode)
// {
//     ir1->info->id = opcode;
//     ir1->cflag |= IR1_PATTERN_MASK;
// }

static inline bool ir1_can_pattern(IR1_INST *pir1)
{
    switch (ir1_opcode(pir1)) {
    /*head*/
    case WRAP(CMP):
    case WRAP(CQO):
    case WRAP(XOR):
    case WRAP(CDQ):
    case WRAP(TEST):
    case WRAP(UCOMISD):
#ifdef CONFIG_LATX_SMC_OPT
    case WRAP(MOVAPS):
    case WRAP(MOVDQA):
#endif
    case WRAP(NEG):

    /*tail*/
    case WRAP(SBB):
    case WRAP(IDIV):
    case WRAP(DIV):
    case WRAP(SETB):
    case WRAP(SETAE):
    case WRAP(SETE):
    case WRAP(SETNE):
    case WRAP(SETBE):
    case WRAP(SETA):
    case WRAP(SETL):
    case WRAP(SETGE):
    case WRAP(SETLE):
    case WRAP(SETG):
    case WRAP(SETS):
    case WRAP(SETNS):
    case WRAP(SETNO):
    case WRAP(SETO):
    case WRAP(CMOVE):
    case WRAP(CMOVNE):
    case WRAP(CMOVS):
    case WRAP(CMOVNS):
    case WRAP(CMOVLE):
    case WRAP(CMOVG):
    case WRAP(CMOVNO):
    case WRAP(CMOVO):
    case WRAP(CMOVB):
    case WRAP(CMOVBE):
    case WRAP(CMOVA):
    case WRAP(CMOVAE):
    case WRAP(CMOVL):
    case WRAP(CMOVGE):
        return true;
     default:
        return false;
     }
 }

static inline bool ir1_is_pattern_head(IR1_INST *pir1)
{
    switch (ir1_opcode(pir1)) {
    case WRAP(CMP):
    case WRAP(CQO):
    case WRAP(XOR):
    case WRAP(CDQ):
    case WRAP(TEST):
    case WRAP(UCOMISD):
#ifdef CONFIG_LATX_SMC_OPT
    case WRAP(MOVAPS):
    case WRAP(MOVDQA):
#endif
    case WRAP(NEG):
        return true;
    default:
        return false;
    }
 }

static inline void scan_clear(scan_elem_t *scan)
{
    if (scan[0] == -1) return;
    memset(scan, -1, sizeof(scan_elem_t) * INSTPTN_BUF_SIZE);
}

static inline void scan_push(scan_elem_t *scan, int pir1_index)
{
    for(int i = INSTPTN_BUF_SIZE - 1; i > 0; --i) {
        scan[i] = scan[i-1];
    }
    scan[0] = pir1_index;
}

static bool is_contain_edx(IR1_OPND *opnd)
{
    if (ir1_opnd_is_gpr(opnd)) {
        switch (opnd->reg) {
        case dt_X86_REG_DL: case dt_X86_REG_DH:
        case dt_X86_REG_DX: case dt_X86_REG_EDX:
        case dt_X86_REG_RDX:
            return true;
        default:
            break;
        }
    } else if (ir1_opnd_is_mem(opnd)) {
        switch (opnd->mem.base) {
        case dt_X86_REG_DL: case dt_X86_REG_DH:
        case dt_X86_REG_DX: case dt_X86_REG_EDX:
        case dt_X86_REG_RDX:
            return true;
        default:
            break;
        }
        switch (opnd->mem.index) {
        case dt_X86_REG_DL: case dt_X86_REG_DH:
        case dt_X86_REG_DX: case dt_X86_REG_EDX:
        case dt_X86_REG_RDX:
            return true;
        default:
            break;
        }
    }
    return false;
}

static int inst_pattern(TranslationBlock *tb,
        IR1_INST *pir1, scan_elem_t *scan)
{
    IR1_INST *ir1 = NULL;
    IR1_OPND *opnd0 = NULL;
    IR1_OPND *opnd1 = NULL;

    /*
     * pir1 is pattern head
     * scan[] contains ir1 following the head
     */
    switch (ir1_opcode(pir1)) {
    case WRAP(CMP): {
        SCAN_CHECK(scan, 0);
        ir1 = SCAN_IR1(tb, scan, 0);
        if(ir1_opcode(ir1) == WRAP(SBB)) {
            instptn_check_cmp_sbb_0();

            opnd0 = ir1_get_opnd(ir1, 0);
            opnd1 = ir1_get_opnd(ir1, 1);
            if (!ir1_opnd_is_same_reg(opnd0, opnd1)) {
                INSTPTN_REJECT_AND_RETURN(INSTPTN_OPT_CMP_SBB,
                                          INSTPTN_REJECT_UNSUPPORTED_OPERAND,
                                          0);
            }
            pir1->instptn.opc  = INSTPTN_OPC_CMP_SBB;
            pir1->instptn.next = ir1;
            ir1->instptn.opc  = INSTPTN_OPC_NOP;
            // ir1->instptn.next = NULL;
            return 1;
        }
        instptn_check_cmp_xxcc_0();
        switch (ir1_opcode(ir1)) {
        case WRAP(SETB):
        case WRAP(SETAE):
        case WRAP(SETE):
        case WRAP(SETNE):
        case WRAP(SETBE):
        case WRAP(SETA):
        case WRAP(SETL):
        case WRAP(SETGE):
        case WRAP(SETLE):
        case WRAP(SETG):
        case WRAP(CMOVB):
        case WRAP(CMOVAE):
        case WRAP(CMOVE):
        case WRAP(CMOVNE):
        case WRAP(CMOVBE):
        case WRAP(CMOVA):
        case WRAP(CMOVL):
        case WRAP(CMOVGE):
        case WRAP(CMOVLE):
        case WRAP(CMOVG):
            pir1->instptn.opc  = INSTPTN_OPC_CMP_XXCC;
            pir1->instptn.next = ir1;
            ir1->instptn.opc  = INSTPTN_OPC_NOP;
            // ir1->instptn.next = NULL;
            return 1;
        default:
            INSTPTN_REJECT_AND_RETURN(INSTPTN_OPT_CMP_XXCC,
                                      INSTPTN_REJECT_UNSUPPORTED_CC, 0);
        }
    }
    case WRAP(TEST): {
        SCAN_CHECK(scan, 0);
        instptn_check_test_xxcc_0();
        ir1 = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1)) {
        case WRAP(SETS):
        case WRAP(SETNS):
        case WRAP(SETLE):
        case WRAP(SETG):
        case WRAP(CMOVS):
        case WRAP(CMOVNS):
        case WRAP(CMOVLE):
        case WRAP(CMOVG):
            opnd0 = ir1_get_opnd(pir1, 0);
            opnd1 = ir1_get_opnd(pir1, 1);
            if (!ir1_opnd_is_same_reg(opnd0, opnd1)) {
                INSTPTN_REJECT_AND_RETURN(
                    INSTPTN_OPT_TEST_XXCC,
                    INSTPTN_REJECT_UNSUPPORTED_OPERAND, 1);
            }
            __attribute__((fallthrough));
        case WRAP(SETE):
        case WRAP(SETNE):
        case WRAP(SETNO):
        case WRAP(SETO):
        case WRAP(SETB):
        case WRAP(SETBE):
        case WRAP(SETA):
        case WRAP(SETAE):
        case WRAP(CMOVE):
        case WRAP(CMOVNE):
        case WRAP(CMOVNO):
        case WRAP(CMOVO):
        case WRAP(CMOVB):
        case WRAP(CMOVBE):
        case WRAP(CMOVA):
        case WRAP(CMOVAE):
            pir1->instptn.opc  = INSTPTN_OPC_TEST_XXCC;
            pir1->instptn.next = ir1;
            ir1->instptn.opc  = INSTPTN_OPC_NOP;
            // ir1->instptn.next = NULL;
            return 1;
        default:
            INSTPTN_REJECT_AND_RETURN(INSTPTN_OPT_TEST_XXCC,
                                      INSTPTN_REJECT_UNSUPPORTED_CC, 0);
        }
    }
    case WRAP(CQO):
        SCAN_CHECK(scan, 0);
        instptn_check_cqo_idiv_0();

        ir1 = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1)) {
        case WRAP(IDIV):
            opnd0 = ir1_get_opnd(ir1, 0);
            if (!ir1_opnd_is_gpr(opnd0) ||
                ir1_opnd_size(opnd0) != 64 ||
                is_contain_edx(opnd0)) {
                INSTPTN_REJECT_AND_RETURN(
                    INSTPTN_OPT_CQO_IDIV,
                    INSTPTN_REJECT_UNSUPPORTED_OPERAND, 0);
            }
            pir1->instptn.opc  = INSTPTN_OPC_CQO_IDIV;
            pir1->instptn.next = ir1;
            ir1->instptn.opc  = INSTPTN_OPC_NOP_DIV;
            // ir1->instptn.next = NULL;
            return 1;
        default:
            INSTPTN_REJECT_AND_RETURN(INSTPTN_OPT_CQO_IDIV,
                                      INSTPTN_REJECT_OTHER, 0);
        }
    case WRAP(XOR):
        SCAN_CHECK(scan, 0);
        instptn_check_xor_div_0();

        opnd0 = ir1_get_opnd(pir1, 0);
        opnd1 = ir1_get_opnd(pir1, 1);

        ir1 = SCAN_IR1(tb, scan, 0);
        if (ir1_opcode(ir1) == WRAP(DIV)) {
            if (ir1_opnd_is_gpr(opnd0) && ir1_opnd_is_gpr(opnd1) &&
                ((opnd0->reg == dt_X86_REG_EDX && opnd1->reg == dt_X86_REG_EDX &&
                ir1_opnd_size(ir1_get_opnd(ir1, 0)) == 32 &&
                ir1_opnd_is_gpr(ir1_get_opnd(ir1, 0))) ||
                (opnd0->reg == dt_X86_REG_RDX && opnd1->reg == dt_X86_REG_RDX &&
                ir1_opnd_size(ir1_get_opnd(ir1, 0)) == 64 &&
                ir1_opnd_is_gpr(ir1_get_opnd(ir1, 0))))) {
                    if (is_contain_edx(opnd0)) {
                        INSTPTN_REJECT_AND_RETURN(
                            INSTPTN_OPT_XOR_DIV,
                            INSTPTN_REJECT_UNSUPPORTED_OPERAND, 0);
                    }
                    pir1->instptn.opc  = INSTPTN_OPC_XOR_DIV;
                    pir1->instptn.next = ir1;
                    ir1->instptn.opc  = INSTPTN_OPC_NOP_DIV;
                    // ir1->instptn.next = NULL;
                    return 1;
                }
        }
        INSTPTN_REJECT_AND_RETURN(INSTPTN_OPT_XOR_DIV,
                                  INSTPTN_REJECT_UNSUPPORTED_OPERAND, 0);
    case WRAP(CDQ):
        SCAN_CHECK(scan, 0);
        instptn_check_cdq_idiv_0();

        ir1 = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1)) {
        case WRAP(IDIV):
            opnd0 = ir1_get_opnd(ir1, 0);
            if (!ir1_opnd_is_gpr(opnd0) ||
                ir1_opnd_size(opnd0) != 32 ||
                is_contain_edx(opnd0)) {
                INSTPTN_REJECT_AND_RETURN(
                    INSTPTN_OPT_CDQ_IDIV,
                    INSTPTN_REJECT_UNSUPPORTED_OPERAND, 0);
            }
            pir1->instptn.opc  = INSTPTN_OPC_CDQ_IDIV;
            pir1->instptn.next = ir1;
            ir1->instptn.opc  = INSTPTN_OPC_NOP_DIV;
            // ir1->instptn.next = NULL;
            return 1;
        default:
            INSTPTN_REJECT_AND_RETURN(INSTPTN_OPT_CDQ_IDIV,
                                      INSTPTN_REJECT_OTHER, 0);
        }
    case WRAP(UCOMISD):
        SCAN_CHECK(scan, 0);
        instptn_check_ucomisd_seta_0();

        ir1 = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1)) {
        case WRAP(SETA):
            pir1->instptn.opc  = INSTPTN_OPC_UCOMISD_SETA;
            pir1->instptn.next = ir1;
            ir1->instptn.opc  = INSTPTN_OPC_NOP;
            // ir1->instptn.next = NULL;
            return 1;
        default:
            INSTPTN_REJECT_AND_RETURN(INSTPTN_OPT_UCOMISD_SETA,
                                      INSTPTN_REJECT_UNSUPPORTED_CC, 0);
        }
#ifdef CONFIG_LATX_SMC_OPT
    case WRAP(MOVAPS):
    case WRAP(MOVDQA): {
        SCAN_CHECK(scan, 0);
        instptn_check_movaps_vst_x4_0();
        if (!tb_use_smc_opt(tb))
            return 0;
        if (scan[0] >= 0 && scan[1] >= 0 && scan[2] >= 0) {
            ir1 = SCAN_IR1(tb, scan, 0);
            IR1_INST *ir2 = SCAN_IR1(tb, scan, 1);
            IR1_INST *ir3 = SCAN_IR1(tb, scan, 2);
            if (ir1_opcode(ir1) == WRAP(MOVAPS) &&
                    ir1_opcode(ir2) == WRAP(MOVAPS) &&
                    ir1_opcode(ir3) == WRAP(MOVAPS)) {
                pir1->instptn.opc  = INSTPTN_OPC_MOVAPS_VST_X4;
                pir1->instptn.next = ir1;
                ir1->instptn.opc  = INSTPTN_OPC_NOP;
                ir1->instptn.next = ir2;
                ir2->instptn.opc  = INSTPTN_OPC_NOP;
                ir2->instptn.next = ir3;
                ir3->instptn.opc  = INSTPTN_OPC_NOP;
                return 1;
            }
        }
        return 0;
    }
#endif
    case WRAP(NEG): {
        SCAN_CHECK(scan, 0);
        instptn_check_neg_cmovcc_0();
        ir1 = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1)) {
            case WRAP(CMOVS):
            case WRAP(CMOVNS):
                pir1->instptn.opc  = INSTPTN_OPC_NEG_CMOVCC;
                pir1->instptn.next = ir1;
                ir1->instptn.opc  = INSTPTN_OPC_NOP;
                return 1;
            default:
                INSTPTN_REJECT_AND_RETURN(
                    INSTPTN_OPT_NEG_CMOVCC,
                    INSTPTN_REJECT_UNSUPPORTED_CC, 0);
        }
    }
    default:
        return 0;
    }
}

void insts_pattern_scan_con(TranslationBlock *tb, IR1_INST *ir1, int index, scan_elem_t *scan_buf)
{
    if (!ir1_can_pattern(ir1)) {
        scan_clear(scan_buf);
        return;
    }
    if (!ir1_is_pattern_head(ir1)) {
        scan_push(scan_buf, index);
        return;
    }

    if (inst_pattern(tb, ir1, scan_buf)) {
        instptn_stats_record_match_opcode(ir1->instptn.opc);
        scan_clear(scan_buf);
    } else {
        scan_push(scan_buf, index);
    }
}

bool insts_pattern_scan_jcc_end(TranslationBlock *tb, IR1_INST *pir1, int pir1_index, scan_elem_t *scan)
{

    if (pir1_index == tb_ir1_num(tb) - 1) {
        if (!pir1_index) return false; /* tb->icount > 1*/
        switch (ir1_opcode(pir1)) {
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
        case WRAP(JNO):
        case WRAP(JO):
        case WRAP(JS):
        case WRAP(JNS):
            scan[0] = pir1_index;
            return true;
        default:
            return false;
        }
    }

    IR1_INST *ir1_jcc = NULL;
    IR1_OPND *opnd0 = NULL;
    IR1_OPND *opnd1 = NULL;
    InstPtnOption shift_option;
    InstPtnOpcode shift_opcode;
    switch (ir1_opcode(pir1)) {
        case WRAP(CMP):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
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
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_cmp_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_CMP_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else {
                instptn_check_cmp_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_CMP_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_CMP_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst(tb, pir1_index);
                tb->has_jcc_end_ptn = true;
            }
            return false;
        case WRAP(JS):
        case WRAP(JNS):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_cmp_jcc_0();
                pir1->instptn.opc = INSTPTN_OPC_CMP_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc = INSTPTN_OPC_NOP;
            } else {
                instptn_stats_record_reject(
                    INSTPTN_OPT_CMP_JCC,
                    INSTPTN_REJECT_NON_ADJACENT);
            }
            return false;
        default:
            INSTPTN_REJECT_AND_RETURN(
                pir1_index + 1 == SCAN_IDX(scan, 0) ?
                    INSTPTN_OPT_CMP_JCC : INSTPTN_OPT_CMP_XX_JCC,
                INSTPTN_REJECT_UNSUPPORTED_CC, false);
        }
    case WRAP(TEST):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JS):
        case WRAP(JNS):
        case WRAP(JLE):
        case WRAP(JG):
            opnd0 = ir1_get_opnd(pir1, 0);
            opnd1 = ir1_get_opnd(pir1, 1);
            if (!ir1_opnd_is_same_reg(opnd0, opnd1)) {
                INSTPTN_REJECT_AND_RETURN(
                    pir1_index + 1 == SCAN_IDX(scan, 0) ?
                        INSTPTN_OPT_TEST_JCC : INSTPTN_OPT_TEST_XX_JCC,
                    INSTPTN_REJECT_UNSUPPORTED_OPERAND, false);
            }
            __attribute__((fallthrough));
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JNO):
        case WRAP(JO):
        case WRAP(JB):
        case WRAP(JBE):
        case WRAP(JA):
        case WRAP(JAE):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_test_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_TEST_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else {
                instptn_check_test_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_TEST_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_TEST_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst(tb, pir1_index);
                tb->has_jcc_end_ptn = true;
            }
            return false;
        default:
            INSTPTN_REJECT_AND_RETURN(
                pir1_index + 1 == SCAN_IDX(scan, 0) ?
                    INSTPTN_OPT_TEST_JCC : INSTPTN_OPT_TEST_XX_JCC,
                INSTPTN_REJECT_UNSUPPORTED_CC, false);
        }
    case WRAP(BT):
        SCAN_CHECK(scan, 0);
        opnd0 = ir1_get_opnd(pir1, 0);
        if (!ir1_opnd_is_gpr(opnd0)) {
            INSTPTN_REJECT_AND_RETURN(
                pir1_index + 1 == SCAN_IDX(scan, 0) ?
                    INSTPTN_OPT_BT_JCC : INSTPTN_OPT_BT_XX_JCC,
                INSTPTN_REJECT_UNSUPPORTED_OPERAND, false);
        }
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JB):
        case WRAP(JAE):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_bt_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_BT_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else {
                instptn_check_bt_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_BT_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_BT_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst(tb, pir1_index);
                tb->has_jcc_end_ptn = true;
            }
            return false;
        default:
            INSTPTN_REJECT_AND_RETURN(
                pir1_index + 1 == SCAN_IDX(scan, 0) ?
                    INSTPTN_OPT_BT_JCC : INSTPTN_OPT_BT_XX_JCC,
                INSTPTN_REJECT_UNSUPPORTED_CC, false);
        }
    case WRAP(SUB):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
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
        case WRAP(JS):
        case WRAP(JNS):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_sub_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_SUB_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else {
                instptn_stats_record_reject(
                    INSTPTN_OPT_SUB_JCC,
                    INSTPTN_REJECT_NON_ADJACENT);
            }
            return false;
        default:
            INSTPTN_REJECT_AND_RETURN(INSTPTN_OPT_SUB_JCC,
                                      INSTPTN_REJECT_UNSUPPORTED_CC, false);
        }
    case WRAP(SAR):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        opnd0 = ir1_get_opnd(pir1, 0);
        opnd1 = ir1_get_opnd(pir1, 1);
        if (!ir1_opnd_is_imm(opnd1)) {
            INSTPTN_REJECT_AND_RETURN(
                INSTPTN_OPT_SAR_JCC,
                INSTPTN_REJECT_UNSUPPORTED_OPERAND, false);
        }
        if ((ir1_opnd_uimm(opnd1) &
             (ir1_opnd_size(opnd0) == 64 ? 63 : 31)) == 0) {
            INSTPTN_REJECT_AND_RETURN(
                INSTPTN_OPT_SAR_JCC,
                INSTPTN_REJECT_ZERO_SHIFT_COUNT, false);
        }
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JS):
        case WRAP(JNS):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_sar_jcc_0();
                pir1->instptn.opc = INSTPTN_OPC_SAR_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc = INSTPTN_OPC_NOP;
            } else {
                instptn_stats_record_reject(
                    INSTPTN_OPT_SAR_JCC,
                    INSTPTN_REJECT_NON_ADJACENT);
            }
            return false;
        default:
            INSTPTN_REJECT_AND_RETURN(INSTPTN_OPT_SAR_JCC,
                                      INSTPTN_REJECT_UNSUPPORTED_CC, false);
        }
    case WRAP(SHR):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JE):
            shift_option = INSTPTN_OPT_SHR_JE;
            shift_opcode = INSTPTN_OPC_SHR_JE;
            break;
        case WRAP(JNE):
            shift_option = INSTPTN_OPT_SHR_JCC;
            shift_opcode = INSTPTN_OPC_SHR_JCC;
            break;
        default:
            INSTPTN_REJECT_AND_RETURN(INSTPTN_OPT_SHR_JCC,
                                      INSTPTN_REJECT_UNSUPPORTED_CC, false);
        }
        opnd0 = ir1_get_opnd(pir1, 0);
        opnd1 = ir1_get_opnd(pir1, 1);
        if (!ir1_opnd_is_imm(opnd1)) {
            INSTPTN_REJECT_AND_RETURN(
                shift_option,
                INSTPTN_REJECT_UNSUPPORTED_OPERAND, false);
        }
        if ((ir1_opnd_uimm(opnd1) &
             (ir1_opnd_size(opnd0) == 64 ? 63 : 31)) == 0) {
            INSTPTN_REJECT_AND_RETURN(
                shift_option,
                INSTPTN_REJECT_ZERO_SHIFT_COUNT, false);
        }
        if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
            if (shift_option == INSTPTN_OPT_SHR_JE) {
                instptn_check_shr_je_0();
            } else {
                instptn_check_shr_jcc_0();
            }
            pir1->instptn.opc = shift_opcode;
            pir1->instptn.next = ir1_jcc;
            ir1_jcc->instptn.opc = INSTPTN_OPC_NOP;
        } else {
            instptn_stats_record_reject(
                shift_option, INSTPTN_REJECT_NON_ADJACENT);
        }
        return false;
    case WRAP(AND):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JE):
        case WRAP(JNE):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_and_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_AND_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else {
                instptn_stats_record_reject(
                    INSTPTN_OPT_AND_JCC,
                    INSTPTN_REJECT_NON_ADJACENT);
            }
            return false;
        default:
            INSTPTN_REJECT_AND_RETURN(INSTPTN_OPT_AND_JCC,
                                      INSTPTN_REJECT_UNSUPPORTED_CC, false);
        }
    case WRAP(DEC):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        opnd0 = ir1_get_opnd(pir1, 0);
        if (!ir1_opnd_is_gpr(opnd0)) {
            INSTPTN_REJECT_AND_RETURN(
                INSTPTN_OPT_DEC_JCC,
                INSTPTN_REJECT_UNSUPPORTED_OPERAND, false);
        }
        if (ir1_is_prefix_lock(pir1)) {
            INSTPTN_REJECT_AND_RETURN(
                INSTPTN_OPT_DEC_JCC,
                INSTPTN_REJECT_FAULT_OR_HELPER, false);
        }
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JE):
        case WRAP(JNE):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_dec_jcc_0();
                pir1->instptn.opc = INSTPTN_OPC_DEC_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc = INSTPTN_OPC_NOP;
            } else {
                instptn_stats_record_reject(
                    INSTPTN_OPT_DEC_JCC,
                    INSTPTN_REJECT_NON_ADJACENT);
            }
            return false;
        default:
            INSTPTN_REJECT_AND_RETURN(INSTPTN_OPT_DEC_JCC,
                                      INSTPTN_REJECT_UNSUPPORTED_CC, false);
        }
    case WRAP(ADD):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        opnd0 = ir1_get_opnd(pir1, 0);
        opnd1 = ir1_get_opnd(pir1, 1);
        if ((!ir1_opnd_is_gpr(opnd0) && !ir1_opnd_is_mem(opnd0)) ||
            (!ir1_opnd_is_gpr(opnd1) && !ir1_opnd_is_mem(opnd1) &&
             !ir1_opnd_is_imm(opnd1))) {
            INSTPTN_REJECT_AND_RETURN(
                INSTPTN_OPT_ADD_JCC,
                INSTPTN_REJECT_UNSUPPORTED_OPERAND, false);
        }
        if (ir1_is_prefix_lock(pir1)) {
            INSTPTN_REJECT_AND_RETURN(
                INSTPTN_OPT_ADD_JCC,
                INSTPTN_REJECT_FAULT_OR_HELPER, false);
        }
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JS):
        case WRAP(JNS):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_add_jcc_0();
                pir1->instptn.opc = INSTPTN_OPC_ADD_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc = INSTPTN_OPC_NOP;
            } else {
                instptn_stats_record_reject(
                    INSTPTN_OPT_ADD_JCC,
                    INSTPTN_REJECT_NON_ADJACENT);
            }
            return false;
        default:
            INSTPTN_REJECT_AND_RETURN(INSTPTN_OPT_ADD_JCC,
                                      INSTPTN_REJECT_UNSUPPORTED_CC, false);
        }
    case WRAP(OR):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        bool adjacent = pir1_index + 1 == SCAN_IDX(scan, 0);
        InstPtnOption or_option = adjacent ? INSTPTN_OPT_OR_JCC :
                                            INSTPTN_OPT_OR_XX_JCC;
        opnd0 = ir1_get_opnd(pir1, 0);
        opnd1 = ir1_get_opnd(pir1, 1);
        if ((!ir1_opnd_is_gpr(opnd0) && !ir1_opnd_is_mem(opnd0)) ||
            (!ir1_opnd_is_gpr(opnd1) && !ir1_opnd_is_mem(opnd1) &&
             !ir1_opnd_is_imm(opnd1))) {
            INSTPTN_REJECT_AND_RETURN(
                or_option,
                INSTPTN_REJECT_UNSUPPORTED_OPERAND, false);
        }
        if (ir1_is_prefix_lock(pir1)) {
            INSTPTN_REJECT_AND_RETURN(
                or_option,
                INSTPTN_REJECT_FAULT_OR_HELPER, false);
        }
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JS):
        case WRAP(JNS):
            if (adjacent) {
                instptn_check_or_jcc_0();
                pir1->instptn.opc = INSTPTN_OPC_OR_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc = INSTPTN_OPC_NOP;
            } else {
                instptn_stats_record_reject(
                    INSTPTN_OPT_OR_JCC,
                    INSTPTN_REJECT_NON_ADJACENT);
                if (!ir1_opnd_is_gpr(opnd0)) {
                    INSTPTN_REJECT_AND_RETURN(
                        INSTPTN_OPT_OR_XX_JCC,
                        INSTPTN_REJECT_FAULT_OR_HELPER, false);
                }
                instptn_check_or_xx_jcc_0();
                pir1->instptn.opc = INSTPTN_OPC_OR_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc = INSTPTN_OPC_OR_XX_JCC;
                ir1_jcc->instptn.next = pir1;
                tb->has_jcc_end_ptn = true;
            }
            return false;
        default:
            INSTPTN_REJECT_AND_RETURN(or_option,
                                      INSTPTN_REJECT_UNSUPPORTED_CC, false);
        }
    case WRAP(XOR):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        opnd0 = ir1_get_opnd(pir1, 0);
        opnd1 = ir1_get_opnd(pir1, 1);
        if ((!ir1_opnd_is_gpr(opnd0) && !ir1_opnd_is_mem(opnd0)) ||
            (!ir1_opnd_is_gpr(opnd1) && !ir1_opnd_is_mem(opnd1) &&
             !ir1_opnd_is_imm(opnd1))) {
            INSTPTN_REJECT_AND_RETURN(
                INSTPTN_OPT_XOR_JCC,
                INSTPTN_REJECT_UNSUPPORTED_OPERAND, false);
        }
        if (ir1_is_prefix_lock(pir1)) {
            INSTPTN_REJECT_AND_RETURN(
                INSTPTN_OPT_XOR_JCC,
                INSTPTN_REJECT_FAULT_OR_HELPER, false);
        }
        switch (ir1_opcode(ir1_jcc)) {
        case WRAP(JE):
        case WRAP(JNE):
        case WRAP(JS):
        case WRAP(JNS):
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_xor_jcc_0();
                pir1->instptn.opc = INSTPTN_OPC_XOR_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc = INSTPTN_OPC_NOP;
            } else {
                instptn_stats_record_reject(
                    INSTPTN_OPT_XOR_JCC,
                    INSTPTN_REJECT_NON_ADJACENT);
            }
            return false;
        default:
            INSTPTN_REJECT_AND_RETURN(INSTPTN_OPT_XOR_JCC,
                                      INSTPTN_REJECT_UNSUPPORTED_CC, false);
        }
#ifdef CONFIG_LATX_XCOMISX_OPT
    case WRAP(COMISD):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
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
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_comisd_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_COMISD_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else {
                instptn_check_comisd_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_COMISD_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_COMISD_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst(tb, pir1_index);
                tb->has_jcc_end_ptn = true;
            }
            return false;
        default:
            INSTPTN_REJECT_AND_RETURN(
                pir1_index + 1 == SCAN_IDX(scan, 0) ?
                    INSTPTN_OPT_COMISD_JCC : INSTPTN_OPT_COMISD_XX_JCC,
                INSTPTN_REJECT_UNSUPPORTED_CC, false);
        }
    case WRAP(COMISS):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
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
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_comiss_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_COMISS_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else {
                instptn_check_comiss_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_COMISS_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_COMISS_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst(tb, pir1_index);
                tb->has_jcc_end_ptn = true;
            }
            return false;
        default:
            INSTPTN_REJECT_AND_RETURN(
                pir1_index + 1 == SCAN_IDX(scan, 0) ?
                    INSTPTN_OPT_COMISS_JCC : INSTPTN_OPT_COMISS_XX_JCC,
                INSTPTN_REJECT_UNSUPPORTED_CC, false);
        }
    case WRAP(UCOMISD):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
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
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_ucomisd_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_UCOMISD_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                // ir1_jcc->instptn.next = NULL;
            } else {
                instptn_check_ucomisd_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_UCOMISD_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_UCOMISD_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst(tb, pir1_index);
                tb->has_jcc_end_ptn = true;
            }
            return false;
        default:
            INSTPTN_REJECT_AND_RETURN(
                pir1_index + 1 == SCAN_IDX(scan, 0) ?
                    INSTPTN_OPT_UCOMISD_JCC :
                    INSTPTN_OPT_UCOMISD_XX_JCC,
                INSTPTN_REJECT_UNSUPPORTED_CC, false);
        }
    case WRAP(UCOMISS):
        SCAN_CHECK(scan, 0);
        ir1_jcc = SCAN_IR1(tb, scan, 0);
        switch (ir1_opcode(ir1_jcc)) {
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
            if (pir1_index + 1 == SCAN_IDX(scan, 0)) {
                instptn_check_ucomiss_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_UCOMISS_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_NOP;
                ir1_jcc->instptn.next = NULL;
            } else {
                instptn_check_ucomiss_xx_jcc_0();
                pir1->instptn.opc  = INSTPTN_OPC_UCOMISS_XX_JCC;
                pir1->instptn.next = ir1_jcc;
                ir1_jcc->instptn.opc  = INSTPTN_OPC_UCOMISS_XX_JCC;
                ir1_jcc->instptn.next = tb_ir1_inst(tb, pir1_index);
                tb->has_jcc_end_ptn = true;
            }
            return false;
        default:
            INSTPTN_REJECT_AND_RETURN(
                pir1_index + 1 == SCAN_IDX(scan, 0) ?
                    INSTPTN_OPT_UCOMISS_JCC :
                    INSTPTN_OPT_UCOMISS_XX_JCC,
                INSTPTN_REJECT_UNSUPPORTED_CC, false);
        }
#endif
    case WRAP(MOV): {
        opnd0 = ir1_get_opnd(pir1, 0);
        opnd1 = ir1_get_opnd(pir1, 1);
        if (ir1_opnd_is_mem(opnd0) && ir1_opnd_is_gpr(opnd1) &&
            (!ir1_opnd_is_8h(opnd1)) && tb_use_smc_opt(lsenv->tr_data->curr_tb)) {
                return false;
        }
        return true;
    }
    case WRAP(ADDSD):
    case WRAP(ADDSS):
    case WRAP(LEA):
    case WRAP(MOVAPD):
    case WRAP(MOVHPS):
    case WRAP(MOVLPS):
    case WRAP(MOVSD):
    case WRAP(MOVSS):
    case WRAP(MOVSX):
    case WRAP(MOVSXD):
    case WRAP(MOVZX):
    case WRAP(MULPS):
    case WRAP(MULSD):
    case WRAP(MULSS):
    case WRAP(NOP):
    case WRAP(PSHUFD):
    case WRAP(PUNPCKLWD):
    case WRAP(PUSH):
        return true;
    default:
        return false;
    }
}

#undef WRAP

#endif
