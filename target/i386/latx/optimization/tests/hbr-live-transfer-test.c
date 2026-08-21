/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <assert.h>

#include "hbr-semantic.h"

#define XMM(n) (1U << (n))
#define XMM_ALL 0xffffU

int main(void)
{
    assert(shbr_live_before(XMM(0), XMM(0), XMM(1) | XMM(2), 0) ==
           (XMM(1) | XMM(2)));
    assert(shbr_live_before(XMM(3), XMM(0), XMM(1) | XMM(2), 0) == XMM(3));
    assert(shbr_live_before(XMM(0), 0, 0, XMM(4)) ==
           (XMM(0) | XMM(4)));
    assert(shbr_live_before(0, XMM(0), XMM(1), XMM(5)) == XMM(5));
    assert(shbr_live_before(XMM(0), XMM(0), XMM(0) | XMM(1), 0) ==
           (XMM(0) | XMM(1)));
    assert(shbr_live_before(XMM(0) | XMM(1), 0, 0,
           XMM(0) | XMM(1)) == (XMM(0) | XMM(1)));
    assert(shbr_live_before(XMM(0) | XMM(1), XMM(0) | XMM(1), 0, 0) == 0);
    assert(shbr_live_before(XMM_ALL, XMM_ALL, 0, XMM_ALL) == XMM_ALL);

    assert(shbr_dest_is_dead(0, XMM(0), 0));
    assert(!shbr_dest_is_dead(XMM(0), XMM(0), 0));
    assert(!shbr_dest_is_dead(0, XMM(0), XMM(0)));
    return 0;
}
