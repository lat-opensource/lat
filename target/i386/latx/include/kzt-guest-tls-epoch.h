/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef LATX_KZT_GUEST_TLS_EPOCH_H
#define LATX_KZT_GUEST_TLS_EPOCH_H

#include <stdint.h>

static inline uint32_t kzt_guest_tls_epoch_begin_change(
    uint32_t sequence)
{
    if (!sequence || (sequence & 1)) {
        return sequence;
    }
    return sequence + 1;
}

static inline uint32_t kzt_guest_tls_epoch_publish_stable(
    uint32_t sequence)
{
    if (!(sequence & 1) || sequence == UINT32_MAX) {
        return sequence;
    }
    return sequence + 1;
}

static inline uint32_t kzt_guest_tls_epoch_after_fork(
    uint32_t sequence)
{
    return sequence ? 1 : 0;
}

static inline int kzt_guest_tls_epoch_can_reuse(
    uint32_t sequence_before,
    uint32_t sequence_after,
    int loader_consistent,
    uintptr_t pending_generation,
    uintptr_t state_generation,
    uintptr_t process_generation)
{
    return sequence_before != 0 &&
           !(sequence_before & 1) &&
           sequence_before == sequence_after &&
           loader_consistent &&
           pending_generation == 0 &&
           state_generation == process_generation;
}

#endif
