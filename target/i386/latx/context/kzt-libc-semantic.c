/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <errno.h>
#include <dlfcn.h>
#include <langinfo.h>
#include <locale.h>
#include <netdb.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "box64context.h"
#include "callback.h"
#include "debug.h"
#include "elfloader.h"
#include "kzt-guest-tls.h"
#include "kzt-libc-semantic.h"
#include "myalign.h"
#include "qemu/compiler.h"
#include "qemu.h"
#include "lsenv.h"

#define KZT_LIBC_SEMANTIC_MAX_DEPTH 16
#define KZT_LIBC_LOCALE_CATEGORIES 12
#define KZT_LIBC_LOCALE_NAME_MAX 128

typedef enum kzt_libc_semantic_direction {
    KZT_LIBC_HOST_TO_GUEST,
    KZT_LIBC_GUEST_TO_HOST,
} kzt_libc_semantic_direction_t;

typedef struct kzt_libc_semantic_frame {
    kzt_libc_semantic_direction_t direction;
} kzt_libc_semantic_frame_t;

typedef struct kzt_libc_locale_projection {
    char names[KZT_LIBC_LOCALE_CATEGORIES][KZT_LIBC_LOCALE_NAME_MAX];
    uintptr_t guest_locale;
    locale_t host_locale;
    struct kzt_libc_locale_projection *next;
} kzt_libc_locale_projection_t;

typedef struct kzt_libc_locale_state {
    char current[KZT_LIBC_LOCALE_CATEGORIES][KZT_LIBC_LOCALE_NAME_MAX];
    char global[KZT_LIBC_LOCALE_CATEGORIES][KZT_LIBC_LOCALE_NAME_MAX];
    int uses_global;
} kzt_libc_locale_state_t;

typedef struct kzt_libc_semantic_state {
    uintptr_t guest_errno_slot;
    uintptr_t guest_h_errno_slot;
    /*
     * Borrowed current-locale handles must never escape this context's
     * lifetime. Cache entries are not shared with other contexts.
     */
    kzt_libc_locale_projection_t *locale_projections;
    kzt_libc_semantic_frame_t frames[KZT_LIBC_SEMANTIC_MAX_DEPTH];
    kzt_libc_semantic_frame_t *extra_frames;
    size_t extra_capacity;
    size_t depth;
    size_t internal_depth;
    size_t internal_overflow_depth;
    int internal_guest_errno[KZT_LIBC_SEMANTIC_MAX_DEPTH];
    int internal_guest_errno_valid[KZT_LIBC_SEMANTIC_MAX_DEPTH];
    int internal_guest_h_errno[KZT_LIBC_SEMANTIC_MAX_DEPTH];
    int internal_guest_h_errno_valid[KZT_LIBC_SEMANTIC_MAX_DEPTH];
} kzt_libc_semantic_state_t;

static uintptr_t guest_errno_location;
static intptr_t guest_errno_offset;
static int guest_errno_offset_valid;
static uintptr_t guest_h_errno_location;
static intptr_t guest_h_errno_offset;
static int guest_h_errno_offset_valid;
static uintptr_t guest_newlocale;
static uintptr_t guest_uselocale;
static uintptr_t guest_freelocale;
static uintptr_t guest_duplocale;
static uintptr_t guest_nl_langinfo_l;
static uintptr_t guest_setlocale;
static GMutex kzt_libc_locale_projection_lock;
static GMutex kzt_libc_locale_sync_lock;
static int kzt_libc_semantic_required;
static GRecMutex kzt_libc_init_lock;
static gsize kzt_libc_init_lock_ready;

static void kzt_libc_semantic_reinitialize_lock(GMutex *lock)
{
    memset(lock, 0, sizeof(*lock));
    g_mutex_init(lock);
}

void kzt_libc_semantic_after_fork_child(void)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return;
    }
    if (kzt_libc_init_lock_ready) {
        memset(&kzt_libc_init_lock, 0, sizeof(kzt_libc_init_lock));
        g_rec_mutex_init(&kzt_libc_init_lock);
    }
    kzt_libc_semantic_reinitialize_lock(&kzt_libc_locale_sync_lock);
    kzt_libc_semantic_reinitialize_lock(
        &kzt_libc_locale_projection_lock);
}

static const int kzt_libc_locale_categories[KZT_LIBC_LOCALE_CATEGORIES] = {
    LC_CTYPE,
    LC_NUMERIC,
    LC_TIME,
    LC_COLLATE,
    LC_MONETARY,
    LC_MESSAGES,
    LC_PAPER,
    LC_NAME,
    LC_ADDRESS,
    LC_TELEPHONE,
    LC_MEASUREMENT,
    LC_IDENTIFICATION,
};

static const int kzt_libc_locale_masks[KZT_LIBC_LOCALE_CATEGORIES] = {
    LC_CTYPE_MASK,
    LC_NUMERIC_MASK,
    LC_TIME_MASK,
    LC_COLLATE_MASK,
    LC_MONETARY_MASK,
    LC_MESSAGES_MASK,
    LC_PAPER_MASK,
    LC_NAME_MASK,
    LC_ADDRESS_MASK,
    LC_TELEPHONE_MASK,
    LC_MEASUREMENT_MASK,
    LC_IDENTIFICATION_MASK,
};

/* POSIX pthread APIs return positive error numbers rather than -errno. */
static void QEMU_NORETURN kzt_libc_semantic_abort_boundary(
    const char *reason)
{
    fprintf(stderr, "KZT libc semantic boundary failed: %s\n", reason);
    _exit(EXIT_FAILURE);
}

