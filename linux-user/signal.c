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
#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include <sys/ucontext.h>
#include <sys/resource.h>

#include "qemu.h"
#include "trace.h"
#include "signal-common.h"
#include "hw/core/tcg-cpu-ops.h"
#include "tcg/tcg.h"
#include "fpu/softfloat.h"
#ifdef CONFIG_LATX_AOT
#include "aot.h"
#endif
#ifdef CONFIG_LATX
#include "jrra.h"
#include "latx-signal.h"
#include "reg-map.h"
#include "latx-options.h"
#include "loongarch-extcontext.h"
#include "lsenv.h"
#endif
#include "exec/translate-all.h"
#if defined(CONFIG_LATX_KZT) && defined(CONFIG_LATX_DEBUG)
#include "debug.h"
#endif

static pthread_mutex_t sigact_mutex = PTHREAD_MUTEX_INITIALIZER;
static __thread int sigact_lock_count;
static void sigact_lock(void)
{
    if (sigact_lock_count++ == 0) {
        pthread_mutex_lock(&sigact_mutex);
    }
}

static void sigact_unlock(void)
{
    if (--sigact_lock_count == 0) {
        pthread_mutex_unlock(&sigact_mutex);
    }
}

/* Grab lock to make sure things are in a consistent state after fork().  */
void sigact_fork_start(void)
{
    if (sigact_lock_count)
        abort();
    pthread_mutex_lock(&sigact_mutex);
}

void sigact_fork_end(int child)
{
    if (child)
        pthread_mutex_init(&sigact_mutex, NULL);
    else
        pthread_mutex_unlock(&sigact_mutex);
}

#ifndef CONFIG_LOONGARCH_NEW_WORLD
static struct target_sigaction sigact_table[TARGET_NSIG + 1];
#else
static struct target_sigaction sigact_table[TARGET_NSIG];
#endif

static void host_signal_handler(int host_signum, siginfo_t *info,
                                void *puc);

/* Fallback addresses into sigtramp page. */
abi_ulong default_sigreturn;
abi_ulong default_rt_sigreturn;

/*
 * System includes define _NSIG as SIGRTMAX + 1,
 * but qemu (like the kernel) defines TARGET_NSIG as TARGET_SIGRTMAX
 * and the first signal is SIGHUP defined as 1
 * Signal number 0 is reserved for use as kill(pid, 0), to test whether
 * a process exists without sending it a signal.
 */
QEMU_BUILD_BUG_ON(__SIGRTMAX + 1 != _NSIG);
static uint8_t host_to_target_signal_table[_NSIG] = {
    [SIGHUP] = TARGET_SIGHUP,
    [SIGINT] = TARGET_SIGINT,
    [SIGQUIT] = TARGET_SIGQUIT,
    [SIGILL] = TARGET_SIGILL,
    [SIGTRAP] = TARGET_SIGTRAP,
    [SIGABRT] = TARGET_SIGABRT,
/*    [SIGIOT] = TARGET_SIGIOT,*/
    [SIGBUS] = TARGET_SIGBUS,
    [SIGFPE] = TARGET_SIGFPE,
    [SIGKILL] = TARGET_SIGKILL,
    [SIGUSR1] = TARGET_SIGUSR1,
    [SIGSEGV] = TARGET_SIGSEGV,
    [SIGUSR2] = TARGET_SIGUSR2,
    [SIGPIPE] = TARGET_SIGPIPE,
    [SIGALRM] = TARGET_SIGALRM,
    [SIGTERM] = TARGET_SIGTERM,
#ifdef SIGSTKFLT
    [SIGSTKFLT] = TARGET_SIGSTKFLT,
#endif
    [SIGCHLD] = TARGET_SIGCHLD,
    [SIGCONT] = TARGET_SIGCONT,
    [SIGSTOP] = TARGET_SIGSTOP,
    [SIGTSTP] = TARGET_SIGTSTP,
    [SIGTTIN] = TARGET_SIGTTIN,
    [SIGTTOU] = TARGET_SIGTTOU,
    [SIGURG] = TARGET_SIGURG,
    [SIGXCPU] = TARGET_SIGXCPU,
    [SIGXFSZ] = TARGET_SIGXFSZ,
    [SIGVTALRM] = TARGET_SIGVTALRM,
    [SIGPROF] = TARGET_SIGPROF,
    [SIGWINCH] = TARGET_SIGWINCH,
    [SIGIO] = TARGET_SIGIO,
    [SIGPWR] = TARGET_SIGPWR,
    [SIGSYS] = TARGET_SIGSYS,
    /* next signals stay the same */
};

static uint8_t target_to_host_signal_table[TARGET_NSIG + 1];

/* valid sig is between 1 and _NSIG - 1 */
int host_to_target_signal(int sig)
{
    if (sig < 1 || sig >= _NSIG) {
        return sig;
    }
    return host_to_target_signal_table[sig];
}

/* valid sig is between 1 and TARGET_NSIG */
int target_to_host_signal(int sig)
{
    if (sig < 1 || sig > TARGET_NSIG) {
        return sig;
    }
    return target_to_host_signal_table[sig];
}

static inline void target_sigaddset(target_sigset_t *set, int signum)
{
    signum--;
    abi_ulong mask = (abi_ulong)1 << (signum % TARGET_NSIG_BPW);
    set->sig[signum / TARGET_NSIG_BPW] |= mask;
}

static inline int target_sigismember(const target_sigset_t *set, int signum)
{
    signum--;
    abi_ulong mask = (abi_ulong)1 << (signum % TARGET_NSIG_BPW);
    return ((set->sig[signum / TARGET_NSIG_BPW] & mask) != 0);
}

void host_to_target_sigset_internal(target_sigset_t *d,
                                    const sigset_t *s)
{
    int host_sig, target_sig;
    target_sigemptyset(d);
    for (host_sig = 1; host_sig < _NSIG; host_sig++) {
        target_sig = host_to_target_signal(host_sig);
        if (target_sig < 1 || target_sig > TARGET_NSIG) {
            continue;
        }
        if (sigismember(s, host_sig)) {
            target_sigaddset(d, target_sig);
        }
    }
}

void target_sigpending(target_sigset_t *d)
{
    /* Assuming argument is correctly set before calling,
     * we just append the emulated pending signals.
     */
    TaskState *ts = (TaskState *)thread_cpu->opaque;
    int sig;
    for (sig = 1; sig <= TARGET_NSIG; sig++) {
        if (ts->sigtab[sig - 1].pending) {
            target_sigaddset(d, sig);
        }
    }
}

void host_to_target_sigset(target_sigset_t *d, const sigset_t *s)
{
    target_sigset_t d1;
    int i;

    host_to_target_sigset_internal(&d1, s);
    for(i = 0;i < TARGET_NSIG_WORDS; i++)
        d->sig[i] = tswapal(d1.sig[i]);
}

void target_to_host_sigset_internal(sigset_t *d,
                                    const target_sigset_t *s)
{
    int host_sig, target_sig;
    sigemptyset(d);
    for (target_sig = 1; target_sig <= TARGET_NSIG; target_sig++) {
        host_sig = target_to_host_signal(target_sig);
        if (host_sig < 1 || host_sig >= _NSIG) {
            continue;
        }
        if (target_sigismember(s, target_sig)) {
            sigaddset(d, host_sig);
        }
    }
}

