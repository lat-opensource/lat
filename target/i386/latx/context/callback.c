/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#include "config-host.h"

#include <stdarg.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>

#include "callback-args.h"
#include "callback-fpr.h"
#include "callback.h"
#include "debug.h"
#include "lsenv.h"
#include "qemu.h"
#include "kzt-guest-tls.h"

#ifdef TARGET_X86_64
typedef struct CallbackFrame {
    CPUX86State *cpu;
    CPUState *cs;
    uintptr_t old_rbp;
    size_t stack_words;
} CallbackFrame;

typedef struct CallbackResult {
    uint64_t rax;
    uint64_t rdx;
    uint64_t xmm0;
    uint64_t xmm1;
    unsigned __int128 st0;
} CallbackResult;

static const int callback_gpr_regs[LATX_CALLBACK_GPR_ARGS] = {
    R_EDI, R_ESI, R_EDX, R_ECX, R_R8, R_R9,
};

static int64_t Pop64(CPUX86State *cpu)
{
    uint64_t *st = (uint64_t *)cpu->regs[R_ESP];
    cpu->regs[R_ESP] += 8;

    return *st;
}

static void Push64(CPUX86State *cpu, uint64_t v)
{
    cpu->regs[R_ESP] -= 8;
    *(uint64_t *)cpu->regs[R_ESP] = v;
}

static CallbackFrame callback_frame_enter(size_t stack_args,
                                          bool align_guest_entry)
{
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    CallbackFrame frame = {
        .cpu = cpu,
        .cs = env_cpu(cpu),
    };

    Push64(cpu, cpu->regs[R_EBP]);
    frame.old_rbp = cpu->regs[R_EBP] = cpu->regs[R_ESP];

    Push64(cpu, cpu->regs[R_EDI]);
    Push64(cpu, cpu->regs[R_ESI]);
    Push64(cpu, cpu->regs[R_EDX]);
    Push64(cpu, cpu->regs[R_ECX]);
    Push64(cpu, cpu->regs[R_R8]);
    Push64(cpu, cpu->regs[R_R9]);
    Push64(cpu, cpu->regs[R_R10]);
    Push64(cpu, cpu->regs[R_R11]);
    Push64(cpu, cpu->regs[R_EBX]);
    Push64(cpu, cpu->regs[R_R12]);
    Push64(cpu, cpu->regs[R_R13]);
    Push64(cpu, cpu->regs[R_R14]);
    Push64(cpu, cpu->regs[R_R15]);

    frame.stack_words = align_guest_entry
                            ? latx_callback_stack_words(cpu->regs[R_ESP],
                                                        stack_args)
                            : stack_args + (stack_args & 1);
    cpu->regs[R_ESP] -= frame.stack_words * sizeof(uint64_t);

    return frame;
}

static uint64_t callback_frame_run_result(CallbackFrame *frame,
                                          uintptr_t fnc,
                                          CallbackResult *result)
{
    CPUX86State *cpu = frame->cpu;
    CPUState *cs = frame->cs;
    uintptr_t oldip = cpu->eip;
    uintptr_t old_running;
    sigjmp_buf buf;

    /* cpu-exec.c recognizes this exact address as the callback sentinel. */
    Push64(cpu, (uint64_t)&RunFunctionWithState);
    cpu->eip = fnc;
    memcpy(&buf, &cs->jmp_env, sizeof(buf));

    old_running = qatomic_read(&cs->running);
    latx_kzt_callback_cpu_loop(cpu);
    qatomic_set(&cs->running, old_running);

    memcpy(&cs->jmp_env, &buf, sizeof(buf));
    cpu->eip = oldip;

    if (result) {
        result->rax = cpu->regs[R_EAX];
        result->rdx = cpu->regs[R_EDX];
        result->xmm0 = cpu->xmm_regs[0].ZMM_Q(0);
        result->xmm1 = cpu->xmm_regs[1].ZMM_Q(0);
        result->st0 = 0;
        memcpy(&result->st0,
               &cpu->fpregs[(cpu->fpstt) & 7].d.low,
               sizeof(uint64_t));
    }

    cpu->regs[R_ESP] += frame->stack_words * sizeof(uint64_t);
    cpu->regs[R_R15] = Pop64(cpu);
    cpu->regs[R_R14] = Pop64(cpu);
    cpu->regs[R_R13] = Pop64(cpu);
    cpu->regs[R_R12] = Pop64(cpu);
    cpu->regs[R_EBX] = Pop64(cpu);
    cpu->regs[R_R11] = Pop64(cpu);
    cpu->regs[R_R10] = Pop64(cpu);
    cpu->regs[R_R9] = Pop64(cpu);
    cpu->regs[R_R8] = Pop64(cpu);
    cpu->regs[R_ECX] = Pop64(cpu);
    cpu->regs[R_EDX] = Pop64(cpu);
    cpu->regs[R_ESI] = Pop64(cpu);
    cpu->regs[R_EDI] = Pop64(cpu);

    cpu->regs[R_ESP] = frame->old_rbp;
    cpu->regs[R_EBP] = Pop64(cpu);

    return result ? result->rax : cpu->regs[R_EAX];
}