static uintptr_t kzt_find_guest_libc_symbol(const char *name)
{
    if (!my_context || !name) {
        return 0;
    }

    for (int i = 0; i < my_context->elfsize; ++i) {
        elfheader_t *head = my_context->elfs[i];
        const char *elf_name;
        const char *base_name;

        if (!head) {
            continue;
        }
        elf_name = ElfName(head);
        if (!elf_name) {
            continue;
        }
        base_name = strrchr(elf_name, '/');
        base_name = base_name ? base_name + 1 : elf_name;
        if (strcmp(base_name, "libc.so.6") != 0 &&
            strncmp(base_name, "libc-", 5) != 0) {
            continue;
        }
        return FindElfSymbolAddress(head, name);
    }
    return 0;
}

static int kzt_libc_semantic_read_guest_errno(
    const kzt_libc_semantic_state_t *state, int *value)
{
    int *slot;

    if (!state || !state->guest_errno_slot || !value) {
        return -1;
    }
    slot = lock_user(VERIFY_READ, (abi_ulong)state->guest_errno_slot,
                     sizeof(*slot), 1);
    if (!slot) {
        return -1;
    }
    *value = *slot;
    unlock_user(slot, (abi_ulong)state->guest_errno_slot, 0);
    return 0;
}

static int kzt_libc_semantic_write_guest_errno(
    const kzt_libc_semantic_state_t *state, int value)
{
    int *slot;

    if (!state || !state->guest_errno_slot) {
        return -1;
    }
    slot = lock_user(VERIFY_WRITE, (abi_ulong)state->guest_errno_slot,
                     sizeof(*slot), 0);
    if (!slot) {
        return -1;
    }
    *slot = value;
    unlock_user(slot, (abi_ulong)state->guest_errno_slot,
                sizeof(*slot));
    return 0;
}

static int kzt_libc_semantic_read_guest_h_errno(
    const kzt_libc_semantic_state_t *state, int *value)
{
    int *slot;

    if (!state || !state->guest_h_errno_slot || !value) {
        return -1;
    }
    slot = lock_user(VERIFY_READ, (abi_ulong)state->guest_h_errno_slot,
                     sizeof(*slot), 1);
    if (!slot) {
        return -1;
    }
    *value = *slot;
    unlock_user(slot, (abi_ulong)state->guest_h_errno_slot, 0);
    return 0;
}

static int kzt_libc_semantic_write_guest_h_errno(
    const kzt_libc_semantic_state_t *state, int value)
{
    int *slot;

    if (!state || !state->guest_h_errno_slot) {
        return -1;
    }
    slot = lock_user(VERIFY_WRITE, (abi_ulong)state->guest_h_errno_slot,
                     sizeof(*slot), 0);
    if (!slot) {
        return -1;
    }
    *slot = value;
    unlock_user(slot, (abi_ulong)state->guest_h_errno_slot,
                sizeof(*slot));
    return 0;
}

static kzt_libc_semantic_state_t *kzt_libc_semantic_state(
    CPUX86State *env)
{
    return env ? env->kzt_libc_semantic_state : NULL;
}

static int kzt_libc_locale_names_equal(
    const char left[KZT_LIBC_LOCALE_CATEGORIES][KZT_LIBC_LOCALE_NAME_MAX],
    const char right[KZT_LIBC_LOCALE_CATEGORIES][KZT_LIBC_LOCALE_NAME_MAX])
{
    for (size_t index = 0;
         index < KZT_LIBC_LOCALE_CATEGORIES; ++index) {
        if (strcmp(left[index], right[index]) != 0) {
            return 0;
        }
    }
    return 1;
}

static int kzt_libc_capture_host_names(
    locale_t locale,
    char names[KZT_LIBC_LOCALE_CATEGORIES][KZT_LIBC_LOCALE_NAME_MAX])
{
    for (size_t index = 0;
         index < KZT_LIBC_LOCALE_CATEGORIES; ++index) {
        const char *name = nl_langinfo_l(
            _NL_LOCALE_NAME(kzt_libc_locale_categories[index]), locale);
        size_t length;

        if (!name) {
            return -1;
        }
        length = strlen(name);
        if (!length || length >= KZT_LIBC_LOCALE_NAME_MAX) {
            return -1;
        }
        memcpy(names[index], name, length + 1);
    }
    return 0;
}

static int kzt_libc_capture_host_locale_state(
    kzt_libc_locale_state_t *state)
{
    locale_t current;
    locale_t global;
    int result = -1;

    if (!state) {
        return -1;
    }
    current = uselocale((locale_t)0);
    if (!current) {
        return -1;
    }
    global = duplocale(LC_GLOBAL_LOCALE);
    if (!global) {
        return -1;
    }
    state->uses_global = current == LC_GLOBAL_LOCALE;
    if (kzt_libc_capture_host_names(global, state->global) != 0) {
        goto out;
    }
    if (state->uses_global) {
        memcpy(state->current, state->global,
               sizeof(state->current));
    } else if (kzt_libc_capture_host_names(
                   current, state->current) != 0) {
        goto out;
    }
    result = 0;

out:
    freelocale(global);
    return result;
}

