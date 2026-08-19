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

GO(hb_icu_get_unicode_funcs, pFv)
GO(hb_icu_script_from_script, iFu)
GO(hb_icu_script_to_script, uFi)
