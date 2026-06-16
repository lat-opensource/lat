/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef _MEM_H_
#define _MEM_H_

#include "error.h"
#include <stdlib.h>

void *mm_malloc(int size);
void *mm_calloc(int num, int size);
void *mm_realloc(void *ptr, int size);
void mm_free(void *ptr);
void *mmi_palloc(int size, char *file_name, int line_num);

#define mm_palloc(size) mmi_palloc(size, __FUNCTION__, __LINE__)

#endif
