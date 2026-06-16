/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "mem.h"

void *mm_malloc(int size)
{
    lsassertm(size, "LATX-ERR %s size = 0!\n", __func__);
    void *retval = malloc(size);
    lsassertm(retval != NULL, "dbt: cannot allocate memory (%d bytes)\n", size);
    return retval;
}

void *mm_calloc(int num, int size)
{
    lsassertm(size, "LATX-ERR %s size = 0!\n", __func__);
    void *retval = calloc(num, size);
    lsassertm((retval != NULL) && size ,
        "%d dbt: cannot allocate memory (%d bytes)\n", getpid(), size);
    return retval;
}

void *mm_realloc(void *ptr, int size)
{
    lsassertm(size, "LATX-ERR %s size = 0!\n", __func__);
    void *retval = realloc(ptr, size);
    lsassertm(retval != NULL, "dbt: cannot allocate memory (%d bytes)\n", size);
    return retval;
}

void mm_free(void *ptr) { free(ptr); }

void *mmi_palloc(int size, char *file_name, int line_num)
{
    lsassertm(size, "LATX-ERR %s size = 0!\n", __func__);
    void *retval = malloc(size);
    lsassertm(retval != NULL, "dbt: cannot allocate memory (%d bytes)\n", size);
    return retval;
}
