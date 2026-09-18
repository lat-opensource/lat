/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef KZT_CXX_TLS_LIFETIME_SHARED_H
#define KZT_CXX_TLS_LIFETIME_SHARED_H

class ThreadGuard {
public:
    ThreadGuard();
    ~ThreadGuard();

    int value;
};

extern "C" void kzt_cxx_tls_helper_set_destructor_counter(int *counter);

#endif
