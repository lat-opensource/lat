/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file aot_smc.h
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT header
 */
#ifndef __AOT_SMC_H_
#define __AOT_SMC_H_

#include "qemu-def.h"

typedef struct smc_info {
    target_ulong begin;
    target_ulong end;
    /* bool sign; */
} smc_info;

void smc_tree_init(void);
void smc_tree_insert(abi_ulong begin, abi_ulong end);
smc_info *smc_tree_lookup(target_ulong begin, 
        target_ulong end);
gint get_smc_num(void);
void do_smc_record(smc_info **smc_info_vector);

#endif
