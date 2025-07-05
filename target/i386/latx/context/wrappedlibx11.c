#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "wrappedlibs.h"

#include "debug.h"
#include "wrapper.h"
#include "bridge.h"
#include "library_private.h"
#include "library.h"
#include "callback.h"
#include "librarian.h"
#include "box64context.h"
#include "myalign.h"
#include "wrappertbbridge.h"

#ifdef ANDROID
    const char* libx11Name = "libX11.so";
#else
    const char* libx11Name = "libX11.so.6";
#endif

#define LIBNAME libx11

typedef int (*XErrorHandler)(void *, void *);
void* my_XSetErrorHandler(XErrorHandler handler);
typedef int (*XIOErrorHandler)(void *);
void* my_XSetIOErrorHandler(XIOErrorHandler handler);
void* my_XESetCloseDisplay(void* display, int32_t extension, void* handler);
typedef int (*WireToEventProc)(void*, void*, void*);
typedef int(*EventHandler) (void*,void*,void*);
int32_t my_XIfEvent(void* d,void* ev, EventHandler h, void* arg);

typedef struct XImageSave_s {
    int   anyEmu;
    void* create;
    void* destroy;
    void* get;
    void* put;
    void* sub;
    void* add;
} XImageSave_t;

typedef struct ximage_s {
    void*(*create_image)(
            void*           /* display */,
            void*           /* visual */,
            uint32_t        /* depth */,
            int32_t         /* format */,
            int32_t         /* offset */,
            void*           /* data */,
            uint32_t        /* width */,
            uint32_t        /* height */,
            int32_t         /* bitmap_pad */,
            int32_t         /* bytes_per_line */);
    int32_t (*destroy_image)        (void*);
    uintptr_t (*get_pixel)           (void*, int32_t, int32_t);
    int32_t (*put_pixel)            (void*, int32_t, int32_t, uintptr_t);
    void*(*sub_image)    (void*, int32_t, int32_t, uint32_t, uint32_t); //sub_image return a new XImage that need bridging => custom wrapper
    int32_t (*add_pixel)            (void*, intptr_t);
} ximage_t;

typedef struct _XImage {
    int32_t width, height;          /* size of image */
    int32_t xoffset;                /* number of pixels offset in X direction */
    int32_t format;                 /* XYBitmap, XYPixmap, ZPixmap */
    void*   data;                   /* pointer to image data */
    int32_t byte_order;             /* data byte order, LSBFirst, MSBFirst */
    int32_t bitmap_unit;            /* quant. of scanline 8, 16, 32 */
    int32_t bitmap_bit_order;       /* LSBFirst, MSBFirst */
    int32_t bitmap_pad;             /* 8, 16, 32 either XY or ZPixmap */
    int32_t depth;                  /* depth of image */
    int32_t bytes_per_line;         /* accelarator to next line */
    int32_t bits_per_pixel;         /* bits per pixel (ZPixmap) */
    uintptr_t red_mask;             /* bits in z arrangment */
    uintptr_t green_mask;
    uintptr_t blue_mask;
    void*    obdata;                 /* hook for the object routines to hang on */
    ximage_t f;
} XImage;

typedef unsigned long XID;
struct my_XFreeFuncs {
    void* atoms;
    void* modifiermap;
    void* key_bindings;
    void* context_db;
    void* defaultCCCs;
    void* clientCmaps;
    void* intensityMaps;
    void* im_filters;
    void* xkb;
};

struct my_XExten {
        struct my_XExten *next;
        void* codes;    // XExtCodes
        void* create_GC;    // CreateGCType
        void* copy_GC;  // CopyGCType
        void* flush_GC; // FlushGCType
        void* free_GC;  // FreeGCType
        void* create_Font;  // CreateFontType
        void* free_Font;    // FreeFontType
        void* close_display;    // CloseDisplayType
        void* error;    // ErrorType
        void* error_string; // ErrorStringType
        char *name;
        void* error_values; // PrintErrorType
        void* before_flush; // BeforeFlushType
        struct my_XExten *next_flush;
};

struct my_XInternalAsync {
    struct my_XInternalAsync *next;
    int (*handler)(void*, void*, char*, int, void*);
    void* data;
};

struct my_XLockPtrs {
    void (*lock_display)(void* dpy);
    void (*unlock_display)(void *dpy);
};

struct my_XConnectionInfo {
    int fd;
    void* read_callback;    // _XInternalConnectionProc
    void* call_data;
    void* *watch_data;
    struct my_XConnectionInfo *next;
};

struct my_XConnWatchInfo {
    void* fn;   // XConnectionWatchProc
    void* client_data;
    struct _XConnWatchInfo *next;
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

typedef void (*vFp_t)(void*);
typedef uint32_t (*uFv_t)(void);
typedef int32_t (*iFpl_t)(void*, intptr_t);
typedef uintptr_t (*LFpii_t)(void*, int32_t, int32_t);
typedef int32_t (*iFpiiL_t)(void*, int32_t, int32_t, uintptr_t);
typedef void* (*pFpiiuu_t)(void*, int32_t, int32_t, uint32_t, uint32_t);

#define ADDED_FUNCTIONS()       \
    GO(XInitThreads, uFv_t)     \
    GO(XLockDisplay, vFp_t)     \
    GO(XUnlockDisplay, vFp_t)

#include "generated/wrappedlibx11types.h"

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

// wire_to_event
#define GO(A)   \
static uintptr_t my_wire_to_event_fct_##A = 0;                      \
static int my_wire_to_event_##A(void* dpy, void* re, void* event)   \
{                                                                   \
    return (int)RunFunctionWithState(my_wire_to_event_fct_##A, 3, dpy, re, event);\
}
SUPER()
#undef GO
static void* findwire_to_eventFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_wire_to_event_fct_##A == (uintptr_t)fct) return my_wire_to_event_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_wire_to_event_fct_##A == 0) {my_wire_to_event_fct_##A = (uintptr_t)fct; return my_wire_to_event_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 wire_to_event callback\n");
    return NULL;
}
static void* reverse_wire_to_eventFct(library_t* lib, void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(lib->priv.w.bridge, fct))
        return (void*)CheckBridged(lib->priv.w.bridge, fct);
    #define GO(A) if(my_wire_to_event_##A == fct) return (void*)my_wire_to_event_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(lib->priv.w.bridge, iFppp, fct, 0, NULL);
}

// event_to_wire
#define GO(A)   \
static uintptr_t my_event_to_wire_fct_##A = 0;                      \
static int my_event_to_wire_##A(void* dpy, void* re, void* event)   \
{                                                                   \
    return (int)RunFunctionWithState(my_event_to_wire_fct_##A, 3, dpy, re, event);\
}
SUPER()
#undef GO
static void* findevent_to_wireFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_event_to_wire_fct_##A == (uintptr_t)fct) return my_event_to_wire_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_event_to_wire_fct_##A == 0) {my_event_to_wire_fct_##A = (uintptr_t)fct; return my_event_to_wire_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 event_to_wire callback\n");
    return NULL;
}
static void* reverse_event_to_wireFct(library_t* lib, void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(lib->priv.w.bridge, fct))
        return (void*)CheckBridged(lib->priv.w.bridge, fct);
    #define GO(A) if(my_event_to_wire_##A == fct) return (void*)my_event_to_wire_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(lib->priv.w.bridge, iFppp, fct, 0, NULL);
}

