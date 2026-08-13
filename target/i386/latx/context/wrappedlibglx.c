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

#include "debug.h"
#include "wrapper.h"
#include "bridge.h"
#include "library_private.h"
#include "box64context.h"
#include "librarian.h"
#include "library.h"
#include "wrappedlibs.h"
#include "myalign.h"

const char* libglxName = "libGLX.so.0";
#define LIBNAME libglx

#include "generated/wrappedlibglxtypes.h"
#include "wrappercallback.h"

EXPORT void myx_glXDestroyContext(void* dpy, void* v2);
EXPORT void myx_glXDestroyContext(void* dpy, void* v2)
{
    my->glXDestroyContext(dpy,v2);
    latx_dpy_xcb_sync(dpy);
}
EXPORT void myx_glXDestroyPbuffer(void* dpy, unsigned long v2);
EXPORT void myx_glXDestroyPbuffer(void* dpy, unsigned long v2)
{
    my->glXDestroyPbuffer(dpy, v2);
    latx_dpy_xcb_sync(dpy);
}
EXPORT unsigned long myx_glXCreatePbuffer(void* dpy, void* v2, void* v3);
EXPORT unsigned long myx_glXCreatePbuffer(void* dpy, void* v2, void* v3)
{
    unsigned long ret = my->glXCreatePbuffer(dpy,v2, v3);
    latx_dpy_xcb_sync(dpy);
    return ret;
}
EXPORT int32_t myx_glXMakeContextCurrent(void* dpy, unsigned long v2, unsigned long v3, void* v4);
EXPORT int32_t myx_glXMakeContextCurrent(void* dpy, unsigned long v2, unsigned long v3, void* v4)
{
    int32_t ret = my->glXMakeContextCurrent(dpy, v2, v3, v4);
    latx_dpy_xcb_sync(dpy);
    return ret;
}
EXPORT void* myx_glXCreateNewContext(void*, void*, int32_t, void*, int32_t);
EXPORT void* myx_glXCreateNewContext(void* v1, void* v2, int32_t v3, void* v4, int32_t v5)
{
    void* ret = my->glXCreateNewContext(v1,v2, v3, v4, v5);
    latx_dpy_xcb_sync(v1);
    return ret;
}

EXPORT int32_t myx_glXMakeCurrent(void* dpy, unsigned long drawable, void* context);
EXPORT int32_t myx_glXMakeCurrent(void* dpy, unsigned long drawable, void* context)
{
    if(!my->glXMakeCurrent)
        return 0;
    int32_t ret = my->glXMakeCurrent(dpy, drawable, context);
    latx_dpy_xcb_sync(dpy);
    return ret;
}

// libGL.so.1 shares these synchronization wrappers under the regular my_ prefix.
EXPORT void my_glXDestroyContext(void* dpy, void* context) __attribute__((alias("myx_glXDestroyContext")));
EXPORT void my_glXDestroyPbuffer(void* dpy, unsigned long pbuffer) __attribute__((alias("myx_glXDestroyPbuffer")));
EXPORT unsigned long my_glXCreatePbuffer(void* dpy, void* config, void* attributes) __attribute__((alias("myx_glXCreatePbuffer")));
EXPORT int32_t my_glXMakeContextCurrent(void* dpy, unsigned long draw, unsigned long read, void* context) __attribute__((alias("myx_glXMakeContextCurrent")));

static void freeGLXProcWrapper(void);

#define CUSTOM_INIT \
     getMy(lib); \
     SETALT(myx_); \
     if (!box64->glxprocaddress) \
         box64->glxprocaddress = (procaddess_t)my->glXGetProcAddress;


#define CUSTOM_FINI \
    freeGLXProcWrapper(); \
    freeMy();

#include "wrappedlib_init.h"

#define SUPER() \
GO(0)   \
GO(1)   \
GO(2)   \
GO(3)   \


#undef SUPER

typedef void* (*glprocaddress_t)(const char* name);

typedef struct gl_wrappers_s {
    glprocaddress_t      procaddress;
    kh_symbolmap_t      *glwrappers;    // the map of wrapper for glProcs (for GLX or SDL1/2)
    kh_symbolmap_t      *glmymap;       // link to the mysymbolmap of libGL
} gl_wrappers_t;

KHASH_MAP_INIT_INT64(gl_wrappers, gl_wrappers_t*)

static kh_gl_wrappers_t *gl_wrappers = NULL;