static uint64_t callback_frame_run(CallbackFrame *frame, uintptr_t fnc)
{
    return callback_frame_run_result(frame, fnc, NULL);
}
#endif

typedef enum LatxGuestCallKind {
    LATX_GUEST_USER_CALLBACK,
    LATX_GUEST_INTERNAL_HELPER,
    LATX_GUEST_INTERNAL_NO_REFRESH,
} LatxGuestCallKind;

#define LATX_GUEST_TLS_REFRESH_RETRIES 1000

static bool callback_is_user(LatxGuestCallKind kind)
{
    return kind == LATX_GUEST_USER_CALLBACK;
}

static int callback_disable_cancellation(LatxGuestCallKind kind,
                                         int *old_state)
{
    if (!latx_kzt_guest_tls_enabled() ||
        !callback_is_user(kind)) {
        return 0;
    }
    return pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, old_state) == 0
               ? 1 : -1;
}

static void callback_restore_cancellation(int disabled, int old_state,
                                          int saved_errno,
                                          int saved_h_errno)
{
    if (disabled > 0) {
        (void)pthread_setcancelstate(old_state, NULL);
    }
    errno = saved_errno;
    h_errno = saved_h_errno;
}

typedef struct CallbackScope {
    CPUX86State *cpu;
    int old_cancel_state;
    int cancellation_disabled;
    bool execution_entered;
} CallbackScope;

static void callback_scope_leave(CallbackScope *scope)
{
    CallbackScope current = *scope;
    int saved_errno;
    int saved_h_errno;

    /* Restoring cancellation may not return; cleanup must be idempotent. */
    *scope = (CallbackScope) { 0 };

    if (current.execution_entered) {
        kzt_guest_tls_execution_leave(current.cpu);
    }
    if (current.cancellation_disabled > 0) {
        saved_errno = errno;
        saved_h_errno = h_errno;
        callback_restore_cancellation(current.cancellation_disabled,
                                      current.old_cancel_state,
                                      saved_errno, saved_h_errno);
    }
}

static int callback_scope_enter(CallbackScope *scope, LatxGuestCallKind kind)
{
    int entry_errno;
    int entry_h_errno;
    int result = 0;

    memset(scope, 0, sizeof(*scope));
    if (!latx_kzt_guest_tls_enabled()) {
        return 0;
    }
    entry_errno = errno;
    entry_h_errno = h_errno;
    scope->cancellation_disabled = callback_disable_cancellation(
        kind, &scope->old_cancel_state);
    if (scope->cancellation_disabled < 0) {
        return -1;
    }
    if ((!lsenv || !lsenv->cpu_state) &&
        latx_attach_current_host_thread() != 0) {
        fprintf(stderr, "KZT cannot attach native thread for Guest callback\n");
        result = -1;
        goto out;
    }
    scope->cpu = (CPUX86State *)lsenv->cpu_state;
    if (kind != LATX_GUEST_INTERNAL_NO_REFRESH) {
        for (int attempt = 0; attempt < LATX_GUEST_TLS_REFRESH_RETRIES;
             ++attempt) {
            result = kzt_guest_tls_refresh_if_needed(scope->cpu);
            if (result != KZT_GUEST_TLS_REFRESH_BUSY ||
                !callback_is_user(kind)) {
                break;
            }
            g_usleep(1000);
        }
        if (result != 0) {
            goto out;
        }
    }
    if (callback_is_user(kind)) {
        kzt_guest_tls_execution_enter(scope->cpu);
        scope->execution_entered = true;

    }
out:
    errno = entry_errno;
    h_errno = entry_h_errno;
    return result;
}

