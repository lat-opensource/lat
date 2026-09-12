/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>

static int plugin_b_anchor;
static __thread long plugin_b_values[3] = { 301, 302, 303 };
static __thread void *plugin_b_pointer = &plugin_b_anchor;
/* Force each callback to touch both ends of the replacement TLS block. */
static __thread unsigned char volatile
    plugin_b_lifetime_probe[2 * 1024 * 1024];
static __thread int plugin_b_calls;

int kzt_host_thread_tls_plugin_check(void)
{
    if (plugin_b_values[0] != 301 || plugin_b_values[2] != 303 ||
        plugin_b_pointer != &plugin_b_anchor ||
        plugin_b_lifetime_probe[0] != 0 ||
        plugin_b_lifetime_probe[
            sizeof(plugin_b_lifetime_probe) - 1] != 0 ||
        plugin_b_calls < 0 || plugin_b_calls > 3) {
        return 111;
    }
    ++plugin_b_calls;
    return 0;
}
