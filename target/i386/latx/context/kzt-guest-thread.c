/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <errno.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "box64context.h"
#include "callback.h"
#include "debug.h"
#include "elfloader.h"
#include "kzt-guest-tls.h"
#include "kzt-guest-thread.h"
#include "myalign.h"
#include "qemu.h"

#define KZT_LIBC_TSD_KEYS 1024
#define KZT_LIBC_TSD_DESTRUCTOR_ITERATIONS 4
#define KZT_LIBC_TSD_REFRESH_RETRIES 1000
#define KZT_LIBC_TSD_REFRESH_RETRY_US 1000
#define KZT_LIBC_CXA_TLS_DESTRUCTORS 1024

typedef struct kzt_libc_tsd_key {
    uintptr_t destructor;
    int in_use;
} kzt_libc_tsd_key_t;

typedef struct kzt_libc_guest_tsd_helpers {
    uintptr_t key_create;
    uintptr_t key_delete;
    uintptr_t setspecific;
    uintptr_t getspecific;
    uintptr_t cxa_thread_atexit_impl;
} kzt_libc_guest_tsd_helpers_t;

typedef struct kzt_libc_cxa_tls_destructor {
    uintptr_t destructor;
    uintptr_t object;
    uintptr_t dso_handle;
    uintptr_t loader_hold;
} kzt_libc_cxa_tls_destructor_t;


typedef struct kzt_guest_thread_state {
    kzt_libc_cxa_tls_destructor_t
        cxa_tls_destructors[KZT_LIBC_CXA_TLS_DESTRUCTORS];
    size_t cxa_tls_destructor_count;
} kzt_guest_thread_state_t;

static kzt_libc_guest_tsd_helpers_t *guest_tsd_helpers;
static GMutex kzt_libc_tsd_resolve_lock;
static kzt_libc_tsd_key_t kzt_libc_tsd_keys[KZT_LIBC_TSD_KEYS];
static GMutex kzt_libc_tsd_lock;
static GMutex kzt_libc_tsd_operation_lock;

static kzt_guest_thread_state_t *kzt_guest_thread_state(CPUX86State *env)
{
    return env ? env->kzt_guest_thread_state : NULL;
}

void kzt_guest_thread_initialize(CPUX86State *env)
{
    g_assert(latx_kzt_guest_tls_enabled());
    g_assert(env && env->kzt_guest_tls_allocation);
    env->kzt_guest_thread_state = g_new0(kzt_guest_thread_state_t, 1);
}

void kzt_guest_thread_after_fork_child(void)
{
    GMutex *locks[] = {
        &kzt_libc_tsd_resolve_lock,
        &kzt_libc_tsd_operation_lock,
        &kzt_libc_tsd_lock,
    };

    if (!latx_kzt_guest_tls_enabled()) {
        return;
    }
    for (size_t i = 0; i < G_N_ELEMENTS(locks); ++i) {
        memset(locks[i], 0, sizeof(*locks[i]));
        g_mutex_init(locks[i]);
    }
}

static int kzt_libc_pthread_error(int error_number)
{
    return host_to_target_errno(error_number);
}

static void QEMU_NORETURN kzt_guest_thread_abort(const char *reason)
{
    fprintf(stderr, "KZT Guest thread cleanup failed: %s\n", reason);
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

static uintptr_t kzt_find_guest_pthread_symbol(const char *name)
{
    if (!my_context || !name) {
        return 0;
    }

    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < my_context->elfsize; ++i) {
            elfheader_t *head = my_context->elfs[i];
            const char *elf_name;
            const char *base_name;
            int matches;

            if (!head) {
                continue;
            }
            elf_name = ElfName(head);
            if (!elf_name) {
                continue;
            }
            base_name = strrchr(elf_name, '/');
            base_name = base_name ? base_name + 1 : elf_name;
            if (pass == 0) {
                matches = strcmp(base_name, "libc.so.6") == 0 ||
                          strncmp(base_name, "libc-", 5) == 0;
            } else {
                matches = strcmp(base_name, "libpthread.so.0") == 0 ||
                          strncmp(base_name, "libpthread-", 11) == 0;
            }
            if (matches) {
                uintptr_t address = FindElfSymbolAddress(head, name);

                if (address) {
                    return address;
                }
            }
        }
    }
    return 0;
}