static int kzt_libc_read_guest_string(
    uintptr_t address, char output[KZT_LIBC_LOCALE_NAME_MAX])
{
    if (!address) {
        return -1;
    }
    for (size_t index = 0; index < KZT_LIBC_LOCALE_NAME_MAX; ++index) {
        char *byte = lock_user(
            VERIFY_READ, (abi_ulong)(address + index), 1, 1);

        if (!byte) {
            return -1;
        }
        output[index] = *byte;
        unlock_user(byte, (abi_ulong)(address + index), 0);
        if (!output[index]) {
            return index ? 0 : -1;
        }
    }
    return -1;
}

static int kzt_libc_capture_guest_names(
    uintptr_t locale,
    char names[KZT_LIBC_LOCALE_CATEGORIES][KZT_LIBC_LOCALE_NAME_MAX])
{
    for (size_t index = 0;
         index < KZT_LIBC_LOCALE_CATEGORIES; ++index) {
        uintptr_t name = RunFunctionWithStateInternalNoRefresh(
            guest_nl_langinfo_l, 2,
            (uint64_t)_NL_LOCALE_NAME(kzt_libc_locale_categories[index]),
            (uint64_t)locale);

        if (kzt_libc_read_guest_string(name, names[index]) != 0) {
            return -1;
        }
    }
    return 0;
}

static int kzt_libc_capture_guest_locale_state(
    kzt_libc_locale_state_t *state)
{
    uintptr_t current;
    uintptr_t global;
    int result = -1;

    if (!state) {
        return -1;
    }
    current = RunFunctionWithStateInternalNoRefresh(
        guest_uselocale, 1, 0);
    if (!current) {
        return -1;
    }
    global = RunFunctionWithStateInternalNoRefresh(
        guest_duplocale, 1, (uint64_t)-1);
    if (!global) {
        return -1;
    }
    state->uses_global = current == UINTPTR_MAX;
    if (kzt_libc_capture_guest_names(global, state->global) != 0) {
        goto out;
    }
    if (state->uses_global) {
        memcpy(state->current, state->global,
               sizeof(state->current));
    } else if (kzt_libc_capture_guest_names(
                   current, state->current) != 0) {
        goto out;
    }
    result = 0;

out:
    RunFunctionWithStateInternalNoRefresh(
        guest_freelocale, 1, (uint64_t)global);
    return result;
}

static kzt_libc_locale_projection_t *kzt_libc_find_locale_projection(
    kzt_libc_semantic_state_t *owner,
    const char names[KZT_LIBC_LOCALE_CATEGORIES][KZT_LIBC_LOCALE_NAME_MAX])
{
    kzt_libc_locale_projection_t *projection;

    for (projection = owner->locale_projections;
         projection; projection = projection->next) {
        if (kzt_libc_locale_names_equal(projection->names, names)) {
            return projection;
        }
    }
    projection = g_new0(kzt_libc_locale_projection_t, 1);
    memcpy(projection->names, names, sizeof(projection->names));
    projection->next = owner->locale_projections;
    owner->locale_projections = projection;
    return projection;
}

static uintptr_t kzt_libc_get_guest_locale_projection(
    kzt_libc_semantic_state_t *owner,
    const char names[KZT_LIBC_LOCALE_CATEGORIES][KZT_LIBC_LOCALE_NAME_MAX])
{
    kzt_libc_locale_projection_t *projection;
    uintptr_t locale;

    g_mutex_lock(&kzt_libc_locale_projection_lock);
    projection = kzt_libc_find_locale_projection(owner, names);
    if (!projection->guest_locale) {
        locale = 0;
        for (size_t index = 0;
             index < KZT_LIBC_LOCALE_CATEGORIES; ++index) {
            uintptr_t updated = RunFunctionWithStateInternalNoRefresh(
                guest_newlocale, 3,
                (uint64_t)kzt_libc_locale_masks[index],
                (uint64_t)(uintptr_t)names[index],
                (uint64_t)locale);

            if (!updated) {
                if (locale) {
                    RunFunctionWithStateInternalNoRefresh(
                        guest_freelocale, 1, (uint64_t)locale);
                }
                locale = 0;
                break;
            }
            locale = updated;
        }
        projection->guest_locale = locale;
    }
    locale = projection->guest_locale;
    g_mutex_unlock(&kzt_libc_locale_projection_lock);
    return locale;
}

static locale_t kzt_libc_get_host_locale_projection(
    kzt_libc_semantic_state_t *owner,
    const char names[KZT_LIBC_LOCALE_CATEGORIES][KZT_LIBC_LOCALE_NAME_MAX])
{
    kzt_libc_locale_projection_t *projection;
    locale_t locale;

    g_mutex_lock(&kzt_libc_locale_projection_lock);
    projection = kzt_libc_find_locale_projection(owner, names);
    if (!projection->host_locale) {
        locale = (locale_t)0;
        for (size_t index = 0;
             index < KZT_LIBC_LOCALE_CATEGORIES; ++index) {
            locale_t updated = newlocale(
                kzt_libc_locale_masks[index], names[index], locale);

            if (!updated) {
                if (locale) {
                    freelocale(locale);
                }
                locale = (locale_t)0;
                break;
            }
            locale = updated;
        }
        projection->host_locale = locale;
    }
    locale = projection->host_locale;
    g_mutex_unlock(&kzt_libc_locale_projection_lock);
    return locale;
}

static void kzt_libc_restore_host_global_locale(
    const char names[KZT_LIBC_LOCALE_CATEGORIES][KZT_LIBC_LOCALE_NAME_MAX])
{
    for (size_t index = 0;
         index < KZT_LIBC_LOCALE_CATEGORIES; ++index) {
        (void)setlocale(kzt_libc_locale_categories[index], names[index]);
    }
}

