#ifndef __SMC_RELOAD_H_
#define __SMC_RELOAD_H_

#include "qemu-def.h"

typedef struct reoload_info {
    int tb_num;
    target_ulong page_addr;
    TranslationBlock **tb_vector;
    void *ir1_code_buffer;
} reload_info;

void reload_tree_init(void);
void reload_tree_insert(reload_info *reload_node);
reload_info *reload_tree_lookup(target_ulong page_addr);
void reload_tree_remove(reload_info *reload_node);
gint get_reload_node_num(void);
void reload_tree_foreach(void);
void reload_tree_clear(void);

#endif
