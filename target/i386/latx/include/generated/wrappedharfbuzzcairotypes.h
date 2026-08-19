/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef __wrappedharfbuzzcairoTYPES_H_
#define __wrappedharfbuzzcairoTYPES_H_

#ifndef LIBNAME
#error You should only #include this file inside a wrapped*.c file
#endif
#ifndef ADDED_FUNCTIONS
#define ADDED_FUNCTIONS()
#endif

typedef void* (*pFp_t)(void*);
typedef uint32_t (*uFp_t)(void*);
typedef void (*vFpiddddpippppp_t)(void*, int32_t, double, double, double, double, void*, int32_t, void*, void*, void*, void*, void*);
typedef void (*vFpppp_t)(void*, void*, void*, void*);
typedef void (*vFpu_t)(void*, uint32_t);

#define SUPER() ADDED_FUNCTIONS() \
    GO(hb_cairo_font_face_create_for_face, pFp_t) \
    GO(hb_cairo_font_face_create_for_font, pFp_t) \
    GO(hb_cairo_font_face_get_face, pFp_t) \
    GO(hb_cairo_font_face_get_font, pFp_t) \
    GO(hb_cairo_font_face_get_scale_factor, uFp_t) \
    GO(hb_cairo_font_face_set_font_init_func, vFpppp_t) \
    GO(hb_cairo_font_face_set_scale_factor, vFpu_t) \
    GO(hb_cairo_glyphs_from_buffer, vFpiddddpippppp_t) \
    GO(hb_cairo_scaled_font_get_font, pFp_t)

#endif /* __wrappedharfbuzzcairoTYPES_H_ */
