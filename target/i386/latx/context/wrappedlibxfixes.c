/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "wrappedlibs.h"

#include "wrapper.h"
#include "bridge.h"
#include "library_private.h"

const char* libxfixesName = "libXfixes.so.3";
#define LIBNAME libxfixes

#define CUSTOM_INIT \
    setNeededLibs(lib, 4,           \
        "libX11.so.6",              \
        "libxcb.so.1",              \
        "libXau.so.6",              \
        "libXdmcp.so.6");


#include "wrappedlib_init.h"

