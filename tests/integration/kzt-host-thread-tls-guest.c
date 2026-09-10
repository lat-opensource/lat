#define _GNU_SOURCE
/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <langinfo.h>
#include <locale.h>
#include <netdb.h>
#include <pthread.h>
#include <resolv.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlibint.h>

#include "kzt-pthread-tsd-alias.h"
#include "x11-async-bridge-values.h"

typedef struct callback_context {
    Display *expected_display;
    int hits;
    int error;
    uintptr_t callback_entry;
} callback_context;

typedef int (*plugin_check_fn)(void);
typedef int (*plugin_counts_fn)(int *, int *);

static plugin_check_fn plugin_check;
static plugin_check_fn ie_plugin_check;
static plugin_check_fn cxx_plugin_check;
static plugin_check_fn tlsdesc_plugin_check;
static plugin_counts_fn cxx_plugin_counts;
static const char *plugin_path;
static const char *plugin_b_path;
static const char *ie_plugin_path;
static const char *cxx_plugin_path;
static const char *tlsdesc_plugin_path;
static void *plugin_handle;
static void *ie_plugin_handle;
static void *cxx_plugin_handle;
static void *tlsdesc_plugin_handle;
static size_t plugin_tls_module_id;
static int plugin_load_state;
static int ie_plugin_load_state;
static int cxx_plugin_load_state;
static int tlsdesc_plugin_load_state;
static int ie_plugin_load_waiters;
static pthread_key_t inherited_tsd_key;
static pthread_key_t callback_tsd_key;
static pthread_key_t alias_tsd_key;
static pthread_key_t reused_tsd_key;
static pthread_key_t iterating_tsd_key;
static int callback_tsd_destructor_count;
static int iterating_tsd_destructor_count;
static int tsd_reuse_phase;
static __thread int guest_static_tls_counter;
static __thread locale_t guest_c_locale;
static __thread locale_t guest_utf8_locale;
static __thread int guest_locale_phase;
static int plugin_refresh_phase;

static int32_t poisoned_tolower_storage[384];
static int32_t poisoned_toupper_storage[384];
static uint16_t poisoned_ctype_storage[384];
static struct __res_state *resolver_state_a;
static struct __res_state *resolver_state_b;
static int resolver_state_lock;
static const int32_t **parent_tolower_slot;
static const int32_t **parent_toupper_slot;
static const uint16_t **parent_ctype_slot;

static void callback_tsd_destructor(void *value)
{
    if (value == (void *)(uintptr_t)0x5678) {
        __sync_fetch_and_add(&callback_tsd_destructor_count, 1);
    }
}

static void iterating_tsd_destructor(void *value)
{
    uintptr_t remaining = (uintptr_t)value;

    __sync_fetch_and_add(&iterating_tsd_destructor_count, 1);
    if (remaining > 1) {
        pthread_setspecific(iterating_tsd_key,
                            (void *)(remaining - 1));
    }
}

typedef struct guest_dtv_entry {
    uintptr_t value;
    uintptr_t to_free;
} guest_dtv_entry;

static guest_dtv_entry *read_guest_dtv(void)
{
    guest_dtv_entry *dtv;

    __asm__ volatile("movq %%fs:8, %0" : "=r"(dtv));
    return dtv;
}

typedef struct guest_worker {
    Display *display;
    int result;
    int start;
    int done;
} guest_worker;

static void *run_native_callbacks_from_guest_worker(void *opaque)
{
    guest_worker *worker = opaque;

    while (!__atomic_load_n(&worker->start, __ATOMIC_ACQUIRE)) {
        sched_yield();
    }
    errno = EAGAIN;
    h_errno = HOST_NOT_FOUND;
    worker->result = XEventsQueued(worker->display, QueuedAfterReading);
    if (worker->result == ASYNC_PROBE_EVENTS_RETURN && errno != EIO) {
        worker->result = -70;
    }
    if (worker->result == ASYNC_PROBE_EVENTS_RETURN &&
        (h_errno != TRY_AGAIN || MB_CUR_MAX != 1 ||
         strcmp(setlocale(LC_CTYPE, NULL), "C") != 0)) {
        worker->result = -82;
    }
    __atomic_store_n(&worker->done, 1, __ATOMIC_RELEASE);
    return NULL;
}

static int parent_ctype_is_poisoned(void)
{
    return *__ctype_tolower_loc() == poisoned_tolower_storage + 128 &&
           *__ctype_toupper_loc() == poisoned_toupper_storage + 128 &&
           *__ctype_b_loc() == poisoned_ctype_storage + 128;
}

