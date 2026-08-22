/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"

#include "wrappedlibs.h"

#include "box64context.h"
#include "bridge.h"
#include "kzt-groups.h"
#include "library_private.h"
#include "wrappedglib-preflight.h"
#include "wrapper.h"

const char *glib2Name = "libglib-2.0.so.0";
#define LIBNAME glib2

#define PRE_INIT_GUEST \
    do { \
        if (latx_glib_leaf_preflight_or_disable( \
                &box64->box64_ld_lib, glib2Name) != 0) { \
            return -1; \
        } \
        kzt_groups_log_wrapper_limitation( \
            glib2Name, \
            "only stateless GLib leaf functions are native; " \
            "allocators, containers, GObject, GIO, and callbacks remain " \
            "guest"); \
    } while (0);

#include "wrappedlib_init.h"
