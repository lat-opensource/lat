/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef KZT_RELRO_PREPROTECT_H
#define KZT_RELRO_PREPROTECT_H

#include <stddef.h>
#include <stdint.h>

/* Called with mmap_lock held after linux-user validates guest mprotect. */
void kzt_try_bind_before_guest_relro(uintptr_t start, size_t length, int prot);

#endif
