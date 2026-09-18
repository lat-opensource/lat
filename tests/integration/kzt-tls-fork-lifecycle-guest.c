/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/futex.h>
#include <linux/seccomp.h>
#include <stddef.h>
#include <pthread.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/Xlibint.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL:%d %s errno=%d\n", __LINE__, #c, errno); exit(1); } } while (0)
static const char *mode;
static int phase, ack[2], data[2], word;
static int *(*cell)(void);
static long worker_tid;
static uintptr_t control[2];
static void fork_once(void);
static void deny_fork(int reject_errno);
static int idle_stop;

static void *guest_idle(void *unused)
{
    const struct timespec delay = { .tv_nsec = 1000000 };
    (void)unused;
    while (!__atomic_load_n(&idle_stop, __ATOMIC_ACQUIRE)) {
        nanosleep(&delay, NULL);
    }
    return NULL;
}

static Bool callback(Display *d, xReply *r, char *b, int n, XPointer opaque)
{
    char byte = 1;
    (void)d; (void)r; (void)b; (void)n; (void)opaque;
    long tid = syscall(SYS_gettid);
    if (worker_tid && worker_tid != tid) return 90;
    worker_tid = tid;
    control[0] = tid;
    if (!strncmp(mode, "attached-seccomp", 16)) {
        int reject_errno = !strcmp(mode, "attached-seccomp") ? EPERM : 512;

        deny_fork(reject_errno);
        for (int i = 0; i < 2; i++) {
            errno = 0;
            CHECK(fork() == -1 && errno == reject_errno);
        }
    } else if (!strcmp(mode, "dtv")) {
        if (phase == 1) {
            if (*cell() != 17) return 91;
            *cell() = 12345;
        } else if (phase == 2) {
            int value = *cell();
            fprintf(stderr, "RETAINED_TLS_VALUE=%d expected=12345\n", value);
            if (value != 12345) return 92;
        }
    } else if (!strcmp(mode, "read") || !strcmp(mode, "futex")) {
        if (write(ack[1], &byte, 1) != 1) return 93;
        if (!strcmp(mode, "read")) {
            if (syscall(SYS_read, data[0], &byte, 1) != 1) return 94;
        } else {
            while (!__atomic_load_n(&word, __ATOMIC_ACQUIRE)) {
                if (syscall(SYS_futex, &word, FUTEX_WAIT_PRIVATE, 0,
                            NULL, NULL, 0) < 0 && errno != EAGAIN && errno != EINTR) return 95;
            }
        }
    } else if (!strcmp(mode, "flush")) {
        if (write(ack[1], &byte, 1) != 1) return 96;
        flockfile(stderr);
        funlockfile(stderr);
    }
    return 0;
}

static void fork_once(void)
{
    int status;
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) _exit(0);
    CHECK(waitpid(child, &status, 0) == child);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "CHILD_STATUS=%#x signal=%d\n", status,
                WIFSIGNALED(status) ? WTERMSIG(status) : 0);
    }
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void deny_fork(int reject_errno)
{
    struct sock_filter code[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_clone, 3, 0),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_fork, 2, 0),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_vfork, 1, 0),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | reject_errno),
    };
    struct sock_fprog filter = { sizeof(code) / sizeof(code[0]), code };
    CHECK(prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == 0);
    CHECK(prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &filter) == 0);
}

int main(int argc, char **argv)
{
    Display *display = calloc(1, sizeof(*display));
    _XAsyncHandler handler = { 0 };
    char byte = 1;
    pthread_t idle_thread;
    CHECK(argc == 4 && display);
    mode = argv[1];
    handler.handler = callback;
    handler.data = (XPointer)control;
    display->async_handlers = &handler;
    (void)dlerror();
    if (!strcmp(mode, "dtv")) {
        void *keep, *last;
        CHECK(XEventsQueued(display, 0) == 0);
        for (int i = 0; i < 3; i++) {
            void *temporary = dlopen(argv[3], RTLD_NOW | RTLD_LOCAL);
            CHECK(temporary && dlclose(temporary) == 0);
        }
        keep = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
        CHECK(keep);
        *(void **)(&cell) = dlsym(keep, "review_tls_cell");
        CHECK(cell);
        phase = 1;
        CHECK(XEventsQueued(display, 0) == 0);
        for (int i = 0; i < 3; i++) {
            void *temporary = dlopen(argv[3], RTLD_NOW | RTLD_LOCAL);
            CHECK(temporary && dlclose(temporary) == 0);
        }
        last = dlopen(argv[3], RTLD_NOW | RTLD_LOCAL);
        CHECK(last);
        phase = 2;
        CHECK(XEventsQueued(display, 0) == 0);
        CHECK(XFlush(display) == 0);
        CHECK(dlclose(last) == 0 && dlclose(keep) == 0);
    } else if (!strncmp(mode, "attached-seccomp", 16)) {
        CHECK(XEventsQueued(display, 0) == 0);
        CHECK(XFlush(display) == 0);
    } else if (!strncmp(mode, "seccomp", 7)) {
        int reject_errno = !strcmp(mode, "seccomp") ? EPERM : 512;

        deny_fork(reject_errno);
        for (int i = 0; i < 2; i++) {
            errno = 0;
            CHECK(fork() == -1 && errno == reject_errno);
            fprintf(stderr, "FORK_DENIED=%d\n", i + 1);
        }
        CHECK(XEventsQueued(display, 0) == 0);
        CHECK(XFlush(display) == 0);
    } else {
        CHECK(pipe(ack) == 0 && pipe(data) == 0);
        if (!strcmp(mode, "flush")) {
            char *single_threaded;

            fork_once();
            CHECK(pthread_create(&idle_thread, NULL, guest_idle, NULL) == 0);
            single_threaded = dlsym(RTLD_DEFAULT, "__libc_single_threaded");
            CHECK(!single_threaded || !*single_threaded);
            CHECK(XNoOp(display) == 0);
            (void)syscall(SYS_getpid);
            fork_once();
            fprintf(stderr, "POST_FLUSH_FORK_COMPLETE\n");
            flockfile(stderr);
        }
        display->fd = !strcmp(mode, "read") ? 1 : 2;
        control[1] = !strcmp(mode, "read") ? (uintptr_t)data[0]
            : !strcmp(mode, "flush") ? (uintptr_t)stderr->_lock
            : (uintptr_t)&word;
        CHECK(XEventsQueued(display, 1) == 0);
        CHECK(read(ack[0], &byte, 1) == 1);
        CHECK(XNoOp(display) == 0);
        fork_once();
        if (strcmp(mode, "flush")) {
            fprintf(stderr, "FORK_RETURNED_BEFORE_WAKE\n");
        }
        if (!strcmp(mode, "flush")) {
            funlockfile(stderr);
        } else if (!strcmp(mode, "read")) {
            CHECK(write(data[1], &byte, 1) == 1);
        } else {
            __atomic_store_n(&word, 1, __ATOMIC_RELEASE);
            CHECK(syscall(SYS_futex, &word, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0) >= 0);
        }
        CHECK(XFlush(display) == 0);
        if (!strcmp(mode, "flush")) {
            __atomic_store_n(&idle_stop, 1, __ATOMIC_RELEASE);
            CHECK(pthread_join(idle_thread, NULL) == 0);
        }
    }
    printf("PASS: review lifecycle %s\n", mode);
    free(display);
    return 0;
}
