/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef LATX_KZT_GUEST_TLS_H
#define LATX_KZT_GUEST_TLS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef CONFIG_LATX_KZT
#include "kzt-runtime.h"
#else
static inline bool latx_kzt_guest_tls_enabled(void)
{
    return false;
}
#endif

typedef struct CPUX86State CPUX86State;

#define KZT_GUEST_TLS_REFRESH_BUSY 1

#ifdef CONFIG_LATX_KZT
int kzt_guest_tls_snapshot_parent(CPUX86State *parent,
                                  CPUX86State *child);
int kzt_guest_tls_clone_parent_snapshot(const CPUX86State *parent,
                                        CPUX86State *child);
/* BUSY is returned only before allocating or executing Guest TLS state. */
int kzt_guest_tls_initialize(CPUX86State *parent, CPUX86State *child);
int kzt_guest_tls_refresh(CPUX86State *env);
int kzt_guest_tls_refresh_if_needed(CPUX86State *env);
int kzt_guest_tls_refresh_local(CPUX86State *env);
void kzt_guest_tls_loader_tracking_enable(void);
void kzt_guest_tls_loader_tracking_reset(void);
void kzt_guest_tls_loader_event_begin(void);
void kzt_guest_tls_execution_enter(CPUX86State *env);
void kzt_guest_tls_execution_leave(CPUX86State *env);
int kzt_guest_tls_execution_pause(CPUX86State *env);
void kzt_guest_tls_execution_resume(CPUX86State *env, int paused);
void kzt_guest_tls_fork_prepare(void);
void kzt_guest_tls_fork_prepare_early(void);
void kzt_guest_tls_fork_parent(void);
void kzt_guest_tls_after_fork_child(CPUX86State *env);
void kzt_guest_loader_after_fork_child(void);
uintptr_t kzt_guest_loader_hold_open(uintptr_t guest_name);
void kzt_guest_loader_hold_close(uintptr_t handle);
int kzt_guest_tls_preinitialize_static(CPUX86State *env,
                                       uintptr_t link_map_addr);
void kzt_guest_tls_destroy(CPUX86State *env);
int kzt_guest_tls_cleanup_robust_list(CPUX86State *env);
#else
static inline int kzt_guest_tls_snapshot_parent(CPUX86State *parent,
                                                CPUX86State *child)
{
    (void)parent;
    (void)child;
    return 0;
}

static inline int kzt_guest_tls_clone_parent_snapshot(
    const CPUX86State *parent, CPUX86State *child)
{
    (void)parent;
    (void)child;
    return 0;
}

static inline int kzt_guest_tls_initialize(CPUX86State *parent,
                                           CPUX86State *child)
{
    (void)parent;
    (void)child;
    return 0;
}

static inline void kzt_guest_tls_destroy(CPUX86State *env)
{
    (void)env;
}

static inline int kzt_guest_tls_cleanup_robust_list(CPUX86State *env)
{
    (void)env;
    return 0;
}

static inline int kzt_guest_tls_refresh(CPUX86State *env)
{
    (void)env;
    return 0;
}

static inline int kzt_guest_tls_refresh_if_needed(CPUX86State *env)
{
    (void)env;
    return 0;
}

static inline int kzt_guest_tls_refresh_local(CPUX86State *env)
{
    (void)env;
    return 0;
}

static inline void kzt_guest_tls_loader_tracking_enable(void)
{
}

static inline void kzt_guest_tls_loader_tracking_reset(void)
{
}

static inline void kzt_guest_tls_loader_event_begin(void)
{
}

static inline void kzt_guest_tls_execution_enter(CPUX86State *env)
{
    (void)env;
}

static inline void kzt_guest_tls_execution_leave(CPUX86State *env)
{
    (void)env;
}

static inline int kzt_guest_tls_execution_pause(CPUX86State *env)
{
    (void)env;
    return 0;
}

static inline void kzt_guest_tls_execution_resume(CPUX86State *env,
                                                  int paused)
{
    (void)env;
    (void)paused;
}

static inline void kzt_guest_tls_fork_prepare(void)
{
}

static inline void kzt_guest_tls_fork_prepare_early(void)
{
}

static inline void kzt_guest_tls_fork_parent(void)
{
}

static inline void kzt_guest_tls_after_fork_child(CPUX86State *env)
{
    (void)env;
}

static inline void kzt_guest_loader_after_fork_child(void)
{
}

static inline uintptr_t kzt_guest_loader_hold_open(
    uintptr_t guest_name)
{
    (void)guest_name;
    return 0;
}

static inline void kzt_guest_loader_hold_close(uintptr_t handle)
{
    (void)handle;
}

static inline int kzt_guest_tls_preinitialize_static(
    CPUX86State *env, uintptr_t link_map_addr)
{
    (void)env;
    (void)link_map_addr;
    return 0;
}

#endif

#endif
