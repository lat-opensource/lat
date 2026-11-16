/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#undef NDEBUG
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <unistd.h>
#include "aot_defer.h"

static const char *hold_marker;

static void write_hold_marker(const char *suffix, const char *value)
{
    char *path, *temporary;
    assert(asprintf(&path, "%s%s", hold_marker, suffix) >= 0);
    assert(asprintf(&temporary, "%s.tmp", path) >= 0);
    int fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0600);
    assert(fd >= 0 && write(fd, value, 2) == 2);
    assert(close(fd) == 0);
    assert(rename(temporary, path) == 0);
    free(temporary);
    free(path);
}

ssize_t __real_recv(int fd, void *buffer, size_t size, int flags);
ssize_t __wrap_recv(int fd, void *buffer, size_t size, int flags);
ssize_t __wrap_recv(int fd, void *buffer, size_t size, int flags)
{
    /* aot_defer_wait reaches recv only after READY and the W/log-fd reply.
     * Publish once, including when the real recv is retried after EINTR. */
    static int waiting;
    if (hold_marker && !waiting) {
        write_hold_marker(".waiting", "W\n");
        waiting = 1;
    }
    return __real_recv(fd, buffer, size, flags);
}

static pid_t snapshot(const char *marker, int status)
{
    int fd = aot_defer_connect(getenv(AOT_DEFER_ENV));
    assert(fd >= 0);
    pid_t pid = fork();
    assert(pid >= 0);
    if (!pid) {
        if (aot_defer_wait(fd)) {
            _exit(42);
        }
        close(fd);
        fd = open(marker, O_WRONLY | O_CREAT | O_APPEND, 0600);
        if (fd < 0 || write(fd, "G\n", 2) != 2) {
            _exit(43);
        }
        close(fd);
        usleep(50000);
        _exit(status);
    }
    close(fd);
    return pid;
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    if (!strcmp(argv[1], "pipe")) {
        int pipes[2], status;
        assert(pipe(pipes) == 0);
        pid_t pid = fork();
        assert(pid >= 0);
        if (!pid) {
            close(pipes[0]);
            assert(dup2(pipes[1], STDOUT_FILENO) >= 0);
            snapshot(argv[2], 0);
            _exit(0);
        }
        close(pipes[1]);
        char byte;
        assert(read(pipes[0], &byte, 1) == 0);
        close(pipes[0]);
        assert(waitpid(pid, &status, 0) == pid && status == 0);
        assert(access(argv[2], F_OK) < 0);
        return 0;
    }
    if (!strcmp(argv[1], "orphan")) {
        pid_t pid = fork();
        assert(pid >= 0);
        if (!pid) {
            pid = fork();
            assert(pid >= 0);
            if (!pid) {
                snapshot(argv[2], 0);
                usleep(250000);
                assert(access(argv[2], F_OK) < 0);
                _exit(0);
            }
            _exit(0);
        }
        return 0;
    }
    if (!strcmp(argv[1], "hold")) {
        int status;
        hold_marker = argv[2];
        pid_t worker = snapshot(argv[2], 0);
        assert(waitpid(worker, &status, 0) == worker);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 42);
        assert(access(argv[2], F_OK) < 0);
        write_hold_marker(".cancelled", "C\n");
        return 0;
    }
    snapshot(argv[2], !strcmp(argv[1], "worker-failure") ? 7 : 0);
    snapshot(argv[2], 0);
    assert(access(argv[2], F_OK) < 0);
    return !strcmp(argv[1], "app-failure") ? 23 : 0;
}