// error_handler
#define GO(A)   \
static uintptr_t my_error_handler_fct_##A = 0;                      \
static int my_error_handler_##A(void* dpy, void* error)   \
{                                                                   \
    return (int)RunFunctionWithState(my_error_handler_fct_##A, 2, dpy, error);\
}
SUPER()
#undef GO
static void* finderror_handlerFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_error_handler_fct_##A == (uintptr_t)fct) return my_error_handler_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_error_handler_fct_##A == 0) {my_error_handler_fct_##A = (uintptr_t)fct; return my_error_handler_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 error_handler callback\n");
    return NULL;
}
static void* reverse_error_handlerFct(library_t* lib, void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(lib->priv.w.bridge, fct))
        return (void*)CheckBridged(lib->priv.w.bridge, fct);
    #define GO(A) if(my_error_handler_##A == fct) return (void*)my_error_handler_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(lib->priv.w.bridge, iFpp, fct, 0, NULL);
}

// ioerror_handler
#define GO(A)   \
static uintptr_t my_ioerror_handler_fct_##A = 0;                      \
static int my_ioerror_handler_##A(void* dpy)   \
{                                                                   \
    return (int)RunFunctionWithState(my_ioerror_handler_fct_##A, 1, dpy);\
}
SUPER()
#undef GO
static void* findioerror_handlerFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_ioerror_handler_fct_##A == (uintptr_t)fct) return my_ioerror_handler_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_ioerror_handler_fct_##A == 0) {my_ioerror_handler_fct_##A = (uintptr_t)fct; return my_ioerror_handler_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 ioerror_handler callback\n");
    return NULL;
}
static void* reverse_ioerror_handlerFct(library_t* lib, void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(lib->priv.w.bridge, fct))
        return (void*)CheckBridged(lib->priv.w.bridge, fct);
    #define GO(A) if(my_ioerror_handler_##A == fct) return (void*)my_ioerror_handler_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(lib->priv.w.bridge, iFp, fct, 0, NULL);
}

// exterror_handler
#define GO(A)   \
static uintptr_t my_exterror_handler_fct_##A = 0;                      \
static int my_exterror_handler_##A(void* dpy, void* err, void* codes, int* ret_code)   \
{                                                                   \
    return (int)RunFunctionWithState(my_exterror_handler_fct_##A, 4, dpy, err, codes, ret_code);\
}
SUPER()
#undef GO
static void* findexterror_handlerFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_exterror_handler_fct_##A == (uintptr_t)fct) return my_exterror_handler_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_exterror_handler_fct_##A == 0) {my_exterror_handler_fct_##A = (uintptr_t)fct; return my_exterror_handler_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 exterror_handler callback\n");
    return NULL;
}
static void* reverse_exterror_handlerFct(library_t* lib, void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(lib->priv.w.bridge, fct))
        return (void*)CheckBridged(lib->priv.w.bridge, fct);
    #define GO(A) if(my_exterror_handler_##A == fct) return (void*)my_exterror_handler_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(lib->priv.w.bridge, iFpppp, fct, 0, NULL);
}

// close_display
#define GO(A)   \
static uintptr_t my_close_display_fct_##A = 0;                      \
static int my_close_display_##A(void* dpy, void* codes)   \
{                                                                   \
    return (int)RunFunctionWithState(my_close_display_fct_##A, 2, dpy, codes);\
}
SUPER()
#undef GO
static void* findclose_displayFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_close_display_fct_##A == (uintptr_t)fct) return my_close_display_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_close_display_fct_##A == 0) {my_close_display_fct_##A = (uintptr_t)fct; return my_close_display_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 close_display callback\n");
    return NULL;
}
static void* reverse_close_displayFct(library_t* lib, void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(lib->priv.w.bridge, fct))
        return (void*)CheckBridged(lib->priv.w.bridge, fct);
    #define GO(A) if(my_close_display_##A == fct) return (void*)my_close_display_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(lib->priv.w.bridge, iFpp, fct, 0, NULL);
}

// register_im
#define GO(A)   \
static uintptr_t my_register_im_fct_##A = 0;                        \
static void my_register_im_##A(void* dpy, void* u, void* d)         \
{                                                                   \
    RunFunctionWithState(my_register_im_fct_##A, 3, dpy, u, d);  \
}
SUPER()
#undef GO
static void* findregister_imFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_register_im_fct_##A == (uintptr_t)fct) return my_register_im_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_register_im_fct_##A == 0) {my_register_im_fct_##A = (uintptr_t)fct; return my_register_im_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 register_im callback\n");
    return NULL;
}
static void* reverse_register_imFct(library_t* lib, void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(lib->priv.w.bridge, fct))
        return (void*)CheckBridged(lib->priv.w.bridge, fct);
    #define GO(A) if(my_register_im_##A == fct) return (void*)my_register_im_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(lib->priv.w.bridge, iFppp, fct, 0, NULL);
}