static unsigned long read_vmsize_kib(void)
{
    char line[256];
    FILE *status = fopen("/proc/self/status", "r");

    if (!status) {
        return 0;
    }
    while (fgets(line, sizeof(line), status)) {
        unsigned long size = 0;

        if (strncmp(line, "VmSize:", 7) == 0) {
            const char *cursor = line + 7;

            size = 0;
            while (*cursor == ' ' || *cursor == '\t') {
                ++cursor;
            }
            while (*cursor >= '0' && *cursor <= '9') {
                size = size * 10 + (unsigned long)(*cursor - '0');
                ++cursor;
            }
        }
        if (size) {
            fclose(status);
            return size;
        }
    }
    fclose(status);
    return 0;
}

static locale_t create_utf8_locale(void)
{
    locale_t locale = newlocale(LC_ALL_MASK, "C.UTF-8", NULL);

    if (!locale) {
        locale = newlocale(LC_ALL_MASK, "C.utf8", NULL);
    }
    return locale;
}

static int locale_has_expected_categories(locale_t locale, int utf8)
{
    static const int categories[] = {
        LC_CTYPE, LC_NUMERIC, LC_TIME, LC_COLLATE,
        LC_MONETARY, LC_MESSAGES, LC_PAPER, LC_NAME,
        LC_ADDRESS, LC_TELEPHONE, LC_MEASUREMENT,
        LC_IDENTIFICATION,
    };

    for (size_t index = 0;
         index < sizeof(categories) / sizeof(categories[0]); ++index) {
        const char *name = nl_langinfo_l(
            _NL_LOCALE_NAME(categories[index]), locale);

        if (!name || (utf8
                ? strcmp(name, "C.UTF-8") != 0 &&
                  strcmp(name, "C.utf8") != 0
                : strcmp(name, "C") != 0)) {
            return 0;
        }
    }
    return 1;
}

static int install_utf8_global_locale(void)
{
    return setlocale(LC_ALL, "C.UTF-8") != NULL ||
           setlocale(LC_ALL, "C.utf8") != NULL;
}

static int load_guest_tls_plugin(void)
{
    size_t current_module_id = 0;

    if (__sync_bool_compare_and_swap(&plugin_load_state, 0, 1)) {
        plugin_handle = dlopen(plugin_path, RTLD_NOW | RTLD_LOCAL);
        if (plugin_handle) {
            plugin_check = (plugin_check_fn)dlsym(
                plugin_handle, "kzt_host_thread_tls_plugin_check");
        }
        if (!plugin_handle || !plugin_check ||
            dlinfo(plugin_handle, RTLD_DI_TLS_MODID,
                   &plugin_tls_module_id) != 0 ||
            !plugin_tls_module_id) {
            __atomic_store_n(&plugin_load_state, -1, __ATOMIC_RELEASE);
            return -1;
        }
        __atomic_store_n(&plugin_load_state, 2, __ATOMIC_RELEASE);
    } else {
        while (__atomic_load_n(&plugin_load_state,
                               __ATOMIC_ACQUIRE) == 1) {
            sched_yield();
        }
    }
    if (__atomic_load_n(&plugin_load_state,
                        __ATOMIC_ACQUIRE) != 2 ||
        dlinfo(plugin_handle, RTLD_DI_TLS_MODID,
               &current_module_id) != 0 ||
        current_module_id != plugin_tls_module_id) {
        return -1;
    }
    return 0;
}

static int load_guest_ie_tls_plugin(void)
{
    if (__sync_bool_compare_and_swap(
            &ie_plugin_load_state, 0, 1)) {
        ie_plugin_handle = dlopen(
            ie_plugin_path, RTLD_NOW | RTLD_LOCAL);
        if (ie_plugin_handle) {
            ie_plugin_check = (plugin_check_fn)dlsym(
                ie_plugin_handle,
                "kzt_host_thread_tls_ie_plugin_check");
        }
        __atomic_store_n(
            &ie_plugin_load_state,
            ie_plugin_handle && ie_plugin_check ? 2 : -1,
            __ATOMIC_RELEASE);
    } else {
        while (__atomic_load_n(
                   &ie_plugin_load_state, __ATOMIC_ACQUIRE) == 1) {
            sched_yield();
        }
    }
    return __atomic_load_n(
               &ie_plugin_load_state, __ATOMIC_ACQUIRE) == 2
               ? 0 : -1;
}

