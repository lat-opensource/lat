/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdint.h>

static int plugin_anchor;
static int constructor_error;

__thread int ie_value
    __attribute__((tls_model("initial-exec"))) = 0x1357;
/* Force each callback to perform the TLS load instead of folding the value. */
__thread void *volatile ie_pointer
    __attribute__((tls_model("initial-exec"))) = &plugin_anchor;
static __thread int ie_bss
    __attribute__((tls_model("initial-exec")));
/* Keep the aligned TLS object and its end-byte accesses observable. */
__thread unsigned char volatile ie_aligned[64]
    __attribute__((aligned(64), tls_model("initial-exec")));
static __thread int ie_calls
    __attribute__((tls_model("initial-exec")));

static void __attribute__((constructor)) check_initial_exec_constructor(void)
{
    constructor_error = (ie_value != 0x1357) |
                        ((ie_pointer != &plugin_anchor) << 1) |
                        ((ie_bss != 0) << 2) |
                        ((((uintptr_t)ie_aligned & 63) != 0) << 3);
    ie_value = 0x2468;
}

int kzt_host_thread_tls_ie_plugin_check(void)
{
    if (constructor_error) {
        return 100 + constructor_error;
    }
    if (ie_pointer != &plugin_anchor) {
        return 91;
    }
    if (ie_value != 0x1357 && ie_value != 0x2468) {
        return 124;
    }
    if (((uintptr_t)ie_aligned & 63) != 0) {
        return 125;
    }
    if (!ie_calls) {
        if (ie_bss != 0) {
            return 92;
        }
        ie_bss = 0x369c;
    } else if (ie_bss != 0x369c) {
        return 93;
    }
    ie_aligned[0] = (unsigned char)ie_calls;
    ie_aligned[sizeof(ie_aligned) - 1] =
        (unsigned char)(ie_calls + 1);
    ++ie_calls;
    return 0;
}