static gl_wrappers_t* getGLXProcWrapper(glprocaddress_t procaddress)
{
    int cnt, ret;
    khint_t k;
    if(!gl_wrappers) {
        gl_wrappers = kh_init(gl_wrappers);
    }
    k = kh_put(gl_wrappers, gl_wrappers, (uintptr_t)procaddress, &ret);
    if(!ret)
        return kh_value(gl_wrappers, k);
    gl_wrappers_t* wrappers = kh_value(gl_wrappers, k) = (gl_wrappers_t*)calloc(1, sizeof(gl_wrappers_t));

    wrappers->procaddress = procaddress;
    wrappers->glwrappers = kh_init(symbolmap);
    // populates maps...
    cnt = sizeof(libglxsymbolmap)/sizeof(map_onesymbol_t);
    for (int i=0; i<cnt; ++i) {
        k = kh_put(symbolmap, wrappers->glwrappers, libglxsymbolmap[i].name, &ret);
        kh_value(wrappers->glwrappers, k) = libglxsymbolmap[i].w;
    }
    // and the my_ symbols map
    cnt = sizeof(MAPNAME(mysymbolmap))/sizeof(map_onesymbol_t);
    for (int i=0; i<cnt; ++i) {
        k = kh_put(symbolmap, wrappers->glwrappers, libglxmysymbolmap[i].name, &ret);
        kh_value(wrappers->glwrappers, k) = libglxmysymbolmap[i].w;
    }
    // my_* map
    wrappers->glmymap = kh_init(symbolmap);
    cnt = sizeof(MAPNAME(mysymbolmap))/sizeof(map_onesymbol_t);
    for (int i=0; i<cnt; ++i) {
        k = kh_put(symbolmap, wrappers->glmymap, libglxmysymbolmap[i].name, &ret);
        kh_value(wrappers->glmymap, k) = libglxmysymbolmap[i].w;
    }
    return wrappers;
}
static void freeGLXProcWrapper(void)
{
    if(!gl_wrappers)
        return;
    gl_wrappers_t* wrappers;
    kh_foreach_value(gl_wrappers, wrappers,
        if(wrappers->glwrappers)
            kh_destroy(symbolmap, wrappers->glwrappers);
        if(wrappers->glmymap)
            kh_destroy(symbolmap, wrappers->glmymap);
        wrappers->glwrappers = NULL;
        wrappers->glmymap = NULL;
        free(wrappers);
    );
    kh_destroy(gl_wrappers, gl_wrappers);
    gl_wrappers = NULL;
}

static void* getGLXProcAddress(glprocaddress_t procaddr, const char* rname)
{
    khint_t k;
    printf_dlsym(LOG_DEBUG, "Calling getGLProcAddress[%p](\"%s\") => ", procaddr, rname);
    gl_wrappers_t* wrappers = getGLXProcWrapper(procaddr);
    // check if glxprocaddress is filled, and search for lib and fill it if needed
    // get proc adress using actual glXGetProcAddress
    k = kh_get(symbolmap, wrappers->glmymap, rname);
    int is_my = (k==kh_end(wrappers->glmymap))?0:1;
    void* native_symbol = procaddr ? procaddr(rname) : NULL;
    void* symbol = native_symbol;
    if(is_my) {
        // try again, by using custom "my_" now...
        #define GO(A, B) else if(!strcmp(rname, #B)) symbol = find_##B##_Fct(procaddr(rname));
        if(0) {}
        //SUPER()
        //else {
            if(strcmp(rname, "glXGetProcAddress") && strcmp(rname, "glXGetProcAddressARB")) {
                printf_log(LOG_NONE, "Warning, %s defined as GOM, but find_%s_Fct not defined\n", rname, rname);
            }
            if(!native_symbol)
                return NULL;
            size_t length = strlen(rname);
            if(length > SIZE_MAX - 5)
                return NULL;
            char* tmp = (char*)malloc(length + 5);
            if(!tmp)
                return NULL;
            memcpy(tmp, "myx_", 4);
            memcpy(tmp + 4, rname, length + 1);
            symbol = dlsym(my_context->box64lib, tmp);
            free(tmp);
        //}
        #undef GO
        #undef SUPER
    }
    if(!symbol) {
        printf_dlsym(LOG_DEBUG, "%p\n", NULL);
        return NULL;    // easy
    }
    // check if alread bridged
    uintptr_t ret = CheckBridged(my_context->system, symbol);
    if(ret) {
        printf_dlsym(LOG_DEBUG, "%p\n", (void*)ret);
        return (void*)ret; // already bridged
    }
    // get wrapper
    k = kh_get(symbolmap, wrappers->glwrappers, rname);
    if(k==kh_end(wrappers->glwrappers) && strstr(rname, "ARB")==NULL) {
        // try again, adding ARB at the end if not present
        size_t length = strlen(rname);
        if(length > SIZE_MAX - 4)
            return NULL;
        char* tmp = (char*)malloc(length + 4);
        if(tmp) {
            memcpy(tmp, rname, length);
            memcpy(tmp + length, "ARB", 4);
            k = kh_get(symbolmap, wrappers->glwrappers, tmp);
            free(tmp);
        }
    }
    if(k==kh_end(wrappers->glwrappers) && strstr(rname, "EXT")==NULL) {
        // try again, adding EXT at the end if not present
        size_t length = strlen(rname);
        if(length > SIZE_MAX - 4)
            return NULL;
        char* tmp = (char*)malloc(length + 4);
        if(tmp) {
            memcpy(tmp, rname, length);
            memcpy(tmp + length, "EXT", 4);
            k = kh_get(symbolmap, wrappers->glwrappers, tmp);
            free(tmp);
        }
    }
    if(k==kh_end(wrappers->glwrappers)) {
        printf_dlsym(LOG_DEBUG, "%p\n", NULL);
        printf_dlsym(LOG_INFO, "Warning, no wrapper for %s\n", rname);
        return NULL;
    }
    const char* constname = kh_key(wrappers->glwrappers, k);
    AddOffsetSymbol(my_context->maplib, symbol, rname);
    ret = AddBridge(my_context->system, kh_value(wrappers->glwrappers, k), symbol, 0, constname);
    printf_dlsym(LOG_DEBUG, "%p\n", (void*)ret);
    return (void*)ret;
}

EXPORT void* myx_glXGetProcAddress(void* name);
EXPORT void* myx_glXGetProcAddress(void* name)
{
    const char* rname = (const char*)name;
    return getGLXProcAddress((glprocaddress_t)my->glXGetProcAddress, rname);
}

EXPORT void* myx_glXGetProcAddressARB(void* name);
EXPORT void* myx_glXGetProcAddressARB(void* name)
{
    const char* rname = (const char*)name;
    return getGLXProcAddress((glprocaddress_t)my->glXGetProcAddressARB, rname);
}
