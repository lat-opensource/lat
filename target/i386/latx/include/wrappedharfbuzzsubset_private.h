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

GO(hb_subset_input_create_or_fail, pFv)
GO(hb_subset_input_destroy, vFp)
GO(hb_subset_input_get_flags, uFp)
GOM(hb_subset_input_get_user_data, pFEpp)
GO(hb_subset_input_glyph_set, pFp)
GO(hb_subset_input_keep_everything, vFp)
GO(hb_subset_input_old_to_new_glyph_mapping, pFp)
GO(hb_subset_input_pin_all_axes_to_default, iFpp)
GO(hb_subset_input_pin_axis_location, iFppuf)
GO(hb_subset_input_pin_axis_to_default, iFppu)
GO(hb_subset_input_reference, pFp)
GO(hb_subset_input_set, pFpu)
GO(hb_subset_input_set_flags, vFpu)
GOM(hb_subset_input_set_user_data, iFEppppi)
GO(hb_subset_input_unicode_set, pFp)
GO(hb_subset_or_fail, pFpp)
GO(hb_subset_plan_create_or_fail, pFpp)
GO(hb_subset_plan_destroy, vFp)
GO(hb_subset_plan_execute_or_fail, pFp)
GOM(hb_subset_plan_get_user_data, pFEpp)
GO(hb_subset_plan_new_to_old_glyph_mapping, pFp)
GO(hb_subset_plan_old_to_new_glyph_mapping, pFp)
GO(hb_subset_plan_reference, pFp)
GOM(hb_subset_plan_set_user_data, iFEppppi)
GO(hb_subset_plan_unicode_to_old_glyph_mapping, pFp)
GO(hb_subset_preprocess, pFp)
