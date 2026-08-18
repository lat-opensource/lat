/*
 * FriBidi public and compatibility ABI declarations.
 *
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#if !(defined(GO) && defined(GOM) && defined(GO2) && defined(DATA))
#error Meh...
#endif

GO(fribidi_cap_rtl_to_unicode, iFpip)
GO(fribidi_char_set_desc, pFi)
GO(fribidi_char_set_desc_cap_rtl, pFv)
GO(fribidi_char_set_name, pFi)
GO(fribidi_char_set_title, pFi)
GO(fribidi_charset_to_unicode, iFipip)
GO(fribidi_cp1255_to_unicode_c, uFc)
GO(fribidi_cp1256_to_unicode_c, uFc)
GO(fribidi_debug_status, iFv)
GO(fribidi_get_bidi_type, uFu)
GO(fribidi_get_bidi_type_name, pFu)
GO(fribidi_get_bidi_types, vFpip)
GO(fribidi_get_bracket, uFu)
GO(fribidi_get_bracket_types, vFpipp)
GO(fribidi_get_joining_type, CFu)
GO(fribidi_get_joining_type_name, pFC)
GO(fribidi_get_joining_types, vFpip)
GO(fribidi_get_mirror_char, iFup)
GO(fribidi_get_par_direction, uFpi)
GO(fribidi_get_par_embedding_levels, cFpipp)
GO(fribidi_get_par_embedding_levels_ex, cFppipp)
GO(fribidi_get_type, uFu)
GO(fribidi_get_type_internal, uFu)
GO(fribidi_iso8859_6_to_unicode_c, uFc)
GO(fribidi_iso8859_8_to_unicode_c, uFc)
GO(fribidi_join_arabic, vFpipp)
GO(fribidi_log2vis, cFpippppp)
GO(fribidi_log2vis_get_embedding_levels, cFpipp)
GO(fribidi_mirroring_status, iFv)
GO(fribidi_parse_charset, iFp)
GO(fribidi_remove_bidi_marks, iFpippp)
GO(fribidi_reorder_line, cFupiiuppp)
GO(fribidi_reorder_nsm_status, iFv)
GO(fribidi_set_debug, iFi)
GO(fribidi_set_mirroring, iFi)
GO(fribidi_set_reorder_nsm, iFi)
GO(fribidi_shape, vFupipp)
GO(fribidi_shape_arabic, vFupipp)
GO(fribidi_shape_mirroring, vFpip)
GO(fribidi_unicode_to_cap_rtl, iFpip)
GO(fribidi_unicode_to_charset, iFipip)
GO(fribidi_unicode_to_cp1255_c, cFu)
GO(fribidi_unicode_to_cp1256_c, cFu)
GO(fribidi_unicode_to_iso8859_6_c, cFu)
GO(fribidi_unicode_to_iso8859_8_c, cFu)
GO(fribidi_unicode_to_utf8, iFpip)
GO(fribidi_utf8_to_unicode, iFpip)

DATAM(fribidi_unicode_version, sizeof(void *))
DATAM(fribidi_version_info, sizeof(void *))
