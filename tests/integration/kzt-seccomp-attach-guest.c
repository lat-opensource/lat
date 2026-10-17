/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <errno.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <stddef.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/Xlibint.h>

#define CHECK(c) do { \
    if (!(c)) { \
        fprintf(stderr, "FAIL:%d: %s errno=%d\n", __LINE__, #c, errno); \
        exit(1); \
    } \
} while (0)

static long main_tid, callback_tid, callback_result;
static int callback_errno, hits, idle_ready, idle_stop;

static void install_filter(unsigned flags, int error)
{
    struct sock_filter code[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_getppid, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | error),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };
    struct sock_fprog program = { .len = 4, .filter = code };

    CHECK(syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, flags, &program) == 0);
}

static void check_main(int error)
{
    errno = 0;
    CHECK(syscall(SYS_getppid) == -1 && errno == error);
}

static Bool callback(Display *d, xReply *r, char *b, int n, XPointer p)
{
    (void)d; (void)r; (void)b; (void)n; (void)p;
    callback_tid = syscall(SYS_gettid);
    errno = 0;
    callback_result = syscall(SYS_getppid);
    callback_errno = errno;
    hits++;
    return True;
}

static void check_callback(Display *display, int error)
{
    int before = hits;

    CHECK(XEventsQueued(display, 0) == True);
    CHECK(hits == before + 1 && callback_tid != main_tid);
    fprintf(stderr, "CALLBACK_RET=%ld CALLBACK_ERRNO=%d EXPECTED_ERRNO=%d\n",
            callback_result, callback_errno, error);
    if (error) {
        CHECK(callback_result == -1 && callback_errno == error);
    } else {
        CHECK(callback_result > 0 && callback_errno == 0);
    }
}

static void *idle(void *unused)
{
    (void)unused;
    __atomic_store_n(&idle_ready, 1, __ATOMIC_RELEASE);
    while (!__atomic_load_n(&idle_stop, __ATOMIC_ACQUIRE)) {
        sched_yield();
    }
    return NULL;
}

int main(int argc, char **argv)
{
    Display *display = calloc(1, sizeof(*display));
    _XAsyncHandler handler = { .handler = callback };
    const char *mode;

    CHECK(argc == 2 && display);
    mode = argv[1];
    main_tid = syscall(SYS_gettid);
    display->async_handlers = &handler;
    CHECK(prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == 0);

    if (!strcmp(mode, "divergent")) {
        pthread_t thread;

        CHECK(pthread_create(&thread, NULL, idle, NULL) == 0);
        while (!__atomic_load_n(&idle_ready, __ATOMIC_ACQUIRE)) {
            sched_yield();
        }
        install_filter(0, EACCES);
        check_main(EACCES);
        CHECK(XEventsQueued(display, 0) == False);
        CHECK(hits == 0); /* Refusal must happen before any Guest callback. */
        CHECK(XFlush(display) == 0);
        __atomic_store_n(&idle_stop, 1, __ATOMIC_RELEASE);
        CHECK(pthread_join(thread, NULL) == 0);
        /* Once the divergent task leaves, the common policy is usable. */
        check_callback(display, EACCES);
    } else if (!strcmp(mode, "live-tsync")) {
        check_callback(display, 0);
        for (int i = 0; i < 16; i++) {
            int error = i & 1 ? EPERM : EACCES;

            install_filter(SECCOMP_FILTER_FLAG_TSYNC, error);
            check_main(error);
            check_callback(display, error);
            CHECK(XFlush(display) == 0);
            check_callback(display, error); /* Fresh thread, current root. */
        }
    } else {
        unsigned flags = !strcmp(mode, "late-private")
            ? 0 : SECCOMP_FILTER_FLAG_TSYNC;

        CHECK(!strcmp(mode, "late-tsync") || !strcmp(mode, "late-private") ||
              !strcmp(mode, "fork"));
        install_filter(flags, EACCES);
        check_main(EACCES);
        check_callback(display, EACCES);
        if (!strcmp(mode, "fork")) {
            int status;
            pid_t child = fork();

            CHECK(child >= 0);
            if (!child) {
                check_main(EACCES);
                _exit(0);
            }
            CHECK(waitpid(child, &status, 0) == child);
            CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
            check_main(EACCES);
            check_callback(display, EACCES);
        }
    }
    CHECK(XFlush(display) == 0);
    free(display);
    printf("PASS: attached seccomp %s\n", mode);
    return 0;
}
