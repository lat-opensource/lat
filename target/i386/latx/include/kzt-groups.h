/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_KZT_GROUPS_H
#define LATX_KZT_GROUPS_H

#include <stdbool.h>
#include <stdint.h>

#include "kzt-group-list.h"

typedef enum KztLibraryGroup {
    KZT_GROUP_NONE = 0,
#define KZT_GROUP_ENUM(identifier, name, bit, stability, dependencies) \
    KZT_GROUP_##identifier = 1U << bit,
    KZT_GROUP_LIST(KZT_GROUP_ENUM)
#undef KZT_GROUP_ENUM
} KztLibraryGroup;

/*
 * Group selection controls whether a guest library is eligible for its KZT
 * wrapper.  It does not imply that every custom wrapper function executes
 * only host code: loader coordination and callback wrappers may deliberately
 * re-enter guest execution to preserve guest-visible state.
 */

#define KZT_GROUP_STABLE_MASK_STABLE(group) | (group)
#define KZT_GROUP_STABLE_MASK_EXPERIMENTAL(group)
#define KZT_GROUP_STABLE_MASK(identifier, name, bit, stability, dependencies) \
    KZT_GROUP_STABLE_MASK_##stability(KZT_GROUP_##identifier)
enum {
    KZT_GROUP_STABLE = KZT_GROUP_NONE
        KZT_GROUP_LIST(KZT_GROUP_STABLE_MASK),
};
#undef KZT_GROUP_STABLE_MASK
#undef KZT_GROUP_STABLE_MASK_EXPERIMENTAL
#undef KZT_GROUP_STABLE_MASK_STABLE

KztLibraryGroup kzt_group_for_library(const char *soname);
const char *kzt_group_name(KztLibraryGroup group);
void kzt_groups_reset(void);
bool kzt_groups_configure(const char *spec, bool log_enabled);
void kzt_groups_reject_configuration(const char *reason, bool log_enabled);
uint32_t kzt_groups_enabled_mask(void);
bool kzt_group_is_enabled(KztLibraryGroup group);
bool kzt_library_is_enabled(const char *soname);
const char *kzt_groups_last_error(void);
void kzt_groups_log_library(const char *soname, bool enabled);

#endif /* LATX_KZT_GROUPS_H */
