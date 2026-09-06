/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include "kzt-runtime.h"

int option_kzt;
int option_kzt_guest_tls;
uint32_t kzt_effective_groups;

static void check(int expected)
{
    if (latx_kzt_guest_tls_enabled() != expected) {
        fprintf(stderr, "TLS policy mismatch: kzt=%d tls=%d groups=%u\n",
                option_kzt, option_kzt_guest_tls, kzt_effective_groups);
        exit(1);
    }
}

int main(void)
{
    check(0);
    option_kzt = 1;
    kzt_effective_groups = 1;
    check(0);
    option_kzt_guest_tls = 1;
    check(1);
    option_kzt = 0;
    check(0);
    option_kzt = 2;
    check(1);
    kzt_effective_groups = 0;
    check(0);
    option_kzt_guest_tls = 0;
    kzt_effective_groups = 1;
    check(0);
    puts("kzt Guest TLS opt-in policy: PASS");
    return 0;
}
