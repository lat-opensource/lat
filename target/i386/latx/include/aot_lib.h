/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file aot_lib.h
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT header
 */
#ifndef __AOT_LIB_H_
#define __AOT_LIB_H_

#include "qemu-def.h"

typedef struct lib_info {
    char *name;
    void *buffer;
    bool is_unmapped;
} lib_info;

void lib_tree_init(void);

lib_info *lib_tree_insert(char *name, void *buffer);

gint get_lib_num(void);
void do_lib_record(lib_info **lib_info_vector);
lib_info *lib_tree_lookup(char *name);

#endif
