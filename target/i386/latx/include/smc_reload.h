#ifndef LATX_SMC_RELOAD_H
#define LATX_SMC_RELOAD_H

#include "qemu-def.h"

struct seg_info;

typedef struct SMCReloadInfo {
    unsigned int tb_count;
    target_ulong page_addr;
    TranslationBlock **tb_vector;
    uint8_t *ir1_code_buffer;
} SMCReloadInfo;

void smc_reload_tree_init(void);
void smc_reload_tree_insert(SMCReloadInfo *reload_node);
SMCReloadInfo *smc_reload_tree_lookup(target_ulong page_addr);
void smc_reload_tree_remove(SMCReloadInfo *reload_node);
unsigned int smc_reload_tree_get_node_count(void);
void smc_reload_tree_foreach(void);
void smc_reload_tree_clear(void);
bool smc_reload_segment_is_candidate(const struct seg_info *segment);
void smc_reload_save_tb_code(uint8_t *dest, target_ulong guest_pc,
                             size_t size);

#endif
