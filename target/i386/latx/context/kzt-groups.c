/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"

#include "kzt-groups.h"

typedef struct KztLibraryGroupEntry {
    const char *soname;
    KztLibraryGroup group;
} KztLibraryGroupEntry;

typedef struct KztGroupDefinition {
    KztLibraryGroup group;
    const char *name;
    bool stable;
    uint32_t dependencies;
} KztGroupDefinition;

#define KZT_GROUP_IS_STABLE true
#define KZT_GROUP_IS_EXPERIMENTAL false
#define KZT_GROUP_DEFINITION(identifier, name, bit, stability, dependencies) \
    { KZT_GROUP_##identifier, name, KZT_GROUP_IS_##stability, dependencies },
static const KztGroupDefinition kzt_group_definitions[] = {
    KZT_GROUP_LIST(KZT_GROUP_DEFINITION)
};
#undef KZT_GROUP_DEFINITION
#undef KZT_GROUP_IS_EXPERIMENTAL
#undef KZT_GROUP_IS_STABLE

#define GO(soname, wrapper, group) { soname, group },
#define GOALIAS(soname, wrapper, group) { soname, group },
static const KztLibraryGroupEntry kzt_library_groups[] = {
#include "library_list.h"
};
#undef GO
#undef GOALIAS

static uint32_t enabled_groups = KZT_GROUP_STABLE;
static bool group_log_enabled;
static char group_error[128];
static bool library_decision_logged[ARRAY_SIZE(kzt_library_groups)];
static GMutex group_log_lock;

typedef enum KztGroupSpecMode {
    KZT_GROUP_SPEC_UNSET,
    KZT_GROUP_SPEC_EXACT,
    KZT_GROUP_SPEC_MODIFY,
} KztGroupSpecMode;

static KztLibraryGroup kzt_group_from_name(const char *name)
{
    for (size_t i = 0; i < ARRAY_SIZE(kzt_group_definitions); i++) {
        if (!strcasecmp(name, kzt_group_definitions[i].name)) {
            return kzt_group_definitions[i].group;
        }
    }
    return KZT_GROUP_NONE;
}

static void kzt_set_unknown_group_error(const char *name)
{
    GString *valid = g_string_new(NULL);

    for (size_t i = 0; i < ARRAY_SIZE(kzt_group_definitions); i++) {
        g_string_append_printf(valid, "%s%s", i ? "," : "",
                               kzt_group_definitions[i].name);
    }
    snprintf(group_error, sizeof(group_error),
             "contains unknown group '%s' (valid groups: %s)", name,
             valid->str);
    g_string_free(valid, true);
}

static const char *kzt_skip_space(const char *cursor)
{
    while (*cursor && g_ascii_isspace(*cursor)) {
        cursor++;
    }
    return cursor;
}

static bool kzt_parse_group_spec(const char *spec, KztGroupSpecMode *mode,
                                 uint32_t *selected, uint32_t *added,
                                 uint32_t *removed, bool *use_stable)
{
    const char *cursor;
    bool saw_token = false;

    if (!spec || !*kzt_skip_space(spec)) {
        return true;
    }

    cursor = spec;
    while (true) {
        char token[32];
        const char *start;
        const char *end;
        char operation = '\0';
        size_t length;
        KztLibraryGroup group;

        cursor = kzt_skip_space(cursor);
        if (!*cursor || *cursor == ',') {
            snprintf(group_error, sizeof(group_error),
                     "contains an empty library group");
            return false;
        }
        if (*cursor == '+' || *cursor == '-') {
            operation = *cursor++;
        }
        start = cursor;
        while (*cursor && *cursor != ',') {
            cursor++;
        }
        end = cursor;
        while (end > start && g_ascii_isspace(end[-1])) {
            end--;
        }
        length = end - start;
        if (!length) {
            snprintf(group_error, sizeof(group_error),
                     "contains a sign without a library group");
            return false;
        }
        if (length >= sizeof(token)) {
            snprintf(group_error, sizeof(group_error),
                     "contains a group name longer than %zu bytes",
                     sizeof(token) - 1);
            return false;
        }
        memcpy(token, start, length);
        token[length] = '\0';

        if (!strcasecmp(token, "stable")) {
            if (operation || saw_token || *cursor == ',') {
                snprintf(group_error, sizeof(group_error),
                         "'stable' must be used by itself");
                return false;
            }
            *mode = KZT_GROUP_SPEC_EXACT;
            *use_stable = true;
            saw_token = true;
        } else {
            group = kzt_group_from_name(token);
            if (group == KZT_GROUP_NONE) {
                kzt_set_unknown_group_error(token);
                return false;
            }

            if (operation) {
                if (*mode == KZT_GROUP_SPEC_EXACT) {
                    snprintf(group_error, sizeof(group_error),
                             "cannot mix exact groups with + or - modifiers");
                    return false;
                }
                *mode = KZT_GROUP_SPEC_MODIFY;
                if (operation == '+') {
                    if (*removed & group) {
                        snprintf(group_error, sizeof(group_error),
                                 "configures group '%s' as both enabled "
                                 "and disabled", token);
                        return false;
                    }
                    *added |= group;
                } else {
                    if (*added & group) {
                        snprintf(group_error, sizeof(group_error),
                                 "configures group '%s' as both enabled "
                                 "and disabled", token);
                        return false;
                    }
                    *removed |= group;
                }
            } else {
                if (*mode == KZT_GROUP_SPEC_MODIFY) {
                    snprintf(group_error, sizeof(group_error),
                             "cannot mix exact groups with + or - modifiers");
                    return false;
                }
                *mode = KZT_GROUP_SPEC_EXACT;
                *selected |= group;
            }
            saw_token = true;
        }

        if (!*cursor) {
            break;
        }
        cursor++;
        if (!*kzt_skip_space(cursor)) {
            snprintf(group_error, sizeof(group_error),
                     "contains a trailing comma");
            return false;
        }
    }
    return true;
}

static uint32_t kzt_add_dependencies(uint32_t groups)
{
    uint32_t previous;

    do {
        previous = groups;
        for (size_t i = 0; i < ARRAY_SIZE(kzt_group_definitions); i++) {
            if (groups & kzt_group_definitions[i].group) {
                groups |= kzt_group_definitions[i].dependencies;
            }
        }
    } while (groups != previous);
    return groups;
}

static uint32_t kzt_prune_missing_dependencies(uint32_t groups)
{
    uint32_t previous;

    do {
        previous = groups;
        for (size_t i = 0; i < ARRAY_SIZE(kzt_group_definitions); i++) {
            const KztGroupDefinition *definition =
                &kzt_group_definitions[i];

            if ((groups & definition->group) &&
                (groups & definition->dependencies) !=
                    definition->dependencies) {
                groups &= ~definition->group;
            }
        }
    } while (groups != previous);
    return groups;
}

static void kzt_groups_log_mask(const char *label, uint32_t groups)
{
    bool first = true;

    fprintf(stderr, "KZT: %s: ", label);
    for (size_t i = 0; i < ARRAY_SIZE(kzt_group_definitions); i++) {
        if (!(groups & kzt_group_definitions[i].group)) {
            continue;
        }
        fprintf(stderr, "%s%s", first ? "" : ",",
                kzt_group_definitions[i].name);
        first = false;
    }
    fprintf(stderr, "%s\n", first ? "none" : "");
}

static void kzt_groups_log_effective(const char *spec, uint32_t dependencies,
                                     uint32_t removed, uint32_t pruned)
{
    if (!group_log_enabled) {
        return;
    }
    fprintf(stderr, "KZT: requested library groups: %s\n",
            spec && *kzt_skip_space(spec) ? spec : "stable");
    if (dependencies) {
        kzt_groups_log_mask("added required library groups", dependencies);
    }
    if (removed) {
        kzt_groups_log_mask("explicitly disabled library groups", removed);
    }
    if (pruned) {
        kzt_groups_log_mask("disabled dependent library groups", pruned);
    }
    kzt_groups_log_mask("enabled library groups", enabled_groups);
}

KztLibraryGroup kzt_group_for_library(const char *soname)
{
    if (!soname) {
        return KZT_GROUP_NONE;
    }

    for (size_t i = 0; i < ARRAY_SIZE(kzt_library_groups); i++) {
        if (!strcmp(soname, kzt_library_groups[i].soname)) {
            return kzt_library_groups[i].group;
        }
    }

    return KZT_GROUP_NONE;
}

const char *kzt_group_name(KztLibraryGroup group)
{
    for (size_t i = 0; i < ARRAY_SIZE(kzt_group_definitions); i++) {
        if (group == kzt_group_definitions[i].group) {
            return kzt_group_definitions[i].name;
        }
    }
    return "none";
}

void kzt_groups_print_available(void)
{
    puts("Available KZT library groups:");
    for (size_t i = 0; i < ARRAY_SIZE(kzt_group_definitions); i++) {
        const KztGroupDefinition *definition = &kzt_group_definitions[i];
        uint32_t dependencies =
            kzt_add_dependencies(definition->group) & ~definition->group;
        bool first = true;

        printf("  %-8s status=%s requires=", definition->name,
               definition->stable ? "stable" : "experimental");
        for (size_t j = 0; j < ARRAY_SIZE(kzt_group_definitions); j++) {
            if (!(dependencies & kzt_group_definitions[j].group)) {
                continue;
            }
            printf("%s%s", first ? "" : ",",
                   kzt_group_definitions[j].name);
            first = false;
        }
        puts(first ? "none" : "");
    }
    puts("Selection syntax:");
    puts("  LATX_KZT_LIBS=stable       use the current stable set");
    puts("  LATX_KZT_LIBS=name,...      use only the named groups and "
         "dependencies");
    puts("  LATX_KZT_LIBS=+name,-name   modify the current stable set");
}

void kzt_groups_reset(void)
{
    enabled_groups = KZT_GROUP_STABLE;
    group_log_enabled = false;
    group_error[0] = '\0';
    memset(library_decision_logged, 0, sizeof(library_decision_logged));
}

bool kzt_groups_configure(const char *spec, bool log_enabled)
{
    KztGroupSpecMode mode = KZT_GROUP_SPEC_UNSET;
    uint32_t selected = KZT_GROUP_NONE;
    uint32_t added = KZT_GROUP_NONE;
    uint32_t removed = KZT_GROUP_NONE;
    uint32_t groups;
    uint32_t before_dependencies;
    uint32_t before_prune;
    uint32_t dependencies;
    uint32_t pruned;
    bool use_stable = false;

    group_error[0] = '\0';
    memset(library_decision_logged, 0, sizeof(library_decision_logged));
    if (!kzt_parse_group_spec(spec, &mode, &selected, &added, &removed,
                              &use_stable)) {
        enabled_groups = KZT_GROUP_NONE;
        group_log_enabled = log_enabled;
        return false;
    }

    if (mode == KZT_GROUP_SPEC_EXACT && !use_stable) {
        groups = selected;
    } else {
        groups = KZT_GROUP_STABLE;
    }
    if (mode == KZT_GROUP_SPEC_MODIFY) {
        groups |= added;
    }
    before_dependencies = groups;
    groups = kzt_add_dependencies(groups);
    dependencies = groups & ~before_dependencies;
    groups &= ~removed;
    before_prune = groups;
    enabled_groups = kzt_prune_missing_dependencies(groups);
    pruned = before_prune & ~enabled_groups;
    group_log_enabled = log_enabled;
    kzt_groups_log_effective(spec, dependencies, removed, pruned);
    return true;
}

void kzt_groups_reject_configuration(const char *reason, bool log_enabled)
{
    enabled_groups = KZT_GROUP_NONE;
    group_log_enabled = log_enabled;
    snprintf(group_error, sizeof(group_error), "%s", reason);
}

uint32_t kzt_groups_enabled_mask(void)
{
    return enabled_groups;
}

bool kzt_group_is_enabled(KztLibraryGroup group)
{
    return group != KZT_GROUP_NONE && (enabled_groups & group) == group;
}

bool kzt_library_is_enabled(const char *soname)
{
    return kzt_group_is_enabled(kzt_group_for_library(soname));
}

const char *kzt_groups_last_error(void)
{
    return group_error;
}

void kzt_groups_log_library(const char *soname, bool enabled)
{
    if (!group_log_enabled) {
        return;
    }

    for (size_t i = 0; i < ARRAY_SIZE(kzt_library_groups); i++) {
        if (strcmp(soname, kzt_library_groups[i].soname)) {
            continue;
        }

        g_mutex_lock(&group_log_lock);
        if (!library_decision_logged[i]) {
            library_decision_logged[i] = true;
            fprintf(stderr, "KZT: %s KZT wrapper for %s (group %s)\n",
                    enabled ? "using" : "skipping", soname,
                    kzt_group_name(kzt_library_groups[i].group));
        }
        g_mutex_unlock(&group_log_lock);
        return;
    }
}
