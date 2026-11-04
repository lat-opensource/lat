/* SPDX-License-Identifier: GPL-2.0-only */
#include "qemu/osdep.h"

void *__wrap_malloc(size_t size);
void __wrap_free(void *ptr);

#define malloc __wrap_malloc
#define free __wrap_free
#include "../file_ctx.c"
