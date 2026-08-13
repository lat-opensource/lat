/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#include "box64context.h"
#include "debug.h"
#include "elfloader.h"
#include "myalign.h"
#include "x86dlfun.h"

int init_x86dlfun_from(const char *primary, const char *fallback)
{
    enum { X86_DL_SYMBOL_COUNT = 8 };
    static const char *symbols[X86_DL_SYMBOL_COUNT] = {
        "dlopen", "dlsym", "dlclose", "dladdr",
        "dladdr1", "dlinfo", "dlvsym", "dlerror",
    };
    void *resolved[X86_DL_SYMBOL_COUNT] = {0};
    elfheader_t *header = loadElfFromFile(primary);
    int resolved_count = 0;

    lsassert(header);
    ResetSpecialCaseElf(header, symbols, X86_DL_SYMBOL_COUNT,
                        resolved, &resolved_count);
    if (resolved_count != X86_DL_SYMBOL_COUNT) {
        header = loadElfFromFile(fallback);
        ResetSpecialCaseElf(header, symbols, X86_DL_SYMBOL_COUNT,
                            resolved, &resolved_count);
    }
    lsassert(resolved_count == X86_DL_SYMBOL_COUNT);

    my_context->dlprivate->x86dlopen = resolved[0];
    my_context->dlprivate->x86dlsym = resolved[1];
    my_context->dlprivate->x86dlclose = resolved[2];
    my_context->dlprivate->x86dladdr = resolved[3];
    my_context->dlprivate->x86dladdr1 = resolved[4];
    my_context->dlprivate->x86dlinfo = resolved[5];
    my_context->dlprivate->x86dlvsym = resolved[6];
    my_context->dlprivate->x86dlerror = resolved[7];
    return 0;
}
