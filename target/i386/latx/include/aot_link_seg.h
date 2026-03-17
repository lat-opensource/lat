/**
 * @file aot_link_seg.h
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT header
 */
#ifndef _AOT_LINK_SEG_H_
#define _AOT_LINK_SEG_H_

#include "qemu-def.h"
#include "qemu.h"

typedef struct aot_link_info {
    TranslationBlock *curr;
    target_ulong aim1_pc;
    target_ulong aim2_pc;
} aot_link_info;

void aot_link_tree_init(void);
void aot_link_tree_insert(TranslationBlock *curr,
        target_ulong aim1_pc, target_ulong aim2_pc);
void try_aot_link(void);
#endif
