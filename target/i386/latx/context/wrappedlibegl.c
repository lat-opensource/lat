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
#include "box64context.h"
#include "librarian.h"
#include "callback.h"
#include "library.h"

const char* libeglName = "libEGL.so.1";
#define LIBNAME libegl

#include "generated/wrappedlibegltypes.h"
#include "wrappercallback.h"

EXPORT void* my_eglGetProcAddress(void* name);

EXPORT void* my_eglGetProcAddress(void* name)
{
    khint_t k;
    const char* rname = (const char*)name;
    if(relocation_log) printf_log(LOG_INFO, "Calling eglGetProcAddress(\"%s\") => ", rname);
    if(!my_context->glwrappers)
        fillGLProcWrapper();
    // check if glxprocaddress is filled, and search for lib and fill it if needed
    // get proc adress using actual glXGetProcAddress
    k = kh_get(symbolmap, my_context->glmymap, rname);
    int is_my = (k==kh_end(my_context->glmymap))?0:1;
    void* symbol;
    if(is_my) {
        // try again, by using custom "my_" now...
        char tmp[200];
        strcpy(tmp, "my_");
        strcat(tmp, rname);
        symbol = dlsym(my_context->box64lib, tmp);
    } else
        symbol = my->eglGetProcAddress((void*)rname);
    if(!symbol) {
        if(relocation_log<LOG_DEBUG) printf_log(LOG_NONE, "%p\n", NULL);
        return NULL;    // easy
    }
    // check if alread bridged
    uintptr_t ret = CheckBridged(my_context->system, symbol);
    if(ret) {
        if(relocation_log<LOG_DEBUG) printf_log(LOG_NONE, "%p\n", (void*)ret);
        return (void*)ret; // already bridged
    }
    // get wrapper
    k = kh_get(symbolmap, my_context->glwrappers, rname);
    if(k==kh_end(my_context->glwrappers) && strstr(rname, "ARB")==NULL) {
        // try again, adding ARB at the end if not present
        char tmp[200];
        strcpy(tmp, rname);
        strcat(tmp, "ARB");
        k = kh_get(symbolmap, my_context->glwrappers, tmp);
    }
    if(k==kh_end(my_context->glwrappers) && strstr(rname, "EXT")==NULL) {
        // try again, adding EXT at the end if not present
        char tmp[200];
        strcpy(tmp, rname);
        strcat(tmp, "EXT");
        k = kh_get(symbolmap, my_context->glwrappers, tmp);
    }
    if(k==kh_end(my_context->glwrappers)) {
        return NULL;
    }
    const char* constname = kh_key(my_context->glwrappers, k);
    AddOffsetSymbol(my_context->maplib, symbol, rname);
    ret = AddBridge(my_context->system, kh_value(my_context->glwrappers, k), symbol, 0, constname);
    if(relocation_log<LOG_DEBUG) printf_log(LOG_NONE, "%p\n", (void*)ret);
    return (void*)ret;

}


#define CUSTOM_INIT                 \
    getMy(lib);                     \
    setNeededLibs(lib, 1, "libGL.so.1");\
    if (!box64->glxprocaddress)     \
        box64->glxprocaddress = (procaddess_t)my->eglGetProcAddress;

#define CUSTOM_FINI \
    freeMy();


#include "wrappedlib_init.h"
