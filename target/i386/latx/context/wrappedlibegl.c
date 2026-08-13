/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdint.h>
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

static char* make_proc_name(const char* prefix, const char* name, const char* suffix)
{
    const size_t prefix_len = strlen(prefix);
    const size_t name_len = strlen(name);
    const size_t suffix_len = strlen(suffix);
    if(name_len > SIZE_MAX - prefix_len - suffix_len - 1)
        return NULL;
    char* result = (char*)malloc(prefix_len + name_len + suffix_len + 1);
    if(!result)
        return NULL;
    memcpy(result, prefix, prefix_len);
    memcpy(result + prefix_len, name, name_len);
    memcpy(result + prefix_len + name_len, suffix, suffix_len + 1);
    return result;
}

static khint_t find_egl_wrapper(kh_symbolmap_t* wrappers, const char* rname)
{
    khint_t k = kh_get(symbolmap, wrappers, rname);
    static const char* const suffixes[] = {"ARB", "EXT"};
    for(size_t i = 0; k == kh_end(wrappers) && i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        if(strstr(rname, suffixes[i]) != NULL)
            continue;
        char* alternate = make_proc_name("", rname, suffixes[i]);
        if(!alternate)
            return kh_end(wrappers);
        k = kh_get(symbolmap, wrappers, alternate);
        free(alternate);
    }
    return k;
}

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
    void* native_symbol = my->eglGetProcAddress((void*)rname);
    void* symbol = native_symbol;
    if(is_my) {
        // try again, by using custom "my_" now...
        if(!native_symbol)
            symbol = NULL;
        else {
            char* alternate = make_proc_name("my_", rname, "");
            symbol = alternate ? dlsym(my_context->box64lib, alternate) : NULL;
            free(alternate);
        }
    }
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
    k = find_egl_wrapper(my_context->glwrappers, rname);
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
