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
#include "debug.h"
#include "myalign.h"

const char* libx11xcbName = "libX11-xcb.so.1";
#define LIBNAME libx11xcb
#include "generated/wrappedlibx11xcbtypes.h"
#include "wrappercallback.h"

EXPORT void* my_XGetXCBConnection(void* a);
EXPORT void* my_XGetXCBConnection(void* a)
{
    return add_xcb_connection(my->XGetXCBConnection(a));
}

#define CUSTOM_INIT \
     getMy(lib);


#define CUSTOM_FINI \
    freeMy();

#include "wrappedlib_init.h"

