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
#include "callback.h"
#include "myalign.h"

const char* libglName = "libGL.so.1";
#define LIBNAME libgl
typedef unsigned long XID;
#include "generated/wrappedlibgltypes.h"

#include "wrappercallback.h"

struct my_Async {
    struct my_Async *next;
    int (*handler)(void*, void*, char*, int, void*);
    void* data;
};

typedef struct my_XDisplay_s
{
        void *ext_data;
        struct my_XFreeFuncs *free_funcs;
        int fd;
        int conn_checker;
        int proto_major_version;
        int proto_minor_version;
        char *vendor;
        XID resource_base;
        XID resource_mask;
        XID resource_id;
        int resource_shift;
        XID (*resource_alloc)(void*);
        int byte_order;
        int bitmap_unit;
        int bitmap_pad;
        int bitmap_bit_order;
        int nformats;
        void *pixmap_format;
        int vnumber;
        int release;
        void *head, *tail;
        int qlen;
        unsigned long last_request_read;
        unsigned long request;
        char *last_req;
        char *buffer;
        char *bufptr;
        char *bufmax;
        unsigned max_request_size;
        void* *db;
        int (*synchandler)(void*);
        char *display_name;
        int default_screen;
        int nscreens;
        void *screens;
        unsigned long motion_buffer;
        volatile unsigned long flags;
        int min_keycode;
        int max_keycode;
        void *keysyms;
        void *modifiermap;
        int keysyms_per_keycode;
        char *xdefaults;
        char *scratch_buffer;
        unsigned long scratch_length;
        int ext_number;
        struct my_XExten *ext_procs;
        int (*event_vec[128])(void *, void *, void *);
        int (*wire_vec[128])(void *, void *, void *);
        XID lock_meaning;
        void* lock;
        struct my_XInternalAsync *async_handlers;
        unsigned long bigreq_size;
        struct my_XLockPtrs *lock_fns;
        void (*idlist_alloc)(void *, void *, int);
        void* key_bindings;
        XID cursor_font;
        void* *atoms;
        unsigned int mode_switch;
        unsigned int num_lock;
        void* context_db;
        int (**error_vec)(void*, void*, void*);
        struct {
           void* defaultCCCs;
           void* clientCmaps;
           void* perVisualIntensityMaps;
        } cms;
        void* im_filters;
        void* qfree;
        unsigned long next_event_serial_num;
        struct my_XExten *flushes;
        struct my_XConnectionInfo *im_fd_info;
        int im_fd_length;
        struct my_XConnWatchInfo *conn_watchers;
        int watcher_count;
        void* filedes;
        int (*savedsynchandler)(void *);
        XID resource_max;
        int xcmisc_opcode;
        void* *xkb_info;
        void* *trans_conn;
        void* *xcb;
        unsigned int next_cookie;
        int (*generic_event_vec[128])(void*, void*, void*);
        int (*generic_event_copy_vec[128])(void*, void*, void*);
        void *cookiejar;
        unsigned long last_request_read_upper32bit; // 64bits only
        unsigned long request_upper32bit;   // 64bits only
        void* error_threads;
        void* exit_handler;
        void* exit_handler_data;
} my_XDisplay_t;

