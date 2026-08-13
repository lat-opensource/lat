/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LATX_CALLBACK_ARGS_H
#define LATX_CALLBACK_ARGS_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LATX_CALLBACK_GPR_ARGS 6
#define LATX_CALLBACK_XMM_ARGS 8

typedef struct LatxCallbackArgs {
    uint64_t gpr[LATX_CALLBACK_GPR_ARGS];
    uint64_t xmm[LATX_CALLBACK_XMM_ARGS];
    uint64_t *stack;
    size_t gpr_count;
    size_t xmm_count;
    size_t stack_count;
} LatxCallbackArgs;

bool latx_callback_stack_args(const char *fmt, size_t *stack_count);
size_t latx_callback_stack_words(uintptr_t rsp, size_t stack_args);
void latx_callback_collect_args(LatxCallbackArgs *args, const char *fmt,
                                va_list *ap);

#endif /* LATX_CALLBACK_ARGS_H */
