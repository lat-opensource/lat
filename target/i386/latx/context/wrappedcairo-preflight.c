/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"

#include <string.h>

#include "wrappedcairo-preflight.h"
#include "wrappedlib-preflight.h"

#define GO(name, signature) #name,
#define GOM(name, signature) #name,
#define GOW(name, signature) #name,
#define GOWM(name, signature) #name,
#define GO2(name, signature, alias) #name,
#define GOS(name, signature) #name,
#define DATA(name, size)
#define DATAV(name, size)
#define DATAB(name, size)
#define DATAM(name, size)
static const char *const cairo_supported_symbols[] = {
#include "wrappedcairo_private.h"
};
#undef GO
#undef GOM
#undef GOW
#undef GOWM
#undef GO2
#undef GOS
#undef DATA
#undef DATAV
#undef DATAB
#undef DATAM

static bool cairo_symbol_filter(const char *name)
{
    return !strncmp(name, "cairo_", 6);
}

bool latx_cairo_preflight_guest(const char *guest_path,
                                const char *host_soname,
                                char *reason, size_t reason_size)
{
    return latx_wrappedlib_preflight_guest(
        guest_path, host_soname, "Cairo", cairo_symbol_filter,
        cairo_supported_symbols, ARRAY_SIZE(cairo_supported_symbols),
        reason, reason_size);
}
