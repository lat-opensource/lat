/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#include "callback-args.h"

#include <assert.h>
#include <string.h>

static bool callback_arg_is_sse(char type)
{
    return type == 'f' || type == 'd';
}

static bool callback_arg_is_gpr(char type)
{
    switch (type) {
    case 'p':
    case 'i':
    case 'u':
    case 'I':
    case 'U':
    case 'L':
    case 'l':
    case 'w':
    case 'W':
    case 'c':
    case 'C':
        return true;
    default:
        return false;
    }
}

bool latx_callback_stack_args(const char *fmt, size_t *stack_count)
{
    size_t gpr_count = 0;
    size_t xmm_count = 0;
    size_t count = 0;

    if (!fmt || !stack_count) {
        return false;
    }

    for (; *fmt; fmt++) {
        if (callback_arg_is_sse(*fmt)) {
            if (xmm_count < LATX_CALLBACK_XMM_ARGS) {
                xmm_count++;
            } else {
                count++;
            }
        } else if (callback_arg_is_gpr(*fmt)) {
            if (gpr_count < LATX_CALLBACK_GPR_ARGS) {
                gpr_count++;
            } else {
                count++;
            }
        } else {
            return false;
        }
    }

    *stack_count = count;
    return true;
}

size_t latx_callback_stack_words(uintptr_t rsp, size_t stack_args)
{
    size_t stack_words = stack_args;

    if ((rsp - stack_words * sizeof(uint64_t)) & 0xf) {
        stack_words++;
    }
    return stack_words;
}

static void callback_store_gpr(LatxCallbackArgs *args, uint64_t value)
{
    if (args->gpr_count < LATX_CALLBACK_GPR_ARGS) {
        args->gpr[args->gpr_count++] = value;
    } else {
        args->stack[args->stack_count++] = value;
    }
}

static void callback_store_xmm(LatxCallbackArgs *args, uint64_t value)
{
    if (args->xmm_count < LATX_CALLBACK_XMM_ARGS) {
        args->xmm[args->xmm_count++] = value;
    } else {
        args->stack[args->stack_count++] = value;
    }
}

static uint64_t callback_double_bits(double value)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t callback_float_bits(double promoted)
{
    float value = promoted;
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

void latx_callback_collect_args(LatxCallbackArgs *args, const char *fmt,
                                va_list *ap)
{
    for (; *fmt; fmt++) {
        switch (*fmt) {
        case 'f':
            callback_store_xmm(args,
                               callback_float_bits(va_arg(*ap, double)));
            break;
        case 'd':
            callback_store_xmm(args,
                               callback_double_bits(va_arg(*ap, double)));
            break;
        case 'p':
            callback_store_gpr(args,
                               (uintptr_t)va_arg(*ap, void *));
            break;
        case 'i':
            callback_store_gpr(args,
                               (uint64_t)(int64_t)va_arg(*ap, int));
            break;
        case 'u':
            callback_store_gpr(args, va_arg(*ap, uint32_t));
            break;
        case 'I':
            callback_store_gpr(args,
                               (uint64_t)va_arg(*ap, int64_t));
            break;
        case 'U':
            callback_store_gpr(args, va_arg(*ap, uint64_t));
            break;
        case 'L':
            callback_store_gpr(args, va_arg(*ap, unsigned long));
            break;
        case 'l':
            callback_store_gpr(args,
                               (uint64_t)(int64_t)va_arg(*ap, long));
            break;
        case 'w':
            callback_store_gpr(
                args, (uint64_t)(int64_t)(int16_t)va_arg(*ap, int));
            break;
        case 'W':
            callback_store_gpr(args,
                               (uint16_t)va_arg(*ap, int));
            break;
        case 'c':
            callback_store_gpr(
                args, (uint64_t)(int64_t)(int8_t)va_arg(*ap, int));
            break;
        case 'C':
            callback_store_gpr(args,
                               (uint8_t)va_arg(*ap, int));
            break;
        default:
            assert(false);
        }
    }
}
