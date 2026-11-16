/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#undef NDEBUG
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include "aot_defer.h"

static char marker[4096], generated[4096];

static void mark(void)
{
    char temporary[4200];
    snprintf(temporary, sizeof(temporary), "%s.tmp.%d", marker, getpid());
    FILE *file = fopen(temporary, "w");
    assert(file);
    fprintf(file, "%d\n", getpid());
    assert(fclose(file) == 0);
    assert(rename(temporary, marker) == 0);
}

static void clean_exit(int sig)
{
    (void)sig;
    _exit(0);
}

ssize_t __real_recv(int fd, void *buffer, size_t size, int flags);
ssize_t __wrap_recv(int fd, void *buffer, size_t size, int flags);
ssize_t __wrap_recv(int fd, void *buffer, size_t size, int flags)
{
    static int marked;
    /* Production aot_defer_wait reaches recv after READY and the W reply. */
    if (!marked) {
        mark();
        marked = 1;
    }
    return __real_recv(fd, buffer, size, flags);
}

static void generate(int control_fd)
{
    if (aot_defer_wait(control_fd)) {
        _exit(42);
    }
    close(control_fd);
    int fd = open(generated, O_CREAT | O_WRONLY | O_EXCL, 0600);
    if (fd < 0) {
        _exit(43);
    }
    if (write(fd, "G", 1) != 1) {
        _exit(44);
    }
    close(fd);
    _exit(0);
}

int main(int argc, char **argv)
{
    assert(argc == 4);
    alarm(8);
    assert(prctl(PR_SET_CHILD_SUBREAPER, 1) == 0);
    const char *stage = argv[1];
    int sig = atoi(argv[2]);
    snprintf(marker, sizeof(marker), "%s/stage", argv[3]);
    snprintf(generated, sizeof(generated), "%s/generated", argv[3]);
    if (!strcmp(stage, "root-ready")) {
        int fd = aot_defer_connect(getenv(AOT_DEFER_ENV));
        assert(fd >= 0);
        pid_t worker = fork();
        assert(worker >= 0);
        if (!worker) {
            generate(fd);
        }
        close(fd);
        while (access(marker, F_OK)) {
            usleep(1000);
        }
        if (sig) {
            assert(kill(getpid(), sig) == 0);
            for (;;) {
                pause();
            }
        }
        return 0;
    }
    pid_t app = fork();
    assert(app >= 0);
    if (!app) {
        signal(SIGUSR1, clean_exit);
        if (!strcmp(stage, "before-connect")) {
            mark();
            for (;;) {
                pause();
            }
        }
        int fd = aot_defer_connect(getenv(AOT_DEFER_ENV));
        assert(fd >= 0);
        if (!strcmp(stage, "connected")) {
            mark();
            for (;;) {
                pause();
            }
        }
        pid_t worker = fork();
        assert(worker >= 0);
        if (!worker) {
            if (!strcmp(stage, "forked-before-ready")) {
                mark();
                for (;;) {
                    pause();
                }
            }
            generate(fd);
        }
        close(fd);
        if (!strcmp(stage, "ready-app")) {
            for (;;) {
                pause();
            }
        }
        _exit(0);
    }
    FILE *file;
    while (!(file = fopen(marker, "r"))) {
        usleep(1000);
    }
    int marked;
    assert(fscanf(file, "%d", &marked) == 1);
    fclose(file);
    pid_t target = !strcmp(stage, "ready-app") ? app : marked;
    if (target != app) {
        int status;
        assert(waitpid(app, &status, 0) == app && status == 0);
    }
    assert(kill(target, sig ? sig : SIGUSR1) == 0);
    siginfo_t info = {0};
    assert(waitid(P_PID, target, &info, WEXITED | WNOWAIT) == 0);
    char path[4096];
    snprintf(path, sizeof(path), "%s/observed.json", argv[3]);
    file = fopen(path, "w");
    assert(file);
    fprintf(file, "{\"app_pid\":%d,\"target_pid\":%d,"
            "\"worker_marker_pid\":%d,\"si_code\":%d,\"si_status\":%d,"
            "\"generated_before_root_exit\":%s}\n",
            app, target, marked, info.si_code, info.si_status,
            access(generated, F_OK) == 0 ? "true" : "false");
    fclose(file);
    if (target == app) {
        int status;
        assert(waitpid(app, &status, 0) == app);
    }
    /* Let the controller adopt/reap the worker and observe its raw status. */
    return 0;
}
