/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef LATX_KZT_GUEST_THREAD_H
#define LATX_KZT_GUEST_THREAD_H

#include <stdint.h>

typedef struct CPUX86State CPUX86State;

#ifdef CONFIG_LATX_KZT
void kzt_guest_thread_initialize(CPUX86State *env);
void kzt_guest_thread_destroy(CPUX86State *env);
void kzt_guest_thread_after_fork_child(void);
int kzt_guest_thread_key_create(CPUX86State *env, unsigned int *key,
                                uintptr_t destructor);
int kzt_guest_thread_key_delete(CPUX86State *env, unsigned int key);
int kzt_guest_thread_cxa_atexit(CPUX86State *env, uintptr_t destructor,
                               uintptr_t object, uintptr_t dso_handle);
#else
static inline void kzt_guest_thread_initialize(CPUX86State *env) {}
static inline void kzt_guest_thread_destroy(CPUX86State *env) {}
static inline void kzt_guest_thread_after_fork_child(void) {}
static inline int kzt_guest_thread_key_create(CPUX86State *env,
    unsigned int *key, uintptr_t destructor) { return -1; }
static inline int kzt_guest_thread_key_delete(CPUX86State *env,
    unsigned int key) { return -1; }
static inline int kzt_guest_thread_cxa_atexit(CPUX86State *env,
    uintptr_t destructor, uintptr_t object, uintptr_t dso_handle) { return -1; }
#endif

#endif
