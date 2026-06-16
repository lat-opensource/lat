/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "include/exec/user/abitypes.h"
#include <la-ir2.h>

typedef void (*FILL_METHOD)(void);
typedef void (*FILL_ARG_METHOD)(IR2_OPND *);

struct method_trans {
    FILL_ARG_METHOD fill_argument;
    FILL_METHOD fill_return;
    FILL_METHOD fill_method_body;
};

struct lib_method_item {
    char *method_name;
    void *loongarch_addr;
    struct method_trans trans;
};

extern struct lib_method_item method_table[];
extern const int method_table_size;
void reg_priv_plt(abi_ulong method, abi_ulong plt_addr, abi_ulong org_value);