static int kzt_libc_resolve_guest_tsd(
    const kzt_libc_guest_tsd_helpers_t **resolved)
{
    kzt_libc_guest_tsd_helpers_t candidate = { 0 };
    kzt_libc_guest_tsd_helpers_t *published;

    if (!resolved) {
        return -1;
    }
    published = g_atomic_pointer_get(&guest_tsd_helpers);
    if (published) {
        *resolved = published;
        return 0;
    }

    g_mutex_lock(&kzt_libc_tsd_resolve_lock);
    published = g_atomic_pointer_get(&guest_tsd_helpers);
    if (published) {
        g_mutex_unlock(&kzt_libc_tsd_resolve_lock);
        *resolved = published;
        return 0;
    }

    candidate.key_create =
        kzt_find_guest_pthread_symbol("pthread_key_create");
    candidate.key_delete =
        kzt_find_guest_pthread_symbol("pthread_key_delete");
    candidate.setspecific =
        kzt_find_guest_pthread_symbol("pthread_setspecific");
    candidate.getspecific =
        kzt_find_guest_pthread_symbol("pthread_getspecific");
    if (!candidate.key_create) {
        candidate.key_create = kzt_resolve_guest_object_symbol(
            "libc.so.6", "pthread_key_create");
    }
    if (!candidate.key_delete) {
        candidate.key_delete = kzt_resolve_guest_object_symbol(
            "libc.so.6", "pthread_key_delete");
    }
    if (!candidate.setspecific) {
        candidate.setspecific = kzt_resolve_guest_object_symbol(
            "libc.so.6", "pthread_setspecific");
    }
    if (!candidate.getspecific) {
        candidate.getspecific = kzt_resolve_guest_object_symbol(
            "libc.so.6", "pthread_getspecific");
    }
    if (!candidate.key_create) {
        candidate.key_create = kzt_resolve_guest_object_symbol(
            "libpthread.so.0", "pthread_key_create");
    }
    if (!candidate.key_delete) {
        candidate.key_delete = kzt_resolve_guest_object_symbol(
            "libpthread.so.0", "pthread_key_delete");
    }
    if (!candidate.setspecific) {
        candidate.setspecific = kzt_resolve_guest_object_symbol(
            "libpthread.so.0", "pthread_setspecific");
    }
    if (!candidate.getspecific) {
        candidate.getspecific = kzt_resolve_guest_object_symbol(
            "libpthread.so.0", "pthread_getspecific");
    }
    candidate.cxa_thread_atexit_impl =
        kzt_find_guest_libc_symbol("__cxa_thread_atexit_impl");
    if (!candidate.cxa_thread_atexit_impl) {
        candidate.cxa_thread_atexit_impl =
            kzt_resolve_guest_object_symbol(
                "libc.so.6", "__cxa_thread_atexit_impl");
    }
    if (!candidate.key_create || !candidate.key_delete ||
        !candidate.setspecific || !candidate.getspecific ||
        !candidate.cxa_thread_atexit_impl) {
        printf_log(LOG_INFO,
                   "KZT cannot resolve Guest thread-state helpers\n");
        g_mutex_unlock(&kzt_libc_tsd_resolve_lock);
        return -1;
    }
    published = g_new(kzt_libc_guest_tsd_helpers_t, 1);
    *published = candidate;
    g_atomic_pointer_set(&guest_tsd_helpers, published);
    g_mutex_unlock(&kzt_libc_tsd_resolve_lock);
    *resolved = published;
    return 0;
}


