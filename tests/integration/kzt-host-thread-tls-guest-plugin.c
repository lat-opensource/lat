/* SPDX-License-Identifier: GPL-2.0-or-later */

static int plugin_anchor;
/* Force each callback to perform the TLS load instead of folding the value. */
__thread void *volatile plugin_tls_pointer = &plugin_anchor;
static __thread int plugin_tls_counter;
static __thread int plugin_constructor_marker;
/* Touch both ends so the lifetime check covers the full dynamic TLS block. */
static __thread unsigned char volatile
    plugin_tls_lifetime_probe[8 * 1024 * 1024];
static int constructor_error;
static int constructor_marker_observed;
static int total_checks;

static void __attribute__((constructor)) check_tls_constructor(void)
{
    constructor_error =
        (plugin_tls_pointer != &plugin_anchor) |
        ((plugin_tls_counter != 0) << 1) |
        ((plugin_tls_lifetime_probe[0] != 0) << 2) |
        ((plugin_tls_lifetime_probe[
              sizeof(plugin_tls_lifetime_probe) - 1] != 0) << 3);
    plugin_constructor_marker = 0x5a5a;
}

int kzt_host_thread_tls_plugin_check(void)
{
    int checks;

    if (constructor_error) {
        return 130 + constructor_error;
    }
    if (plugin_tls_pointer != &plugin_anchor) {
        return 139;
    }
    if (plugin_tls_counter < 0 || plugin_tls_counter > 3) {
        return 49;
    }
    if (plugin_constructor_marker == 0x5a5a) {
        __atomic_store_n(
            &constructor_marker_observed, 1, __ATOMIC_RELEASE);
    }
    checks = __atomic_add_fetch(&total_checks, 1, __ATOMIC_ACQ_REL);
    if (checks >= 4 &&
        !__atomic_load_n(
            &constructor_marker_observed, __ATOMIC_ACQUIRE)) {
        return 50;
    }
    plugin_tls_lifetime_probe[0] = (unsigned char)plugin_tls_counter;
    plugin_tls_lifetime_probe[sizeof(plugin_tls_lifetime_probe) - 1] =
        (unsigned char)plugin_tls_counter;
    ++plugin_tls_counter;
    return 0;
}
