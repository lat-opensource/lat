/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdint.h>

#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

xcb_pixmap_t xcb_create_pixmap_from_bitmap_data(
    xcb_connection_t *connection, xcb_drawable_t drawable, uint8_t *data,
    uint32_t width, uint32_t height, uint32_t depth, uint32_t foreground,
    uint32_t background, xcb_gcontext_t *gcontext)
{
    return XCB_NONE;
}

xcb_image_t *xcb_image_create(
    uint16_t width, uint16_t height, xcb_image_format_t format, uint8_t xpad,
    uint8_t depth, uint8_t bpp, uint8_t unit, xcb_image_order_t byte_order,
    xcb_image_order_t bit_order, void *base, uint32_t bytes, uint8_t *data)
{
    return NULL;
}

xcb_image_t *xcb_image_create_native(
    xcb_connection_t *connection, uint16_t width, uint16_t height,
    xcb_image_format_t format, uint8_t depth, void *base, uint32_t bytes,
    uint8_t *data)
{
    return NULL;
}

xcb_image_t *xcb_image_get(
    xcb_connection_t *connection, xcb_drawable_t drawable, int16_t x,
    int16_t y, uint16_t width, uint16_t height, uint32_t plane_mask,
    xcb_image_format_t format)
{
    return NULL;
}

xcb_image_t *xcb_image_native(xcb_connection_t *connection,
                              xcb_image_t *image, int convert)
{
    return NULL;
}

xcb_void_cookie_t xcb_image_put(
    xcb_connection_t *connection, xcb_drawable_t drawable,
    xcb_gcontext_t gcontext, xcb_image_t *image, int16_t x, int16_t y,
    uint8_t left_pad)
{
    xcb_void_cookie_t cookie = {0};

    return cookie;
}

xcb_image_t *xcb_image_shm_put(
    xcb_connection_t *connection, xcb_drawable_t drawable,
    xcb_gcontext_t gcontext, xcb_image_t *image,
    xcb_shm_segment_info_t shminfo, int16_t src_x, int16_t src_y,
    int16_t dest_x, int16_t dest_y, uint16_t src_width,
    uint16_t src_height, uint8_t send_event)
{
    return NULL;
}
