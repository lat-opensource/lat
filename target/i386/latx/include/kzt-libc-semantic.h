/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef LATX_KZT_LIBC_SEMANTIC_H
#define LATX_KZT_LIBC_SEMANTIC_H

#include <stddef.h>
#include <stdint.h>

typedef struct CPUX86State CPUX86State;

#ifdef CONFIG_LATX_KZT
/*
 * Non-global locale projections are owned by this Guest context. Handles
 * observed through uselocale(0) are borrowed until context destruction:
 * use duplocale() before passing them to freelocale() or as newlocale() base.
 * Never share such a borrowed handle with another thread/context.
 */
int kzt_libc_semantic_enter_current(void);
void kzt_libc_semantic_leave_current(void);
int kzt_libc_semantic_initialize(CPUX86State *env);
int kzt_libc_semantic_process_ready(void);
void kzt_libc_semantic_process_reset(CPUX86State *env);
void kzt_libc_semantic_after_fork_child(void);
int kzt_libc_semantic_prepare_process_locale(CPUX86State *env);
void kzt_libc_semantic_destroy(CPUX86State *env);

void kzt_libc_semantic_internal_enter(CPUX86State *env);
void kzt_libc_semantic_internal_leave(CPUX86State *env);

int kzt_libc_semantic_host_to_guest_enter(CPUX86State *env,
                                           int host_errno,
                                           int host_h_errno);
void kzt_libc_semantic_host_to_guest_leave(CPUX86State *env);

void kzt_libc_semantic_guest_to_host_enter(CPUX86State *env);
void kzt_libc_semantic_guest_to_host_leave(CPUX86State *env);

uintptr_t kzt_libc_semantic_setlocale(CPUX86State *env, int category,
                                      const char *locale);

#else
static inline int kzt_libc_semantic_initialize(CPUX86State *env)
{
    (void)env;
    return 0;
}

static inline int kzt_libc_semantic_process_ready(void)
{
    return 0;
}

static inline void kzt_libc_semantic_process_reset(CPUX86State *env)
{
    (void)env;
}

static inline void kzt_libc_semantic_after_fork_child(void)
{
}

static inline void kzt_libc_semantic_destroy(CPUX86State *env)
{
    (void)env;
}

static inline int kzt_libc_semantic_prepare_process_locale(
    CPUX86State *env)
{
    (void)env;
    return 0;
}

static inline void kzt_libc_semantic_internal_enter(CPUX86State *env)
{
    (void)env;
}

static inline void kzt_libc_semantic_internal_leave(CPUX86State *env)
{
    (void)env;
}

static inline int kzt_libc_semantic_host_to_guest_enter(
    CPUX86State *env, int host_errno, int host_h_errno)
{
    (void)env;
    (void)host_errno;
    (void)host_h_errno;
    return 0;
}

static inline void kzt_libc_semantic_host_to_guest_leave(CPUX86State *env)
{
    (void)env;
}

static inline void kzt_libc_semantic_guest_to_host_enter(CPUX86State *env)
{
    (void)env;
}

static inline void kzt_libc_semantic_guest_to_host_leave(CPUX86State *env)
{
    (void)env;
}

static inline uintptr_t kzt_libc_semantic_setlocale(
    CPUX86State *env, int category, const char *locale)
{
    (void)env;
    (void)category;
    (void)locale;
    return 0;
}

#endif

#endif
