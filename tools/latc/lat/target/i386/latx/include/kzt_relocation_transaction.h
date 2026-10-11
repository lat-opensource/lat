/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef KZT_RELOCATION_TRANSACTION_H
#define KZT_RELOCATION_TRANSACTION_H

#include <stddef.h>
#include <stdint.h>

typedef int (*kzt_relocation_validate_fn)(uintptr_t *slot, void *opaque);

typedef struct kzt_relocation_write {
    uintptr_t *slot;
    uintptr_t value;
} kzt_relocation_write_t;

typedef struct kzt_relocation_transaction {
    kzt_relocation_write_t *writes;
    size_t count;
    size_t capacity;
    kzt_relocation_validate_fn validate;
    void *validate_opaque;
} kzt_relocation_transaction_t;

void kzt_relocation_transaction_init(
    kzt_relocation_transaction_t *transaction,
    kzt_relocation_write_t *writes,
    size_t capacity,
    kzt_relocation_validate_fn validate,
    void *validate_opaque);

int kzt_relocation_transaction_add(
    kzt_relocation_transaction_t *transaction,
    uintptr_t *slot,
    uintptr_t value);

int kzt_relocation_transaction_commit(
    const kzt_relocation_transaction_t *transaction);

#endif
