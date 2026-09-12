/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include "latx/liblat.h"

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL:%d: %s\n", __LINE__, #c); exit(1); \
} } while (0)

static int (*init_runtime)(bool, int, char **, LatHostSymbolQuery,
                           LatHostSymbolOffsetQuery);
static void (*end_runtime)(void);
static void (*finalize_all)(void *);
static void *(*open_guest)(void *, int);
static void *(*find_guest)(void *, void *);
static int (*close_guest)(void *);
static void (*run_guest)(uintptr_t, const long *, int, const long *, int,
                         const long *, int, long *, long *, long *, long *,
                         unsigned __int128 *);
static int (*register_cleanup)(void *);
static int (*register_argument_cleanup)(void *, void *, void *);
static uintptr_t recurse_entry, tls_entry, fourth_cleanup;
static long observed[8];
static unsigned observed_count;
static atomic_int errors;
static pthread_barrier_t barrier;

static long call_guest(uintptr_t entry, long a, long b, int count)
{
    long arguments[2] = {a, b};
    long result = -999;

    run_guest(entry, arguments, count, NULL, 0, NULL, 0,
              &result, NULL, NULL, NULL, NULL);
    return result;
}

static long host_recurse(long depth)
{
    return call_guest(recurse_entry, (long)host_recurse, depth, 2);
}

static void host_observe(long value)
{
    CHECK(observed_count < sizeof(observed) / sizeof(observed[0]));
    observed[observed_count++] = value;
    if (value == 3) {
        CHECK(register_cleanup((void *)fourth_cleanup) == 0);
        /* A running callback must already have been removed from its list. */
        finalize_all(NULL);
    }
}

static uint64_t host_stub(const char *signature, uintptr_t entry,
                          long *gpr, long *xmm, char *stack,
                          long *rax, long *rdx, long *xmm0,
                          double *st0, uintptr_t adapter)
{
    (void)signature; (void)xmm; (void)stack; (void)rdx;
    (void)xmm0; (void)st0; (void)adapter;
    if (entry == (uintptr_t)host_recurse) {
        *rax = host_recurse(gpr[0]);
    } else {
        CHECK(entry == (uintptr_t)host_observe);
        host_observe(gpr[0]);
        *rax = 0;
    }
    return *rax;
}

static const char *query_host(uintptr_t entry, void *stub, void *adapter,
                              char *signature)
{
    if (entry != (uintptr_t)host_recurse && entry != (uintptr_t)host_observe) {
        return NULL;
    }
    *(uintptr_t *)stub = (uintptr_t)host_stub;
    *(uintptr_t *)adapter = 0;
    signature[0] = 0;
    return entry == (uintptr_t)host_recurse ? "lFl" : "vFl";
}