// XConnectionWatchProc
#define GO(A)   \
static uintptr_t my_XConnectionWatchProc_fct_##A = 0;                               \
static void my_XConnectionWatchProc_##A(void* dpy, void* data, int op, void* d)     \
{                                                                                   \
    RunFunctionWithState(my_XConnectionWatchProc_fct_##A, 4, dpy, data, op, d);  \
}
SUPER()
#undef GO
static void* findXConnectionWatchProcFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_XConnectionWatchProc_fct_##A == (uintptr_t)fct) return my_XConnectionWatchProc_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_XConnectionWatchProc_fct_##A == 0) {my_XConnectionWatchProc_fct_##A = (uintptr_t)fct; return my_XConnectionWatchProc_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 XConnectionWatchProc callback\n");
    return NULL;
}
// xifevent
#define GO(A)   \
static uintptr_t my_xifevent_fct_##A = 0;                                   \
static int my_xifevent_##A(void* dpy, void* event, void* d)                 \
{                                                                           \
    return RunFunctionWithState(my_xifevent_fct_##A, 3, dpy, event, d);  \
}
SUPER()
#undef GO
static void* findxifeventFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_xifevent_fct_##A == (uintptr_t)fct) return my_xifevent_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_xifevent_fct_##A == 0) {my_xifevent_fct_##A = (uintptr_t)fct; return my_xifevent_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 xifevent callback\n");
    return NULL;
}
// XInternalAsyncHandler
#define GO(A)   \
static uintptr_t my_XInternalAsyncHandler_fct_##A = 0;                                              \
static int my_XInternalAsyncHandler_##A(void* dpy, void* rep, void* buf, int len, void* data)       \
{                                                                                                   \
    return RunFunctionWithState(my_XInternalAsyncHandler_fct_##A, 5, dpy, rep, buf, len, data);  \
}
SUPER()
#undef GO
static void* findXInternalAsyncHandlerFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_XInternalAsyncHandler_fct_##A == (uintptr_t)fct) return my_XInternalAsyncHandler_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_XInternalAsyncHandler_fct_##A == 0) {my_XInternalAsyncHandler_fct_##A = (uintptr_t)fct; return my_XInternalAsyncHandler_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 XInternalAsyncHandler callback\n");
    return NULL;
}
//XInternAtomAsync
#define GO(A)   \
static uintptr_t my_XInternAtomAsyncHandler_fct_##A = 0;                                              \
static int my_XInternAtomAsyncHandler_##A(void* dpy, void* rep, void* buf, int len, void* data)       \
{                                                                                                   \
    return RunFunctionWithState(my_XInternAtomAsyncHandler_fct_##A, 5, dpy, rep, buf, len, data);  \
}
SUPER()
#undef GO
static void* findXInternAtomAsyncHandlerFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_XInternAtomAsyncHandler_fct_##A == (uintptr_t)fct) return my_XInternAtomAsyncHandler_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_XInternAtomAsyncHandler_fct_##A == 0) {my_XInternAtomAsyncHandler_fct_##A = (uintptr_t)fct; return my_XInternAtomAsyncHandler_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 XInternAtom AsyncHandler callback\n");
    return NULL;
}
//XSyncAsync
#define GO(A)   \
static uintptr_t my_XSyncAsyncHandler_fct_##A = 0;                                              \
static int my_XSyncAsyncHandler_##A(void* dpy, void* rep, void* buf, int len, void* data)       \
{                                                                                                   \
    return RunFunctionWithState(my_XSyncAsyncHandler_fct_##A, 5, dpy, rep, buf, len, data);  \
}
SUPER()
#undef GO
static void* findXSyncAsyncHandlerFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_XSyncAsyncHandler_fct_##A == (uintptr_t)fct) return my_XSyncAsyncHandler_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_XSyncAsyncHandler_fct_##A == 0) {my_XSyncAsyncHandler_fct_##A = (uintptr_t)fct; return my_XSyncAsyncHandler_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 XSync AsyncHandler callback\n");
    return NULL;
}
//XTranslateCoordinates
#define GO(A)   \
static uintptr_t my_XTranslateCoordinatesAsyncHandler_fct_##A = 0;                                              \
static int my_XTranslateCoordinatesAsyncHandler_##A(void* dpy, void* rep, void* buf, int len, void* data)       \
{                                                                                                   \
    return RunFunctionWithState(my_XTranslateCoordinatesAsyncHandler_fct_##A, 5, dpy, rep, buf, len, data);  \
}
SUPER()
#undef GO
static void* findXTranslateCoordinatesAsyncHandlerFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_XTranslateCoordinatesAsyncHandler_fct_##A == (uintptr_t)fct) return my_XTranslateCoordinatesAsyncHandler_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_XTranslateCoordinatesAsyncHandler_fct_##A == 0) {my_XTranslateCoordinatesAsyncHandler_fct_##A = (uintptr_t)fct; return my_XTranslateCoordinatesAsyncHandler_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 XTranslateCoordinates AsyncHandler callback\n");
    return NULL;
}
//XPending
#define GO(A)   \
static uintptr_t my_XPendingAsyncHandler_fct_##A = 0;                                              \
static int my_XPendingAsyncHandler_##A(void* dpy, void* rep, void* buf, int len, void* data)       \
{                                                                                                   \
    return RunFunctionWithState(my_XPendingAsyncHandler_fct_##A, 5, dpy, rep, buf, len, data);  \
}
SUPER()
#undef GO
static void* findXPendingAsyncHandlerFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_XPendingAsyncHandler_fct_##A == (uintptr_t)fct) return my_XPendingAsyncHandler_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_XPendingAsyncHandler_fct_##A == 0) {my_XPendingAsyncHandler_fct_##A = (uintptr_t)fct; return my_XPendingAsyncHandler_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 XPending AsyncHandler callback\n");
    return NULL;
}

// XSynchronizeProc
#define GO(A)   \
static uintptr_t my_XSynchronizeProc_fct_##A = 0;                       \
static int my_XSynchronizeProc_##A(void)                                    \
{                                                                       \
    return (int)RunFunctionWithState(my_XSynchronizeProc_fct_##A, 0);\
}
SUPER()
#undef GO
static void* findXSynchronizeProcFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_XSynchronizeProc_fct_##A == (uintptr_t)fct) return my_XSynchronizeProc_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_XSynchronizeProc_fct_##A == 0) {my_XSynchronizeProc_fct_##A = (uintptr_t)fct; return my_XSynchronizeProc_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 XSynchronizeProc callback\n");
    return NULL;
}

//XSyncFunctionPre
#define GO(A)   \
static uintptr_t my_XSyncFunctionPreAsyncHandler_fct_##A = 0;                                              \
static int my_XSyncFunctionPreAsyncHandler_##A(void* dpy, void* rep, void* buf, int len, void* data)       \
{                                                                                                   \
    return RunFunctionWithState(my_XSyncFunctionPreAsyncHandler_fct_##A, 5, dpy, rep, buf, len, data);  \
}
SUPER()
#undef GO
static void* findXSyncFunctionPreAsyncHandlerFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_XSyncFunctionPreAsyncHandler_fct_##A == (uintptr_t)fct) return my_XSyncFunctionPreAsyncHandler_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_XSyncFunctionPreAsyncHandler_fct_##A == 0) {my_XSyncFunctionPreAsyncHandler_fct_##A = (uintptr_t)fct; return my_XSyncFunctionPreAsyncHandler_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 XSyncFunctionPre AsyncHandler callback\n");
    return NULL;
}

#define GO(A)   \
static uintptr_t my_XGetWindowProperty_fct_##A = 0;                                              \
static int my_XGetWindowProperty_##A(void* dpy, void* rep, void* buf, int len, void* data)       \
{                                                                                                   \
    return RunFunctionWithState(my_XGetWindowProperty_fct_##A, 5, dpy, rep, buf, len, data);  \
}
SUPER()
#undef GO
static void* findXGetWindowPropertyFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_XGetWindowProperty_fct_##A == (uintptr_t)fct) return my_XGetWindowProperty_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_XGetWindowProperty_fct_##A == 0) {my_XGetWindowProperty_fct_##A = (uintptr_t)fct; return my_XGetWindowProperty_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 XIGrabDevice AsyncHandler callback\n");
    return NULL;
}

static void* reverse_XSynchronizeProcFct(library_t* lib, void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(lib->priv.w.bridge, fct))
        return (void*)CheckBridged(lib->priv.w.bridge, fct);
    #define GO(A) if(my_XSynchronizeProc_##A == fct) return (void*)my_XSynchronizeProc_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(lib->priv.w.bridge, iFppp, fct, 0, NULL);
}
#if 0
// XLockDisplay
#define GO(A)   \
static uintptr_t my_XLockDisplay_fct_##A = 0;                   \
static void my_XLockDisplay_##A(void* dpy)                      \
{                                                               \
    RunFunctionWithState(my_XLockDisplay_fct_##A, 1, dpy);   \
}
SUPER()
#undef GO
static void* findXLockDisplayFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_XLockDisplay_fct_##A == (uintptr_t)fct) return my_XLockDisplay_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_XLockDisplay_fct_##A == 0) {my_XLockDisplay_fct_##A = (uintptr_t)fct; return my_XLockDisplay_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 XLockDisplay callback\n");
    return NULL;
}
// XUnlockDisplay
#define GO(A)   \
static uintptr_t my_XUnlockDisplay_fct_##A = 0;                 \
static void my_XUnlockDisplay_##A(void* dpy)                    \
{                                                               \
    RunFunctionWithState(my_XUnlockDisplay_fct_##A, 1, dpy); \
}
SUPER()
#undef GO
static void* findXUnlockDisplayFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_XUnlockDisplay_fct_##A == (uintptr_t)fct) return my_XUnlockDisplay_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_XUnlockDisplay_fct_##A == 0) {my_XUnlockDisplay_fct_##A = (uintptr_t)fct; return my_XUnlockDisplay_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 XUnlockDisplay callback\n");
    return NULL;
}
#endif
//XGrabKeyboard
#define GO(A)   \
static uintptr_t my_XGrabKeyboardAsyncHandler_fct_##A = 0;                                              \
static int my_XGrabKeyboardAsyncHandler_##A(void* dpy, void* rep, void* buf, int len, void* data)       \
{                                                                                                   \
    return RunFunctionWithState(my_XGrabKeyboardAsyncHandler_fct_##A, 5, dpy, rep, buf, len, data);  \
}
SUPER()
#undef GO
static void* findXGrabKeyboardAsyncHandlerFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_XGrabKeyboardAsyncHandler_fct_##A == (uintptr_t)fct) return my_XGrabKeyboardAsyncHandler_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_XGrabKeyboardAsyncHandler_fct_##A == 0) {my_XGrabKeyboardAsyncHandler_fct_##A = (uintptr_t)fct; return my_XGrabKeyboardAsyncHandler_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 XGrabKeyboard AsyncHandler callback\n");
    return NULL;
}

