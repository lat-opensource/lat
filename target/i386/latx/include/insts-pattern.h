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

#define INSTPTN_OPC_CMP_XXCC_CON    0x400000
#define INSTPTN_OPC_TEST_XXCC_CON   0x800000

#define INSTPTN_OPC_UCOMISD_SETA   0x1000000
#define INSTPTN_OPC_SUB_JCC        0x2000000

typedef int scan_elem_t;

#ifdef CONFIG_LATX_DECODE_DEBUG
void insts_pattern_scan_con(TranslationBlock *tb, IR1_INST *ir1, int index, scan_elem_t *scan_buf);
bool insts_pattern_scan_jcc_end(TranslationBlock *tb, IR1_INST *ir1, int index, scan_elem_t *scan_buf);
#endif

void insts_pattern_scan_con_bd(TranslationBlock *tb, IR1_INST *ir1, int index, scan_elem_t *scan_buf);
bool insts_pattern_scan_jcc_end_bd(TranslationBlock *tb, IR1_INST *pir1, int pir1_index, scan_elem_t *scan);


#ifdef CONFIG_LATX_INSTS_PATTERN

#define DEF_INSTS_PTN(_prex) \
        __attribute__((unused)) scan_elem_t _prex##_scaned_cond[INSTPTN_BUF_SIZE] = {-1, -1, -1, -1}; \
        __attribute__((unused)) scan_elem_t _prex##_scaned_jcc_end[1] = {-1}; \
        __attribute__((unused)) bool scan_head = true;
#ifdef CONFIG_LATX_DECODE_DEBUG
#define OPT_INSTS_PTN(tb, inst, index, _prex) \
        do { \
            if (option_instptn) { \
                if(inst->decode_engine == OPT_DECODE_BY_CAPSTONE) { \
                    insts_pattern_scan_con(tb, inst, index, _prex##_scaned_cond); \
                } else { \
                    insts_pattern_scan_con_bd(tb, inst, index, _prex##_scaned_cond); \
                } \
                if (scan_head) {  \
                    if(inst->decode_engine == OPT_DECODE_BY_CAPSTONE) { \
                        scan_head = insts_pattern_scan_jcc_end(tb, inst, index, _prex##_scaned_jcc_end); \
                    } else { \
                        scan_head = insts_pattern_scan_jcc_end_bd(tb, inst, index, _prex##_scaned_jcc_end); \
                    } \
                } \
            } \
        } while (0)
#else
#define OPT_INSTS_PTN(tb, inst, index, _prex) \
        do { \
            if (option_instptn) { \
                insts_pattern_scan_con_bd(tb, inst, index, _prex##_scaned_cond); \
                if (scan_head) {  \
                    scan_head = insts_pattern_scan_jcc_end_bd(tb, inst, index, _prex##_scaned_jcc_end); \
                } \
            } \
        } while (0)
#endif
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

#define instptn_check_cmp_xxcc_con_0() INSTPTN_CHECK_XX_0(CMP_XXCC_CON)
#define instptn_check_test_xxcc_con_0() INSTPTN_CHECK_XX_0(TEST_XXCC_CON)

#define instptn_check_ucomisd_seta_0() INSTPTN_CHECK_XX_0(UCOMISD_SETA)
#define instptn_check_sub_jcc_0() INSTPTN_CHECK_XX_0(SUB_JCC)

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

#define instptn_check_cmp_xxcc_con_0()
#define instptn_check_test_xxcc_con_0()

#define instptn_check_ucomisd_seta_0()
#define instptn_check_sub_jcc_0()
#endif

bool try_translate_instptn(IR1_INST *pir1);
bool try_translate_instptn_bd(IR1_INST *pir1);

#endif
