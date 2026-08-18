/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: MIT
 */

#include "qemu/osdep.h"

#include "wrappedlibs.h"

#include "bridge.h"
#include "library_private.h"
#include "wrapper.h"

const char *xkbcommonx11Name = "libxkbcommon-x11.so.0";
#define LIBNAME xkbcommonx11

#define CUSTOM_INIT \
    setNeededLibs(lib, 2, "libxkbcommon.so.0", "libxcb.so.1");

#include "wrappedlib_init.h"
