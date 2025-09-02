/**
 * @file ts.h
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief TU optimization header file
 */
#ifndef __TS_H_
#define __TS_H_
#include "aot.h"
#include "segment.h"
struct tb_tmp_message{
    target_ulong pc;
    uint32_t cflags;
    uint16_t bool_flags;
    TranslationBlock *tb;
};
typedef struct tb_tmp_message tb_tmp_message;
void pre_translate(int begin_id, int end_id, CPUState *cpu,
		tb_tmp_message *tb_pc_vector);
TranslationBlock *aot_tb_lookup(target_ulong pc, int cflags);
int aot_tb_insert(TranslationBlock *tb);
target_ulong get_curr_seg_end(target_ulong curr_pc);
void get_dynamic_message(TranslationBlock **tb_list, int tb_num,
        seg_info **seg_info_vector, int *seg_info_num);
char is_pe(char *file_name);
int is_pe_file(const char *filename);
void ts_push_back(TranslationBlock *tb);
void pop_back(void);
char is_bad_tb(TranslationBlock *tb);
void my_debug_ts1(TranslationBlock *tb);
void dump_ir1(TranslationBlock *tb);
uint64 translate_lib(seg_info **seg_info_vector, int begin_id,
        int end_id, CPUState *cpu, tb_tmp_message *tb_message_vector);
extern __thread TranslationBlock **ts_vector;
extern __thread int in_pre_translate;
#endif