//XQueryPointer
#define GO(A)   \
static uintptr_t my_XQueryPointerAsyncHandler_fct_##A = 0;                                              \
static int my_XQueryPointerAsyncHandler_##A(void* dpy, void* rep, void* buf, int len, void* data)       \
{                                                                                                   \
    return RunFunctionWithState(my_XQueryPointerAsyncHandler_fct_##A, 5, dpy, rep, buf, len, data);  \
}
SUPER()
#undef GO
static void* findXQueryPointerAsyncHandlerFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_XQueryPointerAsyncHandler_fct_##A == (uintptr_t)fct) return my_XQueryPointerAsyncHandler_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_XQueryPointerAsyncHandler_fct_##A == 0) {my_XQueryPointerAsyncHandler_fct_##A = (uintptr_t)fct; return my_XQueryPointerAsyncHandler_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 XQueryPointer AsyncHandler callback\n");
    return NULL;
}

// ResourceAlloc
#define GO(A)   \
static uintptr_t my_ResourceAlloc_fct_##A = 0;                          \
static XID my_ResourceAlloc_##A(void* dpy)                              \
{                                                                       \
    return (XID)RunFunctionWithState(my_ResourceAlloc_fct_##A, 1, dpy); \
}
SUPER()
#undef GO
static void* findResourceAllocFct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_ResourceAlloc_fct_##A == (uintptr_t)fct) return my_ResourceAlloc_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_ResourceAlloc_fct_##A == 0) {my_ResourceAlloc_fct_##A = (uintptr_t)fct; return my_ResourceAlloc_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libX11 ResourceAlloc callback\n");
    return NULL;
}

#undef SUPER

void* my_XCreateImage(void* disp, void* vis, uint32_t depth, int32_t fmt, int32_t off
                    , void* data, uint32_t w, uint32_t h, int32_t pad, int32_t bpl);

int32_t my_XInitImage(void* img);

void* my_XGetImage(void* disp, void* drawable, int32_t x, int32_t y
                    , uint32_t w, uint32_t h, uint32_t plane, int32_t fmt);

int32_t my_XPutImage(void* disp, void* drawable, void* gc, void* image
                    , int32_t src_x, int32_t src_y, int32_t dst_x, int32_t dst_y
                    , uint32_t w, uint32_t h);

void* my_XGetSubImage(void* disp, void* drawable
                    , int32_t x, int32_t y
                    , uint32_t w, uint32_t h, uintptr_t plane, int32_t fmt
                    , void* image, int32_t dst_x, int32_t dst_y);

void my_XDestroyImage(void* image);

#ifdef PANDORA
void* my_XLoadQueryFont(void* d, void* name);
#endif

typedef void (*XIMProc)(void*, void*, void*);
typedef int (*XICProc)(void*, void*, void*);
typedef struct {
    void* client_data;
    XIMProc callback;
} XIMCallback;

typedef struct {
    void* client_data;
    XICProc callback;
} XICCallback;

#define XNGeometryCallback "geometryCallback"
#define XNDestroyCallback "destroyCallback"
#define XNPreeditStartCallback "preeditStartCallback"
#define XNPreeditDoneCallback "preeditDoneCallback"
#define XNPreeditDrawCallback "preeditDrawCallback"
#define XNPreeditCaretCallback "preeditCaretCallback"
#define XNPreeditStateNotifyCallback "preeditStateNotifyCallback"
#define XNStatusStartCallback "statusStartCallback"
#define XNStatusDoneCallback "statusDoneCallback"
#define XNStatusDrawCallback "statusDrawCallback"
#define XNR6PreeditCallback "r6PreeditCallback"
#define XNStringConversionCallback "stringConversionCallback"

#define SUPER()                     \
GO(XNGeometryCallback)              \
GO(XNDestroyCallback)               \
GO(XNPreeditStartCallback)          \
GO(XNPreeditDoneCallback)           \
GO(XNPreeditDrawCallback)           \
GO(XNPreeditCaretCallback)          \
GO(XNPreeditStateNotifyCallback)    \
GO(XNStatusStartCallback)           \
GO(XNStatusDoneCallback)            \
GO(XNStatusDrawCallback)            \
GO(XNR6PreeditCallback)             \
GO(XNStringConversionCallback)

