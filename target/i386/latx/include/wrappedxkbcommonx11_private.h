/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: MIT
 */

#if !(defined(GO) && defined(GOM) && defined(GO2) && defined(DATA))
#error Meh...
#endif

GO(xkb_x11_get_core_keyboard_device_id, iFb)
GO(xkb_x11_keymap_new_from_device, pFpbiu)
GO(xkb_x11_setup_xkb_extension, iFbWWupppp)
GO(xkb_x11_state_new_from_device, pFpbi)