static void *worker(void *unused)
{
    (void)unused;
    pthread_barrier_wait(&barrier);
    for (long i = 1; i <= 16; i++) {
        if (call_guest(tls_entry, 0, 0, 0) != i) {
            atomic_fetch_add(&errors, 1);
        }
    }
    if (host_recurse(24) != 24) {
        atomic_fetch_add(&errors, 1);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    void *runtime, *module;
    void *native_guard = MAP_FAILED;
    const size_t guard_size = 2 * 1024 * 1024;
    char program_name[] = "liblat-test";
    char missing_path[] = "/this/liblat-bootstrap-does-not-exist";
    char *bootstrap_args[2] = {program_name, NULL};
    const char *mode;
    uintptr_t function;

    CHECK(argc == 5);
    mode = argv[4];
    CHECK(setenv("LATX_KZT", "1", 1) == 0);
    CHECK(setenv("LATX_AOT", "0", 1) == 0);
    runtime = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!runtime) {
        fprintf(stderr, "%s\n", dlerror());
        return 1;
    }
#define LOAD(target, name) do { *(void **)(&(target)) = dlsym(runtime, name); \
    CHECK(target != NULL); } while (0)
    LOAD(init_runtime, "lat_init"); LOAD(end_runtime, "lat_end");
    LOAD(open_guest, "lat_dlopen"); LOAD(find_guest, "lat_dlsym");
    LOAD(close_guest, "lat_dlclose"); LOAD(run_guest, "lat_dlrun_method");
    LOAD(register_cleanup, "my_atexit");
    LOAD(finalize_all, "my___cxa_finalize");
    LOAD(register_argument_cleanup, "my___cxa_atexit");
    bootstrap_args[1] = argv[2];

    if (!strcmp(mode, "bad-arguments")) {
        CHECK(init_runtime(false, 0, NULL, NULL, NULL) == -EINVAL);
        CHECK(init_runtime(true, 2, bootstrap_args, NULL, NULL) == -ENOTSUP);
        bootstrap_args[1] = missing_path;
        CHECK(init_runtime(false, 2, bootstrap_args, NULL, NULL) == -ENOENT);
        bootstrap_args[1] = argv[0];
        CHECK(init_runtime(false, 2, bootstrap_args, NULL, NULL) == -ENOEXEC);
        bootstrap_args[1] = argv[2];
        CHECK(init_runtime(false, 2, bootstrap_args, query_host, NULL) == 0);
        end_runtime();
        puts("PASS: invalid initialization arguments");
        return 0;
    }
    if (!strcmp(mode, "image-collision")) {
        int output[2], status;
        pid_t child;
        char message[4096];
        size_t used = 0;
        ssize_t count;

        CHECK(pipe(output) == 0);
        child = fork();
        CHECK(child >= 0);
        if (!child) {
            void *guard;
            close(output[0]);
            if (dup2(output[1], STDERR_FILENO) < 0) {
                _exit(51);
            }
            close(output[1]);
            guard = mmap((void *)0x400000, guard_size, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (guard != (void *)0x400000) {
                _exit(51); /* Setup failure must not look like rejection. */
            }
            memset(guard, 0x5a, guard_size);
            fprintf(stderr, "INIT_CALLED\n");
            init_runtime(false, 2, bootstrap_args, query_host, NULL);
            _exit(50);
        }
        close(output[1]);
        while ((count = read(output[0], message + used,
                             sizeof(message) - used - 1)) > 0) {
            used += count;
            if (used == sizeof(message) - 1) {
                break;
            }
        }
        message[used] = 0;
        close(output[0]);
        CHECK(waitpid(child, &status, 0) == child);
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_FAILURE);
        CHECK(strstr(message, "INIT_CALLED") &&
              strstr(message, "requires virtual address space that is in use"));
        puts("PASS: bootstrap collision fails closed");
        return 0;
    }
    if (!strcmp(mode, "host-map") || !strncmp(mode, "native-", 7)) {
        if (!strcmp(mode, "native-shmdt")) {
            int id = shmget(IPC_PRIVATE, guard_size, IPC_CREAT | 0600);
            CHECK(id >= 0);
            native_guard = shmat(id, (void *)UINT64_C(0x5500000000), 0);
            CHECK(shmctl(id, IPC_RMID, NULL) == 0);
        } else {
        native_guard = mmap((void *)UINT64_C(0x5500000000), guard_size,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }
        CHECK(native_guard == (void *)UINT64_C(0x5500000000));
        memset(native_guard, 0x5a, guard_size);
    }
    if (!strcmp(mode, "bootstrap-status")) {
        CHECK(init_runtime(false, 2, bootstrap_args, query_host, NULL) < 0);
        puts("PASS: nonzero bootstrap exit is an initialization error");
        return 0;
    }
    CHECK(init_runtime(false, 2, bootstrap_args, query_host, NULL) == 0);
    if (!strcmp(mode, "repeat")) {
        CHECK(init_runtime(false, 2, bootstrap_args, query_host, NULL) == -EALREADY);
    }
    module = open_guest(argv[3], RTLD_NOW | RTLD_GLOBAL);
    CHECK(module);
    function = (uintptr_t)find_guest(module, (void *)"guest_add");
    CHECK(function && call_guest(function, 20, 22, 2) == 42);
    function = (uintptr_t)find_guest(module, (void *)"guest_errno_roundtrip");
    CHECK(function);
    errno = ENOENT;
    CHECK(call_guest(function, ENOENT, 0, 1) == 1 && errno == EAGAIN);
    function = (uintptr_t)find_guest(module, (void *)"guest_decimal_point");
    CHECK(function);
    CHECK(strcmp((const char *)call_guest(function, 0, 0, 0),
                 localeconv()->decimal_point) == 0);
    if (!strncmp(mode, "native-", 7)) {
        long page_size = sysconf(_SC_PAGESIZE);
        const char *symbol = !strcmp(mode, "native-noreplace") ? "guest_noreplace"
            : !strcmp(mode, "native-fixed") ? "guest_fixed"
            : !strcmp(mode, "native-unmap") ? "guest_unmap"
            : !strcmp(mode, "native-shm") ? "guest_shmat_into_host"
            : !strcmp(mode, "native-shmdt") ? "guest_shmdt"
            : "guest_remap_into_host";
        function = (uintptr_t)find_guest(module, (void *)symbol);
        CHECK(function);
        CHECK(call_guest(function, (long)native_guard, page_size, 2) ==
              (!strcmp(mode, "native-unmap") || !strcmp(mode, "native-shmdt")
               ? -EINVAL : -EEXIST));
        if (!strcmp(mode, "native-shm")) {
            function = (uintptr_t)find_guest(module, (void *)"guest_shm_roundtrip");
            CHECK(function && call_guest(function, 0, 0, 0) == 1);
            function = (uintptr_t)find_guest(module, (void *)"guest_shm_partial");
            CHECK(function && call_guest(function, page_size, 0, 1) == 1);
            function = (uintptr_t)find_guest(module, (void *)"guest_shm_replace");
            CHECK(function);
            for (long variant = 0; variant < 4; variant++) {
                CHECK(call_guest(function, page_size, variant, 2) == 1);
            }
            if (page_size > 4096) {
                function = (uintptr_t)find_guest(module, (void *)"guest_shm_subpage");
                CHECK(function && call_guest(function, page_size, 0, 1) == 1);
            }
        }
        if (!strcmp(mode, "native-noreplace")) {
            long fresh = UINT64_C(0x3000000000);
            CHECK(call_guest(function, fresh, page_size, 2) == fresh);
            CHECK(call_guest(function, fresh, page_size, 2) == -EEXIST);
            function = (uintptr_t)find_guest(module, (void *)"guest_unmap");
            CHECK(function && call_guest(function, fresh, page_size, 2) == 0);
        }
    }
    if (!strcmp(mode, "host-map") || !strncmp(mode, "native-", 7)) {
        for (size_t i = 0; i < guard_size; i++) {
            CHECK(((unsigned char *)native_guard)[i] == 0x5a);
        }
        CHECK(close_guest(module) == 0);
        end_runtime();
        if (!strcmp(mode, "native-shmdt")) {
            CHECK(shmdt(native_guard) == 0);
        } else {
            CHECK(munmap(native_guard, guard_size) == 0);
        }
        puts("PASS: native mapping survives Guest allocation");
        return 0;
    }
    if (!strcmp(mode, "repeat")) {
        CHECK(close_guest(module) == 0);
        end_runtime();
        puts("PASS: repeated initialization rejected without losing runtime");
        return 0;
    }
    recurse_entry = (uintptr_t)find_guest(module, (void *)"guest_recurse");
    tls_entry = (uintptr_t)find_guest(module, (void *)"guest_tls_next");
    CHECK(recurse_entry && tls_entry && host_recurse(128) == 128);
    {
        pthread_t threads[4];

        CHECK(pthread_barrier_init(&barrier, NULL, 4) == 0);
        for (int i = 0; i < 4; i++) {
            CHECK(pthread_create(&threads[i], NULL, worker, NULL) == 0);
        }
        for (int i = 0; i < 4; i++) {
            CHECK(pthread_join(threads[i], NULL) == 0);
        }
        CHECK(pthread_barrier_destroy(&barrier) == 0);
        CHECK(atomic_load(&errors) == 0);
    }
    function = (uintptr_t)find_guest(module, (void *)"guest_set_observer");
    CHECK(function);
    call_guest(function, (long)host_observe, 0, 1);
    fourth_cleanup = (uintptr_t)find_guest(module, (void *)"guest_cleanup_four");
    function = (uintptr_t)find_guest(module, (void *)"guest_cleanup_one");
    CHECK(function && register_cleanup((void *)function) == 0);
    function = (uintptr_t)find_guest(module, (void *)"guest_dso_anchor");
    CHECK(function);
    {
        void *anchor = (void *)call_guest(function, 0, 0, 0);
        uintptr_t callback = (uintptr_t)find_guest(module, (void *)"guest_cleanup_argument");

        CHECK(anchor && callback);
        CHECK(register_argument_cleanup((void *)callback, (void *)2, anchor) == 0);
        CHECK(register_argument_cleanup((void *)callback, (void *)5, NULL) == 0);
        CHECK(register_argument_cleanup((void *)callback, (void *)6, (void *)1) != 0);
    }
    function = (uintptr_t)find_guest(module, (void *)"guest_cleanup_three");
    CHECK(function && fourth_cleanup && register_cleanup((void *)function) == 0);
    end_runtime();
    CHECK(observed_count == 5 && observed[0] == 3 && observed[1] == 4 &&
          observed[2] == 5 && observed[3] == 2 && observed[4] == 1);
    end_runtime();
    CHECK(observed_count == 5);
    CHECK(close_guest(module) == 0);
    puts("PASS: calls, thread TLS, deep callbacks, ordered and reentrant cleanup");
    return 0;
}
