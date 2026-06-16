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

const char* libxcbName = "libxcb.so.1";
#define LIBNAME libxcb

#include "generated/wrappedlibxcbtypes.h"
#include "wrappercallback.h"
#include "myalign.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

extern void* x86pthread_setcanceltype;
EXPORT void* my_xcb_wait_for_event(void* v1);
EXPORT void* my_xcb_wait_for_event(void* v1)
{
    int oldtype;
    void* ret;
    uint64_t callbackret = RunFunctionWithState((uintptr_t)x86pthread_setcanceltype , 2, PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);
    ret = my->xcb_wait_for_event(v1);
    callbackret = RunFunctionWithState((uintptr_t)x86pthread_setcanceltype , 2, oldtype, NULL);
    (void)callbackret;
    return ret;
}
EXPORT int32_t my_xcb_flush(void* v1);
EXPORT int32_t my_xcb_flush(void* v1)
{
    int32_t ret = my->xcb_flush(v1);
    sync_xcb_connection(v1);
    return ret;
}

EXPORT void* my_xcb_wait_for_reply(void* v1, uint32_t v2, void* v3);
EXPORT void* my_xcb_wait_for_reply(void* v1, uint32_t v2, void* v3)
{
    int oldtype;
    void* ret;
    uint64_t callbackret = RunFunctionWithState((uintptr_t)x86pthread_setcanceltype , 2, PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);
    ret = my->xcb_wait_for_reply(v1, v2, v3);
    callbackret = RunFunctionWithState((uintptr_t)x86pthread_setcanceltype , 2, oldtype, NULL);
    (void)callbackret;
    return ret;
}

EXPORT void* my_xcb_wait_for_reply64(void* v1, uint64_t v2, void* v3);
EXPORT void* my_xcb_wait_for_reply64(void* v1, uint64_t v2, void* v3)
{
    int oldtype;
    void* ret;
    uint64_t callbackret = RunFunctionWithState((uintptr_t)x86pthread_setcanceltype , 2, PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);
    ret = my->xcb_wait_for_reply64(v1, v2, v3);
    callbackret = RunFunctionWithState((uintptr_t)x86pthread_setcanceltype , 2, oldtype, NULL);
    (void)callbackret;
    return ret;
}

EXPORT void* my_xcb_wait_for_special_event(void* v1, void* v2);
EXPORT void* my_xcb_wait_for_special_event(void* v1, void* v2)
{
    int oldtype;
    void* ret;
    uint64_t callbackret = RunFunctionWithState((uintptr_t)x86pthread_setcanceltype , 2, PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);
    ret = my->xcb_wait_for_special_event(v1, v2);
    callbackret = RunFunctionWithState((uintptr_t)x86pthread_setcanceltype , 2, oldtype, NULL);
    (void)callbackret;
    return ret;
}

EXPORT void* my_xcb_connect(void* dispname, void* screen)
{
	return add_xcb_connection(my->xcb_connect(dispname, screen));
}

EXPORT void my_xcb_disconnect(void* conn)
{
	my->xcb_disconnect(align_xcb_connection(conn));
	del_xcb_connection(conn);
}
#pragma GCC diagnostic pop

#define CUSTOM_INIT \
     getMy(lib);


#define CUSTOM_FINI \
    freeMy();

#include "wrappedlib_init.h"