static int load_guest_cxx_tls_plugin(void)
{
    if (__sync_bool_compare_and_swap(
            &cxx_plugin_load_state, 0, 1)) {
        cxx_plugin_handle = dlopen(
            cxx_plugin_path, RTLD_NOW | RTLD_LOCAL);
        if (cxx_plugin_handle) {
            cxx_plugin_check = (plugin_check_fn)dlsym(
                cxx_plugin_handle, "kzt_host_thread_cxx_tls_check");
            cxx_plugin_counts = (plugin_counts_fn)dlsym(
                cxx_plugin_handle, "kzt_host_thread_cxx_tls_counts");
        }
        __atomic_store_n(
            &cxx_plugin_load_state,
            cxx_plugin_handle && cxx_plugin_check && cxx_plugin_counts
                ? 2 : -1,
            __ATOMIC_RELEASE);
    } else {
        while (__atomic_load_n(
                   &cxx_plugin_load_state, __ATOMIC_ACQUIRE) == 1) {
            sched_yield();
        }
    }
    return __atomic_load_n(
               &cxx_plugin_load_state, __ATOMIC_ACQUIRE) == 2
               ? 0 : -1;
}

static int load_guest_tlsdesc_plugin(void)
{
    if (__sync_bool_compare_and_swap(
            &tlsdesc_plugin_load_state, 0, 1)) {
        tlsdesc_plugin_handle = dlopen(
            tlsdesc_plugin_path, RTLD_NOW | RTLD_LOCAL);
        if (tlsdesc_plugin_handle) {
            tlsdesc_plugin_check = (plugin_check_fn)dlsym(
                tlsdesc_plugin_handle,
                "kzt_host_thread_tls_tlsdesc_check");
        }
        __atomic_store_n(
            &tlsdesc_plugin_load_state,
            tlsdesc_plugin_handle && tlsdesc_plugin_check ? 2 : -1,
            __ATOMIC_RELEASE);
    } else {
        while (__atomic_load_n(
                   &tlsdesc_plugin_load_state,
                   __ATOMIC_ACQUIRE) == 1) {
            sched_yield();
        }
    }
    return __atomic_load_n(
               &tlsdesc_plugin_load_state, __ATOMIC_ACQUIRE) == 2
               ? 0 : -1;
}

static int check_cxx_tls_counts(int expected)
{
    int constructors = -1;
    int destructors = -1;

    if (!cxx_plugin_counts ||
        cxx_plugin_counts(&constructors, &destructors) != 0 ||
        constructors != expected || destructors != expected) {
        fprintf(stderr,
                "FAIL: C++ thread_local lifecycle constructors=%d "
                "destructors=%d expected=%d\n",
                constructors, destructors, expected);
        return -1;
    }
    return 0;
}

