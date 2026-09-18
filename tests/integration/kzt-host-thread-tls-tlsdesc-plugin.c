/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdint.h>

/* Force the pure-.tbss TLSDesc object to survive optimization. */
static __thread unsigned char volatile tlsdesc_page[4096]
    __attribute__((aligned(4096)));
static __thread int tlsdesc_calls;

int kzt_host_thread_tls_tlsdesc_check(void)
{
    if (((uintptr_t)tlsdesc_page & 4095) != 0 ||
        tlsdesc_page[0] != 0 ||
        tlsdesc_page[sizeof(tlsdesc_page) - 1] != 0 ||
        tlsdesc_calls < 0 || tlsdesc_calls > 5) {
        return 117;
    }
    ++tlsdesc_calls;
    return 0;
}
