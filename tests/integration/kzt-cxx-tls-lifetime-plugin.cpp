// SPDX-License-Identifier: GPL-2.0-or-later

#include "kzt-cxx-tls-lifetime-shared.h"

static int *external_constructors;

ThreadGuard::ThreadGuard() : value(0x1234)
{
    if (external_constructors) {
        __atomic_add_fetch(
            external_constructors, 1, __ATOMIC_RELEASE);
    }
}

static thread_local ThreadGuard guard;

extern "C" void kzt_cxx_tls_lifetime_set_counters(
    int *constructors, int *destructors)
{
    external_constructors = constructors;
    kzt_cxx_tls_helper_set_destructor_counter(destructors);
}

extern "C" int kzt_cxx_tls_lifetime_check(void)
{
    return guard.value == 0x1234 ? 0 : 1;
}
