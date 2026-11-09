/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file latx-signal.h
 * @author Hanlu Li <heuleehanlu@gmail.com>
 * @brief JRRA-unlink header file
 */
#ifndef _LATX_SIGNAL_H_
#define _LATX_SIGNAL_H_

#include <inttypes.h>
#include "qemu/osdep.h"
#include "exec/exec-all.h"
void link_indirect_jmp(CPUArchState *env);
void unlink_direct_jmp(TranslationBlock *tb);
#ifdef CONFIG_LATX_TU
void unlink_tu_jmp(TranslationBlock *tb);
#endif
void unlink_indirect_jmp(CPUArchState *env, TranslationBlock *tb, ucontext_t *uc);
bool signal_in_glue(CPUArchState *env, ucontext_t *uc);
void tb_exit_to_qemu(CPUArchState *env, ucontext_t *uc);
/*
 * The lookup lock is not recursive. Ptrace code-change notifications arriving
 * while it is held must be applied after unlocking and before guest execution.
 * These fields are owned by the current thread and its signal handler.
 * pending_page is zero, a page address, or UINTPTR_MAX for a full flush.
 */
extern __thread volatile sig_atomic_t latx_lookup_lock_active;
extern __thread volatile uintptr_t latx_lookup_pending_page;
void latx_lookup_drain_notification(void);
#endif /* _LATX_SIGNAL_H_ */
