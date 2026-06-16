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

#include "debug.h"
#include "wrapper.h"
#include "bridge.h"
#include "library_private.h"
#include "callback.h"
#include "librarian.h"
#include "box64context.h"

#ifdef ANDROID
    const char* libxrandrName = "libXrandr.so";
#else
    const char* libxrandrName = "libXrandr.so.2";
#endif

#define LIBNAME libxrandr

#ifdef ANDROID
    #define CUSTOM_INIT \
        setNeededLibs(lib, 3,           \
            "libX11.so",              \
            "libXext.so",             \
            "libXrender.so");
#else
    #define CUSTOM_INIT \
        setNeededLibs(lib, 3,           \
            "libX11.so.6",              \
            "libXext.so.6",             \
            "libXrender.so.1");
#endif

#include "wrappedlib_init.h"