static int kzt_libc_set_host_global_locale(
    const char names[KZT_LIBC_LOCALE_CATEGORIES][KZT_LIBC_LOCALE_NAME_MAX])
{
    kzt_libc_locale_state_t previous;

    if (kzt_libc_capture_host_locale_state(&previous) != 0) {
        return -1;
    }
    for (size_t index = 0;
         index < KZT_LIBC_LOCALE_CATEGORIES; ++index) {
        if (strcmp(previous.global[index], names[index]) != 0 &&
            !setlocale(kzt_libc_locale_categories[index], names[index])) {
            kzt_libc_restore_host_global_locale(previous.global);
            return -1;
        }
    }
    return 0;
}

static void kzt_libc_restore_guest_global_locale(
    const char names[KZT_LIBC_LOCALE_CATEGORIES][KZT_LIBC_LOCALE_NAME_MAX])
{
    for (size_t index = 0;
         index < KZT_LIBC_LOCALE_CATEGORIES; ++index) {
        (void)RunFunctionWithStateInternalNoRefresh(
            guest_setlocale, 2,
            (uint64_t)kzt_libc_locale_categories[index],
            (uint64_t)(uintptr_t)names[index]);
    }
}

static int kzt_libc_set_guest_global_locale(
    const char names[KZT_LIBC_LOCALE_CATEGORIES][KZT_LIBC_LOCALE_NAME_MAX])
{
    kzt_libc_locale_state_t previous;

    if (kzt_libc_capture_guest_locale_state(&previous) != 0) {
        return -1;
    }
    for (size_t index = 0;
         index < KZT_LIBC_LOCALE_CATEGORIES; ++index) {
        if (strcmp(previous.global[index], names[index]) != 0 &&
            !RunFunctionWithStateInternalNoRefresh(
                guest_setlocale, 2,
                (uint64_t)kzt_libc_locale_categories[index],
                (uint64_t)(uintptr_t)names[index])) {
            kzt_libc_restore_guest_global_locale(previous.global);
            return -1;
        }
    }
    return 0;
}

static int kzt_libc_install_host_locale_state(
    kzt_libc_semantic_state_t *owner,
    const kzt_libc_locale_state_t *state)
{
    locale_t locale;

    if (!state || kzt_libc_set_host_global_locale(state->global) != 0) {
        return -1;
    }
    locale = state->uses_global
                 ? LC_GLOBAL_LOCALE
                 : kzt_libc_get_host_locale_projection(owner, state->current);
    if (!locale || !uselocale(locale)) {
        return -1;
    }
    return 0;
}

static int kzt_libc_install_guest_locale_state(
    kzt_libc_semantic_state_t *owner,
    const kzt_libc_locale_state_t *state)
{
    uintptr_t locale;

    if (!state || kzt_libc_set_guest_global_locale(state->global) != 0) {
        return -1;
    }
    locale = state->uses_global
                 ? UINTPTR_MAX
                 : kzt_libc_get_guest_locale_projection(owner, state->current);
    if (!locale || !RunFunctionWithStateInternalNoRefresh(
                       guest_uselocale, 1, (uint64_t)locale)) {
        return -1;
    }
    return 0;
}

static int kzt_libc_prepare_process_locale(kzt_libc_semantic_state_t *owner)
{
    kzt_libc_locale_state_t guest_state;
    kzt_libc_locale_state_t host_state;
    int result = -1;

    g_mutex_lock(&kzt_libc_locale_sync_lock);
    if (kzt_libc_capture_host_locale_state(&host_state) != 0 ||
        kzt_libc_capture_guest_locale_state(&guest_state) != 0) {
        goto out;
    }
    if ((!host_state.uses_global &&
         !kzt_libc_get_guest_locale_projection(owner, host_state.current)) ||
        (!guest_state.uses_global &&
         !kzt_libc_get_host_locale_projection(owner, guest_state.current))) {
        goto out;
    }
    result = 0;

out:
    g_mutex_unlock(&kzt_libc_locale_sync_lock);
    return result;
}

