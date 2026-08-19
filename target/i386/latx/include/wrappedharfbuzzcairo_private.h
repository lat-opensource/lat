/*
 * HarfBuzz ABI declarations derived from public headers and
 * cross-checked against Box64's MIT-licensed wrapper declarations.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: MIT
 */

#if !(defined(GO) && defined(GOM) && defined(GO2) && defined(DATA))
#error Meh...
#endif

GOM(hb_cairo_font_face_create_for_face, pFEp)
GOM(hb_cairo_font_face_create_for_font, pFEp)
GOM(hb_cairo_font_face_get_face, pFEp)
GOM(hb_cairo_font_face_get_font, pFEp)
GOM(hb_cairo_font_face_get_scale_factor, uFEp)
GOM(hb_cairo_font_face_set_font_init_func, vFEpppp)
GOM(hb_cairo_font_face_set_scale_factor, vFEpu)
GOM(hb_cairo_glyphs_from_buffer, vFEpiddddpippppp)
GOM(hb_cairo_scaled_font_get_font, pFEp)
