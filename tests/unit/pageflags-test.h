/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef LATX_PAGEFLAGS_TEST_H
#define LATX_PAGEFLAGS_TEST_H

target_ulong get_first_page(target_ulong start, target_ulong end);
int page_get_flags(target_ulong address);
bool test_flags(target_ulong address, int flags);
bool pageflags_set_clear(target_ulong start, target_ulong last,
                         int set_flags, int clear_flags);
bool page_check_range(target_ulong start, target_ulong len, int flags);

#endif
