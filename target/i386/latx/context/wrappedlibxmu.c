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

const char* libxmuName = "libXmu.so.6";
#define LIBNAME libxmu

#define CUSTOM_INIT \
    setNeededLibs(lib, 2, "libX11.so.6", "libXext.so.6");

#include "wrappedlib_init.h"

