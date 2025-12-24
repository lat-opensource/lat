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

#define INSTPTN_BUF_SIZE 4

#define INSTPTN_OPC_NONE        0x000000
#define INSTPTN_OPC_NOP         0x000001
#define INSTPTN_OPC_NOP_FOR_EXP 0x000002
#define INSTPTN_OPC_CMP_JCC     0x000010
#define INSTPTN_OPC_TEST_JCC    0x000020
#define INSTPTN_OPC_BT_JCC      0x000040
#define INSTPTN_OPC_CQO_IDIV    0x000080
#define INSTPTN_OPC_CMP_SBB     0x000100
#define INSTPTN_OPC_COMISD_JCC  0x000200
#define INSTPTN_OPC_COMISS_JCC  0x000400
#define INSTPTN_OPC_UCOMISD_JCC 0x000800
#define INSTPTN_OPC_UCOMISS_JCC 0x001000
#define INSTPTN_OPC_XOR_DIV     0x002000
#define INSTPTN_OPC_CDQ_IDIV    0x004000

#define INSTPTN_OPC_CMP_XX_JCC     0x008000
#define INSTPTN_OPC_TEST_XX_JCC    0x010000
#define INSTPTN_OPC_BT_XX_JCC      0x020000
#define INSTPTN_OPC_COMISD_XX_JCC  0x040000
#define INSTPTN_OPC_COMISS_XX_JCC  0x080000
#define INSTPTN_OPC_UCOMISD_XX_JCC 0x100000
#define INSTPTN_OPC_UCOMISS_XX_JCC 0x200000

#define INSTPTN_OPC_CMP_XXCC    0x400000
#define INSTPTN_OPC_TEST_XXCC   0x800000

#define INSTPTN_OPC_UCOMISD_SETA   0x1000000
#define INSTPTN_OPC_SUB_JCC        0x2000000
#define INSTPTN_OPC_MOVAPS_VST_X4  0x4000000
#define INSTPTN_OPC_NEG_CMOVCC     0x8000000

#define INSTPTN_OPC_SHR_JCC        0x10000000
#define INSTPTN_OPC_AND_JCC        0x20000000

typedef int scan_elem_t;

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
                if (scan_head)  \
                    scan_head = insts_pattern_scan_jcc_end(tb, inst, index, _prex##_scaned_jcc_end); \
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
    if (!(option_instptn & (INSTPTN_OPC_##OPC >> 4))) return 0;      \
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
#define instptn_check_and_jcc_0() INSTPTN_CHECK_XX_0(AND_JCC)
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
#define instptn_check_and_jcc_0()
#endif

bool try_translate_instptn(IR1_INST *pir1);

#endif
