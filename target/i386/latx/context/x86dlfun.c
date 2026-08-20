/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#include "box64context.h"
#include "config-host.h"
#include "debug.h"
#include "elfloader.h"
#include "latx-options.h"
#include "myalign.h"
#include "x86dlfun.h"

static void set_x86dlfun(void *const *resolved)
{
    my_context->dlprivate->x86dlopen = resolved[0];
    my_context->dlprivate->x86dlsym = resolved[1];
    my_context->dlprivate->x86dlclose = resolved[2];
    my_context->dlprivate->x86dladdr = resolved[3];
    my_context->dlprivate->x86dladdr1 = resolved[4];
    my_context->dlprivate->x86dlinfo = resolved[5];
    my_context->dlprivate->x86dlvsym = resolved[6];
    my_context->dlprivate->x86dlerror = resolved[7];
}

int init_x86dlfun_from(const char *primary, const char *fallback)
{
    enum { X86_DL_SYMBOL_COUNT = 8 };
    static const char *symbols[X86_DL_SYMBOL_COUNT] = {
        "dlopen", "dlsym", "dlclose", "dladdr",
        "dladdr1", "dlinfo", "dlvsym", "dlerror",
    };
    void *resolved[X86_DL_SYMBOL_COUNT] = {0};
    elfheader_t *header;
    int resolved_count = 0;

#if defined(CONFIG_LOONGARCH_NEW_WORLD) && defined(CONFIG_LATX_KZT)
    if (latx_kzt_runtime_enabled()) {
        for (int index = 0; index < X86_DL_SYMBOL_COUNT; ++index) {
            resolved[index] =
                (void *)kzt_resolve_guest_symbol(symbols[index]);
            if (!resolved[index]) {
                return -1;
            }
        }
        set_x86dlfun(resolved);
        return 0;
    }
#endif

    header = loadElfFromFile(primary);
    if (!header) {
        return -1;
    }
    ResetSpecialCaseElf(header, symbols, X86_DL_SYMBOL_COUNT,
                        resolved, &resolved_count);
    if (resolved_count != X86_DL_SYMBOL_COUNT) {
        header = loadElfFromFile(fallback);
        if (header) {
            ResetSpecialCaseElf(header, symbols, X86_DL_SYMBOL_COUNT,
                                resolved, &resolved_count);
        }
    }
    if (resolved_count != X86_DL_SYMBOL_COUNT) {
        return -1;
    }
    set_x86dlfun(resolved);
    return 0;
}
