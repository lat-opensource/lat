/**
 * @file hbr.h
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief HBR optimization header file
 */
#ifndef _HBR_H_
#define _HBR_H_
#include "common.h"
#include "ir1.h"

#if defined(CONFIG_LATX_HBR) && defined(CONFIG_LATX_TU)
#define SHBR_NTYPE    0x1
#define SHBR_STYPE    0x2
#define SHBR_PTYPE    0x4
#define SHBR_SSE      0x8

void hbr_opt(TranslationBlock **tb_list, int tb_num_in_tu);
/* void tb_xmm_analyse(TranslationBlock *tb); */
uint8_t get_inst_type(IR1_INST *ir1);
bool can_shbr_opt64(IR1_INST *ir1);
bool can_shbr_opt32(IR1_INST *ir1);
#define SHBR_ON_64(_ir1) can_shbr_opt64(_ir1)
#define SHBR_ON_32(_ir1) can_shbr_opt32(_ir1)
#define SHBR_OPT(_tb, _tb_num)              \
    do {                                    \
        shbr_opt(_tb, tb_num);              \
    } while (0)                             \

#ifdef TARGET_X86_64
bool can_ghbr_opt(IR1_INST *ir1);
#define GHBR_ON(_ir1) can_ghbr_opt(_ir1)
#else
#define GHBR_ON(_ir1) (0)
#endif

#else /* !CONFIG_LATX_HBR */
#define SHBR_ON_64(_ir1) (0)
#define SHBR_ON_32(_ir1) (0)
#define GHBR_ON(_ir1) (0)
#endif

#endif
