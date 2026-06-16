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

EXPORT void my_glXDestroyContext(void* dpy, void* v2);
EXPORT void my_glXDestroyContext(void* dpy, void* v2)
{
    my->glXDestroyContext(dpy,v2);
    latx_dpy_xcb_sync(dpy);
}
EXPORT void my_glXDestroyPbuffer(void* dpy, void* v2);
EXPORT void my_glXDestroyPbuffer(void* dpy, void* v2)
{
    my->glXDestroyPbuffer(dpy,v2);
    latx_dpy_xcb_sync(dpy);
}
EXPORT void* my_glXCreatePbuffer(void* dpy, void* v2, void* v3);
EXPORT void* my_glXCreatePbuffer(void* dpy, void* v2, void* v3)
{
    void* ret = my->glXCreatePbuffer(dpy,v2, v3);
    latx_dpy_xcb_sync(dpy);
    return ret;
}
EXPORT int32_t my_glXMakeContextCurrent(void* dpy, void* v2, void* v3, void* v4);
EXPORT int32_t my_glXMakeContextCurrent(void* dpy, void* v2, void* v3, void* v4)
{
    int32_t ret = my->glXMakeContextCurrent(dpy,v2, v3, v4);
    latx_dpy_xcb_sync(dpy);
    return ret;
}
EXPORT void* my_glXCreateNewContext(void*, void*, int32_t, void*, int32_t);
EXPORT void* my_glXCreateNewContext(void* v1, void* v2, int32_t v3, void* v4, int32_t v5)
{
    void* ret = my->glXCreateNewContext(v1,v2, v3, v4, v5);
    latx_dpy_xcb_sync(v1);
    return ret;
}

#define CUSTOM_INIT \
     getMy(lib);


#define CUSTOM_FINI \
    freeMy();

#include "wrappedlib_init.h"

#if 0
#define SUPER() \
GO(0)   \
GO(1)   \
GO(2)   \
GO(3)   \


#undef SUPER

#define CUSTOM_INIT     \
    getMy(lib);         \
    SETALT(myx_);       \

#define CUSTOM_FINI     \
    freeMy();


typedef void* (*glprocaddress_t)(const char* name);

typedef struct gl_wrappers_s {
    glprocaddress_t      procaddress;
    kh_symbolmap_t      *glwrappers;    // the map of wrapper for glProcs (for GLX or SDL1/2)
    kh_symbolmap_t      *glmymap;       // link to the mysymbolmap of libGL
} gl_wrappers_t;

KHASH_MAP_INIT_INT64(gl_wrappers, gl_wrappers_t*)

static kh_gl_wrappers_t *gl_wrappers = NULL;


gl_wrappers_t* getGLProcWrapper(glprocaddress_t procaddress)
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
void freeGLProcWrapper()
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
    );
    kh_destroy(gl_wrappers, gl_wrappers);
    gl_wrappers = NULL;
}

void* getGLProcAddress(glprocaddress_t procaddr, const char* rname)
{
    khint_t k;
    printf_dlsym(LOG_DEBUG, "Calling getGLProcAddress[%p](\"%s\") => ", procaddr, rname);
    gl_wrappers_t* wrappers = getGLProcWrapper(procaddr);
    // check if glxprocaddress is filled, and search for lib and fill it if needed
    // get proc adress using actual glXGetProcAddress
    k = kh_get(symbolmap, wrappers->glmymap, rname);
    int is_my = (k==kh_end(wrappers->glmymap))?0:1;
    void* symbol;
    if(is_my) {
        // try again, by using custom "my_" now...
        #define GO(A, B) else if(!strcmp(rname, #B)) symbol = find_##B##_Fct(procaddr(rname));
        if(0) {}
        //SUPER()
        //else {
            if(strcmp(rname, "glXGetProcAddress") && strcmp(rname, "glXGetProcAddressARB")) {
                printf_log(LOG_NONE, "Warning, %s defined as GOM, but find_%s_Fct not defined\n", rname, rname);
            }
            char tmp[200];
            strcpy(tmp, "my_");
            strcat(tmp, rname);
            symbol = dlsym(my_context->box64lib, tmp);
        //}
        #undef GO
        #undef SUPER
    } else
        symbol = procaddr(rname);
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
        char tmp[200];
        strcpy(tmp, rname);
        strcat(tmp, "ARB");
        k = kh_get(symbolmap, wrappers->glwrappers, tmp);
    }
    if(k==kh_end(wrappers->glwrappers) && strstr(rname, "EXT")==NULL) {
        // try again, adding EXT at the end if not present
        char tmp[200];
        strcpy(tmp, rname);
        strcat(tmp, "EXT");
        k = kh_get(symbolmap, wrappers->glwrappers, tmp);
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

EXPORT void* myx_glXGetProcAddress(void* name)
{
    khint_t k;
    const char* rname = (const char*)name;
    return getGLProcAddress((glprocaddress_t)my->glXGetProcAddress, rname);
}

EXPORT void* myx_glXGetProcAddressARB(void* name)
{
    khint_t k;
    const char* rname = (const char*)name;
    return getGLProcAddress((glprocaddress_t)my->glXGetProcAddressARB, rname);
}


#endif
