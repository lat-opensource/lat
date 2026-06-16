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
#include "myalign.h"

#ifdef ANDROID
	const char* libxcursorName = "libXcursor.so";
#else
	const char* libxcursorName = "libXcursor.so.1";
#endif

#define LIBNAME libxcursor
#include "generated/wrappedlibxcursortypes.h"
#include "wrappercallback.h"

EXPORT void* my_XcursorLibraryLoadCursor(void* dpy, void* v2);
EXPORT void* my_XcursorLibraryLoadCursor(void* dpy, void* v2)
{
    void* ret = my->XcursorLibraryLoadCursor(dpy,v2);
    latx_dpy_xcb_sync(dpy);
    return ret;
}

#define CUSTOM_INIT \
     getMy(lib);


#define CUSTOM_FINI \
    freeMy();

#include "wrappedlib_init.h"