static uint64_t run_function_with_state_va(uintptr_t fnc, int nargs,
                                           LatxGuestCallKind kind,
                                           va_list *ap)
{
#ifdef TARGET_X86_64
    size_t stack_args;
    CallbackFrame frame;
    uint64_t *stack;
    CallbackScope scope __attribute__((cleanup(callback_scope_leave))) = { 0 };

    if (callback_scope_enter(&scope, kind) != 0) {
        return 0;
    }

    lsassert(fnc);
    lsassert(nargs >= 0);
    lsassert(CODEIS64);

    stack_args = nargs > LATX_CALLBACK_GPR_ARGS
                     ? nargs - LATX_CALLBACK_GPR_ARGS
                     : 0;
    frame = callback_frame_enter(stack_args, false);
    stack = (uint64_t *)frame.cpu->regs[R_ESP];

    for (int i = 0; i < nargs; i++) {
        if (i < LATX_CALLBACK_GPR_ARGS) {
            frame.cpu->regs[callback_gpr_regs[i]] =
                va_arg(*ap, uint64_t);
        } else {
            *stack++ = va_arg(*ap, uint64_t);
        }
    }

    return callback_frame_run(&frame, fnc);
#else
    (void)fnc;
    (void)nargs;
    (void)kind;
    (void)ap;
    return 0;
#endif
}

uint64_t RunFunctionWithState(uintptr_t fnc, int nargs, ...)
{
    uint64_t result;
    va_list ap;

    va_start(ap, nargs);
    result = run_function_with_state_va(
        fnc, nargs, LATX_GUEST_USER_CALLBACK, &ap);
    va_end(ap);
    return result;
}

uint64_t RunFunctionWithStateInternal(uintptr_t fnc, int nargs, ...)
{
    uint64_t result;
    va_list ap;

    va_start(ap, nargs);
    result = run_function_with_state_va(
        fnc, nargs, LATX_GUEST_INTERNAL_HELPER, &ap);
    va_end(ap);
    return result;
}

uint64_t RunFunctionWithStateInternalNoRefresh(uintptr_t fnc, int nargs,
                                               ...)
{
    uint64_t result;
    va_list ap;

    va_start(ap, nargs);
    result = run_function_with_state_va(
        fnc, nargs, LATX_GUEST_INTERNAL_NO_REFRESH, &ap);
    va_end(ap);
    return result;
}

uint64_t RunFunctionFmt(uintptr_t fnc, const char *fmt, ...)
{
#ifdef TARGET_X86_64
    size_t stack_args;
    CallbackFrame frame;
    LatxCallbackArgs args;
    va_list ap;
    CallbackScope scope __attribute__((cleanup(callback_scope_leave))) = { 0 };

    if (callback_scope_enter(&scope, LATX_GUEST_USER_CALLBACK) != 0) {
        return 0;
    }

    lsassert(fnc);
    lsassert(fmt);
    lsassert(CODEIS64);
    if (!fnc || !fmt || !CODEIS64 ||
        !latx_callback_stack_args(fmt, &stack_args)) {
        return 0;
    }

    frame = callback_frame_enter(stack_args, true);
    args = (LatxCallbackArgs) {
        .stack = (uint64_t *)frame.cpu->regs[R_ESP],
    };

    va_start(ap, fmt);
    latx_callback_collect_args(&args, fmt, &ap);
    va_end(ap);

    g_assert(args.stack_count == stack_args);
    for (size_t i = 0; i < args.gpr_count; i++) {
        frame.cpu->regs[callback_gpr_regs[i]] = args.gpr[i];
    }
    for (size_t i = 0; i < args.xmm_count; i++) {
        frame.cpu->xmm_regs[i].ZMM_Q(0) = args.xmm[i];
    }

    return callback_frame_run(&frame, fnc);
#else
    return 0;
#endif
}