#define GO(A)                                                               \
static uintptr_t my_XICProc_fct_##A = 0;                                    \
static int my_XICProc_##A(void* a, void* b, void* c)                        \
{                                                                           \
    if (my_XICProc_fct_##A == 0)                                            \
        printf_log(LOG_NONE, "%s cannot find XICProc callback\n", __func__);\
    return (int)RunFunctionWithState(my_XICProc_fct_##A, 3, a, b, c);    \
}                                                                           \
static uintptr_t my_XIMProc_fct_##A = 0;                                    \
static void my_XIMProc_##A(void* a, void* b, void* c)                       \
{                                                                           \
    if (my_XIMProc_fct_##A == 0)                                            \
        printf_log(LOG_NONE, "%s cannot find XIMProc callback\n", __func__);\
    RunFunctionWithState(my_XIMProc_fct_##A, 3, a, b, c);                \
}
SUPER()
#undef GO

#define VA_CALL(FUNC, FIRST_ARG, VAARGS, VAARGSZ, RESULT)       \
switch (VAARGSZ)                                                \
{                                                               \
case 2:                                                         \
    RESULT = FUNC(FIRST_ARG, VAARGS[0], VAARGS[1], NULL);       \
    break;                                                      \
case 4:                                                         \
    RESULT = FUNC(FIRST_ARG, VAARGS[0], VAARGS[1], VAARGS[2], VAARGS[3], NULL);     \
    break;                                                                          \
case 6:                                                                             \
    RESULT = FUNC(FIRST_ARG, VAARGS[0], VAARGS[1], VAARGS[2], VAARGS[3], VAARGS[4], VAARGS[5], NULL);   \
    break;                                                                                              \
case 8:                                                                                                 \
    RESULT = FUNC(FIRST_ARG, VAARGS[0], VAARGS[1], VAARGS[2], VAARGS[3], VAARGS[4], VAARGS[5], VAARGS[6], VAARGS[7], NULL); \
    break;                                                                                                                  \
case 10:                                                                                                                    \
    RESULT = FUNC(FIRST_ARG, VAARGS[0], VAARGS[1], VAARGS[2], VAARGS[3], VAARGS[4], VAARGS[5], VAARGS[6], VAARGS[7], VAARGS[8], VAARGS[9], NULL);   \
    break;                                                                                                                                          \
case 12:                                                                                                                                            \
    RESULT = FUNC(FIRST_ARG, VAARGS[0], VAARGS[1], VAARGS[2], VAARGS[3], VAARGS[4], VAARGS[5], VAARGS[6], VAARGS[7], VAARGS[8], VAARGS[9],  VAARGS[10], VAARGS[11], NULL);  \
    break;                                                                                                                                                                  \
case 14:                                                                                                                                                                    \
    RESULT = FUNC(FIRST_ARG, VAARGS[0], VAARGS[1], VAARGS[2], VAARGS[3], VAARGS[4], VAARGS[5], VAARGS[6], VAARGS[7], VAARGS[8], VAARGS[9],  VAARGS[10], VAARGS[11], VAARGS[12], VAARGS[13], NULL);  \
    break;                                                                                                                                                                                          \
case 16:                                                                                                                                                                                            \
    RESULT = FUNC(FIRST_ARG, VAARGS[0], VAARGS[1], VAARGS[2], VAARGS[3], VAARGS[4], VAARGS[5], VAARGS[6], VAARGS[7], VAARGS[8], VAARGS[9],  VAARGS[10], VAARGS[11], VAARGS[12], VAARGS[13], VAARGS[14], VAARGS[15], NULL);  \
    break;                                                                                                                                                                                                                  \
case 18:                                                                                                                                                                                                                    \
    RESULT = FUNC(FIRST_ARG, VAARGS[0], VAARGS[1], VAARGS[2], VAARGS[3], VAARGS[4], VAARGS[5], VAARGS[6], VAARGS[7], VAARGS[8], VAARGS[9],  VAARGS[10], VAARGS[11], VAARGS[12], VAARGS[13], VAARGS[14], VAARGS[15], VAARGS[16], VAARGS[17], NULL);  \
    break;                                                                                                                                                                                                                                          \
case 20:                                                                                                                                                                                                                                            \
    RESULT = FUNC(FIRST_ARG, VAARGS[0], VAARGS[1], VAARGS[2], VAARGS[3], VAARGS[4], VAARGS[5], VAARGS[6], VAARGS[7], VAARGS[8], VAARGS[9],  VAARGS[10], VAARGS[11], VAARGS[12], VAARGS[13], VAARGS[14], VAARGS[15], VAARGS[16], VAARGS[17], VAARGS[18], VAARGS[19], NULL);  \
    break;                                                                                                                                                                                                                                                                  \
default:                                                                                                                \
    printf_log(LOG_NONE, "warning: %s's vasize (%d) is too large, need create new call case!\n", __func__, VAARGSZ);    \
    break;                                                                                                              \
}

#define GO(A)                                           \
if (new_va[i] && strcmp((char*)new_va[i], A) == 0) {    \
    XICCallback* origin = (XICCallback*)new_va[i+1];    \
    XICCallback* new = callbacks + i;                   \
    new->client_data = origin->client_data;             \
    my_XICProc_fct_##A = (uintptr_t)origin->callback;   \
    new->callback = my_XICProc_##A;                     \
    new_va[i+1] = new;                                  \
}

EXPORT void* my_XVaCreateNestedList(int unused, uintptr_t* va);
EXPORT void* my_XVaCreateNestedList(int unused, uintptr_t* va) {
    int n = 0;
    while (getVArgs(1, va, n)) n+=2 ;
    void** new_va = box_malloc(sizeof(void*) * n);
    XICCallback* callbacks = (XICCallback*)box_malloc(sizeof(XIMCallback) * n);

    for (int i = 0; i < n; i += 2) {
        new_va[i] = (void*)getVArgs(1, va, i);
        new_va[i+1] = (void*)getVArgs(1, va, i+1);
        SUPER()
    }

    void* res = NULL;
    VA_CALL(my->XVaCreateNestedList, unused, new_va, n, res);
    box_free(new_va);
    box_free(callbacks);
    return res;
}

EXPORT void* my_XCreateIC(void* xim, uintptr_t* va);
EXPORT void* my_XCreateIC(void* xim, uintptr_t* va) {
    int n = 0;
    while (getVArgs(1, va, n)) n+=2;
    void** new_va = box_malloc(sizeof(void*) * n);
    XICCallback* callbacks = (XICCallback*)box_malloc(sizeof(XIMCallback) * n);

    for (int i = 0; i < n; i += 2) {
        new_va[i] = (void*)getVArgs(1, va, i);
        new_va[i+1] = (void*)getVArgs(1, va, i+1);
        SUPER()
    }

    void* res = NULL;
    VA_CALL(my->XCreateIC, xim, new_va, n, res);
    box_free(new_va);
    box_free(callbacks);
    return res;
}

EXPORT void* my_XSetICValues(void* xic, uintptr_t* va);
EXPORT void* my_XSetICValues(void* xic, uintptr_t* va) {
    int n = 0;
    while (getVArgs(1, va, n)) n+=2;
    void** new_va = box_malloc(sizeof(void*) * n);
    XICCallback* callbacks = (XICCallback*)box_malloc(sizeof(XIMCallback) * n);

    for (int i = 0; i < n; i += 2) {
        new_va[i] = (void*)getVArgs(1, va, i);
        new_va[i+1] = (void*)getVArgs(1, va, i+1);
        SUPER()
    }

    void* res = NULL;
    VA_CALL(my->XSetICValues, xic, new_va, n, res);
    box_free(new_va);
    box_free(callbacks);
    return res;
}
#undef GO

EXPORT void* my_XSetIMValues(void* xim, uintptr_t* va);
EXPORT void* my_XSetIMValues(void* xim, uintptr_t* va) {
    int n = 0;
    while (getVArgs(1, va, n)) n+=2;
    void** new_va = box_malloc(sizeof(void*) * n);
    XIMCallback* callbacks = (XIMCallback*)box_malloc(sizeof(XIMCallback) * n);

    #define GO(A)                                           \
    if (new_va[i] && strcmp((char*)new_va[i], A) == 0) {    \
        XIMCallback* origin = (XIMCallback*)new_va[i+1];    \
        XIMCallback* new = callbacks + i;                   \
        new->client_data = origin->client_data;             \
        my_XIMProc_fct_##A = (uintptr_t)origin->callback;   \
        new->callback = my_XIMProc_##A;                     \
        new_va[i+1] = new;                                  \
    }
    for (int i = 0; i < n; i += 2) {
        new_va[i] = (void*)getVArgs(1, va, i);
        new_va[i+1] = (void*)getVArgs(1, va, i+1);
        SUPER()
    }
    #undef GO

    void* res = NULL;
    VA_CALL(my->XSetIMValues, xim, new_va, n, res)
    box_free(new_va);
    box_free(callbacks);
    return res;
}
#undef VA_CALL
#undef SUPER

EXPORT void* my_XSetErrorHandler(XErrorHandler handler)
{
    void* ret = my->XSetErrorHandler(finderror_handlerFct(handler));
    return reverse_error_handlerFct(my_lib, ret);
}

EXPORT void* my_XSetIOErrorHandler(XIOErrorHandler handler)
{
    void* ret = my->XSetIOErrorHandler(findioerror_handlerFct(handler));
    return reverse_ioerror_handlerFct(my_lib, ret);
}

EXPORT void* my_XESetError(void* display, int32_t extension, void* handler);
EXPORT void* my_XESetError(void* display, int32_t extension, void* handler)
{
    void* ret = my->XESetError(display, extension, findexterror_handlerFct(handler));
    return reverse_exterror_handlerFct(my_lib, ret);
}

EXPORT void* my_XESetCloseDisplay(void* display, int32_t extension, void* handler)
{
    void* ret = my->XESetCloseDisplay(display, extension, findclose_displayFct(handler));
    return reverse_close_displayFct(my_lib, ret);
}

EXPORT int32_t my_XIfEvent(void* d,void* ev, EventHandler h, void* arg)
{
    int32_t ret = my->XIfEvent(d, ev, findxifeventFct(h), arg);
    return ret;
}

EXPORT int32_t my_XCheckIfEvent(void* d,void* ev, EventHandler h, void* arg);
EXPORT int32_t my_XCheckIfEvent(void* d,void* ev, EventHandler h, void* arg)
{
    int32_t ret = my->XCheckIfEvent(d, ev, findxifeventFct(h), arg);
    return ret;
}

EXPORT int32_t my_XPeekIfEvent(void* d,void* ev, EventHandler h, void* arg);
EXPORT int32_t my_XPeekIfEvent(void* d,void* ev, EventHandler h, void* arg)
{
    int32_t ret = my->XPeekIfEvent(d, ev, findxifeventFct(h), arg);
    return ret;
}

void sub_image_wrapper(uintptr_t fnc);
typedef void* (*sub_image_wrapper_t)(void*, int32_t, int32_t, uint32_t, uint32_t);

void BridgeImageFunc(XImage *img);
void BridgeImageFunc(XImage *img)
{
    bridge_t* system = my_context->system;
    #undef GO
    #define GO(A, W) \
    fnc = CheckBridged(system, img->f.A); \
    if(!fnc) fnc = AddAutomaticBridge(system, W, img->f.A, 0); \
    img->f.A = (W##_t)fnc;

    uintptr_t fnc;

    GO(create_image, pFppuiipuuii)
    GO(destroy_image, iFp)
    GO(get_pixel, LFpii)
    GO(put_pixel, iFpiiL)
    GO(sub_image, sub_image_wrapper)
    GO(add_pixel, iFpl)
    #undef GO
}

void UnbridgeImageFunc(XImage *img);
void UnbridgeImageFunc(XImage *img)
{
    #define GO(A, W) \
    fnc = GetNativeFnc((uintptr_t)img->f.A); \
    if(fnc) \
        img->f.A = (W##_t)fnc;

    void* fnc;

    GO(create_image, pFppuiipuuii)
    GO(destroy_image, iFp)
    GO(get_pixel, LFpii)
    GO(put_pixel, iFpiiL)
    GO(sub_image, sub_image_wrapper)
    GO(add_pixel, iFpl)
    #undef GO
}
#define __CPU CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
#define R_RAX cpu->regs[R_EAX]
#define R_RDI cpu->regs[R_EDI]
#define R_RSI cpu->regs[R_ESI]
#define R_RDX cpu->regs[R_EDX]
#define R_RCX cpu->regs[R_ECX]
#define R_R8 cpu->regs[R_R8]

void sub_image_wrapper(uintptr_t fnc)
{
    __CPU;
    pFpiiuu_t fn = (pFpiiuu_t)fnc;
    void* img = fn(*(void**)(R_RDI), *(int32_t*)(R_RSI), *(int32_t*)(R_RDX), *(uint32_t*)(R_RCX), *(uint32_t*)(R_R8));
    BridgeImageFunc((XImage*)img);
    R_RAX=(uintptr_t)img;
}

#undef __CPU
#undef R_RAX
#undef R_RDI
#undef R_RSI
#undef R_RDX
#undef R_RCX
#undef R_R8
extern void * x86free;
abi_ulong latx_is_shm(abi_ulong maddr);
EXPORT void* my_XCreateImage(void* disp, void* vis, uint32_t depth, int32_t fmt, int32_t off
                    , void* data, uint32_t w, uint32_t h, int32_t pad, int32_t bpl)
{

    XImage *img = my->XCreateImage(disp, vis, depth, fmt, off, data, w, h, pad, bpl);
    if(!img)
        return img;
    // bridge all access functions...

    BridgeImageFunc(img);
    return img;
}

EXPORT int32_t my_XInitImage(void* img)
{

    int ret = my->XInitImage(img);
    // bridge all access functions...
    BridgeImageFunc(img);
    return ret;
}

EXPORT void* my_XGetImage(void* disp, void* drawable, int32_t x, int32_t y
                    , uint32_t w, uint32_t h, uint32_t plane, int32_t fmt)
{

    XImage *img = my->XGetImage(disp, drawable, x, y, w, h, plane, fmt);
    if(!img)
        return img;
    // bridge all access functions...
    BridgeImageFunc(img);
    return img;
}

EXPORT int32_t my_XPutImage(void* disp, void* drawable, void* gc, void* image
                    , int32_t src_x, int32_t src_y, int32_t dst_x, int32_t dst_y
                    , uint32_t w, uint32_t h)
{
    UnbridgeImageFunc((XImage*)image);
    int32_t r = my->XPutImage(disp, drawable, gc, image, src_x, src_y, dst_x, dst_y, w, h);
    // bridge all access functions...
    BridgeImageFunc((XImage*)image);
    return r;
}

EXPORT void* my_XGetSubImage(void* disp, void* drawable
                    , int32_t x, int32_t y
                    , uint32_t w, uint32_t h, uintptr_t plane, int32_t fmt
                    , void* image, int32_t dst_x, int32_t dst_y)
{

    UnbridgeImageFunc((XImage*)image);
    XImage *img = my->XGetSubImage(disp, drawable, x, y, w, h, plane, fmt, image, dst_x, dst_y);
    BridgeImageFunc((XImage*)image);
    return img;
}

EXPORT void my_XDestroyImage(void* image)
{

    UnbridgeImageFunc((XImage*)image);
    XImage* img = image;
    if (img->data && (uintptr_t)img->data  < reserved_va &&
        !latx_is_shm((abi_ulong)img->data)) {
        size_t len = img->bytes_per_line * img->height;
        void *la_data = malloc(len);
        memcpy(la_data, img->data, len);
        lsassert(x86free);
        RunFunctionWithState((uintptr_t)x86free ,1, img->data);
        img->data = la_data;
    }
    my->XDestroyImage(image);
}

typedef struct xintasync_s {
    struct xintasync_s *next;
    int (*handler)(
                    void*,
                    void*,
                    void*,
                    int,
                    void*
                    );
    void* data;
} xintasync_t;

EXPORT void* my_XESetWireToEvent(void* display, int32_t event_number, void* proc);
EXPORT void* my_XESetWireToEvent(void* display, int32_t event_number, void* proc)
{
    void* ret = NULL;

    ret = my->XESetWireToEvent(display, event_number, findwire_to_eventFct(proc));

    return reverse_wire_to_eventFct(my_lib, ret);
}
EXPORT void* my_XESetEventToWire(void* display, int32_t event_number, void* proc);
EXPORT void* my_XESetEventToWire(void* display, int32_t event_number, void* proc)
{
    void* ret = NULL;

    ret = my->XESetEventToWire(display, event_number, findevent_to_wireFct(proc));

    return reverse_event_to_wireFct(my_lib, ret);
}

EXPORT int my_XRegisterIMInstantiateCallback(void* d, void* db, void* res_name, void* res_class, void* cb, void* data);
EXPORT int my_XRegisterIMInstantiateCallback(void* d, void* db, void* res_name, void* res_class, void* cb, void* data)
{
    return my->XRegisterIMInstantiateCallback(d, db, res_name, res_class, findregister_imFct(cb), data);
}

EXPORT int my_XUnregisterIMInstantiateCallback(void* d, void* db, void* res_name, void* res_class, void* cb, void* data);
EXPORT int my_XUnregisterIMInstantiateCallback(void* d, void* db, void* res_name, void* res_class, void* cb, void* data)
{
    return my->XUnregisterIMInstantiateCallback(d, db, res_name, res_class, reverse_register_imFct(my_lib, cb), data);
}

EXPORT int my_XQueryExtension(void* display, char* name, int* major, int* first_event, int* first_error);
EXPORT int my_XQueryExtension(void* display, char* name, int* major, int* first_event, int* first_error)
{
    int ret = my->XQueryExtension(display, name, major, first_event, first_error);
    if(!ret && name && !strcmp(name, "GLX")) {
        // hack to force GLX to be accepted, even if not present
        // left major and first_XXX to default...
        ret = 1;
    }
    return ret;
}

EXPORT int my_XAddConnectionWatch(void* display, char* f, void* data);
EXPORT int my_XAddConnectionWatch(void* display, char* f, void* data)
{
    return my->XAddConnectionWatch(display, findXConnectionWatchProcFct(f), data);
}

EXPORT int my_XRemoveConnectionWatch(void* display, char* f, void* data);
EXPORT int my_XRemoveConnectionWatch(void* display, char* f, void* data)
{
    return my->XRemoveConnectionWatch(display, findXConnectionWatchProcFct(f), data);
}

EXPORT void* my_XSetAfterFunction(void* display, void* f);
EXPORT void* my_XSetAfterFunction(void* display, void* f)
{

    return reverse_XSynchronizeProcFct(my_lib, my->XSetAfterFunction(display, findXSynchronizeProcFct(f)));
}

static void kzt_handleasync(my_XDisplay_t* dpy,void*(*fct)(void*))
{
    if (dpy->async_handlers) {
        struct my_XInternalAsync *async, *next;
        for(async = dpy->async_handlers; async; async = next) {
            next = async->next;
            if ((uintptr_t)async->handler < reserved_va) {
                async->handler = fct(async->handler);
            }
        }
    }
}
static void* kzt_XSyncFunction;
static int kzt_XSyncFunctionPre(my_XDisplay_t* dpy)
{
    kzt_handleasync(dpy, findXSyncFunctionPreAsyncHandlerFct);
    return ((int (*)(void*))kzt_XSyncFunction)(dpy);
}

#define XlibDisplayPrivSync    (1L << 3)
EXPORT void* my_XSynchronize(void* display, int onoff);
EXPORT void* my_XSynchronize(void* display, int onoff)
{
    void* ret = my->XSynchronize(display, onoff);
    my_XDisplay_t* dpy = (my_XDisplay_t*)display;
    if (dpy->flags & XlibDisplayPrivSync) {
        kzt_XSyncFunction = dpy->savedsynchandler;
    } else {
        kzt_XSyncFunction = dpy->synchandler;
    }
    kzt_tbbridge_insert((uintptr_t)kzt_XSyncFunction, (ADDR)kzt_XSyncFunctionPre, iFp);
    return reverse_XSynchronizeProcFct(my_lib, ret);
}

static void* kzt_resource_alloc = NULL;
static XID kzt_resource_allocPre(my_XDisplay_t* dpy)
{
    return ((XID (*)(void*))kzt_resource_alloc)(dpy);
}

#define GO(A)   \
static void* kzt_my_ResourceAlloc_##A = NULL;               \
static XID kzt_my_ResourceAllocPre_##A(my_XDisplay_t* dpy)  \
{                                                           \
    return ((XID (*)(void*))kzt_my_ResourceAlloc_##A)(dpy); \
}
GO(0)
GO(1)
#undef GO

EXPORT void* my_XOpenDisplay(void* d);
EXPORT void* my_XOpenDisplay(void* d)
{
    void* ret = my->XOpenDisplay(d);
    // Added automatic bridge because of thos macro from Xlibint.h
    //#define LockDisplay(d)       if ((d)->lock_fns) (*(d)->lock_fns->lock_display)(d)
    //#define UnlockDisplay(d)     if ((d)->lock_fns) (*(d)->lock_fns->unlock_display)(d)

    my_XDisplay_t* dpy = (my_XDisplay_t*)ret;
    if(!ret)
        return ret;

    kzt_resource_alloc = dpy->resource_alloc;
    kzt_tbbridge_insert((uintptr_t)kzt_resource_alloc, (ADDR)kzt_resource_allocPre, LFp);
    #define GO(A)   \
    kzt_my_ResourceAlloc_##A = my_ResourceAlloc_##A;                                                  \
    kzt_tbbridge_insert((uintptr_t)kzt_my_ResourceAlloc_##A, (ADDR)kzt_my_ResourceAllocPre_##A, LFp);
    GO(0)
    GO(1)
    #undef GO

    bridge_t* system = my_context->system;

    #define GO(A, W)\
    if(dpy->A)      \
        if(!CheckBridged(system, dpy->A)) \
            AddAutomaticBridge(system, W, dpy->A, 0); \

    #define GO2(A, B, W) \
    if(dpy->A && dpy->A->B)  \
        if(!CheckBridged(system, dpy->A->B)) \
            AddAutomaticBridge(system, W, dpy->A->B, 0); \


    GO2(free_funcs, atoms, vFp)
    GO2(free_funcs, modifiermap, iFp)
    GO2(free_funcs, key_bindings, vFp)
    GO2(free_funcs, context_db, vFp)
    GO2(free_funcs, defaultCCCs, vFp)
    GO2(free_funcs, clientCmaps, vFp)
    GO2(free_funcs, intensityMaps, vFp)
    GO2(free_funcs, im_filters, vFp)
    GO2(free_funcs, xkb, vFp)
    GO(resource_alloc, LFp)
    GO(synchandler, iFp)
    //TODO: ext_procs?
    //TODO: event_vec?
    //TODO: wire_vec?
    //TODO: async_handlers?
    GO2(lock_fns, lock_display, vFp);
    GO2(lock_fns, unlock_display, vFp);
    GO(idlist_alloc, vFppi)
    //TODO: error_vec?
    //TODO: flushes
    //TODO: im_fd_info?
    //TODO: conn_watchers
    GO(savedsynchandler, iFp)
    //TODO: generic_event_vec?
    //TODO: generic_event_copy_vec?


    #undef GO
    #undef GO2

    return ret;
}

EXPORT void* my_XInternAtom(my_XDisplay_t* dpy, void* name, int32_t onlyIfExists);
EXPORT void* my_XInternAtom(my_XDisplay_t* dpy, void* name, int32_t onlyIfExists)
{
    kzt_handleasync(dpy, findXInternAtomAsyncHandlerFct);
    return my->XInternAtom(dpy, name, onlyIfExists);
}

EXPORT int32_t my_XSync(my_XDisplay_t* dpy, uint32_t v2);
EXPORT int32_t my_XSync(my_XDisplay_t* dpy, uint32_t v2)
{
    kzt_handleasync(dpy, findXSyncAsyncHandlerFct);
    int32_t ret = my->XSync(dpy, v2);
    latx_dpy_xcb_sync(dpy);
    return ret;
}

EXPORT void my__XDeqAsyncHandler(my_XDisplay_t* dpy, void* data);
EXPORT void my__XDeqAsyncHandler(my_XDisplay_t* dpy, void* data)
{
    kzt_handleasync(dpy, findXInternalAsyncHandlerFct);
    my->_XDeqAsyncHandler(dpy, data);
}

EXPORT int32_t my_XTranslateCoordinates(my_XDisplay_t* dpy, void* v2, void* v3, int32_t v4, int32_t v5, void* v6, void* v7, void* v8);
EXPORT int32_t my_XTranslateCoordinates(my_XDisplay_t* dpy, void* v2, void* v3, int32_t v4, int32_t v5, void* v6, void* v7, void* v8)
{
    kzt_handleasync(dpy, findXTranslateCoordinatesAsyncHandlerFct);
    return my->XTranslateCoordinates(dpy, v2, v3, v4, v5, v6, v7, v8);
}

EXPORT int32_t my_XGrabKeyboard(my_XDisplay_t*, void*, int32_t, int32_t, int32_t, uintptr_t);
EXPORT int32_t my_XGrabKeyboard(my_XDisplay_t* dpy, void* v2, int32_t v3, int32_t v4, int32_t v5, uintptr_t v6)
{
    kzt_handleasync(dpy, findXGrabKeyboardAsyncHandlerFct);
    return my->XGrabKeyboard(dpy, v2, v3, v4, v5, v6);
}

EXPORT int32_t my_XQueryPointer(my_XDisplay_t*, void*, void*, void*, void*, void*, void*, void*, void*);
EXPORT int32_t my_XQueryPointer(my_XDisplay_t* dpy, void* v2, void* v3, void* v4, void* v5, void* v6, void* v7, void* v8, void* v9)
{
    kzt_handleasync(dpy, findXQueryPointerAsyncHandlerFct);
    return my->XQueryPointer(dpy, v2, v3, v4, v5, v6, v7, v8, v9);
}
int latx_dpy_xcb_sync(void *v1)
{
    my_XDisplay_t* dpy = v1;
    return sync_xcb_connection(*dpy->xcb);
}

extern void* x86pthread_setcanceltype;
EXPORT int32_t my_XNextEvent(void* v1, void* v2);
EXPORT int32_t my_XNextEvent(void* v1, void* v2)
{
    int oldtype;
    int32_t ret;
    uint64_t callbackret = RunFunctionWithState((uintptr_t)x86pthread_setcanceltype ,2, PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);
    ret = my->XNextEvent(v1,v2);
    callbackret = RunFunctionWithState((uintptr_t)x86pthread_setcanceltype ,2, oldtype, NULL);
    (void)callbackret;
    return ret;
}

EXPORT void my_XPending(my_XDisplay_t* dpy);
EXPORT void my_XPending(my_XDisplay_t* dpy)
{
    kzt_handleasync(dpy, findXPendingAsyncHandlerFct);
    my->XPending(dpy);
}
EXPORT int32_t my_XGetWindowProperty(my_XDisplay_t* dpy, void* v2, void* v3, intptr_t v4, intptr_t v5, int32_t v6, void* v7, void* v8, void* v9, void* v10, void* v11, void* v12);
EXPORT int32_t my_XGetWindowProperty(my_XDisplay_t* dpy, void* v2, void* v3, intptr_t v4, intptr_t v5, int32_t v6, void* v7, void* v8, void* v9, void* v10, void* v11, void* v12)
{
    if (dpy->async_handlers) {
        struct my_XInternalAsync *async, *next;
        for(async = dpy->async_handlers; async; async = next) {
            next = async->next;
            if ((uintptr_t)async->handler < reserved_va) {
                async->handler = findXGetWindowPropertyFct(async->handler);
            }
        }
    }
    return my->XGetWindowProperty(dpy, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12);
}

EXPORT void* my_XCreateWindow(my_XDisplay_t* dpy, void* v2, int32_t v3, int32_t v4, uint32_t v5, uint32_t v6, uint32_t v7, int32_t v8, uint32_t v9, void* v10, uintptr_t v11, void* v12);
EXPORT void* my_XCreateWindow(my_XDisplay_t* dpy, void* v2, int32_t v3, int32_t v4, uint32_t v5, uint32_t v6, uint32_t v7, int32_t v8, uint32_t v9, void* v10, uintptr_t v11, void* v12)
{
    dpy->resource_alloc = findResourceAllocFct(dpy->resource_alloc);
    void * ret = my->XCreateWindow(dpy, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12);
    latx_dpy_xcb_sync(dpy);
    return ret;
}

EXPORT int32_t my_XFreeColormap(my_XDisplay_t* dpy, void* v2);
EXPORT int32_t my_XFreeColormap(my_XDisplay_t* dpy, void* v2)
{
    int32_t ret = my->XFreeColormap(dpy, v2);
    latx_dpy_xcb_sync(dpy);
    return ret;
}

EXPORT void* my_XCreateGC(my_XDisplay_t* dpy, void* v2, uintptr_t v3, void* v4);
EXPORT void* my_XCreateGC(my_XDisplay_t* dpy, void* v2, uintptr_t v3, void* v4)
{
    void* ret = my->XCreateGC(dpy, v2, v3, v4);
    latx_dpy_xcb_sync(dpy);
    return ret;
}

EXPORT int32_t my_XChangeGC(my_XDisplay_t* dpy, void* v2, uintptr_t v3, void* v4);
EXPORT int32_t my_XChangeGC(my_XDisplay_t* dpy, void* v2, uintptr_t v3, void* v4)
{
    int32_t ret = my->XChangeGC(dpy, v2, v3, v4);
    latx_dpy_xcb_sync(dpy);
    return ret;
}

EXPORT void* my_XOpenIM(my_XDisplay_t* dpy, void* v2, void* v3, void* v4);
EXPORT void* my_XOpenIM(my_XDisplay_t* dpy, void* v2, void* v3, void* v4)
{
    void* ret = my->XOpenIM(dpy, v2, v3, v4);
    kzt_resource_alloc = dpy->resource_alloc;
    kzt_tbbridge_insert((uintptr_t)kzt_resource_alloc, (ADDR)kzt_resource_allocPre, LFp);
    #define GO(A)   \
    kzt_my_ResourceAlloc_##A = my_ResourceAlloc_##A;                                                  \
    kzt_tbbridge_insert((uintptr_t)kzt_my_ResourceAlloc_##A, (ADDR)kzt_my_ResourceAllocPre_##A, LFp);
    GO(0)
    GO(1)
    #undef GO

    bridge_t* system = my_context->system;

    #define GO(A, W)\
    if(dpy->A)      \
        if(!CheckBridged(system, dpy->A)) \
            AddAutomaticBridge(system, W, dpy->A, 0); \

    GO(resource_alloc, LFp)
    #undef GO

    return ret;
}

#define CUSTOM_INIT                 \
    getMy(lib);                     \
    setNeededLibs(lib, 1,           \
        "libXcursor.so.1");
#define CUSTOM_FINI \
    freeMy();


#include "wrappedlib_init.h"