void target_to_host_sigset(sigset_t *d, const target_sigset_t *s)
{
    target_sigset_t s1;
    int i;

    for(i = 0;i < TARGET_NSIG_WORDS; i++)
        s1.sig[i] = tswapal(s->sig[i]);
    target_to_host_sigset_internal(d, &s1);
}

void host_to_target_old_sigset(abi_ulong *old_sigset,
                               const sigset_t *sigset)
{
    target_sigset_t d;
    host_to_target_sigset(&d, sigset);
    *old_sigset = d.sig[0];
}

void target_to_host_old_sigset(sigset_t *sigset,
                               const abi_ulong *old_sigset)
{
    target_sigset_t d;
    int i;

    d.sig[0] = *old_sigset;
    for(i = 1;i < TARGET_NSIG_WORDS; i++)
        d.sig[i] = 0;
    target_to_host_sigset(sigset, &d);
}

int block_signals(void)
{
    TaskState *ts = (TaskState *)thread_cpu->opaque;
    sigset_t set;

    /* It's OK to block everything including SIGSEGV, because we won't
     * run any further guest code before unblocking signals in
     * process_pending_signals().
     */
    sigfillset(&set);
    sigdelset(&set, SIGSEGV);
    sigprocmask(SIG_SETMASK, &set, 0);

    return qatomic_xchg(&ts->signal_pending, 1);
}

/* Wrapper for sigprocmask function
 * Emulates a sigprocmask in a safe way for the guest. Note that set and oldset
 * are host signal set, not guest ones. Returns -TARGET_ERESTARTSYS if
 * a signal was already pending and the syscall must be restarted, or
 * 0 on success.
 * If set is NULL, this is guaranteed not to fail.
 */
int do_sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    TaskState *ts = (TaskState *)thread_cpu->opaque;

    if (oldset) {
        *oldset = ts->signal_mask;
    }

    if (set) {
        int i;

        if (block_signals()) {
            return -TARGET_ERESTARTSYS;
        }

        switch (how) {
        case SIG_BLOCK:
            sigorset(&ts->signal_mask, &ts->signal_mask, set);
            break;
        case SIG_UNBLOCK:
            for (i = 1; i <= NSIG; ++i) {
                if (sigismember(set, i)) {
                    sigdelset(&ts->signal_mask, i);
                }
            }
            break;
        case SIG_SETMASK:
            ts->signal_mask = *set;
            break;
        default:
            g_assert_not_reached();
        }

        /* Silently ignore attempts to change blocking status of KILL or STOP */
        sigdelset(&ts->signal_mask, SIGKILL);
        sigdelset(&ts->signal_mask, SIGSTOP);
    }
    return 0;
}

#if !defined(TARGET_NIOS2)
/* Just set the guest's signal mask to the specified value; the
 * caller is assumed to have called block_signals() already.
 */
void set_sigmask(const sigset_t *set)
{
    TaskState *ts = (TaskState *)thread_cpu->opaque;

    ts->signal_mask = *set;
}
#endif

/* sigaltstack management */

int on_sig_stack(unsigned long sp)
{
    TaskState *ts = (TaskState *)thread_cpu->opaque;

    return (sp - ts->sigaltstack_used.ss_sp
            < ts->sigaltstack_used.ss_size);
}

int sas_ss_flags(unsigned long sp)
{
    TaskState *ts = (TaskState *)thread_cpu->opaque;

    return (ts->sigaltstack_used.ss_size == 0 ? SS_DISABLE
            : on_sig_stack(sp) ? SS_ONSTACK : 0);
}

abi_ulong target_sigsp(abi_ulong sp, struct target_sigaction *ka)
{
    /*
     * This is the X/Open sanctioned signal stack switching.
     */
    TaskState *ts = (TaskState *)thread_cpu->opaque;

    if ((ka->sa_flags & TARGET_SA_ONSTACK) && !sas_ss_flags(sp)) {
        return ts->sigaltstack_used.ss_sp + ts->sigaltstack_used.ss_size;
    }
    return sp;
}

void target_save_altstack(target_stack_t *uss, CPUArchState *env)
{
    TaskState *ts = (TaskState *)thread_cpu->opaque;

    __put_user(ts->sigaltstack_used.ss_sp, &uss->ss_sp);
    __put_user(sas_ss_flags(get_sp_from_cpustate(env)), &uss->ss_flags);
    __put_user(ts->sigaltstack_used.ss_size, &uss->ss_size);
}

/* siginfo conversion */

static inline void host_to_target_siginfo_noswap(target_siginfo_t *tinfo,
                                                 const siginfo_t *info)
{
    int sig = host_to_target_signal(info->si_signo);
    int si_code = info->si_code;
    int si_type;
    tinfo->si_signo = sig;
    tinfo->si_errno = 0;
    tinfo->si_code = info->si_code;

    /* This memset serves two purposes:
     * (1) ensure we don't leak random junk to the guest later
     * (2) placate false positives from gcc about fields
     *     being used uninitialized if it chooses to inline both this
     *     function and tswap_siginfo() into host_to_target_siginfo().
     */
    memset(tinfo->_sifields._pad, 0, sizeof(tinfo->_sifields._pad));

    /* This is awkward, because we have to use a combination of
     * the si_code and si_signo to figure out which of the union's
     * members are valid. (Within the host kernel it is always possible
     * to tell, but the kernel carefully avoids giving userspace the
     * high 16 bits of si_code, so we don't have the information to
     * do this the easy way...) We therefore make our best guess,
     * bearing in mind that a guest can spoof most of the si_codes
     * via rt_sigqueueinfo() if it likes.
     *
     * Once we have made our guess, we record it in the top 16 bits of
     * the si_code, so that tswap_siginfo() later can use it.
     * tswap_siginfo() will strip these top bits out before writing
     * si_code to the guest (sign-extending the lower bits).
     */

    switch (si_code) {
    case SI_USER:
    case SI_TKILL:
    case SI_KERNEL:
        /* Sent via kill(), tkill() or tgkill(), or direct from the kernel.
         * These are the only unspoofable si_code values.
         */
        tinfo->_sifields._kill._pid = info->si_pid;
        tinfo->_sifields._kill._uid = info->si_uid;
        si_type = QEMU_SI_KILL;
        break;
    default:
        /* Everything else is spoofable. Make best guess based on signal */
        switch (sig) {
        case TARGET_SIGCHLD:
            tinfo->_sifields._sigchld._pid = info->si_pid;
            tinfo->_sifields._sigchld._uid = info->si_uid;
            tinfo->_sifields._sigchld._status = info->si_status;
            tinfo->_sifields._sigchld._utime = info->si_utime;
            tinfo->_sifields._sigchld._stime = info->si_stime;
            si_type = QEMU_SI_CHLD;
            break;
        case TARGET_SIGIO:
            tinfo->_sifields._sigpoll._band = info->si_band;
            tinfo->_sifields._sigpoll._fd = info->si_fd;
            si_type = QEMU_SI_POLL;
            break;
        default:
            /* Assume a sigqueue()/mq_notify()/rt_sigqueueinfo() source. */
            tinfo->_sifields._rt._pid = info->si_pid;
            tinfo->_sifields._rt._uid = info->si_uid;
            /* XXX: potential problem if 64 bit */
            tinfo->_sifields._rt._sigval.sival_ptr
                = (abi_ulong)(unsigned long)info->si_value.sival_ptr;
            si_type = QEMU_SI_RT;
            break;
        }
        break;
    }

    tinfo->si_code = deposit32(si_code, 16, 16, si_type);
}

