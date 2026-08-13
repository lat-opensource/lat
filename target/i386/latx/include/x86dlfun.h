/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LATX_X86DLFUN_H
#define LATX_X86DLFUN_H

void *my_dlopen(void *filename, int flag);
void *my_dlmopen(void *lmid, void *filename, int flag);
char *my_dlerror(void);
void *my_dlsym(void *handle, void *symbol);
int my_dlclose(void *handle);
int my_dladdr(void *addr, void *info);
int my_dladdr1(void *addr, void *info, void **extra_info, int flags);
void *my_dlvsym(void *handle, void *symbol, const char *vername);
int my_dlinfo(void *handle, int request, void *info);

int init_x86dlfun_from(const char *primary, const char *fallback);

#endif
