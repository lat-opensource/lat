/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file insts-pattern.h
 * @author huqi <spcreply@outlook.com>
 *         liuchaoyi <lcy285183897@gmail.com>
 * @brief insts-ptn optimization header file
 */
#ifndef _INSTS_PATTERN_H_
#define _INSTS_PATTERN_H_
#include "common.h"
#include "ir1.h"
#include "ir2.h"
#include "latx-options.h"

#define INSTPTN_BUF_SIZE 4

/*
 * Options are indexed independently from InstPtnOpcode.  Preserve the first
 * 26 indices so the default mask remains bit-for-bit compatible with the old
 * (INSTPTN_OPC_* >> 4) encoding.
 */
typedef enum InstPtnOption {
    INSTPTN_OPT_CMP_JCC,
    INSTPTN_OPT_TEST_JCC,
    INSTPTN_OPT_BT_JCC,
    INSTPTN_OPT_CQO_IDIV,
    INSTPTN_OPT_CMP_SBB,
    INSTPTN_OPT_COMISD_JCC,
    INSTPTN_OPT_COMISS_JCC,
    INSTPTN_OPT_UCOMISD_JCC,
    INSTPTN_OPT_UCOMISS_JCC,
    INSTPTN_OPT_XOR_DIV,
    INSTPTN_OPT_CDQ_IDIV,
    INSTPTN_OPT_CMP_XX_JCC,
    INSTPTN_OPT_TEST_XX_JCC,
    INSTPTN_OPT_BT_XX_JCC,
    INSTPTN_OPT_COMISD_XX_JCC,
    INSTPTN_OPT_COMISS_XX_JCC,
    INSTPTN_OPT_UCOMISD_XX_JCC,
    INSTPTN_OPT_UCOMISS_XX_JCC,
    INSTPTN_OPT_CMP_XXCC,
    INSTPTN_OPT_TEST_XXCC,
    INSTPTN_OPT_UCOMISD_SETA,
    INSTPTN_OPT_SUB_JCC,
    INSTPTN_OPT_MOVAPS_VST_X4,
    INSTPTN_OPT_NEG_CMOVCC,
    INSTPTN_OPT_SHR_JCC,
    INSTPTN_OPT_AND_JCC,

    /* Reserved for PERF-001 through PERF-006 and PERF-009. */
    INSTPTN_OPT_ADD_JCC,
    INSTPTN_OPT_OR_JCC,
    INSTPTN_OPT_XOR_JCC,
    INSTPTN_OPT_SAR_JCC,
    INSTPTN_OPT_DEC_JCC,
    INSTPTN_OPT_CMP_JS_JNS,
    INSTPTN_OPT_SUB_JS_JNS,
    INSTPTN_OPT_AND_JE,
    INSTPTN_OPT_SHR_JE,
    INSTPTN_OPT_OR_XX_JCC,

    INSTPTN_OPT_COUNT,
} InstPtnOption;

#define INSTPTN_OPT_FIRST_RESERVED INSTPTN_OPT_ADD_JCC
#define INSTPTN_OPTION_BIT(opt) (UINT64_C(1) << (opt))
#define INSTPTN_DEFAULT_OPTIONS \
    (INSTPTN_OPTION_BIT(INSTPTN_OPT_FIRST_RESERVED) - UINT64_C(1))
#define INSTPTN_ALL_OPTIONS \
    (INSTPTN_OPTION_BIT(INSTPTN_OPT_COUNT) - UINT64_C(1))

_Static_assert(INSTPTN_OPT_COUNT < 64,
               "instruction pattern option mask exceeds uint64_t");
_Static_assert(INSTPTN_DEFAULT_OPTIONS == UINT64_C(0x3ffffff),
               "legacy instruction pattern default mask changed");

typedef enum InstPtnRejectReason {
    /* Existing scanners report a reason only after identifying an option. */
    INSTPTN_REJECT_DISABLED,
    INSTPTN_REJECT_NON_ADJACENT,
    INSTPTN_REJECT_UNSUPPORTED_CC,
    INSTPTN_REJECT_UNSUPPORTED_OPERAND,
    INSTPTN_REJECT_ZERO_SHIFT_COUNT,
    /* Used by later patterns when these conditions reject a known option. */
    INSTPTN_REJECT_FLAGS_LIVE,
    INSTPTN_REJECT_CLOBBER,
    INSTPTN_REJECT_FAULT_OR_HELPER,
    INSTPTN_REJECT_TEMP_LIFETIME,
    INSTPTN_REJECT_OTHER,
    INSTPTN_REJECT_COUNT,
} InstPtnRejectReason;