void tswap_siginfo(target_siginfo_t *tinfo,
                   const target_siginfo_t *info)
{
    int si_type = extract32(info->si_code, 16, 16);
    int si_code = sextract32(info->si_code, 0, 16);

    __put_user(info->si_signo, &tinfo->si_signo);
    __put_user(info->si_errno, &tinfo->si_errno);
    __put_user(si_code, &tinfo->si_code);

    /* We can use our internal marker of which fields in the structure
     * are valid, rather than duplicating the guesswork of
     * host_to_target_siginfo_noswap() here.
     */
    switch (si_type) {
    case QEMU_SI_KILL:
        __put_user(info->_sifields._kill._pid, &tinfo->_sifields._kill._pid);
        __put_user(info->_sifields._kill._uid, &tinfo->_sifields._kill._uid);
        break;
    case QEMU_SI_TIMER:
        __put_user(info->_sifields._timer._timer1,
                   &tinfo->_sifields._timer._timer1);
        __put_user(info->_sifields._timer._timer2,
                   &tinfo->_sifields._timer._timer2);
        break;
    case QEMU_SI_POLL:
        __put_user(info->_sifields._sigpoll._band,
                   &tinfo->_sifields._sigpoll._band);
        __put_user(info->_sifields._sigpoll._fd,
                   &tinfo->_sifields._sigpoll._fd);
        break;
    case QEMU_SI_FAULT:
        __put_user(info->_sifields._sigfault._addr,
                   &tinfo->_sifields._sigfault._addr);
        break;
    case QEMU_SI_CHLD:
        __put_user(info->_sifields._sigchld._pid,
                   &tinfo->_sifields._sigchld._pid);
        __put_user(info->_sifields._sigchld._uid,
                   &tinfo->_sifields._sigchld._uid);
        __put_user(info->_sifields._sigchld._status,
                   &tinfo->_sifields._sigchld._status);
        __put_user(info->_sifields._sigchld._utime,
                   &tinfo->_sifields._sigchld._utime);
        __put_user(info->_sifields._sigchld._stime,
                   &tinfo->_sifields._sigchld._stime);
        break;
    case QEMU_SI_RT:
        __put_user(info->_sifields._rt._pid, &tinfo->_sifields._rt._pid);
        __put_user(info->_sifields._rt._uid, &tinfo->_sifields._rt._uid);
        __put_user(info->_sifields._rt._sigval.sival_ptr,
                   &tinfo->_sifields._rt._sigval.sival_ptr);
        break;
    default:
        g_assert_not_reached();
    }
}

void host_to_target_siginfo(target_siginfo_t *tinfo, const siginfo_t *info)
{
    target_siginfo_t tgt_tmp;
    host_to_target_siginfo_noswap(&tgt_tmp, info);
    tswap_siginfo(tinfo, &tgt_tmp);
}

/* XXX: we support only POSIX RT signals are used. */
/* XXX: find a solution for 64 bit (additional malloced data is needed) */
void target_to_host_siginfo(siginfo_t *info, const target_siginfo_t *tinfo)
{
    /* This conversion is used only for the rt_sigqueueinfo syscall,
     * and so we know that the _rt fields are the valid ones.
     */
    abi_ulong sival_ptr;

    __get_user(info->si_signo, &tinfo->si_signo);
    __get_user(info->si_errno, &tinfo->si_errno);
    __get_user(info->si_code, &tinfo->si_code);
    __get_user(info->si_pid, &tinfo->_sifields._rt._pid);
    __get_user(info->si_uid, &tinfo->_sifields._rt._uid);
    __get_user(sival_ptr, &tinfo->_sifields._rt._sigval.sival_ptr);
    info->si_value.sival_ptr = (void *)(long)sival_ptr;
}

static int fatal_signal (int sig)
{
    switch (sig) {
    case TARGET_SIGCHLD:
    case TARGET_SIGURG:
    case TARGET_SIGWINCH:
        /* Ignored by default.  */
        return 0;
    case TARGET_SIGCONT:
    case TARGET_SIGSTOP:
    case TARGET_SIGTSTP:
    case TARGET_SIGTTIN:
    case TARGET_SIGTTOU:
        /* Job control signals.  */
        return 0;
    default:
        return 1;
    }
}

/* returns 1 if given signal should dump core if not handled */
static int core_dump_signal(int sig)
{
    switch (sig) {
    case TARGET_SIGABRT:
    case TARGET_SIGFPE:
    case TARGET_SIGILL:
    case TARGET_SIGQUIT:
    case TARGET_SIGSEGV:
    case TARGET_SIGTRAP:
    case TARGET_SIGBUS:
        return (1);
    default:
        return (0);
    }
}

static void signal_table_init(void)
{
    int host_sig, target_sig, count;

    /*
     * Signals are supported starting from TARGET_SIGRTMIN and going up
     * until we run out of host realtime signals.
     * glibc at least uses only the lower 2 rt signals and probably
     * nobody's using the upper ones.
     * it's why SIGRTMIN (34) is generally greater than __SIGRTMIN (32)
     * To fix this properly we need to do manual signal delivery multiplexed
     * over a single host signal.
     * Attempts for configure "missing" signals via sigaction will be
     * silently ignored.
     */
    for (host_sig = SIGRTMIN; host_sig <= SIGRTMAX; host_sig++) {
        target_sig = host_sig - SIGRTMIN + TARGET_SIGRTMIN;
        if (target_sig <= TARGET_NSIG) {
            host_to_target_signal_table[host_sig] = target_sig;
        }
    }

    /* generate signal conversion tables */
    for (target_sig = 1; target_sig <= TARGET_NSIG; target_sig++) {
        target_to_host_signal_table[target_sig] = _NSIG; /* poison */
    }
    for (host_sig = 1; host_sig < _NSIG; host_sig++) {
        if (host_to_target_signal_table[host_sig] == 0) {
            host_to_target_signal_table[host_sig] = host_sig;
        }
        target_sig = host_to_target_signal_table[host_sig];
        if (target_sig <= TARGET_NSIG) {
            target_to_host_signal_table[target_sig] = host_sig;
        }
    }

    if (trace_event_get_state_backends(TRACE_SIGNAL_TABLE_INIT)) {
        for (target_sig = 1, count = 0; target_sig <= TARGET_NSIG; target_sig++) {
            if (target_to_host_signal_table[target_sig] == _NSIG) {
                count++;
            }
        }
        trace_signal_table_init(count);
    }
}

