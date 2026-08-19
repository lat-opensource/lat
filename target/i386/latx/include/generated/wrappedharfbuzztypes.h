/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef __wrappedharfbuzzTYPES_H_
#define __wrappedharfbuzzTYPES_H_

#ifndef LIBNAME
#error You should only #include this file inside a wrapped*.c file
#endif
#ifndef ADDED_FUNCTIONS
#define ADDED_FUNCTIONS()
#endif

typedef int32_t (*iFppppi_t)(void*, void*, void*, void*, int32_t);
typedef void* (*pFp_t)(void*);
typedef void* (*pFpp_t)(void*, void*);
typedef void* (*pFppp_t)(void*, void*, void*);
typedef void* (*pFpuupp_t)(void*, uint32_t, uint32_t, void*, void*);
typedef void* (*pFv_t)(void);
typedef uint32_t (*uFp_t)(void*);
typedef uint32_t (*uFpupp_t)(void*, uint32_t, void*, void*);
typedef void (*vFp_t)(void*);
typedef void (*vFppp_t)(void*, void*, void*);
typedef void (*vFpppffff_t)(void*, void*, void*, float, float, float, float);
typedef void (*vFpppffffff_t)(void*, void*, void*, float, float, float, float, float, float);
typedef void (*vFpppp_t)(void*, void*, void*, void*);