static int kzt_libc_semantic_initialize_locked(CPUX86State *env)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return -1;
    }
    kzt_libc_semantic_state_t *state;
    uintptr_t h_errno_slot;
    uintptr_t slot;

    if (!env) {
        return -1;
    }
    if (env->kzt_libc_semantic_state) {
        return 0;
    }
    if (!guest_newlocale) {
        guest_newlocale = kzt_find_guest_libc_symbol("newlocale");
        guest_uselocale = kzt_find_guest_libc_symbol("uselocale");
        guest_freelocale = kzt_find_guest_libc_symbol("freelocale");
        guest_duplocale = kzt_find_guest_libc_symbol("duplocale");
        guest_nl_langinfo_l = kzt_find_guest_libc_symbol("nl_langinfo_l");
        guest_setlocale = kzt_find_guest_libc_symbol("setlocale");
        if (!guest_newlocale) {
            guest_newlocale = kzt_resolve_guest_symbol("newlocale");
        }
        if (!guest_uselocale) {
            guest_uselocale = kzt_resolve_guest_symbol("uselocale");
        }
        if (!guest_freelocale) {
            guest_freelocale = kzt_resolve_guest_symbol("freelocale");
        }
        if (!guest_duplocale) {
            guest_duplocale = kzt_resolve_guest_symbol("duplocale");
        }
        if (!guest_nl_langinfo_l) {
            guest_nl_langinfo_l = kzt_resolve_guest_symbol("nl_langinfo_l");
        }
        if (!guest_setlocale) {
            guest_setlocale = kzt_resolve_guest_symbol("setlocale");
        }
    }
    if (!guest_newlocale || !guest_uselocale || !guest_freelocale ||
        !guest_duplocale || !guest_nl_langinfo_l || !guest_setlocale) {
        printf_log(LOG_INFO,
                   "KZT cannot resolve Guest locale projection helpers\n");
        return -1;
    }
    if (!guest_errno_offset_valid) {
        if (!guest_errno_location) {
            guest_errno_location = kzt_find_guest_libc_symbol(
                "__errno_location");
            if (!guest_errno_location) {
                guest_errno_location = kzt_resolve_guest_symbol(
                    "__errno_location");
            }
        }
        if (!guest_errno_location) {
            printf_log(LOG_INFO,
                       "KZT cannot resolve Guest __errno_location\n");
            return -1;
        }
        slot = RunFunctionWithStateInternal(guest_errno_location, 0);
        if (!slot || !env->segs[R_FS].base) {
            return -1;
        }
        guest_errno_offset = (intptr_t)slot -
                             (intptr_t)env->segs[R_FS].base;
        guest_errno_offset_valid = 1;
    } else {
        if (!env->segs[R_FS].base ||
            (guest_errno_offset > 0 &&
             env->segs[R_FS].base >
                 UINTPTR_MAX - (uintptr_t)guest_errno_offset) ||
            (guest_errno_offset < 0 &&
             env->segs[R_FS].base <
                 (uintptr_t)-guest_errno_offset)) {
            return -1;
        }
        slot = (uintptr_t)((intptr_t)env->segs[R_FS].base +
                           guest_errno_offset);
    }
    if (!guest_h_errno_offset_valid) {
        if (!guest_h_errno_location) {
            guest_h_errno_location = kzt_find_guest_libc_symbol(
                "__h_errno_location");
            if (!guest_h_errno_location) {
                guest_h_errno_location = kzt_resolve_guest_symbol(
                    "__h_errno_location");
            }
        }
        if (!guest_h_errno_location) {
            printf_log(LOG_INFO,
                       "KZT cannot resolve Guest __h_errno_location\n");
            return -1;
        }
        h_errno_slot = RunFunctionWithStateInternal(
            guest_h_errno_location, 0);
        if (!h_errno_slot || !env->segs[R_FS].base) {
            return -1;
        }
        guest_h_errno_offset = (intptr_t)h_errno_slot -
                               (intptr_t)env->segs[R_FS].base;
        guest_h_errno_offset_valid = 1;
    } else {
        if (!env->segs[R_FS].base ||
            (guest_h_errno_offset > 0 &&
             env->segs[R_FS].base >
                 UINTPTR_MAX - (uintptr_t)guest_h_errno_offset) ||
            (guest_h_errno_offset < 0 &&
             env->segs[R_FS].base <
                 (uintptr_t)-guest_h_errno_offset)) {
            return -1;
        }
        h_errno_slot =
            (uintptr_t)((intptr_t)env->segs[R_FS].base +
                        guest_h_errno_offset);
    }
    state = g_new0(kzt_libc_semantic_state_t, 1);
    state->guest_errno_slot = slot;
    state->guest_h_errno_slot = h_errno_slot;
    env->kzt_libc_semantic_state = state;
    return 0;
}

int kzt_libc_semantic_initialize(CPUX86State *env)
{
    int result;

    if (!latx_kzt_guest_tls_enabled()) {
        return -1;
    }
    if (g_once_init_enter(&kzt_libc_init_lock_ready)) {
        g_rec_mutex_init(&kzt_libc_init_lock);
        g_once_init_leave(&kzt_libc_init_lock_ready, 1);
    }
    g_rec_mutex_lock(&kzt_libc_init_lock);
    result = kzt_libc_semantic_initialize_locked(env);
    g_rec_mutex_unlock(&kzt_libc_init_lock);
    return result;
}

int kzt_libc_semantic_enter_current(void)
{
    CPUX86State *env;

    if (!latx_kzt_guest_tls_enabled() || !lsenv || !lsenv->cpu_state) {
        return -1;
    }
    env = (CPUX86State *)lsenv->cpu_state;
    if (kzt_libc_semantic_initialize(env) != 0 ||
        (!kzt_libc_semantic_process_ready() &&
         kzt_libc_semantic_prepare_process_locale(env) != 0)) {
        return -1;
    }
    kzt_libc_semantic_guest_to_host_enter(env);
    return 0;
}

void kzt_libc_semantic_leave_current(void)
{
    if (lsenv && lsenv->cpu_state) {
        kzt_libc_semantic_guest_to_host_leave(
            (CPUX86State *)lsenv->cpu_state);
    }
}

int kzt_libc_semantic_process_ready(void)
{
    return g_atomic_int_get(&kzt_libc_semantic_required);
}

static void kzt_libc_release_locale_projections(
    kzt_libc_semantic_state_t *state, bool guest_available)
{
    kzt_libc_locale_projection_t *projection;

    if (!state) {
        return;
    }
    projection = state->locale_projections;
    state->locale_projections = NULL;
    while (projection) {
        kzt_libc_locale_projection_t *next = projection->next;

        if (projection->host_locale) {
            if (uselocale((locale_t)0) == projection->host_locale) {
                uselocale(LC_GLOBAL_LOCALE);
            }
            freelocale(projection->host_locale);
        }
        if (guest_available && projection->guest_locale) {
            if (RunFunctionWithStateInternalNoRefresh(guest_uselocale, 1, 0)
                    == projection->guest_locale) {
                RunFunctionWithStateInternalNoRefresh(
                    guest_uselocale, 1, (uint64_t)-1);
            }
            RunFunctionWithStateInternalNoRefresh(
                guest_freelocale, 1, projection->guest_locale);
        }
        g_free(projection);
        projection = next;
    }
}

