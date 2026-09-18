#define _GNU_SOURCE
/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <dlfcn.h>
#include <errno.h>
#include <linux/futex.h>
#include <locale.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlibint.h>

#include "x11-async-bridge-values.h"
#include "kzt-tls-dlopen-stress-shared.h"

#define STRESS_ITERATIONS 64
#define FORK_FUTEX_ITERATIONS 16

typedef int (*plugin_check_fn)(void);

static plugin_check_fn plugin_check;
static const char *attached_plugin_path;
static const char *concurrent_plugin_paths[2];
static size_t plugin_module_id;
static int callback_error;
static int callback_hits;
static int run_attached_dlopen;
static unsigned int concurrent_plugin_index;
static int fork_child_after_dlopen;
static int fmt_callback_hits;

typedef struct fork_futex_state {
    _Atomic uint32_t child_word;
    _Atomic uint32_t main_word;
    _Atomic uint32_t worker_ready;
    _Atomic uint32_t child_stage;
} fork_futex_state;

static fork_futex_state *fork_state;

static int exercise_fork_child_semantics(void)
{
    pthread_key_t key;
    void *key_value = (void *)(uintptr_t)0x1357;
    locale_t locale;
    locale_t previous;

    atomic_store_explicit(
        &fork_state->child_stage, 1, memory_order_release);
    if (pthread_key_create(&key, NULL) != 0 ||
        pthread_setspecific(key, key_value) != 0 ||
        pthread_getspecific(key) != key_value ||
        pthread_key_delete(key) != 0) {
        return -1;
    }
    atomic_store_explicit(
        &fork_state->child_stage, 2, memory_order_release);
    if (!setlocale(LC_NUMERIC, NULL)) {
        return -1;
    }
    atomic_store_explicit(
        &fork_state->child_stage, 3, memory_order_release);
    locale = newlocale(LC_ALL_MASK, "C", NULL);
    if (!locale) {
        return -1;
    }
    previous = uselocale(locale);
    if (!previous || !uselocale(previous)) {
        freelocale(locale);
        return -1;
    }
    freelocale(locale);
    atomic_store_explicit(
        &fork_state->child_stage, 4, memory_order_release);
    return 0;
}

static int stress_futex_wait(_Atomic uint32_t *word, uint32_t expected,
                             int private)
{
    int operation = FUTEX_WAIT | (private ? FUTEX_PRIVATE_FLAG : 0);

    return syscall(SYS_futex, word, operation, expected,
                   NULL, NULL, 0);
}

static int stress_futex_wait_timed(
    _Atomic uint32_t *word, uint32_t expected, int private)
{
    const struct timespec timeout = {
        .tv_sec = 0,
        .tv_nsec = 10 * 1000 * 1000,
    };
    int operation = FUTEX_WAIT | (private ? FUTEX_PRIVATE_FLAG : 0);

    return syscall(SYS_futex, word, operation, expected,
                   &timeout, NULL, 0);
}

static void stress_futex_wake(_Atomic uint32_t *word, int private)
{
    int operation = FUTEX_WAKE | (private ? FUTEX_PRIVATE_FLAG : 0);

    (void)syscall(SYS_futex, word, operation, 1, NULL, NULL, 0);
}

static void *run_fork_futex_worker(void *opaque)
{
    (void)opaque;
    atomic_store_explicit(
        &fork_state->worker_ready, 1, memory_order_release);
    stress_futex_wake(&fork_state->worker_ready, 0);
    while (atomic_load_explicit(
               &fork_state->child_word,
               memory_order_acquire) == UINT32_C(0x80000000)) {
        if (stress_futex_wait(
                &fork_state->child_word, UINT32_C(0x80000000), 0) != 0 &&
            errno != EAGAIN && errno != EINTR) {
            return (void *)1;
        }
    }
    atomic_store_explicit(
        &fork_state->main_word, 1, memory_order_release);
    stress_futex_wake(&fork_state->main_word, 1);
    return NULL;
}

