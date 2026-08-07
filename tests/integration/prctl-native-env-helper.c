/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const char *keep = getenv("KEEP");

    if (getenv("_LATX_GUEST_MDWE") || getenv("_LATX_GUEST_TSC")) {
        return 1;
    }
    return !keep || strcmp(keep, "1") != 0;
}