void kzt_libc_semantic_process_reset(CPUX86State *env)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return;
    }
    if (env) {
        /* A replacement Guest image cannot execute the old allocator. */
        kzt_libc_release_locale_projections(env->kzt_libc_semantic_state,
                                           false);
        if (env->kzt_libc_semantic_state) {
            kzt_libc_semantic_state_t *state = env->kzt_libc_semantic_state;

            g_free(state->extra_frames);
        }
        g_free(env->kzt_libc_semantic_state);
        env->kzt_libc_semantic_state = NULL;
    }
    guest_errno_location = 0;
    guest_errno_offset = 0;
    guest_errno_offset_valid = 0;
    guest_h_errno_location = 0;
    guest_h_errno_offset = 0;
    guest_h_errno_offset_valid = 0;
    guest_newlocale = 0;
    guest_uselocale = 0;
    guest_freelocale = 0;
    guest_duplocale = 0;
    guest_nl_langinfo_l = 0;
    guest_setlocale = 0;
    g_atomic_int_set(&kzt_libc_semantic_required, 0);

}

int kzt_libc_semantic_prepare_process_locale(CPUX86State *env)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return -1;
    }
    int result;

    if (!env || !env->kzt_libc_semantic_state) {
        return -1;
    }
    result = kzt_libc_prepare_process_locale(env->kzt_libc_semantic_state);
    if (result == 0) {
        g_atomic_int_set(&kzt_libc_semantic_required, 1);
    }
    return result;
}

uintptr_t kzt_libc_semantic_setlocale(CPUX86State *env, int category,
                                      const char *locale)
{
    kzt_libc_locale_state_t previous;
    kzt_libc_locale_state_t updated;
    uintptr_t guest_result;

    if (!env || !env->kzt_libc_semantic_state || !guest_setlocale) {
        return 0;
    }
    if (!locale) {
        return RunFunctionWithStateInternalNoRefresh(
            guest_setlocale, 2, (uint64_t)category, 0);
    }

    g_mutex_lock(&kzt_libc_locale_sync_lock);
    if (kzt_libc_capture_guest_locale_state(&previous) != 0) {
        g_mutex_unlock(&kzt_libc_locale_sync_lock);
        return 0;
    }
    guest_result = RunFunctionWithStateInternalNoRefresh(
        guest_setlocale, 2, (uint64_t)category,
        (uint64_t)(uintptr_t)locale);
    if (!guest_result ||
        kzt_libc_capture_guest_locale_state(&updated) != 0 ||
        kzt_libc_set_host_global_locale(updated.global) != 0) {
        kzt_libc_restore_guest_global_locale(previous.global);
        guest_result = 0;
    }
    g_mutex_unlock(&kzt_libc_locale_sync_lock);
    return guest_result;
}

void kzt_libc_semantic_destroy(CPUX86State *env)
{
    if (env) {
        kzt_libc_release_locale_projections(env->kzt_libc_semantic_state, true);
        if (env->kzt_libc_semantic_state) {
            kzt_libc_semantic_state_t *state = env->kzt_libc_semantic_state;

            g_free(state->extra_frames);
        }
        g_free(env->kzt_libc_semantic_state);
        env->kzt_libc_semantic_state = NULL;
    }
}

void kzt_libc_semantic_internal_enter(CPUX86State *env)
{
    kzt_libc_semantic_state_t *state = kzt_libc_semantic_state(env);

    if (!state) {
        return;
    }
    if (state->internal_depth == KZT_LIBC_SEMANTIC_MAX_DEPTH) {
        if (state->internal_overflow_depth != SIZE_MAX) {
            ++state->internal_overflow_depth;
        }
        return;
    }
    {
        size_t depth = state->internal_depth++;

        state->internal_guest_errno_valid[depth] =
            kzt_libc_semantic_read_guest_errno(
                state, &state->internal_guest_errno[depth]) == 0;
        state->internal_guest_h_errno_valid[depth] =
            kzt_libc_semantic_read_guest_h_errno(
                state, &state->internal_guest_h_errno[depth]) == 0;
    }
}

void kzt_libc_semantic_internal_leave(CPUX86State *env)
{
    kzt_libc_semantic_state_t *state = kzt_libc_semantic_state(env);

    if (state && state->internal_overflow_depth) {
        --state->internal_overflow_depth;
        return;
    }
    if (state && state->internal_depth) {
        size_t depth = --state->internal_depth;

        if (state->internal_guest_errno_valid[depth]) {
            (void)kzt_libc_semantic_write_guest_errno(
                state, state->internal_guest_errno[depth]);
            state->internal_guest_errno_valid[depth] = 0;
        }
        if (state->internal_guest_h_errno_valid[depth]) {
            (void)kzt_libc_semantic_write_guest_h_errno(
                state, state->internal_guest_h_errno[depth]);
            state->internal_guest_h_errno_valid[depth] = 0;
        }
    }
}

