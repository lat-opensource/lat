/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "kzt_relocation_transaction.h"

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__,         \
                    __LINE__, #condition);                                   \
            exit(1);                                                         \
        }                                                                    \
    } while (0)

typedef struct validation_state {
    uintptr_t *rejected_slot;
    int lock_held;
    int calls;
} validation_state_t;

static int validate_slot(uintptr_t *slot, void *opaque)
{
    validation_state_t *state = opaque;

    ++state->calls;
    return state->lock_held && slot != state->rejected_slot ? 0 : -1;
}

static void test_commit_updates_every_slot(void)
{
    uintptr_t slots[] = { 1, 2 };
    kzt_relocation_write_t writes[2];
    validation_state_t state = { .lock_held = 1 };
    kzt_relocation_transaction_t transaction;

    kzt_relocation_transaction_init(
        &transaction, writes, 2, validate_slot, &state);
    CHECK(kzt_relocation_transaction_add(&transaction, &slots[0], 11) == 0);
    CHECK(kzt_relocation_transaction_add(&transaction, &slots[1], 22) == 0);
    CHECK(kzt_relocation_transaction_commit(&transaction) == 0);
    CHECK(slots[0] == 11);
    CHECK(slots[1] == 22);
}

static void test_rejected_slot_leaves_every_slot_unchanged(void)
{
    uintptr_t slots[] = { 1, 2 };
    kzt_relocation_write_t writes[2];
    validation_state_t state = {
        .rejected_slot = &slots[1],
        .lock_held = 1,
    };
    kzt_relocation_transaction_t transaction;

    kzt_relocation_transaction_init(
        &transaction, writes, 2, validate_slot, &state);
    CHECK(kzt_relocation_transaction_add(&transaction, &slots[0], 11) == 0);
    CHECK(kzt_relocation_transaction_add(&transaction, &slots[1], 22) == 0);
    CHECK(kzt_relocation_transaction_commit(&transaction) == -1);
    CHECK(slots[0] == 1);
    CHECK(slots[1] == 2);
}

static void test_commit_requires_the_callers_lock(void)
{
    uintptr_t slot = 1;
    kzt_relocation_write_t write;
    validation_state_t state = { 0 };
    kzt_relocation_transaction_t transaction;

    kzt_relocation_transaction_init(
        &transaction, &write, 1, validate_slot, &state);
    CHECK(kzt_relocation_transaction_add(&transaction, &slot, 11) == 0);
    CHECK(kzt_relocation_transaction_commit(&transaction) == -1);
    CHECK(slot == 1);
}

static void test_capacity_failure_does_not_write(void)
{
    uintptr_t slots[] = { 1, 2 };
    kzt_relocation_write_t write;
    validation_state_t state = { .lock_held = 1 };
    kzt_relocation_transaction_t transaction;

    kzt_relocation_transaction_init(
        &transaction, &write, 1, validate_slot, &state);
    CHECK(kzt_relocation_transaction_add(&transaction, &slots[0], 11) == 0);
    CHECK(kzt_relocation_transaction_add(&transaction, &slots[1], 22) == -1);
    CHECK(slots[0] == 1);
    CHECK(slots[1] == 2);
}

int main(void)
{
    test_commit_updates_every_slot();
    test_rejected_slot_leaves_every_slot_unchanged();
    test_commit_requires_the_callers_lock();
    test_capacity_failure_does_not_write();
    puts("kzt relocation transaction tests: PASS");
    return 0;
}
