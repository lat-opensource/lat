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
    const char* libxtName = "libXt.so";
#else
    const char* libxtName = "libXt.so.6";
#endif

#define LIBNAME libxt

#include "generated/wrappedlibxttypes.h"

#include "wrappercallback.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#define SUPER() \
GO(0)   \
GO(1)   \
GO(2)   \
GO(3)   \
GO(4)   \
GO(5)   \
GO(6)   \
GO(7)

// Event
#define GO(A)   \
static uintptr_t my_Event_fct_##A = 0;   \
static void my_Event_##A(void* w, void* data, void* event)     \
{                                       \
    RunFunctionWithState(my_Event_fct_##A, 3, w, data, event);\
}
SUPER()
#undef GO
static void* findEventFct(void* fct)
{
    if(!fct) return NULL;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_Event_fct_##A == (uintptr_t)fct) return my_Event_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_Event_fct_##A == 0) {my_Event_fct_##A = (uintptr_t)fct; return my_Event_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libXt Event callback\n");
    return NULL;
}
// WorkProc
#define GO(A)   \
static uintptr_t my_WorkProc_fct_##A = 0;   \
static int my_WorkProc_##A(void* p)         \
{                                           \
    return (int)RunFunctionWithState(my_WorkProc_fct_##A, 1, p);\
}
SUPER()
#undef GO
static void* findWorkProcFct(void* fct)
{
    if(!fct) return NULL;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_WorkProc_fct_##A == (uintptr_t)fct) return my_WorkProc_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_WorkProc_fct_##A == 0) {my_WorkProc_fct_##A = (uintptr_t)fct; return my_WorkProc_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libXt WorkProc callback\n");
    return NULL;
}
// InputCallback
#define GO(A)   \
static uintptr_t my_InputCallback_fct_##A = 0;                      \
static void my_InputCallback_##A(void* p, void* s, void* id)        \
{                                                                   \
    RunFunctionWithState(my_InputCallback_fct_##A, 3, p, s, id); \
}
SUPER()
#undef GO
static void* findInputCallbackFct(void* fct)
{
    if(!fct) return NULL;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_InputCallback_fct_##A == (uintptr_t)fct) return my_InputCallback_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_InputCallback_fct_##A == 0) {my_InputCallback_fct_##A = (uintptr_t)fct; return my_InputCallback_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libXt InputCallback callback\n");
    return NULL;
}
#undef SUPER


EXPORT void my_XtAddEventHandler(void* w, uint32_t mask, int32_t maskable, void* cb, void* data)
{
    void* fct = findEventFct(cb);
    my->XtAddEventHandler(w, mask, maskable, fct, data);
}

EXPORT long my_XtAppAddWorkProc(void* context, void* proc, void* data)
{
    return my->XtAppAddWorkProc(context, findWorkProcFct(proc), data);
}

EXPORT long my_XtAppAddInput(void* context, int source, void* cond, void* proc, void* data)
{
    return my->XtAppAddInput(context, source, cond, findInputCallbackFct(proc), data);
}

#ifdef ANDROID
    #define CUSTOM_INIT \
        getMy(lib);   \
        setNeededLibs(lib, 2, "libX11.so", "libXext.so");
#else
    #define CUSTOM_INIT \
        getMy(lib);   \
        setNeededLibs(lib, 2, "libX11.so.6", "libXext.so.6");
#endif

#define CUSTOM_FINI \
    freeMy();

#pragma GCC diagnostic pop

#include "wrappedlib_init.h"
