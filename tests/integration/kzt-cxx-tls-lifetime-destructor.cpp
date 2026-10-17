// SPDX-License-Identifier: GPL-2.0-or-later

#include "kzt-cxx-tls-lifetime-shared.h"

static int *external_destructors;

ThreadGuard::~ThreadGuard()
{
    if (external_destructors) {
        __atomic_add_fetch(external_destructors, 1, __ATOMIC_RELEASE);
    }
}

extern "C" void kzt_cxx_tls_helper_set_destructor_counter(int *counter)
{
    external_destructors = counter;
}
