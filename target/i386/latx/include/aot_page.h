/**
 * @file aot_page.h
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT header
 */
#ifndef __AOT_PAGE_H_
#define __AOT_PAGE_H_
#include "aot.h"
#include "qemu-def.h"

typedef enum page_state_type{
    PAGE_UNLOAD = 0,
    PAGE_FLUSH,
    PAGE_SMC,
    PAGE_LOADED,
    CANNOT_OVERLOAD,
    PAGE_NOINFO,
    HASH_PAGE_LOADED,
    HASH_PAGE_NOINFO,
}page_state_type;

typedef struct PageDesc {
#ifdef CONFIG_SOFTMMU
    /*
     *in order to optimize self modifying code, we count the number
     *of lookups we do to a given page to use a bitmap
     */
    unsigned long *code_bitmap;
    unsigned int code_write_count;
#endif
#ifndef CONFIG_USER_ONLY
    QemuSpin lock;
    /* list of TBs intersecting this ram page */
    uintptr_t first_tb;
#endif
#ifdef CONFIG_LATX_AOT
    uint8_t page_state;
#endif
} PageDesc;

PageDesc *page_find_alloc(tb_page_addr_t index, int alloc);

typedef struct page_info {
    target_ulong start, end;
    void *buffer;
    aot_segment *p_segment;
    char *seg_name;
    bool is_running;
} page_info;

void page_tree_init(void);
void page_tree_insert(target_ulong start, target_ulong end, 
        void *buffer, aot_segment *p_segment, char *seg_name);
gint get_page_num(void);
void do_page_record(page_info **page_info_vector);
page_info *page_tree_lookup(target_ulong page);
void page_tree_remove(target_ulong start, target_ulong end);

#endif
