#include "qemu/osdep.h"

#include "aot_recover_tb.h"
#include "latx-options.h"
#include "accel/tcg/internal.h"
#include "exec/cpu-all.h"
#include "exec/cpu_ldst.h"
#include "exec/translate-all.h"
#include "aot.h"
#include "smc_reload.h"

#ifdef CONFIG_LATX_AOT
bool smc_reload_segment_is_candidate(const struct seg_info *segment)
{
    return segment == NULL || segment->buffer == NULL;
}

void smc_reload_save_tb_code(uint8_t *dest, target_ulong guest_pc,
                             size_t size)
{
    memcpy(dest, g2h_untagged(guest_pc), size);
}

int smc_page_reload(target_ulong page_addr, uint32_t cflags)
{
    SMCReloadInfo *reload_node;
    TranslationBlock *tb;
    size_t ir1_offset;
    unsigned int tb_id;
    int p_flags;

    if (!option_smc_reload) {
        return 0;
    }
    reload_node = smc_reload_tree_lookup(page_addr);
    if (!reload_node) {
        return 0;
    }

    p_flags = page_get_flags(page_addr);
    if (!(p_flags & (PAGE_READ | PAGE_EXEC))) {
        return 0;
    }
    if ((p_flags & PAGE_WRITE) && !page_is_shadow_not_shmm(page_addr)) {
        mprotect(g2h_untagged(page_addr & qemu_host_page_mask),
                 qemu_host_page_size, PROT_READ);
    }

    tcg_ctx->tb_cflags = cflags;
    ir1_offset = 0;
    for (tb_id = 0; tb_id < reload_node->tb_count; tb_id++) {
        tb = reload_node->tb_vector[tb_id];
        if (!memcmp(g2h_untagged(tb->pc),
                    reload_node->ir1_code_buffer + ir1_offset, tb->size)) {
            tb->cflags &= ~CF_INVALID;
            if (!use_tu_jmp(tb)) {
                if (tb->jmp_reset_offset[0] !=
                    TB_JMP_RESET_OFFSET_INVALID) {
                    tb_reset_jump(tb, 0);
                }
                if (tb->jmp_reset_offset[1] !=
                    TB_JMP_RESET_OFFSET_INVALID) {
                    tb_reset_jump(tb, 1);
                }
            }
            tb->jmp_list_head = 0;
            tb->jmp_list_next[0] = 0;
            tb->jmp_list_next[1] = 0;
            tb->jmp_dest[0] = 0;
            tb->jmp_dest[1] = 0;
            aot_tb_register(tb);
        }
        ir1_offset += tb->size;
    }
    smc_reload_tree_remove(reload_node);
    return 1;
}
#endif
