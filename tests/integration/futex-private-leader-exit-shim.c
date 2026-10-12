/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

static void *(*real_mremap)(void *, size_t, size_t, int, ...);
static void *watched;

static void __attribute__((constructor)) init(void)
{
    real_mremap = dlsym(RTLD_NEXT, "mremap");
    if (!real_mremap) {
        abort();
    }
}

void *mremap(void *old, size_t old_size, size_t new_size, int flags, ...)
{
    const struct timespec delay = { .tv_nsec = 1000000 };
    void *dest = NULL;
    void *result;
    void *watch = __atomic_load_n(&watched, __ATOMIC_RELAXED);
    int saved_errno;
    int widen = 0;

    if (flags & MREMAP_FIXED) {
        va_list args;

        va_start(args, flags);
        dest = va_arg(args, void *);
        va_end(args);
        if (!watch) {
            uint32_t tag[2];
            struct iovec local = {tag, sizeof(tag)};
            struct iovec remote = {old, sizeof(tag)};

            /* Use the running task even when the process leader has exited. */
            if (syscall(SYS_process_vm_readv, syscall(SYS_gettid),
                        &local, 1, &remote, 1, 0) == (long)sizeof(tag) &&
                tag[0] == 0x46555458 && tag[1] == 0x4c454144) {
                __atomic_compare_exchange_n(&watched, &watch, old, 0,
                                            __ATOMIC_RELAXED, __ATOMIC_RELAXED);
                watch = __atomic_load_n(&watched, __ATOMIC_RELAXED);
            }
        }
        widen = old == watch;
    }
    result = real_mremap(old, old_size, new_size, flags, dest);
    saved_errno = errno;
    if (widen && result != MAP_FAILED) {
        /* Widen the real mapping gap without faking futex results. */
        nanosleep(&delay, NULL);
    }
    errno = saved_errno;
    return result;
}
