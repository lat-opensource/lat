/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file aot_merge.h
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT header
 */
#ifndef _AOT_MERGE_H_
#define _AOT_MERGE_H_
#include "qemu-def.h"
#include "aot.h"
extern time_t aot_st_ctime;
typedef struct merge_seg_info {
    GTree *tb_tree;
    size_t offset;
    char *file_name;
    seg_info *s_info;
} merge_seg_info;
typedef struct merge_tb_info {
    size_t offset;
    struct aot_tb *tb;
    aot_rel *aot_rel_table;
    int rel_table_num;
} merge_tb_info;
void do_merge_seg_aot(void);
void merge_segment_tree_init(void);
int is_tb_in_aot(char *seg_name, size_t offset, size_t pc_offset);

void aot2_merge(char *curr_lib_name, int first_seg_id,
		int last_seg_id, CPUState *cpu);
#endif
