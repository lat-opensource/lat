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

const char *harfbuzzicuName = "libharfbuzz-icu.so.0";
#define LIBNAME harfbuzzicu

#define PRE_INIT_GUEST \
    do { \
        if (latx_text_family_preflight_or_disable( \
                &box64->box64_ld_lib, harfbuzzicuName) != 0) { \
            return -1; \
        } \
    } while (0);

#include "wrappedlib_init.h"
