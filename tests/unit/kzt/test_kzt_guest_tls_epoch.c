/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "kzt-guest-tls-epoch.h"

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__,        \
                    __LINE__, #condition);                                   \
            exit(1);                                                         \
        }                                                                    \
    } while (0)

static void test_only_stable_matching_epoch_can_skip_refresh(void)
{
    CHECK(kzt_guest_tls_epoch_can_reuse(
        4, 4, 1, 0, 7, 7));
    CHECK(!kzt_guest_tls_epoch_can_reuse(
        3, 3, 1, 0, 7, 7));
    CHECK(!kzt_guest_tls_epoch_can_reuse(
        4, 6, 1, 0, 7, 7));
    CHECK(!kzt_guest_tls_epoch_can_reuse(
        4, 4, 0, 0, 7, 7));
    CHECK(!kzt_guest_tls_epoch_can_reuse(
        4, 4, 1, 8, 7, 7));
    CHECK(!kzt_guest_tls_epoch_can_reuse(
        4, 4, 1, 0, 6, 7));
}

static void test_loader_events_publish_only_complete_epochs(void)
{
    CHECK(kzt_guest_tls_epoch_begin_change(0) == 0);
    CHECK(kzt_guest_tls_epoch_begin_change(2) == 3);
    CHECK(kzt_guest_tls_epoch_begin_change(3) == 3);
    CHECK(kzt_guest_tls_epoch_publish_stable(3) == 4);
    CHECK(kzt_guest_tls_epoch_publish_stable(4) == 4);
    CHECK(kzt_guest_tls_epoch_begin_change(UINT32_MAX - 1) == UINT32_MAX);
    CHECK(kzt_guest_tls_epoch_publish_stable(UINT32_MAX) == UINT32_MAX);
}

static void test_fork_requires_revalidation_before_fast_path(void)
{
    CHECK(kzt_guest_tls_epoch_after_fork(0) == 0);
    CHECK(kzt_guest_tls_epoch_after_fork(3) == 1);
    CHECK(kzt_guest_tls_epoch_after_fork(4) == 1);
}

int main(void)
{
    test_only_stable_matching_epoch_can_skip_refresh();
    test_loader_events_publish_only_complete_epochs();
    test_fork_requires_revalidation_before_fast_path();
    puts("kzt Guest TLS epoch tests: PASS");
    return 0;
}