static kzt_libc_semantic_frame_t *kzt_libc_semantic_frame_at(
    kzt_libc_semantic_state_t *state, size_t index)
{
    return index < KZT_LIBC_SEMANTIC_MAX_DEPTH
        ? &state->frames[index]
        : &state->extra_frames[index - KZT_LIBC_SEMANTIC_MAX_DEPTH];
}

static kzt_libc_semantic_frame_t *kzt_libc_semantic_push(
    kzt_libc_semantic_state_t *state,
    kzt_libc_semantic_direction_t direction)
{
    kzt_libc_semantic_frame_t *frame;

    if (!state || state->internal_depth || state->internal_overflow_depth ||
        state->depth == SIZE_MAX) {
        return NULL;
    }
    if (state->depth >= KZT_LIBC_SEMANTIC_MAX_DEPTH &&
        state->depth - KZT_LIBC_SEMANTIC_MAX_DEPTH == state->extra_capacity) {
        size_t capacity;
        kzt_libc_semantic_frame_t *extra;

        if (state->extra_capacity > SIZE_MAX / sizeof(*extra) / 2) {
            return NULL;
        }
        capacity = state->extra_capacity ? state->extra_capacity * 2
                                        : KZT_LIBC_SEMANTIC_MAX_DEPTH;
        extra = g_try_realloc_n(state->extra_frames, capacity, sizeof(*extra));
        if (!extra) {
            return NULL;
        }
        state->extra_frames = extra;
        state->extra_capacity = capacity;
    }
    frame = kzt_libc_semantic_frame_at(state, state->depth++);
    memset(frame, 0, sizeof(*frame));
    frame->direction = direction;
    return frame;
}

static int kzt_libc_semantic_pop(
    kzt_libc_semantic_state_t *state,
    kzt_libc_semantic_direction_t direction)
{
    if (!state || !state->depth ||
        kzt_libc_semantic_frame_at(state, state->depth - 1)->direction !=
            direction) {
        return -1;
    }
    --state->depth;
    return 0;
}

int kzt_libc_semantic_host_to_guest_enter(CPUX86State *env,
                                           int host_errno,
                                           int host_h_errno)
{
    kzt_libc_semantic_state_t *state = kzt_libc_semantic_state(env);
    kzt_libc_locale_state_t locale_state;
    int locale_result;

    if (!kzt_libc_semantic_process_ready()) {
        return 0;
    }
    if (!state) {
        if (kzt_libc_semantic_initialize(env) != 0) {
            return -1;
        }
        state = kzt_libc_semantic_state(env);
    }
    if (!kzt_libc_semantic_push(state, KZT_LIBC_HOST_TO_GUEST)) {
        return -1;
    }
    g_mutex_lock(&kzt_libc_locale_sync_lock);
    locale_result =
        kzt_libc_capture_host_locale_state(&locale_state) == 0 &&
        kzt_libc_install_guest_locale_state(state, &locale_state) == 0
            ? 0 : -1;
    g_mutex_unlock(&kzt_libc_locale_sync_lock);
    if (locale_result != 0) {
        --state->depth;
        return -1;
    }
    if (kzt_libc_semantic_write_guest_errno(
            state, host_to_target_errno(host_errno)) != 0) {
        --state->depth;
        return -1;
    }
    if (kzt_libc_semantic_write_guest_h_errno(
            state, host_h_errno) != 0) {
        --state->depth;
        return -1;
    }
    return 0;
}

void kzt_libc_semantic_host_to_guest_leave(CPUX86State *env)
{
    kzt_libc_semantic_state_t *state = kzt_libc_semantic_state(env);
    kzt_libc_locale_state_t locale_state;
    int guest_errno = 0;
    int guest_h_errno = 0;
    int host_errno;
    int have_guest_errno;
    int have_guest_h_errno;
    int locale_result;

    if (!kzt_libc_semantic_process_ready()) {
        return;
    }
    if (!state || !state->depth ||
        kzt_libc_semantic_frame_at(state, state->depth - 1)->direction !=
            KZT_LIBC_HOST_TO_GUEST) {
        return;
    }
    have_guest_errno =
        kzt_libc_semantic_read_guest_errno(state, &guest_errno) == 0;
    have_guest_h_errno =
        kzt_libc_semantic_read_guest_h_errno(
            state, &guest_h_errno) == 0;
    g_mutex_lock(&kzt_libc_locale_sync_lock);
    locale_result =
        kzt_libc_capture_guest_locale_state(&locale_state) == 0 &&
        kzt_libc_install_host_locale_state(state, &locale_state) == 0
            ? 0 : -1;
    g_mutex_unlock(&kzt_libc_locale_sync_lock);
    if (locale_result != 0) {
        kzt_libc_semantic_abort_boundary(
            "cannot propagate Guest locale on callback return");
    }
    if (kzt_libc_semantic_pop(
            state, KZT_LIBC_HOST_TO_GUEST) != 0) {
        kzt_libc_semantic_abort_boundary(
            "unbalanced Host-to-Guest return");
    }
    if (!have_guest_errno) {
        kzt_libc_semantic_abort_boundary(
            "cannot read Guest errno on callback return");
    }
    if (!have_guest_h_errno) {
        kzt_libc_semantic_abort_boundary(
            "cannot read Guest h_errno on callback return");
    }
    host_errno = target_to_host_errno(guest_errno);
    errno = host_errno;
    h_errno = guest_h_errno;
}

