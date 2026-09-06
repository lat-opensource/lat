#define _GNU_SOURCE
/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <errno.h>
#include <dlfcn.h>
#include <linux/futex.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlibint.h>

static pthread_mutex_t *mutex;
static int waiter_result;

static uint32_t read_mutex_word(void)
{
    return __atomic_load_n(
        (uint32_t *)&mutex->__data.__lock, __ATOMIC_RELAXED);
}

static void timeout_handler(int signal_number)
{
    (void)signal_number;
    dprintf(STDERR_FILENO, "TIMEOUT word=0x%08x\n",
            read_mutex_word());
    _exit(124);
}

static Bool attached_owner(Display *display, xReply *reply,
                           char *buffer, int length, XPointer opaque)
{
    long tid = syscall(SYS_gettid);
    int result;

    (void)reply;
    (void)buffer;
    (void)length;
    (void)opaque;
    result = pthread_mutex_lock(mutex);
    fprintf(stderr,
            "ATTACHED_LOCK_RESULT=%d tid=%ld word=0x%08x owner=%d\n",
            result, tid, read_mutex_word(),
            mutex->__data.__owner);
    if (result != 0 ||
        (read_mutex_word() & FUTEX_TID_MASK) !=
            (uint32_t)tid || mutex->__data.__owner != tid) {
        return True;
    }
    return XNoOp(display) != 0;
}

static void *run_waiter(void *opaque)
{
    (void)opaque;
    waiter_result = pthread_mutex_lock(mutex);
    if (waiter_result == EOWNERDEAD) {
        pthread_mutex_consistent(mutex);
        pthread_mutex_unlock(mutex);
    }
    return NULL;
}

int main(void)
{
    const struct timespec poll_delay = {
        .tv_nsec = 1000 * 1000,
    };
    pthread_mutexattr_t attr;
    pthread_t waiter;
    Display *display;
    _XAsyncHandler handler = { 0 };
    struct sigaction alarm_action = {
        .sa_handler = timeout_handler,
    };
    uint32_t lock_word = 0;

    mutex = mmap(NULL, sizeof(*mutex), PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    display = calloc(1, sizeof(*display));
    if (mutex == MAP_FAILED || !display ||
        pthread_mutexattr_init(&attr) != 0 ||
        pthread_mutexattr_setpshared(
            &attr, PTHREAD_PROCESS_SHARED) != 0 ||
        pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST) != 0 ||
        pthread_mutex_init(mutex, &attr) != 0) {
        return 2;
    }
    pthread_mutexattr_destroy(&attr);
    handler.handler = attached_owner;
    display->async_handlers = &handler;
    (void)dlerror();
    if (XEventsQueued(display, 0) != 77 ||
        pthread_create(&waiter, NULL, run_waiter, NULL) != 0) {
        return 3;
    }
    for (int attempt = 0; attempt < 2000; ++attempt) {
        lock_word = read_mutex_word();
        if (lock_word & FUTEX_WAITERS) {
            break;
        }
        nanosleep(&poll_delay, NULL);
    }
    if (!(lock_word & FUTEX_WAITERS)) {
        return 4;
    }
    fprintf(stderr, "WAITER_BLOCKED word=0x%08x\n", lock_word);
    sigemptyset(&alarm_action.sa_mask);
    if (sigaction(SIGALRM, &alarm_action, NULL) != 0) {
        return 7;
    }
    alarm(2);
    if (XFlush(display) != 78 || pthread_join(waiter, NULL) != 0) {
        return 5;
    }
    alarm(0);
    fprintf(stderr, "WAITER_RESULT=%d word=0x%08x\n",
            waiter_result, read_mutex_word());
    if (waiter_result != EOWNERDEAD) {
        return 6;
    }
    puts("PASS: attached robust owner death woke blocked Guest waiter");
    return 0;
}