#define SUPER() ADDED_FUNCTIONS() \
    GO(hb_blob_create, pFpuupp_t) \
    GO(hb_blob_create_or_fail, pFpuupp_t) \
    GO(hb_blob_get_empty, pFv_t) \
    GO(hb_blob_get_user_data, pFpp_t) \
    GO(hb_blob_set_user_data, iFppppi_t) \
    GO(hb_buffer_get_user_data, pFpp_t) \
    GO(hb_buffer_set_message_func, vFpppp_t) \
    GO(hb_buffer_set_user_data, iFppppi_t) \
    GO(hb_color_line_get_color_stops, uFpupp_t) \
    GO(hb_color_line_get_extend, uFp_t) \
    GO(hb_draw_funcs_get_user_data, pFpp_t) \
    GO(hb_draw_funcs_set_close_path_func, vFpppp_t) \
    GO(hb_draw_funcs_set_cubic_to_func, vFpppp_t) \
    GO(hb_draw_funcs_set_line_to_func, vFpppp_t) \
    GO(hb_draw_funcs_set_move_to_func, vFpppp_t) \
    GO(hb_draw_funcs_set_quadratic_to_func, vFpppp_t) \
    GO(hb_draw_funcs_set_user_data, iFppppi_t) \
    GO(hb_face_create_for_tables, pFppp_t) \
    GO(hb_face_get_empty, pFv_t) \
    GO(hb_face_get_user_data, pFpp_t) \
    GO(hb_face_set_user_data, iFppppi_t) \
    GO(hb_font_funcs_get_user_data, pFpp_t) \
    GO(hb_font_funcs_set_draw_glyph_func, vFpppp_t) \
    GO(hb_font_funcs_set_font_h_extents_func, vFpppp_t) \
    GO(hb_font_funcs_set_font_v_extents_func, vFpppp_t) \
    GO(hb_font_funcs_set_glyph_contour_point_func, vFpppp_t) \
    GO(hb_font_funcs_set_glyph_extents_func, vFpppp_t) \
    GO(hb_font_funcs_set_glyph_from_name_func, vFpppp_t) \
    GO(hb_font_funcs_set_glyph_func, vFpppp_t) \
    GO(hb_font_funcs_set_glyph_h_advance_func, vFpppp_t) \
    GO(hb_font_funcs_set_glyph_h_advances_func, vFpppp_t) \
    GO(hb_font_funcs_set_glyph_h_kerning_func, vFpppp_t) \
    GO(hb_font_funcs_set_glyph_h_origin_func, vFpppp_t) \
    GO(hb_font_funcs_set_glyph_name_func, vFpppp_t) \
    GO(hb_font_funcs_set_glyph_shape_func, vFpppp_t) \
    GO(hb_font_funcs_set_glyph_v_advance_func, vFpppp_t) \
    GO(hb_font_funcs_set_glyph_v_advances_func, vFpppp_t) \
    GO(hb_font_funcs_set_glyph_v_kerning_func, vFpppp_t) \
    GO(hb_font_funcs_set_glyph_v_origin_func, vFpppp_t) \
    GO(hb_font_funcs_set_nominal_glyph_func, vFpppp_t) \
    GO(hb_font_funcs_set_nominal_glyphs_func, vFpppp_t) \
    GO(hb_font_funcs_set_paint_glyph_func, vFpppp_t) \
    GO(hb_font_funcs_set_user_data, iFppppi_t) \
    GO(hb_font_funcs_set_variation_glyph_func, vFpppp_t) \
    GO(hb_font_get_empty, pFv_t) \
    GO(hb_font_get_user_data, pFpp_t) \
    GO(hb_font_set_funcs, vFpppp_t) \
    GO(hb_font_set_funcs_data, vFppp_t) \
    GO(hb_font_set_user_data, iFppppi_t) \
    GO(hb_ft_face_create, pFpp_t) \
    GO(hb_ft_face_create_referenced, pFp_t) \
    GO(hb_ft_font_create, pFpp_t) \
    GO(hb_ft_font_create_referenced, pFp_t) \
    GO(hb_ft_font_lock_face, pFp_t) \
    GO(hb_ft_font_unlock_face, vFp_t) \
    GO(hb_glib_blob_create, pFp_t) \
    GO(hb_graphite2_face_get_gr_face, pFp_t) \
    GO(hb_graphite2_font_get_gr_font, pFp_t) \
    GO(hb_map_get_user_data, pFpp_t) \
    GO(hb_map_set_user_data, iFppppi_t) \
    GO(hb_paint_funcs_get_user_data, pFpp_t) \
    GO(hb_paint_funcs_set_color_func, vFpppp_t) \
    GO(hb_paint_funcs_set_color_glyph_func, vFpppp_t) \
    GO(hb_paint_funcs_set_custom_palette_color_func, vFpppp_t) \
    GO(hb_paint_funcs_set_image_func, vFpppp_t) \
    GO(hb_paint_funcs_set_linear_gradient_func, vFpppp_t) \
    GO(hb_paint_funcs_set_pop_clip_func, vFpppp_t) \
    GO(hb_paint_funcs_set_pop_group_func, vFpppp_t) \
    GO(hb_paint_funcs_set_pop_transform_func, vFpppp_t) \
    GO(hb_paint_funcs_set_push_clip_glyph_func, vFpppp_t) \
    GO(hb_paint_funcs_set_push_clip_rectangle_func, vFpppp_t) \
    GO(hb_paint_funcs_set_push_group_func, vFpppp_t) \
    GO(hb_paint_funcs_set_push_transform_func, vFpppp_t) \
    GO(hb_paint_funcs_set_radial_gradient_func, vFpppp_t) \
    GO(hb_paint_funcs_set_sweep_gradient_func, vFpppp_t) \
    GO(hb_paint_funcs_set_user_data, iFppppi_t) \
    GO(hb_paint_linear_gradient, vFpppffffff_t) \
    GO(hb_paint_radial_gradient, vFpppffffff_t) \
    GO(hb_paint_sweep_gradient, vFpppffff_t) \
    GO(hb_set_get_user_data, pFpp_t) \
    GO(hb_set_set_user_data, iFppppi_t) \
    GO(hb_shape_plan_get_user_data, pFpp_t) \
    GO(hb_shape_plan_set_user_data, iFppppi_t) \
    GO(hb_unicode_funcs_get_user_data, pFpp_t) \
    GO(hb_unicode_funcs_set_combining_class_func, vFpppp_t) \
    GO(hb_unicode_funcs_set_compose_func, vFpppp_t) \
    GO(hb_unicode_funcs_set_decompose_compatibility_func, vFpppp_t) \
    GO(hb_unicode_funcs_set_decompose_func, vFpppp_t) \
    GO(hb_unicode_funcs_set_eastasian_width_func, vFpppp_t) \
    GO(hb_unicode_funcs_set_general_category_func, vFpppp_t) \
    GO(hb_unicode_funcs_set_mirroring_func, vFpppp_t) \
    GO(hb_unicode_funcs_set_script_func, vFpppp_t) \
    GO(hb_unicode_funcs_set_user_data, iFppppi_t)

#endif /* __wrappedharfbuzzTYPES_H_ */
