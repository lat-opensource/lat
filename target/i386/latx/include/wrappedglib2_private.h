/*
 * GLib ABI declarations derived from public headers and cross-checked
 * against Box64's MIT-licensed wrapper declarations.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: MIT
 */

#if !(defined(GO) && defined(GOM) && defined(GO2) && defined(DATA))
#error Meh...
#endif

GO(g_ascii_digit_value, iFc)
GO(g_ascii_dtostr, pFpid)
GO(g_ascii_formatd, pFpipd)
GO(g_ascii_strcasecmp, iFpp)
GO(g_ascii_strncasecmp, iFppL)
GO(g_ascii_strtod, dFpp)
GO(g_ascii_strtoll, lFppu)
GO(g_ascii_strtoull, LFppu)
GO(g_ascii_tolower, cFc)
GO(g_ascii_toupper, cFc)
GO(g_ascii_xdigit_value, iFc)
GO(g_bit_nth_lsf, iFLi)
GO(g_bit_nth_msf, iFLi)
GO(g_bit_storage, uFL)
GO(g_direct_equal, iFpp)
GO(g_direct_hash, uFp)
GO(g_double_equal, iFpp)
GO(g_int64_equal, iFpp)
GO(g_int_equal, iFpp)
GO(g_int_hash, uFp)
GO(g_str_equal, iFpp)
GO(g_str_hash, uFp)
GO(g_str_has_prefix, iFpp)
GO(g_str_has_suffix, iFpp)
GO(g_str_is_ascii, iFp)
GO(g_strcanon, pFppc)
GO(g_strchomp, pFp)
GO(g_strchug, pFp)
GO(g_strcmp0, iFpp)
GO(g_strdelimit, pFppc)
GO(g_strlcat, LFppL)
GO(g_strlcpy, LFppL)
GO(g_strreverse, pFp)
GO(g_strrstr, pFpp)
GO(g_strrstr_len, pFplp)
GO(g_strstr_len, pFplp)
GO(g_strv_contains, iFpp)
GO(g_strv_equal, iFpp)
GO(g_strv_length, uFp)
GO(g_unichar_to_utf8, iFup)
GO(g_unichar_validate, iFu)
GO(g_utf8_find_next_char, pFpp)
GO(g_utf8_find_prev_char, pFpp)
GO(g_utf8_get_char, uFp)
GO(g_utf8_get_char_validated, uFpl)
GO(g_utf8_offset_to_pointer, pFpl)
GO(g_utf8_pointer_to_offset, lFpp)
GO(g_utf8_prev_char, pFp)
GO(g_utf8_strchr, pFplu)
GO(g_utf8_strlen, lFpl)
GO(g_utf8_strncpy, pFppL)
GO(g_utf8_strrchr, pFplu)
GO(g_utf8_validate, iFplp)
GO(g_utf8_validate_len, iFpLp)