typedef void* (*glprocaddress_t)(const char* name);
void* getGLProcAddress(glprocaddress_t procaddr, const char* rname);
EXPORT void* my_glXGetProcAddress(void* name);
EXPORT void my_glDebugMessageCallback(void* prod, void* param);
EXPORT int my_glXSwapIntervalMESA(int interval);
EXPORT void my_glProgramCallbackMESA(void* f, void* data);
EXPORT void my_eglSetBlobCacheFuncsANDROID(void* dpy, void* set, void* get);
EXPORT int my_eglDebugMessageControlKHR(void* prod, void* param);
//EXPORT void* my_glGetVkProcAddrNV(void* name);
EXPORT void* my_glXGetProcAddress(void* name)
{
    /*
     * old code
    const char* rname = (const char*)name;
    return getGLProcAddress(my_lib->priv.w.priv, rname);
    */
    khint_t k;
    const char* rname = (const char*)name;
    printf_dlsym(LOG_DEBUG, "Calling glXGetProcAddress(\"%s\") => ", rname);
    if(!my_context->glwrappers)
        fillGLProcWrapper();
    // check if glxprocaddress is filled, and search for lib and fill it if needed
    // get proc adress using actual glXGetProcAddress
    k = kh_get(symbolmap, my_context->glmymap, rname);
    int is_my = (k==kh_end(my_context->glmymap))?0:1;
    void* symbol = NULL;
    if(is_my) {
        // try again, by using custom "my_" now...
        char tmp[200];
        strcpy(tmp, "my_");
        strcat(tmp, rname);
        symbol = dlsym(my_context->box64lib, tmp);
    } else {
        if (my_context->glxprocaddress) symbol = my_context->glxprocaddress(rname);
        if(latx_wine &&!symbol) {//hacking for wine64
            symbol = dlsym(my_context->box64lib, rname);
        }
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
        printf_dlsym(LOG_DEBUG, "%p\n", NULL);
        printf_dlsym(LOG_INFO, "Warning, no wrapper for %s\n", rname);
        return NULL;
    }
    const char* constname = kh_key(my_context->glwrappers, k);
    AddOffsetSymbol(my_context->maplib, symbol, rname);
    ret = AddBridge(my_context->system, kh_value(my_context->glwrappers, k), symbol, 0, constname);
    printf_dlsym(LOG_DEBUG, "%p\n", (void*)ret);
    return (void*)ret;
}

EXPORT void* my_glXGetProcAddressARB(void* name) __attribute__((alias("my_glXGetProcAddress")));

typedef int  (*iFi_t)(int);
typedef void (*vFpp_t)(void*, void*);
typedef void*(*pFp_t)(void*);
typedef void (*debugProc_t)(int32_t, int32_t, uint32_t, int32_t, int32_t, void*, void*);


// old code
typedef struct gl_wrappers_s {
    glprocaddress_t      procaddress;
    kh_symbolmap_t      *glwrappers;    // the map of wrapper for glProcs (for GLX or SDL1/2)
    kh_symbolmap_t      *glmymap;       // link to the mysymbolmap of libGL
} gl_wrappers_t;

KHASH_MAP_INIT_INT64(gl_wrappers, gl_wrappers_t*)

static kh_gl_wrappers_t *gl_wrappers = NULL;


#define SUPER() \
GO(0)   \
GO(1)   \
GO(2)   \
GO(3)   \
GO(4)

// debug_callback ...
#define GO(A)   \
static uintptr_t my_debug_callback_fct_##A = 0;                                                                         \
static void my_debug_callback_##A(int32_t a, int32_t b, uint32_t c, int32_t d, int32_t e, const char* f, const void* g) \
{                                                                                                                       \
    RunFunctionWithState(my_debug_callback_fct_##A, 7, a, b, c, d, e, f, g);                                         \
}
SUPER()
#undef GO

static void* find_debug_callback_Fct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_debug_callback_fct_##A == (uintptr_t)fct) return my_debug_callback_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_debug_callback_fct_##A == 0) {my_debug_callback_fct_##A = (uintptr_t)fct; return my_debug_callback_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libGL debug_callback callback\n");
    return NULL;
}

