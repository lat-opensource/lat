/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef __wrappedgraphite2TYPES_H_
#define __wrappedgraphite2TYPES_H_

#ifndef LIBNAME
#error You should only #include this file inside a wrapped*.c file
#endif
#ifndef ADDED_FUNCTIONS
#define ADDED_FUNCTIONS()
#endif

typedef uint8_t (*CFpi_t)(void *, int32_t);
typedef void (*vFp_t)(void *);
typedef void *(*pFppu_t)(void *, void *, uint32_t);
typedef void *(*pFppuu_t)(void *, void *, uint32_t, uint32_t);
typedef void *(*pFfppp_t)(float, void *, void *, void *);

#define SUPER() ADDED_FUNCTIONS() \
    GO(graphite_start_logging, CFpi_t) \
    GO(gr_face_destroy, vFp_t) \
    GO(gr_font_destroy, vFp_t) \
    GO(gr_make_face, pFppu_t) \
    GO(gr_make_face_with_ops, pFppu_t) \
    GO(gr_make_face_with_seg_cache, pFppuu_t) \
    GO(gr_make_face_with_seg_cache_and_ops, pFppuu_t) \
    GO(gr_make_font_with_advance_fn, pFfppp_t) \
    GO(gr_make_font_with_ops, pFfppp_t)

#endif /* __wrappedgraphite2TYPES_H_ */
