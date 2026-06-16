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
#include "callback.h"

#ifdef ANDROID
    const char* libxiName = "libXi.so";
#else
    const char* libxiName = "libXi.so.6";
#endif

#define LIBNAME libxi

typedef unsigned long XID;
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
        struct my_Async *async_handlers;
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

#include "generated/wrappedlibxitypes.h"
#include "wrappercallback.h"

#define SUPER() \
GO(0)   \
GO(1)   \
GO(2)   \
GO(3)   \
GO(4)   \
GO(5)   \
GO(6)   \
GO(7)   \
GO(8)   \
GO(9)   \
GO(10)  \
GO(11)  \
GO(12)  \
GO(13)  \
GO(14)  \
GO(15)

#define GO(A)   \
static uintptr_t my_XIGrabDeviceAsyncHandler_fct_##A = 0;                                              \
static int my_XIGrabDeviceAsyncHandler_##A(void* dpy, void* rep, void* buf, int len, void* data)       \
{                                                                                                   \
    return RunFunctionWithState(my_XIGrabDeviceAsyncHandler_fct_##A, 5, dpy, rep, buf, len, data);  \
}
SUPER()
#undef GO
static void* findXIGrabDeviceAsyncHandlerFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_XIGrabDeviceAsyncHandler_fct_##A == (uintptr_t)fct) return my_XIGrabDeviceAsyncHandler_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_XIGrabDeviceAsyncHandler_fct_##A == 0) {my_XIGrabDeviceAsyncHandler_fct_##A = (uintptr_t)fct; return my_XIGrabDeviceAsyncHandler_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 XIGrabDevice AsyncHandler callback\n");
    return NULL;
}

#define GO(A)   \
static uintptr_t my_XIQueryPointerAsyncHandler_fct_##A = 0;                                              \
static int my_XIQueryPointerAsyncHandler_##A(void* dpy, void* rep, void* buf, int len, void* data)       \
{                                                                                                   \
    return RunFunctionWithState(my_XIQueryPointerAsyncHandler_fct_##A, 5, dpy, rep, buf, len, data);  \
}
SUPER()
#undef GO
static void* findXIQueryPointerAsyncHandlerFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_XIQueryPointerAsyncHandler_fct_##A == (uintptr_t)fct) return my_XIQueryPointerAsyncHandler_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_XIQueryPointerAsyncHandler_fct_##A == 0) {my_XIQueryPointerAsyncHandler_fct_##A = (uintptr_t)fct; return my_XIQueryPointerAsyncHandler_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 XIQueryPointer AsyncHandler callback\n");
    return NULL;
}

EXPORT int32_t my_XIGrabDevice(
         my_XDisplay_t*           dpy,
         int32_t                deviceid,
         void*             grab_window,
         uintptr_t               time,
         void*             cursor,
         int32_t                grab_mode,
         int32_t                paired_device_mode,
         int32_t               owner_events,
         void*         mask
    );

EXPORT int32_t my_XIGrabDevice(
         my_XDisplay_t*           dpy,
         int32_t                deviceid,
         void*             grab_window,
         uintptr_t               time,
         void*             cursor,
         int32_t                grab_mode,
         int32_t                paired_device_mode,
         int32_t               owner_events,
         void*         mask
    )
{
    if (dpy->async_handlers) {
        struct my_Async *async, *next;
        for(async = dpy->async_handlers; async; async = next) {
            next = async->next;
            if ((uintptr_t)async->handler < reserved_va) {
                async->handler = findXIGrabDeviceAsyncHandlerFct(async->handler);
            }
        }
    }
    return my->XIGrabDevice(dpy, deviceid, grab_window, time, cursor, grab_mode, paired_device_mode, owner_events, mask);
}

EXPORT int32_t my_XIQueryPointer(my_XDisplay_t* dpy, int32_t v2, void* v3, void* v4, void* v5, void* v6, void* v7, void* v8, void* v9, void* v10, void* v11, void* v12);
EXPORT int32_t my_XIQueryPointer(my_XDisplay_t* dpy, int32_t v2, void* v3, void* v4, void* v5, void* v6, void* v7, void* v8, void* v9, void* v10, void* v11, void* v12)
{
    if (dpy->async_handlers) {
        struct my_Async *async, *next;
        for(async = dpy->async_handlers; async; async = next) {
            next = async->next;
            if ((uintptr_t)async->handler < reserved_va) {
                async->handler = findXIQueryPointerAsyncHandlerFct(async->handler);
            }
        }
    }
    return my->XIQueryPointer(dpy, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12);
}

#ifdef ANDROID
    #define CUSTOM_INIT \
         getMy(lib);                     \
        setNeededLibs(lib, 2, "libX11.so", "libXext.so");
#else
    #define CUSTOM_INIT \
         getMy(lib);                     \
        setNeededLibs(lib, 2, "libX11.so.6", "libXext.so.6");
#endif

#define CUSTOM_FINI \
    freeMy();

#include "wrappedlib_init.h"

