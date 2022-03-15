/*
 *  Emulation of Linux signals
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SIGNAL_COMMON_H
#define SIGNAL_COMMON_H

/* Fallback addresses into sigtramp page. */
extern abi_ulong default_sigreturn;
extern abi_ulong default_rt_sigreturn;

void setup_sigtramp(abi_ulong tramp_page);

int on_sig_stack(unsigned long sp);
int sas_ss_flags(unsigned long sp);
abi_ulong target_sigsp(abi_ulong sp, struct target_sigaction *ka);
void target_save_altstack(target_stack_t *uss, CPUArchState *env);

/**
 * process_sigsuspend_mask: read and apply syscall-local signal mask
 *
 * Read the guest signal mask from @sigset, length @sigsize.
 * Convert that to a host signal mask and save it to sigpending_mask.
 *
 * Return value: negative target errno, or zero;
 *               store &sigpending_mask into *pset on success.
 */
int process_sigsuspend_mask(sigset_t **pset, target_ulong sigset,
                            target_ulong sigsize);

/**
 * finish_sigsuspend_mask: finish a sigsuspend-like syscall
 *
 * Set in_sigsuspend if we need to use the modified sigset
 * during process_pending_signals.
 */
static inline void finish_sigsuspend_mask(int ret)
{
    if (ret != -TARGET_ERESTARTSYS) {
        TaskState *ts = (TaskState *)thread_cpu->opaque;
        ts->in_sigsuspend = 1;
    }
}

static inline void target_sigemptyset(target_sigset_t *set)
{
    memset(set, 0, sizeof(*set));
}

void host_to_target_sigset_internal(target_sigset_t *d,
                                    const sigset_t *s);
void target_to_host_sigset_internal(sigset_t *d,
                                    const target_sigset_t *s);
void tswap_siginfo(target_siginfo_t *tinfo,
                   const target_siginfo_t *info);
void set_sigmask(const sigset_t *set);
void force_sig(int sig);
void force_sigsegv(int oldsig);
#if defined(TARGET_ARCH_HAS_SETUP_FRAME)
void setup_frame(int sig, struct target_sigaction *ka,
                 target_sigset_t *set, CPUArchState *env);
#endif
void setup_rt_frame(int sig, struct target_sigaction *ka,
                    target_siginfo_t *info,
                    target_sigset_t *set, CPUArchState *env);
#endif