// program_callback ...
#define GO(A)                                                       \
static uintptr_t my_program_callback_fct_##A = 0;                   \
static void my_program_callback_##A(int32_t a, void* b)             \
{                                                                   \
    RunFunctionWithState(my_program_callback_fct_##A, 2, a, b);  \
}
SUPER()
#undef GO

static void* find_program_callback_Fct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_program_callback_fct_##A == (uintptr_t)fct) return my_program_callback_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_program_callback_fct_##A == 0) {my_program_callback_fct_##A = (uintptr_t)fct; return my_program_callback_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libGL program_callback callback\n");
    return NULL;
}
// egl_debug_callback ...
#define GO(A)   \
static uintptr_t my_egl_debug_callback_fct_##A = 0;                                                     \
    static void my_egl_debug_callback_##A(int32_t a, void* b, int32_t c, void* d, void* e, void* f)       \
{                                                                                                       \
    RunFunctionWithState(my_egl_debug_callback_fct_##A, 6, a, b, c, d, e, f);                          \
}
SUPER()
#undef GO

static void* find_egl_debug_callback_Fct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_egl_debug_callback_fct_##A == (uintptr_t)fct) return my_egl_debug_callback_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_egl_debug_callback_fct_##A == 0) {my_egl_debug_callback_fct_##A = (uintptr_t)fct; return my_egl_debug_callback_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libGL egl_debug_callback callback\n");
    return NULL;
}
// glXSwapIntervalMESA ...
#define GO(A)                                           \
static iFi_t my_glXSwapIntervalMESA_fct_##A = NULL;     \
static int my_glXSwapIntervalMESA_##A(int interval)     \
{                                                       \
    if(!my_glXSwapIntervalMESA_fct_##A)                 \
        return 0;                                       \
    return my_glXSwapIntervalMESA_fct_##A(interval);    \
}
SUPER()
#undef GO
static void* find_glXSwapIntervalMESA_Fct(void* fct)
{
    if(!fct) return fct;
    #define GO(A) if(my_glXSwapIntervalMESA_fct_##A == (iFi_t)fct) return my_glXSwapIntervalMESA_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_glXSwapIntervalMESA_fct_##A == 0) {my_glXSwapIntervalMESA_fct_##A = (iFi_t)fct; return my_glXSwapIntervalMESA_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libGL glXSwapIntervalMESA callback\n");
    return NULL;
}

// set_blob_func ...
#define GO(A)                                                               \
static uintptr_t my_set_blob_func_fct_##A = 0;                              \
static void my_set_blob_func_##A(const void* a, int32_t b, const void* c, int32_t d) \
{                                                                           \
    RunFunctionWithState(my_set_blob_func_fct_##A, 4, a, b, c, d);          \
}
SUPER()
#undef GO

static void* find_set_blob_func_Fct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_set_blob_func_fct_##A == (uintptr_t)fct) return my_set_blob_func_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_set_blob_func_fct_##A == 0) {my_set_blob_func_fct_##A = (uintptr_t)fct; return my_set_blob_func_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libGL set_blob_func callback\n");
    return NULL;
}

// get_blob_func ...
#define GO(A)                                                               \
static uintptr_t my_get_blob_func_fct_##A = 0;                              \
static int32_t my_get_blob_func_##A(const void* a, int32_t b, void* c, int32_t d) \
{                                                                           \
    return (int32_t)RunFunctionWithState(my_get_blob_func_fct_##A, 4, a, b, c, d); \
}
SUPER()
#undef GO

static void* find_get_blob_func_Fct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_get_blob_func_fct_##A == (uintptr_t)fct) return my_get_blob_func_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_get_blob_func_fct_##A == 0) {my_get_blob_func_fct_##A = (uintptr_t)fct; return my_get_blob_func_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libGL get_blob_func callback\n");
    return NULL;
}

#undef SUPER

EXPORT void my_glDebugMessageCallback(void* prod, void* param)
{
    static vFpp_t DebugMessageCallback = NULL;
    static int init = 1;
    if(init) {
        //fix: glxprocaddress is working ?
        DebugMessageCallback = my_context->glxprocaddress("glDebugMessageCallback");
        init = 0;
    }
    if(!DebugMessageCallback)
        return;
    DebugMessageCallback(find_debug_callback_Fct(prod), param);
}

EXPORT void my_glDebugMessageCallbackARB(void* prod, void* param) __attribute__((alias("my_glDebugMessageCallback")));
EXPORT void my_glDebugMessageCallbackAMD(void* prod, void* param) __attribute__((alias("my_glDebugMessageCallback")));
EXPORT void my_glDebugMessageCallbackKHR(void* prod, void* param) __attribute__((alias("my_glDebugMessageCallback")));

EXPORT int my_glXSwapIntervalMESA(int interval)
{
    static iFi_t SwapIntervalMESA = NULL;
    static int init = 1;
    if(init) {
        SwapIntervalMESA = my_context->glxprocaddress("glXSwapIntervalMESA");
        init = 0;
    }
    if(!SwapIntervalMESA)
        return 0;
    return SwapIntervalMESA(interval);
}

EXPORT void my_glProgramCallbackMESA(void* f, void* data)
{
    static vFpp_t ProgramCallbackMESA = NULL;
    static int init = 1;
    if(init) {
        ProgramCallbackMESA = my_context->glxprocaddress("glProgramCallbackMESA");
        init = 0;
    }
    if(!ProgramCallbackMESA)
        return;
    ProgramCallbackMESA(find_program_callback_Fct(f), data);
}

EXPORT void my_eglSetBlobCacheFuncsANDROID(void* dpy, void* set, void* get)
{
    static vFppp_t eglSetBlobCacheFuncsANDROID = NULL;

    if(!eglSetBlobCacheFuncsANDROID && my_context && my_context->glxprocaddress)
        eglSetBlobCacheFuncsANDROID = my_context->glxprocaddress("eglSetBlobCacheFuncsANDROID");

    if(!eglSetBlobCacheFuncsANDROID)
        return;

    eglSetBlobCacheFuncsANDROID(dpy, find_set_blob_func_Fct(set), find_get_blob_func_Fct(get));
}

EXPORT int my_eglDebugMessageControlKHR(void* prod, void* param)
{
    static iFpp_t DebugMessageControlKHR = NULL;
    if(!DebugMessageControlKHR)
        DebugMessageControlKHR = my->eglDebugMessageControlKHR;
    if(!DebugMessageControlKHR)
        DebugMessageControlKHR = (iFpp_t)dlsym(RTLD_DEFAULT, "eglDebugMessageControlKHR");
    if(!DebugMessageControlKHR)
        return 0;
    return DebugMessageControlKHR(find_egl_debug_callback_Fct(prod), param);
}

EXPORT void* my_glXCreateContextAttribsARB(my_XDisplay_t* dpy, void* v2, void*v3, int32_t v4, void*v5);
EXPORT void* my_glXCreateContextAttribsARB(my_XDisplay_t* dpy, void* v2, void*v3, int32_t v4, void*v5)
{
    void* ret = my->glXCreateContextAttribsARB(dpy,v2,v3,v4,v5);
    latx_dpy_xcb_sync(dpy);
    return ret;
}
EXPORT int32_t my_glXMakeCurrent(my_XDisplay_t*, void*, void*);
EXPORT int32_t my_glXMakeCurrent(my_XDisplay_t* dpy, void* v2, void* v3)
{
    int32_t ret = my->glXMakeCurrent(dpy,v2,v3);
    latx_dpy_xcb_sync(dpy);
    return ret;
}
#if 0
void* my_GetVkProcAddr(void* name, void*(*getaddr)(void*));  // defined in wrappedvulkan.c
EXPORT void* my_glGetVkProcAddrNV(void* name)
{
    static pFp_t GetVkProcAddrNV = NULL;
    static int init = 1;
    if(init) {
        GetVkProcAddrNV = my_context->glxprocaddress("glGetVkProcAddrNV");
        init = 0;
    }
    return my_GetVkProcAddr(name, GetVkProcAddrNV);
}
#endif

#define PRE_INIT if(libGL) {lib->priv.w.lib = dlopen(libGL, RTLD_LAZY | RTLD_GLOBAL); lib->path = strdup(libGL);} else
#define CUSTOM_INIT \
    my_lib = lib;   \
    getMy(lib);   \
    lib->priv.w.priv = dlsym(lib->priv.w.lib, "glXGetProcAddress"); \
    setNeededLibs(lib, 2, "libGLX.so.0", "libX11.so.6"); \
    if (!box64->glxprocaddress) \
        box64->glxprocaddress = lib->priv.w.priv;

#define CUSTOM_FINI \
    freeMy();

#include "wrappedlib_init.h"

void fillGLProcWrapper(void)
{
    int cnt, ret;
    khint_t k;
    kh_symbolmap_t * symbolmap = kh_init(symbolmap);
    // populates maps...
    cnt = sizeof(libglsymbolmap)/sizeof(map_onesymbol_t);
    for (int i=0; i<cnt; ++i) {
        k = kh_put(symbolmap, symbolmap, libglsymbolmap[i].name, &ret);
        kh_value(symbolmap, k) = libglsymbolmap[i].w;
    }
    // and the my_ symbols map
    cnt = sizeof(MAPNAME(mysymbolmap))/sizeof(map_onesymbol_t);
    for (int i=0; i<cnt; ++i) {
        k = kh_put(symbolmap, symbolmap, libglmysymbolmap[i].name, &ret);
        kh_value(symbolmap, k) = libglmysymbolmap[i].w;
    }
    my_context->glwrappers = symbolmap;
    // my_* map
    symbolmap = kh_init(symbolmap);
    cnt = sizeof(MAPNAME(mysymbolmap))/sizeof(map_onesymbol_t);
    for (int i=0; i<cnt; ++i) {
        k = kh_put(symbolmap, symbolmap, libglmysymbolmap[i].name, &ret);
        kh_value(symbolmap, k) = libglmysymbolmap[i].w;
    }
    my_context->glmymap = symbolmap;
}

void freeGLProcWrapper(void)
{
    if(!my_context)
        return;
    if(my_context->glwrappers)
        kh_destroy(symbolmap, my_context->glwrappers);
    if(my_context->glmymap)
        kh_destroy(symbolmap, my_context->glmymap);
    my_context->glwrappers = NULL;
    my_context->glmymap = NULL;
}

// old code
#define SUPER()                         \
 GO(iFi_t, glXSwapIntervalMESA)

gl_wrappers_t* getGLProcWrapper(glprocaddress_t procaddress);
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
    cnt = sizeof(libglsymbolmap)/sizeof(map_onesymbol_t);
    for (int i=0; i<cnt; ++i) {
        k = kh_put(symbolmap, wrappers->glwrappers, libglsymbolmap[i].name, &ret);
        kh_value(wrappers->glwrappers, k) = libglsymbolmap[i].w;
    }
    // and the my_ symbols map
    cnt = sizeof(MAPNAME(mysymbolmap))/sizeof(map_onesymbol_t);
    for (int i=0; i<cnt; ++i) {
        k = kh_put(symbolmap, wrappers->glwrappers, libglmysymbolmap[i].name, &ret);
        kh_value(wrappers->glwrappers, k) = libglmysymbolmap[i].w;
    }
    // my_* map
    wrappers->glmymap = kh_init(symbolmap);
    cnt = sizeof(MAPNAME(mysymbolmap))/sizeof(map_onesymbol_t);
    for (int i=0; i<cnt; ++i) {
        k = kh_put(symbolmap, wrappers->glmymap, libglmysymbolmap[i].name, &ret);
        kh_value(wrappers->glmymap, k) = libglmysymbolmap[i].w;
    }
    return wrappers;
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
        SUPER()
        else {
            if(strcmp(rname, "glXGetProcAddress") && strcmp(rname, "glXGetProcAddressARB")) {
                printf_log(LOG_NONE, "Warning, %s defined as GOM, but find_%s_Fct not defined\n", rname, rname);
            }
            char tmp[200];
            strcpy(tmp, "my_");
            strcat(tmp, rname);
            symbol = dlsym(my_context->box64lib, tmp);
        }
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