int kzt_guest_thread_key_create(CPUX86State *env,
                                      unsigned int *key,
                                      uintptr_t destructor)
{
    const kzt_libc_guest_tsd_helpers_t *helpers;
    int result;

    if (!env || !key) {
        return kzt_libc_pthread_error(EINVAL);
    }
    if (kzt_libc_resolve_guest_tsd(&helpers) != 0) {
        return kzt_libc_pthread_error(EAGAIN);
    }
    g_mutex_lock(&kzt_libc_tsd_operation_lock);
    result = (int)RunFunctionWithStateInternalNoRefresh(
        helpers->key_create, 2,
        (uint64_t)(uintptr_t)key, (uint64_t)destructor);
    if (result != 0) {
        g_mutex_unlock(&kzt_libc_tsd_operation_lock);
        return result;
    }
    if (*key >= KZT_LIBC_TSD_KEYS) {
        (void)RunFunctionWithStateInternalNoRefresh(
            helpers->key_delete, 1, (uint64_t)*key);
        g_mutex_unlock(&kzt_libc_tsd_operation_lock);
        return kzt_libc_pthread_error(EAGAIN);
    }

    g_mutex_lock(&kzt_libc_tsd_lock);
    kzt_libc_tsd_keys[*key] = (kzt_libc_tsd_key_t) {
        .destructor = destructor,
        .in_use = 1,
    };
    g_mutex_unlock(&kzt_libc_tsd_lock);
    g_mutex_unlock(&kzt_libc_tsd_operation_lock);
    return 0;
}

int kzt_guest_thread_key_delete(CPUX86State *env,
                                      unsigned int key)
{
    const kzt_libc_guest_tsd_helpers_t *helpers;
    int result;

    if (!env || kzt_libc_resolve_guest_tsd(&helpers) != 0) {
        return kzt_libc_pthread_error(EINVAL);
    }
    g_mutex_lock(&kzt_libc_tsd_operation_lock);
    result = (int)RunFunctionWithStateInternalNoRefresh(
        helpers->key_delete, 1, (uint64_t)key);
    if (result != 0) {
        g_mutex_unlock(&kzt_libc_tsd_operation_lock);
        return result;
    }
    if (key >= KZT_LIBC_TSD_KEYS) {
        g_mutex_unlock(&kzt_libc_tsd_operation_lock);
        return 0;
    }

    g_mutex_lock(&kzt_libc_tsd_lock);
    memset(&kzt_libc_tsd_keys[key], 0,
           sizeof(kzt_libc_tsd_keys[key]));
    g_mutex_unlock(&kzt_libc_tsd_lock);
    g_mutex_unlock(&kzt_libc_tsd_operation_lock);
    return 0;
}

static int kzt_libc_hold_cxa_dso(uintptr_t owner_address,
                                 uintptr_t *loader_hold)
{
    kzt_x86_64_link_map_prefix_t map;
    uintptr_t link_map_addr;
    void *locked_map;
    char *first_name_byte;

    if (!owner_address || !loader_hold || !my_context ||
        !my_context->dlprivate) {
        return -1;
    }
    *loader_hold = 0;
    link_map_addr = kzt_find_guest_link_map_by_address(owner_address);
    if (!link_map_addr) {
        return -1;
    }
    locked_map = lock_user(
        VERIFY_READ, (abi_ulong)link_map_addr, sizeof(map), 1);
    if (!locked_map) {
        return -1;
    }
    memcpy(&map, locked_map, sizeof(map));
    unlock_user(locked_map, (abi_ulong)link_map_addr, 0);
    if (!map.name) {
        return 0;
    }
    first_name_byte = lock_user(
        VERIFY_READ, (abi_ulong)map.name, 1, 1);
    if (!first_name_byte) {
        return -1;
    }
    if (!*first_name_byte) {
        unlock_user(first_name_byte, (abi_ulong)map.name, 0);
        return 0;
    }
    unlock_user(first_name_byte, (abi_ulong)map.name, 0);
    if (!my_context->dlprivate->x86dlopen ||
        !my_context->dlprivate->x86dlclose) {
        return -1;
    }
    *loader_hold = kzt_guest_loader_hold_open(map.name);
    return *loader_hold ? 0 : -1;
}

