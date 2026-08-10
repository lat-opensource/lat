/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "kzt_relocation_transaction.h"

void kzt_relocation_transaction_init(
    kzt_relocation_transaction_t *transaction,
    kzt_relocation_write_t *writes,
    size_t capacity,
    kzt_relocation_validate_fn validate,
    void *validate_opaque)
{
    transaction->writes = writes;
    transaction->count = 0;
    transaction->capacity = capacity;
    transaction->validate = validate;
    transaction->validate_opaque = validate_opaque;
}

int kzt_relocation_transaction_add(
    kzt_relocation_transaction_t *transaction,
    uintptr_t *slot,
    uintptr_t value)
{
    kzt_relocation_write_t *write;

    if (!transaction || !slot ||
        transaction->count >= transaction->capacity) {
        return -1;
    }
    write = &transaction->writes[transaction->count++];
    write->slot = slot;
    write->value = value;
    return 0;
}

int kzt_relocation_transaction_commit(
    const kzt_relocation_transaction_t *transaction)
{
    size_t index;

    if (!transaction || !transaction->validate ||
        (transaction->count && !transaction->writes)) {
        return -1;
    }
    for (index = 0; index < transaction->count; ++index) {
        if (transaction->validate(transaction->writes[index].slot,
                                  transaction->validate_opaque) != 0) {
            return -1;
        }
    }
    for (index = 0; index < transaction->count; ++index) {
        *transaction->writes[index].slot = transaction->writes[index].value;
    }
    return 0;
}