static Bool guest_tls_callback(Display *display, xReply *reply,
                               char *buffer, int length, XPointer opaque)
{
    callback_context *context = (callback_context *)opaque;
    const int32_t **tolower_table;
    const int32_t **toupper_table;
    const uint16_t **ctype_table;
    struct __res_state *resolver_state;
    int incoming_errno = errno;
    int incoming_h_errno = h_errno;
    int locale_phase = guest_locale_phase++;
    int phase = __atomic_load_n(
        &plugin_refresh_phase, __ATOMIC_ACQUIRE);
    int error = 0;

    if (display != context->expected_display || !reply || !buffer ||
        length != ASYNC_PROBE_REPLY_LENGTH) {
        error = 40;
    }
    if (!error && incoming_errno != ENOENT) {
        error = 68;
    }
    if (!error && incoming_h_errno != NO_RECOVERY) {
        error = 79;
    }
    if (!error && ((locale_phase & 1) ? MB_CUR_MAX != 1
                                      : MB_CUR_MAX <= 1)) {
        error = 71;
    }
    if (!guest_c_locale) {
        guest_c_locale = newlocale(LC_ALL_MASK, "C", NULL);
    }
    if (!guest_utf8_locale) {
        guest_utf8_locale = create_utf8_locale();
    }
    if (!error && (!guest_c_locale || !guest_utf8_locale ||
                   !uselocale((locale_phase & 1) ? guest_utf8_locale
                                                 : guest_c_locale))) {
        error = 80;
    }
    if (!error && ((locale_phase & 1) ? MB_CUR_MAX <= 1
                                      : MB_CUR_MAX != 1)) {
        error = 81;
    }
    if (!error && !locale_has_expected_categories(
                      uselocale((locale_t)0), locale_phase & 1)) {
        error = 120;
    }
    tolower_table = __ctype_tolower_loc();
    if (!error && (!tolower_table || !*tolower_table)) {
        error = 45;
    }
    if (!error && (*tolower_table)['A'] != 'a') {
        error = 46;
    }
    toupper_table = __ctype_toupper_loc();
    ctype_table = __ctype_b_loc();
    if (!error &&
        (tolower_table == parent_tolower_slot ||
         toupper_table == parent_toupper_slot ||
         ctype_table == parent_ctype_slot)) {
        error = 116;
    }
    if (!error && (!toupper_table || !*toupper_table ||
                   (*toupper_table)['a'] != 'A')) {
        fprintf(stderr,
                "FAIL: Guest toupper table slot=%p table=%p value=%d\n",
                (void *)toupper_table,
                toupper_table ? (void *)*toupper_table : NULL,
                toupper_table && *toupper_table
                    ? (*toupper_table)['a'] : -1);
        error = 101;
    }
    if (!error && (!ctype_table || !*ctype_table ||
                   !((*ctype_table)['A'] & _ISalpha))) {
        error = 102;
    }
    if (!error && uselocale((locale_t)0) == LC_GLOBAL_LOCALE) {
        error = 47;
    }
    if (!error && locale_phase == 0) {
        char numeric_name[128];
        const char *current_numeric = setlocale(LC_NUMERIC, NULL);

        if (!current_numeric ||
            snprintf(numeric_name, sizeof(numeric_name), "%s",
                     current_numeric) >= (int)sizeof(numeric_name) ||
            setlocale(LC_NUMERIC,
                      "kzt_INVALID_locale_DO_NOT_INSTALL") != NULL ||
            !setlocale(LC_NUMERIC, NULL) ||
            strcmp(setlocale(LC_NUMERIC, NULL), numeric_name) != 0) {
            error = 103;
        }
    }
    if (!error && locale_phase == 0) {
        void *missing_handle;
        const char *loader_error;

        (void)dlerror();
        missing_handle = dlopen(
            "/kzt/definitely/missing/libkzt-no-such-object.so",
            RTLD_NOW | RTLD_LOCAL);
        loader_error = dlerror();
        if (missing_handle || !loader_error || dlerror() != NULL) {
            if (missing_handle) {
                dlclose(missing_handle);
            }
            error = 121;
        }
    }
    resolver_state = __res_state();
    if (!error && !resolver_state) {
        error = 104;
    }
    if (!error && !resolver_state->retrans) {
        if (res_ninit(resolver_state) != 0 ||
            !(resolver_state->options & RES_INIT)) {
            error = 122;
        } else {
            resolver_state->retrans = 0;
        }
    }
    if (!error && phase != 2) {
        while (__sync_lock_test_and_set(&resolver_state_lock, 1)) {
            sched_yield();
        }
        if (!resolver_state_a) {
            resolver_state_a = resolver_state;
            resolver_state->retrans = 11;
        } else if (resolver_state_a == resolver_state) {
            if (resolver_state->retrans != 11) {
                error = 105;
            }
        } else if (!resolver_state_b) {
            resolver_state_b = resolver_state;
            resolver_state->retrans = 12;
        } else if (resolver_state_b == resolver_state) {
            if (resolver_state->retrans != 12) {
                error = 106;
            }
        } else {
            error = 107;
        }
        __sync_lock_release(&resolver_state_lock);
    } else if (!error) {
        if (!resolver_state->retrans) {
            resolver_state->retrans = 13;
        } else if (resolver_state->retrans != 13) {
            error = 110;
        }
    }
    if (!error && pthread_getspecific(inherited_tsd_key) != NULL) {
        error = 53;
    }
    if (!error && !pthread_getspecific(callback_tsd_key) &&
        pthread_setspecific(callback_tsd_key,
                            (void *)(uintptr_t)0x5678) != 0) {
        error = 73;
    }
    if (!error &&
        direct_pthread_getspecific(callback_tsd_key) !=
            (void *)(uintptr_t)0x5678) {
        error = 83;
    }
    if (!error && !direct_pthread_getspecific(alias_tsd_key) &&
        direct_pthread_setspecific(
            alias_tsd_key, (void *)(uintptr_t)0x9abc) != 0) {
        error = 84;
    }
    if (!error &&
        pthread_getspecific(alias_tsd_key) !=
            (void *)(uintptr_t)0x9abc) {
        error = 85;
    }
    if (!error) {
        void *expected = __atomic_load_n(
            &tsd_reuse_phase, __ATOMIC_ACQUIRE)
                ? (void *)(uintptr_t)0x2222
                : (void *)(uintptr_t)0x1111;
        void *current = pthread_getspecific(reused_tsd_key);

        if (!current && direct_pthread_setspecific(
                            reused_tsd_key, expected) == 0) {
            current = direct_pthread_getspecific(reused_tsd_key);
        }
        if (current != expected) {
            error = 86;
        }
    }
    if (!error && !pthread_getspecific(iterating_tsd_key) &&
        pthread_setspecific(iterating_tsd_key,
                            (void *)(uintptr_t)3) != 0) {
        error = 87;
    }
    if (!error && (phase == 1 || phase == 4 || phase == 6)) {
        int load_result = load_guest_tls_plugin();

        if (load_result != 0) {
            error = 62;
        }
    }
    if (!error && phase == 1) {
        if (__atomic_load_n(&ie_plugin_load_state,
                            __ATOMIC_ACQUIRE) == 0) {
            __sync_fetch_and_add(&ie_plugin_load_waiters, 1);
            while (__atomic_load_n(&ie_plugin_load_waiters,
                                   __ATOMIC_ACQUIRE) < 2) {
                sched_yield();
            }
        }
        if (load_guest_ie_tls_plugin() != 0) {
            error = 94;
        }
        if (!error && load_guest_cxx_tls_plugin() != 0) {
            error = 97;
        }
    }
    if (!error && phase == 6 &&
        load_guest_tlsdesc_plugin() != 0) {
        error = 118;
    }
    if (!error) {
        if ((phase == 0 && guest_static_tls_counter > 1) ||
            (phase == 1 && (guest_static_tls_counter < 2 ||
                            guest_static_tls_counter > 5)) ||
            (phase == 2 && guest_static_tls_counter > 1) ||
            (phase == 3 && (guest_static_tls_counter < 6 ||
                            guest_static_tls_counter > 7)) ||
            (phase == 4 && (guest_static_tls_counter < 8 ||
                            guest_static_tls_counter > 9)) ||
            (phase == 5 && (guest_static_tls_counter < 10 ||
                            guest_static_tls_counter > 11)) ||
            (phase == 6 && (guest_static_tls_counter < 12 ||
                            guest_static_tls_counter > 13)) ||
            (phase == 7 && (guest_static_tls_counter < 14 ||
                            guest_static_tls_counter > 15))) {
            error = 57;
        }
        ++guest_static_tls_counter;
    }
    if (!error && plugin_check) {
        guest_dtv_entry *dtv = read_guest_dtv();
        uintptr_t before = dtv[plugin_tls_module_id].value;
        uintptr_t generation_before = dtv[0].value;


        if (!before || before == UINTPTR_MAX) {
            fprintf(stderr,
                    "FAIL: Guest DTV module=%zu value=0x%lx\n",
                    plugin_tls_module_id, (unsigned long)before);
            error = 60;
        } else {
            error = plugin_check();
            if (!error &&
                (dtv[plugin_tls_module_id].value != before ||
                 (phase != 1 &&
                  dtv[0].value != generation_before))) {
                fprintf(stderr,
                        "FAIL: Guest DTV module=%zu changed 0x%lx->0x%lx\n",
                        plugin_tls_module_id, (unsigned long)before,
                        (unsigned long)dtv[plugin_tls_module_id].value);
                fprintf(stderr,
                        "FAIL: Guest DTV generation changed %lu->%lu\n",
                        (unsigned long)generation_before,
                        (unsigned long)dtv[0].value);
                error = 61;
            }
        }
    }
    if (!error && ie_plugin_check) {
        error = ie_plugin_check();
    }
    if (!error && cxx_plugin_check) {
        error = cxx_plugin_check();
        if (error && cxx_plugin_counts) {
            int constructors = -1;
            int destructors = -1;

            cxx_plugin_counts(&constructors, &destructors);
            fprintf(stderr,
                    "FAIL: C++ TLS check=%d constructors=%d "
                    "destructors=%d\n",
                    error, constructors, destructors);
        }
    }
    if (!error && tlsdesc_plugin_check) {
        error = tlsdesc_plugin_check();
    }
    if (!error) {
        int missing_fd;

        errno = 0;
        missing_fd = open(
            "/kzt/definitely/missing/kzt-errno-probe", O_RDONLY);
        if (missing_fd >= 0 || errno != ENOENT) {
            if (missing_fd >= 0) {
                close(missing_fd);
            }
            error = 122;
        }
    }
    if (error) {
        __sync_val_compare_and_swap(&context->error, 0, error);
    }
    __sync_fetch_and_add(&context->hits, 1);
    errno = EACCES;
    h_errno = NO_DATA;
    return False;
}

