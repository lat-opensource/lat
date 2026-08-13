/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdarg.h>

#include "callback.h"
#include "lsenv.h"
#include "qemu.h"

#ifdef TARGET_X86_64
#define CALLBACK_GPR_ARGS 6

typedef struct CallbackFrame {
    CPUX86State *cpu;
    CPUState *cs;
    uintptr_t old_rbp;
    size_t stack_words;
} CallbackFrame;

static const int callback_gpr_regs[CALLBACK_GPR_ARGS] = {
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

static CallbackFrame callback_frame_enter(size_t stack_args)
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

    frame.stack_words = stack_args + (stack_args & 1);
    cpu->regs[R_ESP] -= frame.stack_words * sizeof(uint64_t);

    return frame;
}

static uint64_t callback_frame_run(CallbackFrame *frame, uintptr_t fnc)
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
    cpu_loop(cpu);
    qatomic_set(&cs->running, old_running);

    memcpy(&cs->jmp_env, &buf, sizeof(buf));
    cpu->eip = oldip;

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

    return cpu->regs[R_EAX];
}
#endif

uint64_t RunFunctionWithState(uintptr_t fnc, int nargs, ...)
{
#ifdef TARGET_X86_64
    size_t stack_args;
    CallbackFrame frame;
    uint64_t *stack;
    va_list ap;

    lsassert(fnc);
    lsassert(CODEIS64);

    stack_args = nargs > CALLBACK_GPR_ARGS
                     ? nargs - CALLBACK_GPR_ARGS
                     : 0;
    frame = callback_frame_enter(stack_args);
    stack = (uint64_t *)frame.cpu->regs[R_ESP];

    va_start(ap, nargs);
    for (int i = 0; i < nargs; i++) {
        if (i < CALLBACK_GPR_ARGS) {
            frame.cpu->regs[callback_gpr_regs[i]] =
                va_arg(ap, uint64_t);
        } else {
            *stack++ = va_arg(ap, uint64_t);
        }
    }
    va_end(ap);

    return callback_frame_run(&frame, fnc);
#else
    return 0;
#endif
}