void kzt_libc_semantic_guest_to_host_enter(CPUX86State *env)
{
    kzt_libc_semantic_state_t *state = kzt_libc_semantic_state(env);
    kzt_libc_locale_state_t locale_state;
    int guest_errno;
    int guest_h_errno;
    int locale_result;

    if (!kzt_libc_semantic_process_ready()) {
        return;
    }
    if (!state) {
        /*
         * Attached Host threads initialize Guest TLS before semantic
         * state.  Guest libc bootstrap helpers may cross a wrapped
         * symbol in that narrow interval, but no user callback can run
         * until both initializers succeed.
         */
        if (env && env->kzt_guest_tls_allocation) {
            return;
        }
        if (kzt_libc_semantic_initialize(env) != 0) {
            kzt_libc_semantic_abort_boundary(
                "cannot initialize per-thread state on Guest-to-Host entry");
        }
        state = kzt_libc_semantic_state(env);
    }
    if (state->internal_depth || state->internal_overflow_depth) {
        return;
    }
    if (kzt_libc_semantic_read_guest_errno(state, &guest_errno) != 0) {
        kzt_libc_semantic_abort_boundary(
            "cannot read Guest errno on Guest-to-Host entry");
    }
    if (kzt_libc_semantic_read_guest_h_errno(
            state, &guest_h_errno) != 0) {
        kzt_libc_semantic_abort_boundary(
            "cannot read Guest h_errno on Guest-to-Host entry");
    }
    if (!kzt_libc_semantic_push(state, KZT_LIBC_GUEST_TO_HOST)) {
        kzt_libc_semantic_abort_boundary(
            "cannot allocate Guest-to-Host call frame");
    }
    g_mutex_lock(&kzt_libc_locale_sync_lock);
    locale_result =
        kzt_libc_capture_guest_locale_state(&locale_state) == 0 &&
        kzt_libc_install_host_locale_state(state, &locale_state) == 0
            ? 0 : -1;
    g_mutex_unlock(&kzt_libc_locale_sync_lock);
    if (locale_result != 0) {
        kzt_libc_semantic_abort_boundary(
            "cannot propagate Guest locale on Guest-to-Host entry");
    }
    errno = target_to_host_errno(guest_errno);
    h_errno = guest_h_errno;
}

void kzt_libc_semantic_guest_to_host_leave(CPUX86State *env)
{
    kzt_libc_semantic_state_t *state = kzt_libc_semantic_state(env);
    kzt_libc_locale_state_t locale_state;
    unsigned char guest_fp_return[sizeof(env->fpregs[0])];
    unsigned char guest_xmm0_return[sizeof(env->xmm_regs[0])];
    unsigned char guest_xmm1_return[sizeof(env->xmm_regs[1])];
    uint64_t guest_rax;
    uint64_t guest_rdx;
    int guest_fpstt;
    int host_errno = errno;
    int host_h_errno = h_errno;
    int locale_result;

    if (!kzt_libc_semantic_process_ready()) {
        return;
    }
    if (!state) {
        if (env && env->kzt_guest_tls_allocation) {
            return;
        }
        return;
    }
    if (!state->depth ||
        kzt_libc_semantic_frame_at(state, state->depth - 1)->direction !=
            KZT_LIBC_GUEST_TO_HOST) {
        return;
    }
    if (state->internal_depth || state->internal_overflow_depth) {
        return;
    }
    guest_rax = env->regs[R_EAX];
    guest_rdx = env->regs[R_EDX];
    guest_fpstt = env->fpstt;
    memcpy(guest_xmm0_return, &env->xmm_regs[0],
           sizeof(guest_xmm0_return));
    memcpy(guest_xmm1_return, &env->xmm_regs[1],
           sizeof(guest_xmm1_return));
    memcpy(guest_fp_return, &env->fpregs[guest_fpstt & 7],
           sizeof(guest_fp_return));
    g_mutex_lock(&kzt_libc_locale_sync_lock);
    locale_result =
        kzt_libc_capture_host_locale_state(&locale_state) == 0 &&
        kzt_libc_install_guest_locale_state(state, &locale_state) == 0
            ? 0 : -1;
    g_mutex_unlock(&kzt_libc_locale_sync_lock);
    if (locale_result != 0) {
        kzt_libc_semantic_abort_boundary(
            "cannot propagate Host locale on Guest-to-Host return");
    }
    if (kzt_libc_semantic_pop(state, KZT_LIBC_GUEST_TO_HOST) != 0) {
        kzt_libc_semantic_abort_boundary(
            "unbalanced Guest-to-Host return");
    }
    env->regs[R_EAX] = guest_rax;
    env->regs[R_EDX] = guest_rdx;
    env->fpstt = guest_fpstt;
    memcpy(&env->xmm_regs[0], guest_xmm0_return,
           sizeof(guest_xmm0_return));
    memcpy(&env->xmm_regs[1], guest_xmm1_return,
           sizeof(guest_xmm1_return));
    memcpy(&env->fpregs[guest_fpstt & 7], guest_fp_return,
           sizeof(guest_fp_return));
    if (kzt_libc_semantic_write_guest_errno(
            state, host_to_target_errno(host_errno)) != 0) {
        kzt_libc_semantic_abort_boundary(
            "cannot write Guest errno on Guest-to-Host return");
    }
    if (kzt_libc_semantic_write_guest_h_errno(
            state, host_h_errno) != 0) {
        kzt_libc_semantic_abort_boundary(
            "cannot write Guest h_errno on Guest-to-Host return");
    }
    errno = host_errno;
    h_errno = host_h_errno;
}