static int run_fork_futex_child(const char *fd_text,
                                const char *plugin_path,
                                const char *expected_runtime_root)
{
    const char *runtime_root = getenv("LAT_LD_PREFIX");
    pthread_key_t key;
    void *key_value = (void *)(uintptr_t)0x2468;
    void *handle;
    plugin_check_fn check;
    int fd = 0;

    if (!fd_text[0] || !plugin_path || !expected_runtime_root ||
        !runtime_root || strcmp(runtime_root, expected_runtime_root) != 0) {
        return -1;
    }
    for (const char *cursor = fd_text; *cursor; ++cursor) {
        if (*cursor < '0' || *cursor > '9' ||
            fd > (INT32_MAX - (*cursor - '0')) / 10) {
            return -1;
        }
        fd = fd * 10 + (*cursor - '0');
    }
    fork_state = mmap(NULL, sizeof(*fork_state),
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    if (fork_state == MAP_FAILED) {
        return -1;
    }
    if (pthread_key_create(&key, NULL) != 0 ||
        pthread_setspecific(key, key_value) != 0 ||
        pthread_getspecific(key) != key_value) {
        return -1;
    }
    handle = dlopen(plugin_path, RTLD_NOW | RTLD_LOCAL);
    check = handle ? (plugin_check_fn)dlsym(
                         handle, "kzt_host_thread_tls_plugin_check")
                   : NULL;
    if (!handle || !check || check() != 0 || dlclose(handle) != 0 ||
        pthread_key_delete(key) != 0) {
        return -1;
    }
    atomic_store_explicit(
        &fork_state->child_word, 0, memory_order_release);
    stress_futex_wake(&fork_state->child_word, 0);
    return munmap(fork_state, sizeof(*fork_state));
}

static int run_fork_futex_topology(const char *program_path,
                                   const char *plugin_path)
{
    const char *runtime_root = getenv("LAT_LD_PREFIX");
    int shared_fd = syscall(
        SYS_memfd_create, "kzt-fork-futex", 0);

    if (!program_path || !plugin_path || !runtime_root || shared_fd < 0 ||
        ftruncate(shared_fd, sizeof(*fork_state)) != 0) {
        return -1;
    }
    fork_state = mmap(NULL, sizeof(*fork_state),
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED, shared_fd, 0);
    if (fork_state == MAP_FAILED) {
        return -1;
    }
    for (int iteration = 0;
         iteration < FORK_FUTEX_ITERATIONS; ++iteration) {
        pthread_t thread;
        pid_t child;
        pid_t waited = 0;
        int status;
        int wait_timeouts = 0;
        void *thread_result = NULL;

        atomic_store_explicit(
            &fork_state->child_word,
            UINT32_C(0x80000000), memory_order_relaxed);
        atomic_store_explicit(
            &fork_state->main_word, 0, memory_order_relaxed);
        atomic_store_explicit(
            &fork_state->worker_ready, 0, memory_order_relaxed);
        atomic_store_explicit(
            &fork_state->child_stage, 0, memory_order_relaxed);
        if (pthread_create(
                &thread, NULL, run_fork_futex_worker, NULL) != 0) {
            return -1;
        }
        while (!atomic_load_explicit(
                    &fork_state->worker_ready, memory_order_acquire)) {
            if (stress_futex_wait(
                    &fork_state->worker_ready, 0, 0) != 0 &&
                errno != EAGAIN && errno != EINTR) {
                return -1;
            }
        }
        child = fork();
        if (child == 0) {
            if (iteration & 1) {
                char fd_text[24];

                snprintf(fd_text, sizeof(fd_text), "%d", shared_fd);
                execl(program_path, program_path,
                      "--fork-futex-child", fd_text,
                      plugin_path, runtime_root, NULL);
                _exit(127);
            } else {
                if (exercise_fork_child_semantics() != 0) {
                    _exit(20);
                }
                atomic_store_explicit(
                    &fork_state->child_word, 0, memory_order_release);
                stress_futex_wake(&fork_state->child_word, 0);
                _exit(0);
            }
        }
        if (child < 0) {
            return -1;
        }
        while (!atomic_load_explicit(
                    &fork_state->main_word, memory_order_acquire) &&
               wait_timeouts < 200) {
            int futex_result = stress_futex_wait_timed(
                &fork_state->main_word, 0, 1);

            if (futex_result != 0 && errno != EAGAIN &&
                errno != EINTR && errno != ETIMEDOUT) {
                return -1;
            }
            if (futex_result != 0 && errno == ETIMEDOUT) {
                ++wait_timeouts;
            }
            if (!waited) {
                waited = waitpid(child, &status, WNOHANG);
                if (waited < 0) {
                    return -1;
                }
            }
        }
        if (!atomic_load_explicit(
                &fork_state->main_word, memory_order_acquire) ||
            pthread_join(thread, &thread_result) != 0 || thread_result ||
            (!waited && waitpid(child, &status, 0) != child) ||
            !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr,
                    "FAIL: fork semantic child iteration=%d stage=%u\n",
                    iteration, atomic_load_explicit(
                        &fork_state->child_stage,
                        memory_order_acquire));
            return -1;
        }
    }
    if (munmap(fork_state, sizeof(*fork_state)) != 0) {
        return -1;
    }
    return close(shared_fd);
}

