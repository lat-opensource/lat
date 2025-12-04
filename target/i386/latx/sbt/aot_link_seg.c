/**
 * @file aot_link_seg.c
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT optimization
 */
#include "aot_link_seg.h"
#include "latx-options.h"
#include "reg-map.h"
#include "accel/tcg/internal.h"
#include "opt-jmp.h"

#ifdef CONFIG_LATX_AOT
static aot_link_info *aot_global_info;
static int aot_global_info_total;
static int aot_global_info_index;
void aot_link_tree_init(void)
{
    aot_global_info_total = 1000;
    aot_global_info = malloc(aot_global_info_total * sizeof(aot_link_info));
    aot_global_info_index = 0;
}
void aot_link_tree_insert(CPUState *cpu, TranslationBlock *curr,
    target_ulong pc, const void *addr, uint32_t flags, uint32_t cflags, AOT_LINK_TYPE type)
{
    if (aot_global_info_index >= aot_global_info_total) {
        aot_global_info_total += 1000;
        aot_global_info = realloc(aot_global_info,
            aot_global_info_total * sizeof(aot_link_info));
    }
    aot_link_info *info = aot_global_info + aot_global_info_index;
    info->curr = curr;
    info->pc = pc;
    info->addr = addr;
    info->flags = flags;
    info->cflags = cflags;
    info->cpu = cpu;
    info->type = type;
    aot_global_info_index++;
}

void try_aot_link(void)
{
    for (int i = 0; i < aot_global_info_index; i++) {
        aot_link_info *info = aot_global_info + i;
        if (info->type == AOT_LINK_TYPE_TB_LINK) {
            if (*((uint32_t *)info->addr) != 0x50000800) {
                TranslationBlock *next = tb_htable_lookup(info->cpu,
                    info->pc, 0, info->flags, info->cflags);
                    if (next) {
                        /*has_link++;*/
                        light_tb_target_set_jmp_target(0, (uintptr_t)info->addr,
                          (uintptr_t)info->addr, (uintptr_t)next->tc.ptr);
                    } else {
                        /*no_tb++;*/
                        *((uint32_t *)info->addr) = 0x50000800;/* b 0x8*/
                    }
            }
        } else if (info->type == AOT_LINK_TYPE_JRRA) {
            int index = 0;
            if (tb_page_addr1(info->curr) != -1) {
                index = 1;
            }
            TranslationBlock *next = tb_htable_lookup(info->cpu,
                info->pc, 0, info->flags, info->cflags);
            if (next && (((index ? tb_page_addr1(info->curr) : tb_page_addr0(info->curr)) & TARGET_PAGE_MASK)
                        == (tb_page_addr0(next) & TARGET_PAGE_MASK))) {
                int temp0 = reg_itemp_map[ITEMP0];
                uint64_t patch_pcalau12i, patch_ori;

                ptrdiff_t offset_high = (((uint64_t)next->tc.ptr >> 12) -
                  ((uintptr_t)info->addr >> 12))
                  & 0xfffff;
                ptrdiff_t offset_low = (uint64_t)next->tc.ptr & 0xfff;

                /* pcalau12i itemp0, offset_high */
                patch_pcalau12i = 0x1a000000 | temp0 | (offset_high << 5);
                /* ori itemp0, itemp0, offset_low */
                patch_ori = 0x03800000 | (offset_low << 10) |
                    (temp0 << 5) | temp0;
                *((uint64_t *)info->addr) = (patch_pcalau12i |
                    (patch_ori << 32));
	    } else {
                *((uint32_t *)info->addr) = 0x800;
                *((uint32_t *)info->addr + 1) = 0x50000000 | (3 << 10);
            }
        } else {
            lsassert(0);
        }
    }
    aot_global_info_index = 0;
}
#endif
