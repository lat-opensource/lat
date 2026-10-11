/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef __DEBUG_H_
#define __DEBUG_H_
#include <stdint.h>
#include "qemu/osdep.h"
#include "qemu/rcu.h"
#include "qemu/log.h"
#include "error.h"

typedef struct box64context_s box64context_t;
extern int relocation_log;    // log level
extern int relocation_dump;   // dump elf or not
extern int box64_pagesize;
extern uintptr_t box64_load_addr;
extern int dlsym_error;    // log dlsym error
extern int kzt_call_log;
extern int allow_missing_libs;
extern int  box64_nogtk;
extern int box64_prefer_wrapped;
extern int box64_prefer_emulated;
extern int box64_nopulse;   // disabling the use of wrapped pulseaudio
extern int box64_novulkan;  // disabling the use of wrapped vulkan
extern uintptr_t fmod_smc_start, fmod_smc_end; // to handle libfmod (from Unreal) SMC (self modifying code)
extern int jit_gdb; // launch gdb when a segfault is trapped
extern int box64_tcmalloc_minimal;  // when using tcmalloc_minimal
extern char* libGL;
#define LOG_NONE 0
#define LOG_INFO 1
#define LOG_DEBUG 2
#define LOG_NEVER 3
#define LOG_VERBOSE 3
#ifdef CONFIG_LATX_DEBUG
#if 0//use qemu_log
#define printf_log(L, ...) {if((L)<=relocation_log) {qemu_log(__VA_ARGS__);}}

#define printf_dump(L, ...) {if(relocation_dump || ((L)<=relocation_log)) {qemu_log(__VA_ARGS__);}}

#define printf_dlsym(L, ...) {if(dlsym_error || ((L)<=relocation_log)) {qemu_log(__VA_ARGS__);}}
#define printf_kzt_call(L, ...) {if(kzt_call_log) {qemu_log(__VA_ARGS__);}}
#else
#define printf_log(L, ...) {if((L)<=relocation_log) {fprintf(stderr,__VA_ARGS__);}}

#define printf_dump(L, ...) {if(relocation_dump || ((L)<=relocation_log)) {fprintf(stderr, __VA_ARGS__);}}

#define printf_dlsym(L, ...) {if(dlsym_error || ((L)<=relocation_log)) {fprintf(stderr, __VA_ARGS__);}}
#define printf_kzt_call(L, ...) {if(kzt_call_log) {fprintf(stderr, __VA_ARGS__);}}
#endif
#else
#define printf_log(L, ...)         ((void)0)
#define printf_dump(L, ...)         ((void)0)
#define printf_dlsym(L, ...)         ((void)0)
#define printf_kzt_call(L, ...)         ((void)0)
#endif
#define EXPORT __attribute__((visibility("default")))
#define EXPORTDYN 

extern void* __libc_malloc(size_t);
extern void* __libc_realloc(void*, size_t);
extern void* __libc_calloc(size_t, size_t);
extern void  __libc_free(void*);
extern void* __libc_memalign(size_t, size_t);

#define box_malloc      __libc_malloc
#define box_realloc     __libc_realloc
#define box_calloc      __libc_calloc
#define box_free        __libc_free
#define box_memalign    __libc_memalign 
extern char* box_strdup(const char* s);
#if defined(CONFIG_LATX_KZT) && defined(CONFIG_LATX_DEBUG)
void latx_kzt_debuginfo_check(void);
void AddDebugInfo(int type, char *name, unsigned long start, unsigned long end);
#else
#define latx_kzt_debuginfo_check() ((void)0)
#define AddDebugInfo(type, name, start, end) ((void)0)
#endif
#endif //__DEBUG_H_
