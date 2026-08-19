/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"

#include "wrappedlibs.h"

#include "box64context.h"
#include "bridge.h"
#include "library_private.h"
#include "wrappedtext-preflight.h"
#include "wrapper.h"

const char *fribidiName = "libfribidi.so.0";
#define LIBNAME fribidi

EXPORT const char *my_fribidi_unicode_version;
EXPORT const char *my_fribidi_version_info;

#define PRE_INIT_GUEST \
    do { \
        if (latx_text_family_preflight_or_disable( \
                &box64->box64_ld_lib, fribidiName) != 0) { \
            return -1; \
        } \
    } while (0);

#define CUSTOM_INIT \
    do { \
        const char **unicode_version = \
            dlsym(lib->priv.w.lib, "fribidi_unicode_version"); \
        const char **version_info = \
            dlsym(lib->priv.w.lib, "fribidi_version_info"); \
        if (!unicode_version || !version_info) { \
            return -1; \
        } \
        my_fribidi_unicode_version = *unicode_version; \
        my_fribidi_version_info = *version_info; \
    } while (0);

#include "wrappedlib_init.h"