void signal_init(void)
{
    TaskState *ts = (TaskState *)thread_cpu->opaque;
    struct sigaction act;
    struct sigaction oact;
    int i;
    int host_sig;

    /* initialize signal conversion tables */
    signal_table_init();

    /* Set the signal mask from the host mask. */
    sigprocmask(0, 0, &ts->signal_mask);

    sigfillset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = host_signal_handler;
#ifndef CONFIG_LOONGARCH_NEW_WORLD
    for(i = 1; i <= TARGET_NSIG + 1; i++) {
#else
    for(i = 1; i <= TARGET_NSIG; i++) {
#endif
#ifdef CONFIG_GPROF
        if (i == TARGET_SIGPROF) {
            continue;
        }
#endif
        host_sig = target_to_host_signal(i);
        sigaction(host_sig, NULL, &oact);
        if (oact.sa_sigaction == (void *)SIG_IGN) {
            sigact_table[i - 1]._sa_handler = TARGET_SIG_IGN;
            /* Forked process may inherit SIG_IGN form parent, in this
               case we should bypass the host_signal_handler. If a new
               handler can be registered later by sigaction(). */
            continue;
        } else if (oact.sa_sigaction == (void *)SIG_DFL) {
            sigact_table[i - 1]._sa_handler = TARGET_SIG_DFL;
        }
        /* If there's already a handler installed then something has
           gone horribly wrong, so don't even try to handle that case.  */
        /* Install some handlers for our own use.  We need at least
           SIGSEGV and SIGBUS, to detect exceptions.  We can not just
           trap all signals because it affects syscall interrupt
           behavior.  But do trap all default-fatal signals.  */
        if (fatal_signal (i))
            sigaction(host_sig, &act, NULL);
    }
}

/* Force a synchronously taken signal. The kernel force_sig() function
 * also forces the signal to "not blocked, not ignored", but for QEMU
 * that work is done in process_pending_signals().
 */
void force_sig(int sig)
{
    CPUState *cpu = thread_cpu;
    CPUArchState *env = cpu->env_ptr;
    target_siginfo_t info;

    info.si_signo = sig;
    info.si_errno = 0;
    info.si_code = TARGET_SI_KERNEL;
    info._sifields._kill._pid = 0;
    info._sifields._kill._uid = 0;
    queue_signal(env, info.si_signo, QEMU_SI_KILL, &info);
}

/* Force a SIGSEGV if we couldn't write to memory trying to set
 * up the signal frame. oldsig is the signal we were trying to handle
 * at the point of failure.
 */
#if !defined(TARGET_RISCV)
void force_sigsegv(int oldsig)
{
    if (oldsig == SIGSEGV) {
        /* Make sure we don't try to deliver the signal again; this will
         * end up with handle_pending_signal() calling dump_core_and_abort().
         */
        sigact_table[oldsig - 1]._sa_handler = TARGET_SIG_DFL;
    }
    force_sig(TARGET_SIGSEGV);
}

#endif

/* abort execution with signal */
#if defined(CONFIG_LATX_DEBUG)
const char *latx_demangling(char *symbol);
static void latx_guest_backtrace(CPUArchState *env)
{
    int index = env->func_index;
    printf("\n============ LATX Guest Backtrace ============\n");
    for (int i = 0; i < FUNC_DEPTH; i++) {
        fprintf(stderr, "#%2d  %s\n", i + 1,
            latx_demangling(env->call_func[index % FUNC_DEPTH]));
        index++;
    }
    latx_guest_stack_init(env);
}
#endif
static void QEMU_NORETURN dump_core_and_abort(int target_sig)
{
    CPUState *cpu = thread_cpu;
    CPUArchState *env = cpu->env_ptr;
    TaskState *ts = (TaskState *)cpu->opaque;
    int host_sig, core_dumped = 0;
    struct sigaction act;

    host_sig = target_to_host_signal(target_sig);
    trace_user_force_sig(env, target_sig, host_sig);
    gdb_signalled(env, target_sig);

    /* dump core if supported by target binary format */
    if (core_dump_signal(target_sig) && (ts->bprm->core_dump != NULL)) {
        stop_all_tasks();
        core_dumped =
            ((*ts->bprm->core_dump)(target_sig, env) == 0);
    }
    if (core_dumped) {
        /* we already dumped the core of target process, we don't want
         * a coredump of qemu itself */
        struct rlimit nodump;
        getrlimit(RLIMIT_CORE, &nodump);
        nodump.rlim_cur=0;
        setrlimit(RLIMIT_CORE, &nodump);
#ifdef CONFIG_LATX_DEBUG
        (void) fprintf(stderr, "latx: uncaught target signal %d (%s) - %s\n",
            target_sig, strsignal(host_sig), "core dumped" );
        latx_guest_backtrace(env);
#if defined(CONFIG_LATX_KZT) && defined(CONFIG_LATX_DEBUG)
        latx_kzt_debuginfo_check();
#endif
#endif
    }

    /* The proper exit code for dying from an uncaught signal is
     * -<signal>.  The kernel doesn't allow exit() or _exit() to pass
     * a negative value.  To get the proper exit code we need to
     * actually die from an uncaught signal.  Here the default signal
     * handler is installed, we send ourself a signal and we wait for
     * it to arrive. */
    sigfillset(&act.sa_mask);
    act.sa_handler = SIG_DFL;
    act.sa_flags = 0;
    sigaction(host_sig, &act, NULL);

    /* For some reason raise(host_sig) doesn't send the signal when
     * statically linked on x86-64. */
    kill(getpid(), host_sig);

    /* Make sure the signal isn't masked (just reuse the mask inside
    of act) */
    sigdelset(&act.sa_mask, host_sig);
    sigsuspend(&act.sa_mask);

    /* unreachable */
    abort();
}

/* queue a signal so that it will be send to the virtual CPU as soon
   as possible */
int queue_signal(CPUArchState *env, int sig, int si_type,
                 target_siginfo_t *info)
{
    CPUState *cpu = env_cpu(env);
    TaskState *ts = cpu->opaque;

    trace_user_queue_signal(env, sig);

    info->si_code = deposit32(info->si_code, 16, 16, si_type);

    ts->sync_signal.info = *info;
    ts->sync_signal.pending = sig;
    /* signal that a new signal is pending */
    qatomic_set(&ts->signal_pending, 1);
    return 1; /* indicates that the signal was queued */
}

#ifndef HAVE_SAFE_SYSCALL
static inline void rewind_if_in_safe_syscall(void *puc)
{
    /* Default version: never rewind */
}
#endif

extern long context_switch_native_to_bt_ret_0;
#if defined(CONFIG_LATX_KZT)
static char kzt_ret = 0xc3;
#include "callback.h"
#include "bridge_private.h"
#include <execinfo.h>
#endif

static void Emulate_FTZ(ucontext_t *uc)
{
    uint32_t inst, rd, fd;
    int opnd_type = 0;/*1:s  2:d*/
    float_status status_force_soft;
    memset(&status_force_soft, 0, sizeof(status_force_soft));
    inst = *(uint32_t *)UC_PC(uc);
    rd = inst & 0x1f;
#ifndef CONFIG_LOONGARCH_NEW_WORLD
    /*
     * In old world:
     * WARNING: the offset of __fregs in uc->uc_mcontext is incorrent,
     * we fix the issue by adding a extra offset, the solution is only
     * temporary!
     */
    fd = rd + 1;
    #define OPND_D_32 (UC_FREG(uc)[((inst & 0x1f) + 1)].__val32[0])
    #define OPND_J_32 (UC_FREG(uc)[(((inst >> 5) & 0x1f) + 1)].__val32[0])
    #define OPND_K_32 (UC_FREG(uc)[(((inst >> 10) & 0x1f) + 1)].__val32[0])
    #define OPND_A_32 (UC_FREG(uc)[(((inst >> 15) & 0x1f) + 1)].__val32[0])
    #define OPND_D_64 (UC_FREG(uc)[((inst & 0x1f) + 1)].__val64[0])
    #define OPND_J_64 (UC_FREG(uc)[(((inst >> 5) & 0x1f) + 1)].__val64[0])
    #define OPND_K_64 (UC_FREG(uc)[(((inst >> 10) & 0x1f) + 1)].__val64[0])
    #define OPND_A_64 (UC_FREG(uc)[(((inst >> 15) & 0x1f) + 1)].__val64[0])

    #define OPND_D_32_SET(val) (OPND_D_32 = val)
    #define OPND_J_32_SET(val) (OPND_J_32 = val)
    #define OPND_K_32_SET(val) (OPND_K_32 = val)
    #define OPND_A_32_SET(val) (OPND_A_32 = val)
    #define OPND_D_64_SET(val) (OPND_D_64 = val)
    #define OPND_J_64_SET(val) (OPND_J_64 = val)
    #define OPND_K_64_SET(val) (OPND_K_64 = val)
    #define OPND_A_64_SET(val) (OPND_A_64 = val)
#else
    fd = rd;
    struct extctx_layout extctx;
    memset(&extctx, 0, sizeof(extctx));
    /* we need to parse the extcontext data */
    parse_extcontext(uc, &extctx);
    #define OPND_D_32 (UC_GET_FPR(&extctx, (inst & 0x1f), uint32_t))
    #define OPND_J_32 (UC_GET_FPR(&extctx, ((inst >> 5) & 0x1f), uint32_t))
    #define OPND_K_32 (UC_GET_FPR(&extctx, ((inst >> 10) & 0x1f), uint32_t))
    #define OPND_A_32 (UC_GET_FPR(&extctx, ((inst >> 15) & 0x1f), uint32_t))
    #define OPND_D_64 (UC_GET_FPR(&extctx, (inst & 0x1f), uint64_t))
    #define OPND_J_64 (UC_GET_FPR(&extctx, ((inst >> 5) & 0x1f), uint64_t))
    #define OPND_K_64 (UC_GET_FPR(&extctx, ((inst >> 10) & 0x1f), uint64_t))
    #define OPND_A_64 (UC_GET_FPR(&extctx, ((inst >> 15) & 0x1f), uint64_t))

    #define OPND_D_32_SET(val)                              \
    do {                                                    \
      uint32_t _temp = (val);                               \
      UC_SET_FPR(&extctx, (inst & 0x1f), &_temp, uint32_t); \
    } while (0)
    #define OPND_J_32_SET(val)                                     \
    do {                                                           \
      uint32_t _temp = (val);                                      \
      UC_SET_FPR(&extctx, ((inst >> 5) & 0x1f), &_temp, uint32_t); \
    } while (0)

    #define OPND_K_32_SET(val)                                      \
    do {                                                            \
      uint32_t _temp = (val);                                       \
      UC_SET_FPR(&extctx, ((inst >> 10) & 0x1f), &_temp, uint32_t); \
    } while (0)

    #define OPND_A_32_SET(val)                                      \
    do {                                                            \
      uint32_t _temp = (val);                                       \
      UC_SET_FPR(&extctx, ((inst >> 15) & 0x1f), &_temp, uint32_t); \
    } while (0)

    #define OPND_D_64_SET(val)                              \
    do {                                                    \
      uint64_t _temp = (val);                               \
      UC_SET_FPR(&extctx, (inst & 0x1f), &_temp, uint64_t); \
    } while (0)

    #define OPND_J_64_SET(val)                                     \
    do {                                                           \
      uint64_t _temp = (val);                                      \
      UC_SET_FPR(&extctx, ((inst >> 5) & 0x1f), &_temp, uint64_t); \
    } while (0)

    #define OPND_K_64_SET(val)                                      \
    do {                                                            \
      uint64_t _temp = (val);                                       \
      UC_SET_FPR(&extctx, ((inst >> 10) & 0x1f), &_temp, uint64_t); \
    } while (0)

    #define OPND_A_64_SET(val)                                      \
    do {                                                            \
      uint64_t _temp = (val);                                       \
      UC_SET_FPR(&extctx, ((inst >> 15) & 0x1f), &_temp, uint64_t); \
    } while (0)
#endif

    switch (inst & ~0x7fff) {
      case 0x01008000: /*FADD_S*/
        OPND_D_32_SET(float32_add(OPND_J_32, OPND_K_32, &status_force_soft));
        opnd_type = 1;
        break;
      case 0x01028000: /*FSUB_S*/
        OPND_D_32_SET(float32_sub(OPND_J_32, OPND_K_32, &status_force_soft));
        opnd_type = 1;
        break;
      case 0x01048000: /*FMUL_S*/
        OPND_D_32_SET(float32_mul(OPND_J_32, OPND_K_32, &status_force_soft));
        opnd_type = 1;
        break;
      case 0x01068000: /*FDIV_S*/
        OPND_D_32_SET(float32_div(OPND_J_32, OPND_K_32, &status_force_soft));
        opnd_type = 1;
        break;
      case 0x01108000: /*FSCALEB_S*/
        OPND_D_32_SET(float32_to_int32(OPND_K_32, &status_force_soft));
        OPND_D_32_SET(float32_scalbn(OPND_J_32, OPND_D_32, &status_force_soft));
        opnd_type = 1;
        break;
      case 0x01010000: /*FADD_D*/
        OPND_D_64_SET(float64_add(OPND_J_64, OPND_K_64, &status_force_soft));
        opnd_type = 2;
        break;
      case 0x01030000: /*FSUB_D*/
        OPND_D_64_SET(float64_sub(OPND_J_64, OPND_K_64, &status_force_soft));
        opnd_type = 2;
        break;
      case 0x01050000: /*FMUL_D*/
        OPND_D_64_SET(float64_mul(OPND_J_64, OPND_K_64, &status_force_soft));
        opnd_type = 2;
        break;
      case 0x01070000: /*FDIV_D*/
        OPND_D_64_SET(float64_div(OPND_J_64, OPND_K_64, &status_force_soft));
        opnd_type = 2;
        break;
      case 0x01110000: /*FSCALEB_D*/
        OPND_D_64_SET(float64_to_int64(OPND_K_64, &status_force_soft));
        OPND_D_64_SET(float64_scalbn(OPND_J_64, OPND_D_64, &status_force_soft));
        opnd_type = 2;
        break;
      default:
        break;
    }
    switch (inst & ~0xfffff) {
      case 0x08100000: /*FMADD_S*/
        OPND_D_32_SET(float32_muladd(OPND_J_32, OPND_K_32, OPND_A_32, 0,
                                     &status_force_soft));
        opnd_type = 1;
        break;
      case 0x08500000: /*FMSUB_S*/
        OPND_D_32_SET(float32_muladd(OPND_J_32, OPND_K_32, OPND_A_32,
                                     float_muladd_negate_c,
                                     &status_force_soft));
        opnd_type = 1;
        break;
      case 0x08900000: /*FNMADD_S*/
        OPND_D_32_SET(float32_muladd(OPND_J_32, OPND_K_32, OPND_A_32,
                                     float_muladd_negate_result,
                                     &status_force_soft));
        opnd_type = 1;
        break;
      case 0x08d00000: /*FNMSUB_S*/
        OPND_D_32_SET(
            float32_muladd(OPND_J_32, OPND_K_32, OPND_A_32,
                           float_muladd_negate_c | float_muladd_negate_result,
                           &status_force_soft));
        opnd_type = 1;
        break;
      case 0x08200000: /*FMADD_D*/
        OPND_D_64_SET(float64_muladd(OPND_J_64, OPND_K_64, OPND_A_64, 0,
                                     &status_force_soft));
        opnd_type = 2;
        break;
      case 0x08600000: /*FMSUB_D*/
        OPND_D_64_SET(float64_muladd(OPND_J_64, OPND_K_64, OPND_A_64,
                                     float_muladd_negate_c,
                                     &status_force_soft));
        opnd_type = 2;
        break;
      case 0x08a00000: /*FNMADD_D*/
        OPND_D_64_SET(float64_muladd(OPND_J_64, OPND_K_64, OPND_A_64,
                                     float_muladd_negate_result,
                                     &status_force_soft));
        opnd_type = 2;
        break;
      case 0x08e00000: /*FNMSUB_D*/
        OPND_D_64_SET(
            float64_muladd(OPND_J_64, OPND_K_64, OPND_A_64,
                           float_muladd_negate_c | float_muladd_negate_result,
                           &status_force_soft));
        opnd_type = 2;
        break;
      default:
        break;
    }
    switch (inst & ~0x3ff) {
      case 0x01142400: /*FLOGB_S*/
        OPND_D_32_SET(float32_log2(OPND_J_32, &status_force_soft));
        opnd_type = 1;
        break;
      case 0x01142800: /*FLOGB_D*/
        OPND_D_64_SET(float64_log2(OPND_J_64, &status_force_soft));
        opnd_type = 2;
        break;
      default:
        break;
    }

    assert(opnd_type != 0);

#ifndef CONFIG_LOONGARCH_NEW_WORLD
    if (opnd_type & 0x2) {
      UC_FREG(uc)[fd].__val64[0] &= 1UL << 63;
    } else {
      UC_FREG(uc)[fd].__val32[0] &= 1 << 31;
    }
    /* clear cause bits prevent kernel to raise another SIGFPE */
    UC_FCSR(uc) &= ~(0x1 << 25);
#else
    if (opnd_type & 0x2) {
      UC_SET_FPR(&extctx, fd, (UC_GET_FPR(&extctx, fd, uint64_t) & (1UL << 63)),
                 uint64_t);
    } else {
      UC_SET_FPR(&extctx, fd, (UC_GET_FPR(&extctx, fd, uint32_t) & (1UL << 31)),
                 uint32_t);
    }
    UC_SET_FCSR(&extctx, (UC_GET_FCSR(&extctx, uint32_t)) & (~(0x1 << 25)),
                uint32_t);
#endif
    UC_PC(uc) += 4;
}

static void host_signal_handler(int host_signum, siginfo_t *info,
                                void *puc)
{
    CPUArchState *env = thread_cpu->env_ptr;
    CPUState *cpu = env_cpu(env);
    TaskState *ts = cpu->opaque;

    int sig;
    target_siginfo_t tinfo;
    ucontext_t *uc = puc;
    struct emulated_sigtable *k;

    if (host_signum == SIGSEGV) {
        if ((*(unsigned int *)UC_PC(uc) == WRITE_ILL_INST) || (*(unsigned int *)UC_PC(uc) == READ_ILL_INST)) {
            env->puc = uc;
            uint64_t real_si_addr = env->cr[2];
            int flag = page_get_flags(real_si_addr);
            if (flag & PAGE_VALID) {
                info->si_code = SEGV_ACCERR;
            } else {
                info->si_code = SEGV_MAPERR;
            }

            int aim_flag;
            if (*(unsigned int *)UC_PC(uc) == WRITE_ILL_INST) {
                aim_flag = PAGE_WRITE | PAGE_WRITE_ORG;
            } else {
                aim_flag = PAGE_READ;
            }

            if (flag & aim_flag) {
                UC_PC(uc) += 4;
                return;
            }
            info->si_addr = (void *)real_si_addr;
        }
    }

    /* workaround: Clear FCSR.Cause */
    if (host_signum == SIGFPE) {
#ifndef CONFIG_LOONGARCH_NEW_WORLD
        UC_FCSR(uc) &= ~0x1f000000;
#endif
    }

    if (host_signum == SIGILL && jrra_handle_sigill(env, uc)) {
        return;
    }
#ifdef CONFIG_LATX
    /*
     * store ucontext_t to env for context switch.
     */
    env->puc = uc;
#endif

    /* #define LA_HOOK_PTRACE SIGSEGV */
    if (host_signum == LA_HOOK_PTRACE && info->si_code == SI_QUEUE) {
        void *trace_page = info->si_value.sival_ptr;
        if (trace_page == (void *)-1) {
            tb_flush(cpu);
#ifdef CONFIG_PTRACE_DEBUG
            fprintf(stderr, "[PTRACE_DEBUG] tracee flush all TBs\n");
#endif
        } else if (trace_page) {
            mmap_lock();
            tb_invalidate_phys_page((unsigned long)trace_page);
            mmap_unlock();
#ifdef CONFIG_PTRACE_DEBUG
            fprintf(stderr, "[PTRACE_DEBUG] tracee flush page %p\n", trace_page);
        } else {
            fprintf(stderr, "[PTRACE_DEBUG] recv sig, page error!\n");
#endif
        }
        return;
    }


    /* the CPU emulator uses some host signals to detect exceptions,
       we forward to it some signals */
    if ((host_signum == SIGSEGV || host_signum == SIGBUS)
        && info->si_code > 0) {
        if (cpu_signal_handler(host_signum, info, puc))
            return;
    }


    /*
     * Emulate FTZ by underflow exception, as the SDM says:
     * When the underflow exception is masked and the flush-to-zero mode is enabled,
     * the processor performs the following operations when it detects a floating-point
     * underflow condition:
     * 1. Returns a zero result with the sign of the true result.
     * 2. Sets the precision and underflow exception flags.
     *
     * If the underflow exception is not masked, the flush-to-zero bit is ignored.
     */
    /*
     intel manual 10.2.3.3 : If the underflow exception is not masked, the flush-to-zero bit is ignored.
     */
    if (host_signum == SIGFPE && info->si_code == FPE_FLTUND &&
        env->sse_status.flush_to_zero && !(~(env->mxcsr) & 0x800)) {
        Emulate_FTZ(uc);
        return;
    }


    /* get target signal number */
    sig = host_to_target_signal(host_signum);
    if (sig < 1 || sig > TARGET_NSIG)
        return;

    /*
     * When got siganl SIGFPE, we should exit loop and restore
     * state at the exception pc (Just like SIGSEGV does)
     */

    unsigned long address = (unsigned long)info->si_addr;
    CPUClass *cc;
    greg_t pc = UC_PC(uc);
    if (host_signum == SIGFPE && tcg_tb_lookup(pc)) {
        pc += GETPC_ADJ;
        cc = CPU_GET_CLASS(cpu);
        cc->tcg_ops->tlb_fill(cpu, address, 0, MMU_DATA_LOAD,
                              MMU_USER_IDX, false, pc, info);
        g_assert_not_reached();
    } else if (host_signum == SIGTRAP && tcg_tb_lookup(pc)) {
        /* next instruction */
        pc += GETPC_ADJ + 4;
#ifndef CONFIG_LOONGARCH_NEW_WORLD
        sigset_t *puc_sigmask= (sigset_t *)((void *)&uc->uc_mcontext+0x1540);
        sigprocmask(SIG_SETMASK, puc_sigmask, NULL);
#else
        sigprocmask(SIG_SETMASK, &((ucontext_t *)puc)->uc_sigmask, NULL);
#endif
        clear_helper_retaddr();
        cc = CPU_GET_CLASS(cpu);
        cc->tcg_ops->tlb_fill(cpu, address, 0, MMU_INST_FETCH,
                          MMU_USER_IDX, false, pc, info);
        g_assert_not_reached();
    }

    trace_user_host_signal(env, host_signum, sig);

    rewind_if_in_safe_syscall(puc);

    host_to_target_siginfo_noswap(&tinfo, info);
    k = &ts->sigtab[sig - 1];
    k->info = tinfo;
    k->pending = sig;
    ts->signal_pending = 1;

    /* Block host signals until target signal handler entered. We
     * can't block SIGSEGV or SIGBUS while we're executing guest
     * code in case the guest code provokes one in the window between
     * now and it getting out to the main loop. Signals will be
     * unblocked again in process_pending_signals().
     *
     * WARNING: we cannot use sigfillset() here because the uc_sigmask
     * field is a kernel sigset_t, which is much smaller than the
     * libc sigset_t which sigfillset() operates on. Using sigfillset()
     * would write 0xff bytes off the end of the structure and trash
     * data on the struct.
     * We can't use sizeof(uc->uc_sigmask) either, because the libc
     * headers define the struct field with the wrong (too large) type.
     */
#ifndef CONFIG_LOONGARCH_NEW_WORLD
    /* TODO:workaround of user/kernel uapi mismatch */
    sigset_t *puc_sigmask = (sigset_t *)((void *)&uc->uc_mcontext+0x1540);
    memset(puc_sigmask, 0xff, SIGSET_T_SIZE);
    sigdelset(puc_sigmask, SIGSEGV);
    sigdelset(puc_sigmask, SIGBUS);
#else
    memset(&uc->uc_sigmask, 0xff, SIGSET_T_SIZE);
    sigdelset(&uc->uc_sigmask, SIGSEGV);
    sigdelset(&uc->uc_sigmask, SIGBUS);
#endif

    /* interrupt the virtual CPU as soon as possible */
#ifdef CONFIG_LATX
    tb_exit_to_qemu(env, uc);
#endif
    cpu_exit(thread_cpu);
#if defined(CONFIG_LATX_KZT)
#define SIGCANCEL       __SIGRTMIN
    if (host_signum == SIGCANCEL + 2 && option_kzt && pc > reserved_va) {
        #define BTSIZT 64
        void    * array[BTSIZT] = {0};
        size_t  size;
        size = backtrace(array, BTSIZT);
        if (size <= BTSIZT && size > 1 && array[size -1]) {
            TranslationBlock *kzt_tb = tcg_tb_lookup((uintptr_t)array[size -1]);
            if (kzt_tb) {
                onebridge_t *bridge = (onebridge_t *)kzt_tb->pc;
                if (bridge->CC == 0xCC && bridge->S == 'S' && bridge->C == 'C') {
                    RunFunctionWithState((uintptr_t)&kzt_ret, 0);
                }
            }
        }

    }
#endif
}

/* do_sigaltstack() returns target values and errnos. */
/* compare linux/kernel/signal.c:do_sigaltstack() */
abi_long do_sigaltstack(abi_ulong uss_addr, abi_ulong uoss_addr, abi_ulong sp)
{
    int ret;
    struct target_sigaltstack oss;
    TaskState *ts = (TaskState *)thread_cpu->opaque;

    /* XXX: test errors */
    if(uoss_addr)
    {
        __put_user(ts->sigaltstack_used.ss_sp, &oss.ss_sp);
        __put_user(ts->sigaltstack_used.ss_size, &oss.ss_size);
        __put_user(sas_ss_flags(sp), &oss.ss_flags);
    }

    if(uss_addr)
    {
        struct target_sigaltstack *uss;
        struct target_sigaltstack ss;
        size_t minstacksize = TARGET_MINSIGSTKSZ;

#if defined(TARGET_PPC64)
        /* ELF V2 for PPC64 has a 4K minimum stack size for signal handlers */
        struct image_info *image = ((TaskState *)thread_cpu->opaque)->info;
        if (get_ppc64_abi(image) > 1) {
            minstacksize = 4096;
        }
#endif

        ret = -TARGET_EFAULT;
        if (!lock_user_struct(VERIFY_READ, uss, uss_addr, 1)) {
            goto out;
        }
        __get_user(ss.ss_sp, &uss->ss_sp);
        __get_user(ss.ss_size, &uss->ss_size);
        __get_user(ss.ss_flags, &uss->ss_flags);
        unlock_user_struct(uss, uss_addr, 0);

        ret = -TARGET_EPERM;
        if (on_sig_stack(sp))
            goto out;

        ret = -TARGET_EINVAL;
        if (ss.ss_flags != TARGET_SS_DISABLE
            && ss.ss_flags != TARGET_SS_ONSTACK
            && ss.ss_flags != 0)
            goto out;

        if (ss.ss_flags == TARGET_SS_DISABLE) {
            ss.ss_size = 0;
            ss.ss_sp = 0;
        } else {
            ret = -TARGET_ENOMEM;
            if (ss.ss_size < minstacksize) {
                goto out;
            }
        }

        ts->sigaltstack_used.ss_sp = ss.ss_sp;
        ts->sigaltstack_used.ss_size = ss.ss_size;
    }

    if (uoss_addr) {
        ret = -TARGET_EFAULT;
        if (copy_to_user(uoss_addr, &oss, sizeof(oss)))
            goto out;
    }

    ret = 0;
out:
    return ret;
}

/* do_sigaction() return target values and host errnos */
int do_sigaction(int sig, const struct target_sigaction *act,
                 struct target_sigaction *oact)
{
    struct target_sigaction *k;
    struct sigaction act1;
    int host_sig;
    int ret = 0;

    trace_signal_do_sigaction_guest(sig, TARGET_NSIG);

    if (sig < 1 || sig > TARGET_NSIG) {
        return -TARGET_EINVAL;
    }

    if (act && (sig == TARGET_SIGKILL || sig == TARGET_SIGSTOP)) {
        return -TARGET_EINVAL;
    }

    if (block_signals()) {
        return -TARGET_ERESTARTSYS;
    }

    sigact_lock();
    k = &sigact_table[sig - 1];
    if (oact) {
        __put_user(k->_sa_handler, &oact->_sa_handler);
        __put_user(k->sa_flags, &oact->sa_flags);
#ifdef TARGET_ARCH_HAS_SA_RESTORER
        __put_user(k->sa_restorer, &oact->sa_restorer);
#endif
        /* Not swapped.  */
        oact->sa_mask = k->sa_mask;
    }
    if (act) {
        /* FIXME: This is not threadsafe.  */
        __get_user(k->_sa_handler, &act->_sa_handler);
        __get_user(k->sa_flags, &act->sa_flags);
#ifdef TARGET_ARCH_HAS_SA_RESTORER
        __get_user(k->sa_restorer, &act->sa_restorer);
#endif
        /* To be swapped in target_to_host_sigset.  */
        k->sa_mask = act->sa_mask;

        /* we update the host linux signal state */
        host_sig = target_to_host_signal(sig);
        trace_signal_do_sigaction_host(host_sig, TARGET_NSIG);
        if (host_sig > SIGRTMAX) {
            /* we don't have enough host signals to map all target signals */
            qemu_log_mask(LOG_UNIMP, "Unsupported target signal #%d, ignored\n",
                          sig);
            /*
             * we don't return an error here because some programs try to
             * register an handler for all possible rt signals even if they
             * don't need it.
             * An error here can abort them whereas there can be no problem
             * to not have the signal available later.
             * This is the case for golang,
             *   See https://github.com/golang/go/issues/33746
             * So we silently ignore the error.
             */
            sigact_unlock();
            return 0;
        }
        if (host_sig != SIGSEGV && host_sig != SIGBUS) {
            sigfillset(&act1.sa_mask);
            act1.sa_flags = SA_SIGINFO;
            if (k->sa_flags & TARGET_SA_NOCLDSTOP)
                act1.sa_flags |= SA_NOCLDSTOP;
            if (k->sa_flags & TARGET_SA_RESTART)
                act1.sa_flags |= SA_RESTART;
            if (k->sa_flags & TARGET_SA_NOCLDWAIT)
                act1.sa_flags |= SA_NOCLDWAIT;

            /* NOTE: it is important to update the host kernel signal
               ignore state to avoid getting unexpected interrupted
               syscalls */
            if (k->_sa_handler == TARGET_SIG_IGN) {
                act1.sa_sigaction = (void *)SIG_IGN;
            } else if (k->_sa_handler == TARGET_SIG_DFL) {
                if (fatal_signal (sig))
                    act1.sa_sigaction = host_signal_handler;
                else
                    act1.sa_sigaction = (void *)SIG_DFL;
            } else {
                act1.sa_sigaction = host_signal_handler;
            }
            ret = sigaction(host_sig, &act1, NULL);
        }
    }
    sigact_unlock();
    return ret;
}

static void handle_pending_signal(CPUArchState *cpu_env, int sig,
                                  struct emulated_sigtable *k)
{
    CPUState *cpu = env_cpu(cpu_env);
    abi_ulong handler;
    sigset_t set;
    target_sigset_t target_old_set;
    struct target_sigaction *sa;
    TaskState *ts = cpu->opaque;

    trace_user_handle_signal(cpu_env, sig);
    /* dequeue signal */
    k->pending = 0;

    sig = gdb_handlesig(cpu, sig);
    if (!sig) {
        sa = NULL;
        handler = TARGET_SIG_IGN;
    } else {
        sa = &sigact_table[sig - 1];
        handler = sa->_sa_handler;
    }

    if (unlikely(qemu_loglevel_mask(LOG_STRACE))) {
        print_taken_signal(sig, &k->info);
    }

    if (handler == TARGET_SIG_DFL) {
        /* default handler : ignore some signal. The other are job control or fatal */
        if (sig == TARGET_SIGTSTP || sig == TARGET_SIGTTIN || sig == TARGET_SIGTTOU) {
            kill(getpid(),SIGSTOP);
        } else if (sig != TARGET_SIGCHLD &&
                   sig != TARGET_SIGURG &&
                   sig != TARGET_SIGWINCH &&
                   sig != TARGET_SIGCONT) {
#ifdef CONFIG_LATX_AOT
        if (sig == TARGET_SIGTERM) {
            aot_exit_entry(cpu, true);
        }
#endif
            dump_core_and_abort(sig);
        }
    } else if (handler == TARGET_SIG_IGN) {
        /* ignore sig */
    } else if (handler == TARGET_SIG_ERR) {
        dump_core_and_abort(sig);
    } else {
        /* compute the blocked signals during the handler execution */
        sigset_t *blocked_set;

        target_to_host_sigset(&set, &sa->sa_mask);
        /* SA_NODEFER indicates that the current signal should not be
           blocked during the handler */
        if (!(sa->sa_flags & TARGET_SA_NODEFER))
            sigaddset(&set, target_to_host_signal(sig));

        /* save the previous blocked signal state to restore it at the
           end of the signal execution (see do_sigreturn) */
        host_to_target_sigset_internal(&target_old_set, &ts->signal_mask);

        /* block signals in the handler */
        blocked_set = ts->in_sigsuspend ?
            &ts->sigsuspend_mask : &ts->signal_mask;
        sigorset(&ts->signal_mask, blocked_set, &set);
        ts->in_sigsuspend = 0;

        /* if the CPU is in VM86 mode, we restore the 32 bit values */
#if !defined(TARGET_X86_64)
        {
            CPUX86State *env = cpu_env;
            if (env->eflags & VM_MASK)
                save_v86_state(env);
        }
#else
        if (!CODEIS64) {
            CPUX86State *env = cpu_env;
            if (env->eflags & VM_MASK)
                save_v86_state(env);
        }
#endif
        /* prepare the stack frame of the virtual CPU */
#if defined(TARGET_ARCH_HAS_SETUP_FRAME)
        if (sa->sa_flags & TARGET_SA_SIGINFO) {
            setup_rt_frame(sig, sa, &k->info, &target_old_set, cpu_env);
        } else {
            setup_frame(sig, sa, &target_old_set, cpu_env);
        }
#else
        /* These targets do not have traditional signals.  */
        setup_rt_frame(sig, sa, &k->info, &target_old_set, cpu_env);
#endif
        if (sa->sa_flags & TARGET_SA_RESETHAND) {
            sa->_sa_handler = TARGET_SIG_DFL;
        }
    }
}

void process_pending_signals(CPUArchState *cpu_env)
{
    CPUState *cpu = env_cpu(cpu_env);
    int sig;
    TaskState *ts = cpu->opaque;
    sigset_t set;
    sigset_t *blocked_set;

    sigact_lock();
    while (qatomic_read(&ts->signal_pending)) {
        /* FIXME: This is not threadsafe.  */
        sigfillset(&set);
        sigprocmask(SIG_SETMASK, &set, 0);

    restart_scan:
        sig = ts->sync_signal.pending;
        if (sig) {
            /* Synchronous signals are forced,
             * see force_sig_info() and callers in Linux
             * Note that not all of our queue_signal() calls in QEMU correspond
             * to force_sig_info() calls in Linux (some are send_sig_info()).
             * However it seems like a kernel bug to me to allow the process
             * to block a synchronous signal since it could then just end up
             * looping round and round indefinitely.
             */
            if (sigismember(&ts->signal_mask, target_to_host_signal_table[sig])
                || sigact_table[sig - 1]._sa_handler == TARGET_SIG_IGN) {
                sigdelset(&ts->signal_mask, target_to_host_signal_table[sig]);
                sigact_table[sig - 1]._sa_handler = TARGET_SIG_DFL;
            }

            handle_pending_signal(cpu_env, sig, &ts->sync_signal);
        }

        for (sig = 1; sig <= TARGET_NSIG; sig++) {
            blocked_set = ts->in_sigsuspend ?
                &ts->sigsuspend_mask : &ts->signal_mask;

            if (ts->sigtab[sig - 1].pending &&
                (!sigismember(blocked_set,
                              target_to_host_signal_table[sig]))) {
                handle_pending_signal(cpu_env, sig, &ts->sigtab[sig - 1]);
                /* Restart scan from the beginning, as handle_pending_signal
                 * might have resulted in a new synchronous signal (eg SIGSEGV).
                 */
                goto restart_scan;
            }
        }

        /* if no signal is pending, unblock signals and recheck (the act
         * of unblocking might cause us to take another host signal which
         * will set signal_pending again).
         */
        qatomic_set(&ts->signal_pending, 0);
        ts->in_sigsuspend = 0;
        set = ts->signal_mask;
        sigdelset(&set, SIGSEGV);
        sigdelset(&set, SIGBUS);
        sigprocmask(SIG_SETMASK, &set, 0);
    }
    ts->in_sigsuspend = 0;

    sigact_unlock();
}