typedef struct InstPtnStats {
    uint64_t matches;
    uint64_t rejects[INSTPTN_REJECT_COUNT];
    uint64_t eflags_eliminations;
    uint64_t eflags_fallbacks;
    uint64_t ir2_emitted;
    uint64_t host_emitted;
} InstPtnStats;

typedef int scan_elem_t;

#ifdef CONFIG_LATX_INSTS_PATTERN

extern InstPtnStats instptn_stats[INSTPTN_OPT_COUNT];

static inline bool instptn_option_enabled(InstPtnOption option)
{
    return option >= 0 && option < INSTPTN_OPT_COUNT &&
           (option_instptn & INSTPTN_OPTION_BIT(option));
}

void instptn_stats_reset(void);
void instptn_stats_dump(void);
void instptn_stats_record_match_opcode(InstPtnOpcode opcode);
void instptn_stats_record_reject(InstPtnOption option,
                                 InstPtnRejectReason reason);
void instptn_stats_record_eflags_eliminated(InstPtnOption option);
void instptn_stats_record_eflags_fallback(InstPtnOption option);
void instptn_stats_record_eflags_fallback_opcode(InstPtnOpcode opcode);
void instptn_stats_record_codegen_opcode(InstPtnOpcode opcode,
                                         uint64_t ir2_emitted,
                                         uint64_t host_emitted);

#endif

void insts_pattern_scan_con(TranslationBlock *tb, IR1_INST *ir1, int index, scan_elem_t *scan_buf);
bool insts_pattern_scan_jcc_end(TranslationBlock *tb, IR1_INST *ir1, int index, scan_elem_t *scan_buf);

#ifdef CONFIG_LATX_INSTS_PATTERN

#define DEF_INSTS_PTN(_prex) \
        __attribute__((unused)) scan_elem_t _prex##_scaned_cond[INSTPTN_BUF_SIZE] = {-1, -1, -1, -1}; \
        __attribute__((unused)) scan_elem_t _prex##_scaned_jcc_end[1] = {-1}; \
        __attribute__((unused)) bool scan_head = true;
#define OPT_INSTS_PTN(tb, inst, index, _prex) \
        do { \
            if (option_instptn) { \
                insts_pattern_scan_con(tb, inst, index, _prex##_scaned_cond); \
                if (scan_head) { \
                    InstPtnOpcode _opc_before = (inst)->instptn.opc; \
                    scan_head = insts_pattern_scan_jcc_end(tb, inst, index, _prex##_scaned_jcc_end); \
                    if (_opc_before == INSTPTN_OPC_NONE && \
                        (inst)->instptn.opc != INSTPTN_OPC_NONE) { \
                        instptn_stats_record_match_opcode( \
                            (inst)->instptn.opc); \
                    } \
                } \
            } \
        } while (0)

#else /* !CONFIG_LATX_INSTS_PATTERN */

#define DEF_INSTS_PTN(_prex)        ((void)0)
#define OPT_INSTS_PTN(tb, inst, index, _prex) ((void)0)

#endif

#define INSTPTN_OPTION_CHECK

#ifdef INSTPTN_OPTION_CHECK

#define instptn_check_void() do { \
    if (!option_instptn) return;        \
} while (0)
#define instptn_check_false() do {    \
    if (!option_instptn) return false;      \
} while (0)

#define INSTPTN_CHECK_XX_0(OPC) do {   \
    if (!instptn_option_enabled(INSTPTN_OPT_##OPC)) { \
        instptn_stats_record_reject(INSTPTN_OPT_##OPC, \
                                    INSTPTN_REJECT_DISABLED); \
        return 0; \
    } \
} while (0)

#define instptn_check_cmp_jcc_0() INSTPTN_CHECK_XX_0(CMP_JCC)
#define instptn_check_test_jcc_0() INSTPTN_CHECK_XX_0(TEST_JCC)
#define instptn_check_bt_jcc_0() INSTPTN_CHECK_XX_0(BT_JCC)
#define instptn_check_cqo_idiv_0() INSTPTN_CHECK_XX_0(CQO_IDIV)
#define instptn_check_cmp_sbb_0() INSTPTN_CHECK_XX_0(CMP_SBB)
#define instptn_check_comisd_jcc_0() INSTPTN_CHECK_XX_0(COMISD_JCC)
#define instptn_check_comiss_jcc_0() INSTPTN_CHECK_XX_0(COMISS_JCC)
#define instptn_check_ucomisd_jcc_0() INSTPTN_CHECK_XX_0(UCOMISD_JCC)
#define instptn_check_ucomiss_jcc_0() INSTPTN_CHECK_XX_0(UCOMISS_JCC)
#define instptn_check_xor_div_0() INSTPTN_CHECK_XX_0(XOR_DIV)
#define instptn_check_cdq_idiv_0() INSTPTN_CHECK_XX_0(CDQ_IDIV)

