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
    const char* libxtstName = "libXtst.so";
#else
    const char* libxtstName = "libXtst.so.6";
#endif

#define LIBNAME libxtst

#include "generated/wrappedlibxtsttypes.h"

#include "wrappercallback.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#define SUPER() \
GO(0)   \
GO(1)   \
GO(2)   \
GO(3)   \
GO(4)

// XRecordInterceptProc ...
#define GO(A)   \
static uintptr_t my_XRecordInterceptProc_fct_##A = 0;                   \
static void my_XRecordInterceptProc_##A(void* a, void* b)               \
{                                                                       \
    RunFunctionWithState(my_XRecordInterceptProc_fct_##A, 2, a, b);  \
}
SUPER()
#undef GO
static void* find_XRecordInterceptProc_Fct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_XRecordInterceptProc_fct_##A == (uintptr_t)fct) return my_XRecordInterceptProc_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_XRecordInterceptProc_fct_##A == 0) {my_XRecordInterceptProc_fct_##A = (uintptr_t)fct; return my_XRecordInterceptProc_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libxtst XRecordInterceptProc callback\n");
    return NULL;
}

EXPORT int my_XRecordEnableContextAsync(void* display, void* context, void* cb, void* closure)
{
    return my->XRecordEnableContextAsync(display, context, find_XRecordInterceptProc_Fct(cb), closure);
}

EXPORT int my_XRecordEnableContext(void* display, void* context, void* cb, void* closure)
{
    return my->XRecordEnableContext(display, context, find_XRecordInterceptProc_Fct(cb), closure);
}

#ifdef ANDROID
    #define CUSTOM_INIT \
        getMy(lib);     \
        setNeededLibs(lib, 2, "libX11.so", "libXext.so");
#else
    #define CUSTOM_INIT \
        getMy(lib);     \
        setNeededLibs(lib, 2, "libX11.so.6", "libXext.so.6");
#endif

#define CUSTOM_FINI \
    freeMy();

#pragma GCC diagnostic pop

#include "wrappedlib_init.h"