void kzt_tls_stress_mark_fork_child(void)
{
    fork_child_after_dlopen = 1;
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

static Bool stress_callback(Display *display, xReply *reply,
                            char *buffer, int length,
                            XPointer opaque)
{
    (void)display;
    (void)opaque;
    if (!reply || !buffer || length != ASYNC_PROBE_REPLY_LENGTH) {
        __sync_val_compare_and_swap(&callback_error, 0, 1);
    } else if (run_attached_dlopen) {
        const char *path = run_attached_dlopen == 2
            ? concurrent_plugin_paths[
                  __sync_fetch_and_add(&concurrent_plugin_index, 1) & 1]
            : attached_plugin_path;
        void *handle = dlopen(
            path, RTLD_NOW | RTLD_LOCAL);
        plugin_check_fn attached_check = handle
            ? (plugin_check_fn)dlsym(
                  handle, "kzt_host_thread_tls_plugin_check")
            : NULL;

        if (!handle) {
            const char *loader_error = dlerror();

            fprintf(stderr,
                    "FAIL: attached dlopen path=%s: %s\n", path,
                    loader_error ? loader_error : "no loader error");
            __sync_val_compare_and_swap(&callback_error, 0, 4);
        } else if (!attached_check) {
            const char *loader_error = dlerror();

            fprintf(stderr,
                    "FAIL: attached dlsym path=%s: %s\n", path,
                    loader_error ? loader_error : "no loader error");
            __sync_val_compare_and_swap(&callback_error, 0, 5);
        } else if (attached_check() != 0) {
            __sync_val_compare_and_swap(&callback_error, 0, 6);
        } else if (dlclose(handle) != 0) {
            __sync_val_compare_and_swap(&callback_error, 0, 7);
        }
    } else if (plugin_check) {
        if (plugin_check() != 0) {
            __sync_val_compare_and_swap(&callback_error, 0, 2);
        }
    } else if (plugin_module_id) {
        guest_dtv_entry *dtv = read_guest_dtv();

        if (dtv[plugin_module_id].value != 0 ||
            dtv[plugin_module_id].to_free != 0) {
            __sync_val_compare_and_swap(&callback_error, 0, 3);
        }
    }
    __sync_fetch_and_add(&callback_hits, 1);
    return False;
}

static int stress_fmt_callback(Display *display)
{
    __sync_fetch_and_add(&fmt_callback_hits, 1);
    return XNoOp(display);
}

static int run_cycle(Display *display, const char *path)
{
    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);

    if (!handle) {
        return -1;
    }
    if (fork_child_after_dlopen) {
        (void)dlerror();
        _exit(0);
    }
    plugin_check = (plugin_check_fn)dlsym(
        handle, "kzt_host_thread_tls_plugin_check");
    if (!plugin_check ||
        dlinfo(handle, RTLD_DI_TLS_MODID, &plugin_module_id) != 0 ||
        !plugin_module_id ||
        XEventsQueued(display, QueuedAfterReading) !=
            ASYNC_PROBE_EVENTS_RETURN || callback_error) {
        return -1;
    }
    plugin_check = NULL;
    if (dlclose(handle) != 0 ||
        XEventsQueued(display, QueuedAfterReading) !=
            ASYNC_PROBE_EVENTS_RETURN || callback_error) {
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    Display *display;
    _XAsyncHandler handler = { 0 };
    size_t retired_ie_module_id;
    void *wrapped_handles[4];
    void *normal_handle;
    void *noload_handle;
    plugin_check_fn noload_check;
    void *wrapped_unloaded_handle;
    void *wrapped_noload_handle;

    if (argc == 5 && strcmp(argv[1], "--fork-futex-child") == 0) {
        return run_fork_futex_child(
                   argv[2], argv[3], argv[4]) == 0 ? 0 : 19;
    }
    if (argc != 4) {
        return 2;
    }
    display = calloc(1, sizeof(*display));
    if (!display) {
        return 3;
    }
    handler.handler = stress_callback;
    display->async_handlers = &handler;
    if (XEventsQueued(display, 101) != ASYNC_PROBE_EVENTS_RETURN) {
        return 21;
    }
    if (run_fork_futex_topology(argv[0], argv[1]) != 0) {
        (void)XEventsQueued(display, 102);
        return 18;
    }
    if (XEventsQueued(display, 102) != ASYNC_PROBE_EVENTS_RETURN) {
        return 21;
    }
    for (size_t index = 0; index < 4; ++index) {
        wrapped_handles[index] = dlopen(
            "libX11.so.6", RTLD_NOW | RTLD_LOCAL);
        if (!wrapped_handles[index]) {
            return 11;
        }
    }
    for (size_t index = 0; index < 4; ++index) {
        if (dlclose(wrapped_handles[index]) != 0) {
            return 12;
        }
    }
    attached_plugin_path = argv[1];
    run_attached_dlopen = 1;
    (void)XSetAfterFunction(display, stress_fmt_callback);
    if (XEventsQueued(display, 100) != ASYNC_PROBE_EVENTS_RETURN ||
        callback_error || fmt_callback_hits != 1) {
        fprintf(stderr,
                "FAIL: RunFunctionFmt overlapped Guest TLS propagation "
                "error=%d hits=%d\n",
                callback_error, fmt_callback_hits);
        return 20;
    }
    run_attached_dlopen = 0;
    wrapped_unloaded_handle = dlopen(
        "libxcb.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!wrapped_unloaded_handle ||
        dlclose(wrapped_unloaded_handle) != 0) {
        return 16;
    }
    wrapped_noload_handle = dlopen(
        "libxcb.so.1",
        RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
    if (wrapped_noload_handle) {
        return 17;
    }
    if (XEventsQueued(display, QueuedAfterReading) !=
            ASYNC_PROBE_EVENTS_RETURN || callback_error) {
        return 13;
    }
    if (run_cycle(display, argv[3]) != 0) {
        return 4;
    }
    normal_handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    noload_handle = dlopen(
        argv[1], RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
    noload_check = normal_handle
        ? (plugin_check_fn)dlsym(
              normal_handle, "kzt_host_thread_tls_plugin_check")
        : NULL;
    if (!normal_handle || !noload_handle || !noload_check ||
        dlclose(noload_handle) != 0 || noload_check() != 0 ||
        dlclose(normal_handle) != 0) {
        return 14;
    }
    retired_ie_module_id = plugin_module_id;
    if (run_cycle(display, argv[1]) != 0 ||
        plugin_module_id != retired_ie_module_id ||
        run_cycle(display, argv[2]) != 0) {
        return 4;
    }
    callback_hits = 0;

    for (int iteration = 0; iteration < STRESS_ITERATIONS; ++iteration) {
        const char *path = argv[1 + (iteration & 1)];

        if (run_cycle(display, path) != 0) {
            fprintf(stderr, "FAIL: stress dlopen iteration=%d: %s\n",
                    iteration, dlerror());
            return 5;
        }
    }
    run_attached_dlopen = 1;
    for (int iteration = 0; iteration < STRESS_ITERATIONS; ++iteration) {
        attached_plugin_path = argv[1 + (iteration & 1)];
        if (XEventsQueued(display, QueuedAfterReading) !=
                ASYNC_PROBE_EVENTS_RETURN || callback_error) {
            fprintf(stderr,
                    "FAIL: attached stress dlopen iteration=%d error=%d\n",
                    iteration, callback_error);
            return 6;
        }
    }
    run_attached_dlopen = 0;
    if (XEventsQueued(display, 99) != ASYNC_PROBE_EVENTS_RETURN ||
        callback_error || callback_hits != 3 * STRESS_ITERATIONS + 2) {
        fprintf(stderr,
                "FAIL: concurrent attached warmup hits=%d error=%d\n",
                callback_hits, callback_error);
        return 9;
    }
    run_attached_dlopen = 2;
    concurrent_plugin_paths[0] = argv[1];
    concurrent_plugin_paths[1] = argv[2];
    for (int iteration = 0; iteration < STRESS_ITERATIONS / 2;
         ++iteration) {
        if (XEventsQueued(display, 99) !=
                ASYNC_PROBE_EVENTS_RETURN || callback_error) {
            fprintf(stderr,
                    "FAIL: concurrent attached dlopen iteration=%d "
                    "error=%d\n", iteration, callback_error);
            return 10;
        }
    }
    if (XFlush(display) != ASYNC_PROBE_FLUSH_RETURN) {
        return 7;
    }
    if (callback_hits != 4 * STRESS_ITERATIONS + 2) {
        fprintf(stderr,
                "FAIL: stress hits=%d\n", callback_hits);
        return 8;
    }
    free(display);
    fputs("PASS: attached Guest TLS survived 64 A/B dlopen cycles "
          "from Guest, Host-attached, and concurrent threads\n", stderr);
    return 0;
}