#define instptn_check_cmp_xx_jcc_0() INSTPTN_CHECK_XX_0(CMP_XX_JCC)
#define instptn_check_test_xx_jcc_0() INSTPTN_CHECK_XX_0(TEST_XX_JCC)
#define instptn_check_bt_xx_jcc_0() INSTPTN_CHECK_XX_0(BT_XX_JCC)
#define instptn_check_comisd_xx_jcc_0() INSTPTN_CHECK_XX_0(COMISD_XX_JCC)
#define instptn_check_comiss_xx_jcc_0() INSTPTN_CHECK_XX_0(COMISS_XX_JCC)
#define instptn_check_ucomisd_xx_jcc_0() INSTPTN_CHECK_XX_0(UCOMISD_XX_JCC)
#define instptn_check_ucomiss_xx_jcc_0() INSTPTN_CHECK_XX_0(UCOMISS_XX_JCC)

#define instptn_check_cmp_xxcc_0() INSTPTN_CHECK_XX_0(CMP_XXCC)
#define instptn_check_test_xxcc_0() INSTPTN_CHECK_XX_0(TEST_XXCC)

#define instptn_check_ucomisd_seta_0() INSTPTN_CHECK_XX_0(UCOMISD_SETA)
#define instptn_check_sub_jcc_0() INSTPTN_CHECK_XX_0(SUB_JCC)

#define instptn_check_movaps_vst_x4_0() INSTPTN_CHECK_XX_0(MOVAPS_VST_X4)
#define instptn_check_neg_cmovcc_0() INSTPTN_CHECK_XX_0(NEG_CMOVCC)

#define instptn_check_shr_jcc_0() INSTPTN_CHECK_XX_0(SHR_JCC)
#define instptn_check_sar_jcc_0() INSTPTN_CHECK_XX_0(SAR_JCC)
#define instptn_check_shr_je_0() INSTPTN_CHECK_XX_0(SHR_JE)
#define instptn_check_and_jcc_0() INSTPTN_CHECK_XX_0(AND_JCC)
#define instptn_check_add_jcc_0() INSTPTN_CHECK_XX_0(ADD_JCC)
#define instptn_check_or_jcc_0() INSTPTN_CHECK_XX_0(OR_JCC)
#define instptn_check_xor_jcc_0() INSTPTN_CHECK_XX_0(XOR_JCC)
#define instptn_check_dec_jcc_0() INSTPTN_CHECK_XX_0(DEC_JCC)
#define instptn_check_or_xx_jcc_0() INSTPTN_CHECK_XX_0(OR_XX_JCC)
#else
#define instptn_check_void(option)
#define instptn_check_false(option)
#define instptn_check_cmp_jcc_0()
#define instptn_check_test_jcc_0()
#define instptn_check_bt_jcc_0()
#define instptn_check_cqo_idiv_0()
#define instptn_check_cmp_sbb_0()
#define instptn_check_comisd_jcc_0()
#define instptn_check_comiss_jcc_0()
#define instptn_check_ucomisd_jcc_0()
#define instptn_check_ucomiss_jcc_0()
#define instptn_check_xor_div_0()
#define instptn_check_cdq_idiv_0()

#define instptn_check_cmp_xx_jcc_0()
#define instptn_check_test_xx_jcc_0()
#define instptn_check_bt_xx_jcc_0()
#define instptn_check_comisd_xx_jcc_0()
#define instptn_check_comiss_xx_jcc_0()
#define instptn_check_ucomisd_xx_jcc_0()
#define instptn_check_ucomiss_xx_jcc_0()

#define instptn_check_cmp_xxcc_0()
#define instptn_check_test_xxcc_0()

#define instptn_check_ucomisd_seta_0()
#define instptn_check_sub_jcc_0()

#define instptn_check_movaps_vst_x4_0()
#define instptn_check_neg_cmovcc_0()

#define instptn_check_shr_jcc_0()
#define instptn_check_sar_jcc_0()
#define instptn_check_shr_je_0()
#define instptn_check_and_jcc_0()
#define instptn_check_add_jcc_0()
#define instptn_check_or_jcc_0()
#define instptn_check_xor_jcc_0()
#define instptn_check_dec_jcc_0()
#define instptn_check_or_xx_jcc_0()
#endif

bool try_translate_instptn(IR1_INST *pir1);

#endif
