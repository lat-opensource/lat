/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

#include "x11-xcb-wrapper-abi-values.h"

#define PTR_VALUE(value) ((void *)(uintptr_t)(value))

static int probe_result(const char *name, int passed)
{
    fprintf(stderr, "WRAPPER_ABI_PROBE:%s:%s\n", name,
            passed ? "PASS" : "FAIL");
    return passed;
}

xcb_pixmap_t xcb_create_pixmap_from_bitmap_data(
    xcb_connection_t *connection, xcb_drawable_t drawable, uint8_t *data,
    uint32_t width, uint32_t height, uint32_t depth, uint32_t foreground,
    uint32_t background, xcb_gcontext_t *gcontext)
{
    int passed = connection != NULL && drawable == ABI_DRAWABLE &&
                 data == PTR_VALUE(ABI_DATA_PTR) && width == ABI_WIDTH32 &&
                 height == ABI_HEIGHT32 && depth == ABI_DEPTH32 &&
                 foreground == ABI_FG && background == ABI_BG &&
                 gcontext == PTR_VALUE(ABI_GC_PTR);

    return probe_result("xcb_create_pixmap_from_bitmap_data", passed)
               ? ABI_PIXMAP
               : XCB_NONE;
}

xcb_image_t *xcb_image_create(
    uint16_t width, uint16_t height, xcb_image_format_t format, uint8_t xpad,
    uint8_t depth, uint8_t bpp, uint8_t unit, xcb_image_order_t byte_order,
    xcb_image_order_t bit_order, void *base, uint32_t bytes, uint8_t *data)
{
    int passed = width == ABI_WIDTH16 && height == ABI_HEIGHT16 &&
                 (uint32_t)format == ABI_FORMAT && xpad == ABI_XPAD &&
                 depth == ABI_DEPTH8 && bpp == ABI_BPP && unit == ABI_UNIT &&
                 (uint32_t)byte_order == ABI_BYTE_ORDER &&
                 (uint32_t)bit_order == ABI_BIT_ORDER &&
                 base == PTR_VALUE(ABI_BASE_PTR) && bytes == ABI_BYTES &&
                 data == PTR_VALUE(ABI_DATA_PTR);

    return probe_result("xcb_image_create", passed)
               ? PTR_VALUE(ABI_IMAGE_PTR)
               : NULL;
}

xcb_image_t *xcb_image_create_native(
    xcb_connection_t *connection, uint16_t width, uint16_t height,
    xcb_image_format_t format, uint8_t depth, void *base, uint32_t bytes,
    uint8_t *data)
{
    int passed = connection != NULL && width == ABI_WIDTH16 &&
                 height == ABI_HEIGHT16 && (uint32_t)format == ABI_FORMAT &&
                 depth == ABI_DEPTH8 && base == PTR_VALUE(ABI_BASE_PTR) &&
                 bytes == ABI_BYTES && data == PTR_VALUE(ABI_DATA_PTR);

    return probe_result("xcb_image_create_native", passed)
               ? PTR_VALUE(ABI_IMAGE_PTR)
               : NULL;
}

xcb_image_t *xcb_image_get(
    xcb_connection_t *connection, xcb_drawable_t drawable, int16_t x,
    int16_t y, uint16_t width, uint16_t height, uint32_t plane_mask,
    xcb_image_format_t format)
{
    int passed = connection != NULL && drawable == ABI_DRAWABLE &&
                 x == ABI_GET_X && y == ABI_GET_Y &&
                 width == ABI_GET_WIDTH && height == ABI_GET_HEIGHT &&
                 plane_mask == ABI_PLANE_MASK &&
                 (uint32_t)format == ABI_FORMAT;

    return probe_result("xcb_image_get", passed)
               ? PTR_VALUE(ABI_IMAGE_PTR)
               : NULL;
}

xcb_image_t *xcb_image_native(xcb_connection_t *connection,
                              xcb_image_t *image, int convert)
{
    int passed = connection != NULL && image == PTR_VALUE(ABI_IMAGE_PTR) &&
                 convert == ABI_CONVERT;

    return probe_result("xcb_image_native", passed) ? image : NULL;
}

xcb_void_cookie_t xcb_image_put(
    xcb_connection_t *connection, xcb_drawable_t drawable,
    xcb_gcontext_t gcontext, xcb_image_t *image, int16_t x, int16_t y,
    uint8_t left_pad)
{
    xcb_void_cookie_t cookie = {0};
    int passed = connection != NULL && drawable == ABI_DRAWABLE &&
                 gcontext == ABI_GCONTEXT &&
                 image == PTR_VALUE(ABI_IMAGE_PTR) && x == ABI_PUT_X &&
                 y == ABI_PUT_Y && left_pad == ABI_LEFT_PAD;

    if (probe_result("xcb_image_put", passed)) {
        cookie.sequence = ABI_COOKIE_SEQUENCE;
    }
    return cookie;
}

xcb_image_t *xcb_image_shm_put(
    xcb_connection_t *connection, xcb_drawable_t drawable,
    xcb_gcontext_t gcontext, xcb_image_t *image,
    xcb_shm_segment_info_t shminfo, int16_t src_x, int16_t src_y,
    int16_t dest_x, int16_t dest_y, uint16_t src_width,
    uint16_t src_height, uint8_t send_event)
{
    int passed = connection != NULL && drawable == ABI_DRAWABLE &&
                 gcontext == ABI_GCONTEXT &&
                 image == PTR_VALUE(ABI_IMAGE_PTR) &&
                 shminfo.shmseg == ABI_SHMSEG && shminfo.shmid == ABI_SHMID &&
                 shminfo.shmaddr == PTR_VALUE(ABI_SHM_PTR) &&
                 src_x == ABI_SRC_X && src_y == ABI_SRC_Y &&
                 dest_x == ABI_DEST_X && dest_y == ABI_DEST_Y &&
                 src_width == ABI_SRC_WIDTH &&
                 src_height == ABI_SRC_HEIGHT &&
                 send_event == ABI_SEND_EVENT;

    return probe_result("xcb_image_shm_put", passed) ? image : NULL;
}