int main(int argc, char **argv)
{
    Display *display = calloc(1, sizeof(*display));
    _XAsyncHandler handler = { 0 };
    callback_context context = { 0 };
    const int32_t **main_tolower_table;
    const int32_t **main_toupper_table;
    const uint16_t **main_ctype_table;
    unsigned long pre_plugin_vmsize;
    unsigned long post_retire_vmsize;
    unsigned long baseline_vmsize;
    unsigned long final_vmsize;
    unsigned long parent_progress = 0;
    guest_worker worker = { 0 };
    pthread_t worker_thread;
    pthread_key_t old_reused_tsd_key;
    uintptr_t alias_tsd_value = UINT64_C(0x12345678);
    void *duplicate_plugin;
    int key_create_result;
    int callback_key_create_result;
    int key_set_result;
    int stress_iterations = 0;
    int expected_attached_threads;
    int result;

    if (argc != 7 || !display) {
        return 2;
    }
    plugin_path = argv[1];
    ie_plugin_path = argv[2];
    cxx_plugin_path = argv[3];
    plugin_b_path = argv[4];
    tlsdesc_plugin_path = argv[5];
    for (const char *cursor = argv[6]; *cursor; ++cursor) {
        if (*cursor < '0' || *cursor > '9' ||
            stress_iterations > 1000) {
            return 2;
        }
        stress_iterations =
            stress_iterations * 10 + (*cursor - '0');
    }
    if (!stress_iterations) {
        return 2;
    }
    expected_attached_threads = 4 + 2 * stress_iterations;
    key_create_result = pthread_key_create(&inherited_tsd_key, NULL);
    callback_key_create_result = pthread_key_create(
        &callback_tsd_key, callback_tsd_destructor);
    key_set_result = pthread_setspecific(
        inherited_tsd_key, (void *)(uintptr_t)0x1234);
    if (key_create_result != 0 || callback_key_create_result != 0 ||
        key_set_result != 0 ||
        pthread_key_create(&reused_tsd_key, NULL) != 0 ||
        pthread_key_create(&iterating_tsd_key,
                           iterating_tsd_destructor) != 0) {
        fprintf(stderr,
                "FAIL: Guest TSD setup create=%d callback_create=%d "
                "set=%d\n",
                key_create_result, callback_key_create_result,
                key_set_result);
        free(display);
        return 6;
    }
    if (direct_pthread_key_create(&alias_tsd_key, NULL) != 0 ||
        direct_pthread_setspecific(
            alias_tsd_key, &alias_tsd_value) != 0 ||
        direct_pthread_getspecific(alias_tsd_key) != &alias_tsd_value ||
        pthread_getspecific(alias_tsd_key) != &alias_tsd_value ||
        pthread_setspecific(alias_tsd_key,
                            (void *)(uintptr_t)0x1357) != 0 ||
        direct_pthread_getspecific(alias_tsd_key) !=
            (void *)(uintptr_t)0x1357) {
        fprintf(stderr,
                "FAIL: Guest pthread TSD aliases do not share one "
                "key/value namespace\n");
        free(display);
        return 79;
    }
    if (!install_utf8_global_locale()) {
        free(display);
        return 72;
    }
    poisoned_tolower_storage[128 + 'A'] = 'Z';
    poisoned_toupper_storage[128 + 'a'] = 'z';
    main_tolower_table = __ctype_tolower_loc();
    main_toupper_table = __ctype_toupper_loc();
    main_ctype_table = __ctype_b_loc();
    parent_tolower_slot = main_tolower_table;
    parent_toupper_slot = main_toupper_table;
    parent_ctype_slot = main_ctype_table;

    context.expected_display = display;
    context.callback_entry = (uintptr_t)guest_tls_callback;
    handler.handler = guest_tls_callback;
    handler.data = (XPointer)&context;
    display->async_handlers = &handler;

    worker.display = display;
    if (pthread_create(&worker_thread, NULL,
                       run_native_callbacks_from_guest_worker,
                       &worker) != 0) {
        pthread_key_delete(inherited_tsd_key);
        free(display);
        return 5;
    }
    *main_tolower_table = poisoned_tolower_storage + 128;
    *main_toupper_table = poisoned_toupper_storage + 128;
    *main_ctype_table = poisoned_ctype_storage + 128;
    if (!parent_ctype_is_poisoned()) {
        fprintf(stderr,
                "FAIL: cannot poison parent Guest ctype TLS\n");
        return 115;
    }
    __atomic_store_n(&worker.start, 1, __ATOMIC_RELEASE);
    while (!__atomic_load_n(&worker.done, __ATOMIC_ACQUIRE)) {
        ++parent_progress;
        sched_yield();
    }
    if (pthread_join(worker_thread, NULL) != 0 || !parent_progress ||
        worker.result != ASYNC_PROBE_EVENTS_RETURN || context.hits != 4 ||
        context.error != 0) {
        fprintf(stderr,
                "FAIL: concurrent parent Guest result=%d progress=%lu "
                "hits=%d error=%d\n",
                worker.result, parent_progress, context.hits, context.error);
        pthread_key_delete(inherited_tsd_key);
        free(display);
        return 52;
    }
    if (!resolver_state_a || !resolver_state_b ||
        resolver_state_a == resolver_state_b) {
        fprintf(stderr,
                "FAIL: attached Guest resolver states are not isolated "
                "a=%p b=%p\n",
                (void *)resolver_state_a, (void *)resolver_state_b);
        free(display);
        return 108;
    }
    old_reused_tsd_key = reused_tsd_key;
    if (pthread_key_delete(old_reused_tsd_key) != 0 ||
        direct_pthread_key_create(&reused_tsd_key, NULL) != 0 ||
        reused_tsd_key != old_reused_tsd_key) {
        fprintf(stderr,
                "FAIL: Guest TSD key reuse old=%u new=%u\n",
                (unsigned int)old_reused_tsd_key,
                (unsigned int)reused_tsd_key);
        free(display);
        return 88;
    }
    __atomic_store_n(&tsd_reuse_phase, 1, __ATOMIC_RELEASE);

    pre_plugin_vmsize = read_vmsize_kib();
    __atomic_store_n(&plugin_refresh_phase, 1, __ATOMIC_RELEASE);
    context.hits = 0;
    context.error = 0;

    result = XEventsQueued(display, QueuedAfterReading);
    if (result != ASYNC_PROBE_EVENTS_RETURN || context.hits != 4 ||
        context.error != 0) {
        fprintf(stderr,
                "FAIL: late Guest TLS result=%d hits=%d error=%d\n",
                result, context.hits, context.error);
        free(display);
        if (plugin_handle) {
            dlclose(plugin_handle);
        }
        return 58;
    }
    duplicate_plugin = dlopen(plugin_path, RTLD_NOW | RTLD_LOCAL);
    if (!duplicate_plugin || dlclose(duplicate_plugin) != 0) {
        free(display);
        return 66;
    }
    result = XEventsQueued(display, QueuedAfterReading);
    if (result != ASYNC_PROBE_EVENTS_RETURN || context.hits != 8 ||
        context.error != 0) {
        fprintf(stderr,
                "FAIL: repeated dlopen result=%d hits=%d error=%d\n",
                result, context.hits, context.error);
        free(display);
        return 67;
    }
    plugin_check = NULL;
    __atomic_store_n(&plugin_refresh_phase, 3, __ATOMIC_RELEASE);
    dlclose(plugin_handle);
    plugin_handle = NULL;
    result = XEventsQueued(display, QueuedAfterReading);
    if (result != ASYNC_PROBE_EVENTS_RETURN || context.hits != 12 ||
        context.error != 0) {
        fprintf(stderr,
                "FAIL: retired Guest TLS result=%d hits=%d error=%d\n",
                result, context.hits, context.error);
        free(display);
        return 59;
    }
    __atomic_store_n(&plugin_load_state, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&plugin_refresh_phase, 4, __ATOMIC_RELEASE);
    context.hits = 0;
    context.error = 0;
    result = XEventsQueued(display, QueuedAfterReading);
    if (result != ASYNC_PROBE_EVENTS_RETURN || context.hits != 4 ||
        context.error != 0) {
        fprintf(stderr,
                "FAIL: reloaded Guest TLS result=%d hits=%d error=%d\n",
                result, context.hits, context.error);
        free(display);
        return 77;
    }
    plugin_check = NULL;
    __atomic_store_n(&plugin_refresh_phase, 5, __ATOMIC_RELEASE);
    dlclose(plugin_handle);
    plugin_handle = NULL;
    result = XEventsQueued(display, QueuedAfterReading);
    if (result != ASYNC_PROBE_EVENTS_RETURN || context.hits != 8 ||
        context.error != 0) {
        fprintf(stderr,
                "FAIL: reloaded Guest TLS retirement result=%d hits=%d "
                "error=%d\n",
                result, context.hits, context.error);
        free(display);
        return 78;
    }
    plugin_path = plugin_b_path;
    __atomic_store_n(&plugin_load_state, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&plugin_refresh_phase, 6, __ATOMIC_RELEASE);
    context.hits = 0;
    context.error = 0;
    result = XEventsQueued(display, QueuedAfterReading);
    if (result != ASYNC_PROBE_EVENTS_RETURN || context.hits != 4 ||
        context.error != 0) {
        fprintf(stderr,
                "FAIL: replacement Guest TLS result=%d hits=%d error=%d\n",
                result, context.hits, context.error);
        free(display);
        return 112;
    }
    plugin_check = NULL;
    __atomic_store_n(&plugin_refresh_phase, 7, __ATOMIC_RELEASE);
    if (dlclose(plugin_handle) != 0) {
        free(display);
        return 113;
    }
    plugin_handle = NULL;
    result = XEventsQueued(display, QueuedAfterReading);
    if (result != ASYNC_PROBE_EVENTS_RETURN || context.hits != 8 ||
        context.error != 0) {
        fprintf(stderr,
                "FAIL: replacement Guest TLS retirement result=%d "
                "hits=%d error=%d\n",
                result, context.hits, context.error);
        free(display);
        return 114;
    }
    post_retire_vmsize = read_vmsize_kib();
    if (!pre_plugin_vmsize || !post_retire_vmsize ||
        post_retire_vmsize > pre_plugin_vmsize + 12 * 1024) {
        fprintf(stderr,
                "FAIL: late Guest TLS allocation was not retired "
                "VmSize=%lu->%lu kB\n",
                pre_plugin_vmsize, post_retire_vmsize);
        free(display);
        return 54;
    }
    if (XFlush(display) != ASYNC_PROBE_FLUSH_RETURN) {
        free(display);
        return 55;
    }
    if (__atomic_load_n(&callback_tsd_destructor_count,
                        __ATOMIC_ACQUIRE) != 2) {
        fprintf(stderr, "FAIL: persistent Guest TSD destructors=%d\n",
                callback_tsd_destructor_count);
        free(display);
        return 74;
    }
    if (__atomic_load_n(&iterating_tsd_destructor_count,
                        __ATOMIC_ACQUIRE) != 6) {
        fprintf(stderr, "FAIL: persistent iterating Guest TSD "
                        "destructors=%d\n",
                iterating_tsd_destructor_count);
        free(display);
        return 89;
    }
    if (check_cxx_tls_counts(2) != 0) {
        free(display);
        return 98;
    }
    __atomic_store_n(&plugin_refresh_phase, 2, __ATOMIC_RELEASE);
    context.hits = 0;
    context.error = 0;
    result = XEventsQueued(display, QueuedAfterReading);
    baseline_vmsize = read_vmsize_kib();
    for (int iteration = 0; iteration < stress_iterations; ++iteration) {
        if (result == ASYNC_PROBE_EVENTS_RETURN) {
            result = XEventsQueued(display, QueuedAfterReading);
        }
    }
    for (int retry = 0; retry < 50; ++retry) {
        final_vmsize = read_vmsize_kib();
        if (final_vmsize &&
            final_vmsize <= baseline_vmsize + 16 * 1024) {
            break;
        }
        usleep(20 * 1000);
    }
    if (result != ASYNC_PROBE_EVENTS_RETURN ||
        context.hits != 4 * (stress_iterations + 1) ||
        context.error != 0 || handler.handler == guest_tls_callback) {
        fprintf(stderr,
                "FAIL: native-thread TLS result=%d hits=%d error=%d\n",
                result, context.hits, context.error);
        free(display);
        return context.error ? context.error : 50;
    }
    if (!baseline_vmsize || !final_vmsize ||
        final_vmsize > baseline_vmsize + 16 * 1024) {
        fprintf(stderr,
                "FAIL: native-thread TLS resources leaked VmSize=%lu->%lu kB\n",
                baseline_vmsize, final_vmsize);
        free(display);
        return 51;
    }
    if (__atomic_load_n(&callback_tsd_destructor_count,
                        __ATOMIC_ACQUIRE) != expected_attached_threads) {
        fprintf(stderr, "FAIL: total Guest TSD destructors=%d\n",
                callback_tsd_destructor_count);
        free(display);
        return 75;
    }
    if (__atomic_load_n(&iterating_tsd_destructor_count,
                        __ATOMIC_ACQUIRE) !=
                            3 * expected_attached_threads) {
        fprintf(stderr, "FAIL: total iterating Guest TSD destructors=%d\n",
                iterating_tsd_destructor_count);
        free(display);
        return 90;
    }
    if (check_cxx_tls_counts(expected_attached_threads) != 0) {
        free(display);
        return 99;
    }
    pthread_key_delete(callback_tsd_key);
    pthread_key_delete(inherited_tsd_key);
    pthread_key_delete(alias_tsd_key);
    pthread_key_delete(reused_tsd_key);
    pthread_key_delete(iterating_tsd_key);
    if (ie_plugin_handle && dlclose(ie_plugin_handle) != 0) {
        free(display);
        return 95;
    }
    if (cxx_plugin_handle && dlclose(cxx_plugin_handle) != 0) {
        free(display);
        return 100;
    }
    if (tlsdesc_plugin_handle &&
        dlclose(tlsdesc_plugin_handle) != 0) {
        free(display);
        return 119;
    }
    free(display);
    fputs("PASS: native Host threads synchronized Guest libc semantics "
          "without leaks\n",
          stderr);
    return 0;
}
