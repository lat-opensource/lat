/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

#include "x11-xcb-wrapper-abi-values.h"

#define PTR_VALUE(value) ((void *)(uintptr_t)(value))

extern void *_Xglobal_lock;
extern void (*_XLockMutex_fn)(void *);
extern void (*_XUnlockMutex_fn)(void *);

static Display *callback_display;
static int callback_count;
static int callback_mismatch;

static int after_function(Display *display)
{
    callback_count++;
    if (display != callback_display) {
        callback_mismatch = 1;
    }
    return 0;
}

static int check(int passed, const char *name, int failure_code)
{
    if (!passed) {
        fprintf(stderr, "FAIL:%s\n", name);
        return failure_code;
    }
    printf("PASS:%s\n", name);
    return 0;
}

int main(void)
{
    Display *display;
    xcb_connection_t *connection;
    xcb_shm_segment_info_t shminfo;
    xcb_void_cookie_t cookie;
    xcb_image_t *image;
    xcb_pixmap_t pixmap;
    XImage *xlib_image;
    int destroy_result;
    int screen_number;
    int result;
    __GLXextFuncPtr get_string;

    result = check(XInitThreads() != 0 && _Xglobal_lock != NULL &&
                   _XLockMutex_fn != NULL && _XUnlockMutex_fn != NULL,
                   "XInitThreads-mutex-callbacks", 9);
    if (result) {
        return result;
    }
    _XLockMutex_fn(_Xglobal_lock);
    _XUnlockMutex_fn(_Xglobal_lock);

    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "FAIL:XOpenDisplay\n");
        return 10;
    }
    callback_display = display;
    XSetAfterFunction(display, after_function);
    XNoOp(display);
    XFlush(display);
    result = check(callback_count > 0 && !callback_mismatch,
                   "XSynchronizeProc-Display", 11);
    if (result) {
        return result;
    }

    xlib_image = XCreateImage(
        display, DefaultVisual(display, DefaultScreen(display)),
        DefaultDepth(display, DefaultScreen(display)), ZPixmap, 0,
        calloc(1, 4), 1, 1, 32, 4);
    result = check(xlib_image != NULL && xlib_image->f.destroy_image != NULL,
                   "XImage-destroy-callback", 12);
    if (result) {
        return result;
    }
    destroy_result = xlib_image->f.destroy_image(xlib_image);
    result = check(destroy_result != 0, "XImage-destroy-guest-data", 13);
    if (result) {
        return result;
    }
    get_string = glXGetProcAddress((const GLubyte *)"glGetString");
    result = check(get_string != NULL,
                   "glXGetProcAddress-GL-fallback", 14);
    if (result) {
        return result;
    }
    result = check(((const GLubyte *(*)(GLenum))get_string)(GL_VERSION) != NULL,
                   "glXGetProcAddress-bridged-call", 15);
    if (result) {
        return result;
    }

    connection = xcb_connect(NULL, &screen_number);
    if (!connection || xcb_connection_has_error(connection)) {
        fprintf(stderr, "FAIL:xcb_connect\n");
        return 12;
    }

    pixmap = xcb_create_pixmap_from_bitmap_data(
        connection, ABI_DRAWABLE, PTR_VALUE(ABI_DATA_PTR), ABI_WIDTH32,
        ABI_HEIGHT32, ABI_DEPTH32, ABI_FG, ABI_BG,
        PTR_VALUE(ABI_GC_PTR));
    result = check(pixmap == ABI_PIXMAP,
                   "xcb_create_pixmap_from_bitmap_data-arguments-return", 20);
    if (result) {
        return result;
    }

    image = xcb_image_create(
        ABI_WIDTH16, ABI_HEIGHT16, (xcb_image_format_t)ABI_FORMAT, ABI_XPAD,
        ABI_DEPTH8, ABI_BPP, ABI_UNIT, (xcb_image_order_t)ABI_BYTE_ORDER,
        (xcb_image_order_t)ABI_BIT_ORDER, PTR_VALUE(ABI_BASE_PTR), ABI_BYTES,
        PTR_VALUE(ABI_DATA_PTR));
    result = check(image == PTR_VALUE(ABI_IMAGE_PTR),
                   "xcb_image_create-arguments", 21);
    if (result) {
        return result;
    }

    image = xcb_image_create_native(
        connection, ABI_WIDTH16, ABI_HEIGHT16,
        (xcb_image_format_t)ABI_FORMAT, ABI_DEPTH8,
        PTR_VALUE(ABI_BASE_PTR), ABI_BYTES, PTR_VALUE(ABI_DATA_PTR));
    result = check(image == PTR_VALUE(ABI_IMAGE_PTR),
                   "xcb_image_create_native-arguments", 22);
    if (result) {
        return result;
    }

    image = xcb_image_get(
        connection, ABI_DRAWABLE, ABI_GET_X, ABI_GET_Y, ABI_GET_WIDTH,
        ABI_GET_HEIGHT, ABI_PLANE_MASK, (xcb_image_format_t)ABI_FORMAT);
    result = check(image == PTR_VALUE(ABI_IMAGE_PTR),
                   "xcb_image_get-arguments", 23);
    if (result) {
        return result;
    }

    image = xcb_image_native(connection, PTR_VALUE(ABI_IMAGE_PTR),
                             ABI_CONVERT);
    result = check(image == PTR_VALUE(ABI_IMAGE_PTR),
                   "xcb_image_native-connection-arguments", 24);
    if (result) {
        return result;
    }

    cookie = xcb_image_put(
        connection, ABI_DRAWABLE, ABI_GCONTEXT, PTR_VALUE(ABI_IMAGE_PTR),
        ABI_PUT_X, ABI_PUT_Y, ABI_LEFT_PAD);
    result = check(cookie.sequence == ABI_COOKIE_SEQUENCE,
                   "xcb_image_put-xcb_void_cookie_t-return", 25);
    if (result) {
        return result;
    }

    shminfo.shmseg = ABI_SHMSEG;
    shminfo.shmid = ABI_SHMID;
    shminfo.shmaddr = PTR_VALUE(ABI_SHM_PTR);
    image = xcb_image_shm_put(
        connection, ABI_DRAWABLE, ABI_GCONTEXT, PTR_VALUE(ABI_IMAGE_PTR),
        shminfo, ABI_SRC_X, ABI_SRC_Y, ABI_DEST_X, ABI_DEST_Y,
        ABI_SRC_WIDTH, ABI_SRC_HEIGHT, ABI_SEND_EVENT);
    result = check(image == PTR_VALUE(ABI_IMAGE_PTR),
                   "xcb_image_shm_put-struct-by-value", 26);
    if (result) {
        return result;
    }

    xcb_disconnect(connection);
    XCloseDisplay(display);
    puts("PASS:x11-xcb-wrapper-abi");
    return 0;
}
