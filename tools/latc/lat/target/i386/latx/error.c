/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "error.h"
#include <execinfo.h>
#include <stdlib.h>
#include <stdio.h>

void print_stack_trace(void)
{
#define BTSIZT 64
    void    * array[BTSIZT] = {0};
    size_t  size;
    char    ** strings;
    size_t  i;

    size = backtrace(array, BTSIZT);
    strings = backtrace_symbols (array, size);

    for (i = 0; i < size; i++)
        printf ("#%2ld %s\n", i, strings[i]);

    free (strings);
    strings = NULL;
}
