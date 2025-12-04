/**
 * @file aot_link_seg.h
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT header
 */
#ifndef _AOT_LINK_SEG_H_
#define _AOT_LINK_SEG_H_

#include "qemu-def.h"
#include "qemu.h"

typedef enum AOT_LINK_TYPE {
    AOT_LINK_TYPE_TB_LINK,
    AOT_LINK_TYPE_JRRA,
} AOT_LINK_TYPE;
typedef struct aot_link_info {
    TranslationBlock *curr;
    target_ulong pc;
    const void *addr;
    uint32_t flags;
    uint32_t cflags;
    CPUState *cpu;
    AOT_LINK_TYPE type;
} aot_link_info;

void aot_link_tree_init(void);
void aot_link_tree_insert(CPUState *cpu, TranslationBlock *curr,
    target_ulong pc, const void *addr, uint32_t flags, uint32_t cflags, AOT_LINK_TYPE type);
void try_aot_link(void);
#endif