float RunFunctionFmtFloat(uintptr_t fnc, const char *fmt, ...)
{
#ifdef TARGET_X86_64
    float result;
    size_t stack_args;
    CallbackFrame frame;
    LatxCallbackArgs args;
    va_list ap;
    CallbackScope scope __attribute__((cleanup(callback_scope_leave))) = { 0 };

    if (callback_scope_enter(&scope, LATX_GUEST_USER_CALLBACK) != 0) {
        return 0.0f;
    }

    lsassert(fnc);
    lsassert(fmt);
    lsassert(CODEIS64);
    if (!fnc || !fmt || !CODEIS64 ||
        !latx_callback_stack_args(fmt, &stack_args)) {
        return 0.0f;
    }

    frame = callback_frame_enter(stack_args, true);
    args = (LatxCallbackArgs) {
        .stack = (uint64_t *)frame.cpu->regs[R_ESP],
    };

    va_start(ap, fmt);
    latx_callback_collect_args(&args, fmt, &ap);
    va_end(ap);

    g_assert(args.stack_count == stack_args);
    for (size_t i = 0; i < args.gpr_count; i++) {
        frame.cpu->regs[callback_gpr_regs[i]] = args.gpr[i];
    }
    for (size_t i = 0; i < args.xmm_count; i++) {
        frame.cpu->xmm_regs[i].ZMM_Q(0) = args.xmm[i];
    }

    callback_frame_run(&frame, fnc);
    memcpy(&result, &frame.cpu->xmm_regs[0].ZMM_L(0), sizeof(result));
    return result;
#else
    return 0.0f;
#endif
}

static int run_guest_callback_impl(uintptr_t entry, const long *gpr_args,
                      int gpr_count, const long *xmm_args,
                      int xmm_count, const long *stack_args,
                      int stack_count, long *rax, long *rdx,
                      long *xmm0, long *xmm1,
                      unsigned __int128 *st0, LatxGuestCallKind kind)
{
#ifndef TARGET_X86_64
    return -1;
#else
    CallbackFrame frame;
    CallbackResult result;
    uint64_t *guest_stack;
    CallbackScope scope __attribute__((cleanup(callback_scope_leave))) = { 0 };

    if (!entry || gpr_count < 0 || gpr_count > LATX_CALLBACK_GPR_ARGS ||
        xmm_count < 0 || xmm_count > LATX_CALLBACK_XMM_ARGS ||
        stack_count < 0 || (gpr_count && !gpr_args) ||
        (xmm_count && !xmm_args) || (stack_count && !stack_args)) {
        return -1;
    }
    if (!latx_kzt_guest_tls_enabled() ||
        callback_scope_enter(&scope, kind) != 0) {
        return -1;
    }

    frame = callback_frame_enter((size_t)stack_count, false);
    guest_stack = (uint64_t *)frame.cpu->regs[R_ESP];
    for (int index = 0; index < gpr_count; ++index) {
        frame.cpu->regs[callback_gpr_regs[index]] =
            (uint64_t)gpr_args[index];
    }
    for (int index = 0; index < xmm_count; ++index) {
        frame.cpu->xmm_regs[index].ZMM_Q(0) =
            (uint64_t)xmm_args[index];
    }
    for (int index = 0; index < stack_count; ++index) {
        guest_stack[index] = (uint64_t)stack_args[index];
    }

    callback_frame_run_result(&frame, entry, &result);
    /*
     * Semantic helpers may use Guest result registers. Publish the captured
     * result only after those helpers finish, even if an output aliases env.
     */
    callback_scope_leave(&scope);
    if (rax) {
        *rax = (long)result.rax;
    }
    if (rdx) {
        *rdx = (long)result.rdx;
    }
    if (xmm0) {
        *xmm0 = (long)result.xmm0;
    }
    if (xmm1) {
        *xmm1 = (long)result.xmm1;
    }
    if (st0) {
        *st0 = result.st0;
    }
    return 0;
#endif
}

int latx_run_guest_callback(uintptr_t entry, const long *gpr_args,
                      int gpr_count, const long *xmm_args,
                      int xmm_count, const long *stack_args,
                      int stack_count, long *rax, long *rdx,
                      long *xmm0, long *xmm1,
                      unsigned __int128 *st0)
{
    return run_guest_callback_impl(entry, gpr_args, gpr_count, xmm_args,
        xmm_count, stack_args, stack_count, rax, rdx, xmm0, xmm1, st0,
        LATX_GUEST_USER_CALLBACK);
}