int kzt_guest_thread_cxa_atexit(
    CPUX86State *env, uintptr_t destructor,
    uintptr_t object, uintptr_t dso_handle)
{
    const kzt_libc_guest_tsd_helpers_t *helpers;
    kzt_guest_thread_state_t *state =
        kzt_guest_thread_state(env);
    kzt_libc_cxa_tls_destructor_t *entry;
    uintptr_t loader_hold = 0;

    if (!env || !destructor || !object ||
        kzt_libc_resolve_guest_tsd(&helpers) != 0) {
        return -1;
    }
    if (!env->kzt_guest_tls_allocation) {
        return (int)RunFunctionWithStateInternalNoRefresh(
            helpers->cxa_thread_atexit_impl, 3,
            destructor, object, dso_handle);
    }
    if (!state ||
        state->cxa_tls_destructor_count ==
            KZT_LIBC_CXA_TLS_DESTRUCTORS ||
        kzt_libc_hold_cxa_dso(
            dso_handle ? dso_handle : destructor,
            &loader_hold) != 0) {
        return -1;
    }
    entry = &state->cxa_tls_destructors[
        state->cxa_tls_destructor_count++];
    *entry = (kzt_libc_cxa_tls_destructor_t) {
        .destructor = destructor,
        .object = object,
        .dso_handle = dso_handle,
        .loader_hold = loader_hold,
    };
    return 0;
}

static void kzt_guest_thread_run_cxa_destructors(
    kzt_guest_thread_state_t *state)
{
    while (state->cxa_tls_destructor_count) {
        kzt_libc_cxa_tls_destructor_t entry =
            state->cxa_tls_destructors[
                --state->cxa_tls_destructor_count];

        memset(&state->cxa_tls_destructors[
                   state->cxa_tls_destructor_count],
               0, sizeof(entry));
        RunFunctionWithStateInternalNoRefresh(
            entry.destructor, 1, entry.object);
        if (entry.loader_hold) {
            kzt_guest_loader_hold_close(entry.loader_hold);
        }
    }
}

static void kzt_guest_thread_run_tsd_destructors(CPUX86State *env)
{
    const kzt_libc_guest_tsd_helpers_t *helpers;

    if (kzt_libc_resolve_guest_tsd(&helpers) != 0) {
        kzt_guest_thread_abort(
            "cannot resolve Guest pthread TSD destructors");
    }
    for (int iteration = 0;
         iteration < KZT_LIBC_TSD_DESTRUCTOR_ITERATIONS; ++iteration) {
        int called = 0;

        for (unsigned int key = 0; key < KZT_LIBC_TSD_KEYS; ++key) {
            uintptr_t destructor = 0;
            void *value = NULL;

            g_mutex_lock(&kzt_libc_tsd_lock);
            if (kzt_libc_tsd_keys[key].in_use) {
                destructor = kzt_libc_tsd_keys[key].destructor;
            }
            g_mutex_unlock(&kzt_libc_tsd_lock);

            if (!destructor) {
                continue;
            }
            value = (void *)(uintptr_t)
                RunFunctionWithStateInternalNoRefresh(
                    helpers->getspecific, 1, (uint64_t)key);
            if (value) {
                if ((int)RunFunctionWithStateInternalNoRefresh(
                        helpers->setspecific, 2,
                        (uint64_t)key, 0) != 0) {
                    kzt_guest_thread_abort(
                        "cannot clear Guest pthread TSD value");
                }
                RunFunctionWithStateInternalNoRefresh(
                    destructor, 1, (uint64_t)(uintptr_t)value);
                called = 1;
            }
        }
        if (!called) {
            break;
        }
    }
}

void kzt_guest_thread_destroy(CPUX86State *env)
{
    kzt_guest_thread_state_t *state;
    int refresh_result = -1;

    if (!env) {
        return;
    }
    state = env->kzt_guest_thread_state;
    if (!state) {
        return;
    }
    for (int attempt = 0;
         attempt < KZT_LIBC_TSD_REFRESH_RETRIES; ++attempt) {
        refresh_result = kzt_guest_tls_refresh(env);
        if (refresh_result == 0) {
            break;
        }
        g_usleep(KZT_LIBC_TSD_REFRESH_RETRY_US);
    }
    if (refresh_result != 0) {
        kzt_guest_thread_abort(
            "Guest TLS refresh timed out before thread destructors");
    }
    kzt_guest_tls_execution_enter(env);
    kzt_guest_thread_run_cxa_destructors(state);
    kzt_guest_thread_run_tsd_destructors(env);
    kzt_guest_tls_execution_leave(env);
    g_free(state);
    env->kzt_guest_thread_state = NULL;
}
