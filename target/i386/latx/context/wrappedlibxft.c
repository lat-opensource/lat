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

#ifdef ANDROID
    const char* libxftName = "libXft.so";
#else
    const char* libxftName = "libXft.so.2";
#endif

#define LIBNAME libxft
#ifdef ANDROID
    #define CUSTOM_INIT \
        setNeededLibs(lib, 4,           \
            "libX11.so",              \
            "libfontconfig.so",       \
            "libXrender.so",          \
            "libfreetype.so");
#else
    #define CUSTOM_INIT \
        setNeededLibs(lib, 4,           \
            "libX11.so.6",              \
            "libfontconfig.so.1",       \
            "libXrender.so.1",          \
            "libfreetype.so.6");
#endif


#include "wrappedlib_init.h"

