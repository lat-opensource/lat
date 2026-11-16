/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "aot_defer.h"

typedef struct Worker {
    int fd;
    pid_t pid, peer_pid;
    bool released, reaped;
    int status;
    double ready_ms, start_ms, end_ms;
} Worker;

typedef struct ExitRecord {
    pid_t pid;
    int status;
    double time_ms;
    struct ExitRecord *next;
} ExitRecord;

typedef struct RegistrationAbort {
    pid_t peer_pid;
    double time_ms;
    struct RegistrationAbort *next;
} RegistrationAbort;

static double epoch;
static double now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1000000.0 - epoch;
}

static int exit_code(int status)
{
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

static Worker *find_worker(Worker *workers, int limit, pid_t pid)
{
    for (int i = 0; i < limit; i++) {
        if (workers[i].pid == pid && pid > 0) {
            return &workers[i];
        }
    }
    return NULL;
}

/* Subreaper adoption makes each remaining subtree visible as a direct child.
 * An unknown child, including a worker not registered yet, holds the barrier.
 */
static int only_workers(Worker *workers, int limit, int *children)
{
    char path[80], *line = NULL, *p, *end;
    size_t size = 0;
    FILE *file;
    int answer = 1;
    ssize_t length;

    snprintf(path, sizeof(path), "/proc/self/task/%ld/children", (long)getpid());
    file = fopen(path, "r");
    if (!file) {
        return -1;
    }
    errno = 0;
    length = getline(&line, &size, file);
    if (length < 0 && ferror(file)) {
        fclose(file);
        free(line);
        return -1;
    }
    fclose(file);
    *children = 0;
    p = length > 0 ? line : NULL;
    while (p && *p) {
        long pid = strtol(p, &end, 10);
        Worker *worker;
        if (end == p) {
            break;
        }
        if (pid <= 0 || pid > INT_MAX) {
            answer = -1;
            break;
        }
        (*children)++;
        worker = find_worker(workers, limit, pid);
        if (!worker || worker->reaped) {
            answer = 0;
        }
        p = end;
    }
    free(line);
    return answer;
}

static int send_wait_log(int fd)
{
    char command = 'W';
    int log_fd = STDERR_FILENO;
    struct iovec iov = { .iov_base = &command, .iov_len = 1 };
    union { struct cmsghdr align; char data[CMSG_SPACE(sizeof(int))]; } control;
    struct msghdr message = { .msg_iov = &iov, .msg_iovlen = 1,
        .msg_control = control.data, .msg_controllen = sizeof(control) };
    struct cmsghdr *header;
    ssize_t result;

    memset(&control, 0, sizeof(control));
    header = CMSG_FIRSTHDR(&message);
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_RIGHTS;
    header->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(header), &log_fd, sizeof(log_fd));
    do {
        result = sendmsg(fd, &message, MSG_NOSIGNAL);
    } while (result < 0 && errno == EINTR);
    return result == 1 ? 0 : -1;
}

int main(int argc, char **argv)
{
    const char *report = NULL;
    int limit = 64, arg = 1, listener = -1, root_status = 0;
    int failures = 0, children = 0;
    bool root_done = false, released = false;
    double root_exit_ms = 0, app_exit_ms = 0;
    char directory[] = "/tmp/latx-aot-XXXXXX";
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    Worker *workers;
    struct pollfd *polls;
    ExitRecord *exits = NULL;
    RegistrationAbort *aborts = NULL;
    pid_t root;
    struct sigaction inherited_sigchld, wait_sigchld = { .sa_handler = SIG_DFL };

    epoch = now_ms();
    while (arg < argc && strcmp(argv[arg], "--")) {
        if (!strcmp(argv[arg], "--report") && arg + 1 < argc) {
            report = argv[++arg];
        } else if (!strcmp(argv[arg], "--max-workers") && arg + 1 < argc) {
            char *end;
            long value = strtol(argv[++arg], &end, 10);
            if (*end || value < 1 || value > 1024) {
                fprintf(stderr, "invalid --max-workers (1..1024)\n");
                return 2;
            }
            limit = value;
        } else {
            break;
        }
        arg++;
    }
    if (arg < argc && !strcmp(argv[arg], "--")) {
        arg++;
    }
    if (arg >= argc) {
        fprintf(stderr, "usage: latx-aot-collect [--report FILE] [--max-workers N] -- COMMAND [ARG...]\n");
        return 2;
    }
    workers = calloc(limit, sizeof(*workers));
    polls = calloc(limit + 1, sizeof(*polls));
    if (!workers || !polls || prctl(PR_SET_CHILD_SUBREAPER, 1) < 0 ||
        !mkdtemp(directory)) {
        perror("latx-aot-collect setup");
        return 2;
    }
    for (int i = 0; i < limit; i++) {
        workers[i].fd = -1;
    }
    snprintf(address.sun_path, sizeof(address.sun_path), "%s/control", directory);
    listener = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (listener < 0 || bind(listener, (struct sockaddr *)&address,
                             sizeof(address)) < 0 || listen(listener, 64) < 0) {
        perror("latx-aot-collect socket");
        if (listener >= 0) {
            close(listener);
        }
        unlink(address.sun_path);
        rmdir(directory);
        return 2;
    }
    /* SIG_IGN would auto-reap children and make waitpid lose their status. */
    sigemptyset(&wait_sigchld.sa_mask);
    if (sigaction(SIGCHLD, &wait_sigchld, &inherited_sigchld) < 0) {
        perror("latx-aot-collect SIGCHLD");
        close(listener);
        unlink(address.sun_path);
        rmdir(directory);
        return 2;
    }
    root = fork();
    if (root < 0) {
        perror("latx-aot-collect fork");
        close(listener);
        unlink(address.sun_path);
        rmdir(directory);
        return 2;
    }
    if (!root) {
        close(listener);
        if (sigaction(SIGCHLD, &inherited_sigchld, NULL) < 0) {
            _exit(126);
        }
        if (setenv(AOT_DEFER_ENV, address.sun_path, 1) ||
            setenv("LATX_QUICKTRANS_AOT_COLLECT", "1", 1) ||
            setenv("LATX_FAST_TRANSLATOR", "1", 1) ||
            setenv("LATX_FAST_TRANSLATOR_STRICT", "1", 1) ||
            setenv("LATX_AOT", "0", 1) || setenv("LATX_LOAD_AOT", "0", 1) ||
            setenv("LATX_AOT_GENERATE", "0", 1) || setenv("LATX_KZT", "0", 1)) {
            _exit(126);
        }
        execvp(argv[arg], &argv[arg]);
        perror("latx-aot-collect exec");
        _exit(127);
    }
    for (;;) {
        int status, pending = 0;
        pid_t pid;
        polls[0] = (struct pollfd){ .fd = listener, .events = POLLIN };
        for (int i = 0; i < limit; i++) {
            polls[i + 1] = (struct pollfd){ .fd = workers[i].fd, .events = POLLIN };
        }
        if (poll(polls, limit + 1, 10) < 0 && errno != EINTR) {
            perror("latx-aot-collect poll");
            failures++;
            break;
        }
        if (polls[0].revents & POLLIN) {
            int fd;
            while ((fd = accept4(listener, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK)) >= 0) {
                struct ucred peer;
                socklen_t length = sizeof(peer);
                int slot;
                for (slot = 0; slot < limit; slot++) {
                    if (workers[slot].fd < 0 && !workers[slot].pid) {
                        break;
                    }
                }
                if (released || slot == limit ||
                    getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peer, &length) < 0 ||
                    peer.uid != geteuid()) {
                    fprintf(stderr, "AOT deferred worker failed: registration rejected\n");
                    close(fd);
                    failures++;
                } else {
                    workers[slot].fd = fd;
                    workers[slot].peer_pid = peer.pid;
                }
            }
        }
        for (int i = 0; i < limit; i++) {
            Worker *worker = &workers[i];
            AOTDeferReady ready;
            ssize_t count;
            if (worker->fd < 0) {
                continue;
            }
            count = recv(worker->fd, &ready, sizeof(ready), MSG_DONTWAIT);
            if (count < 0 && (errno == EAGAIN || errno == EINTR)) {
                continue;
            }
            if (count == sizeof(ready) && !worker->pid &&
                ready.magic == AOT_DEFER_MAGIC && ready.pid > 0 &&
                !find_worker(workers, limit, ready.pid)) {
                worker->pid = ready.pid;
                worker->ready_ms = now_ms();
                for (ExitRecord *e = exits; e; e = e->next) {
                    if (e->pid == ready.pid) {
                        worker->reaped = true;
                        worker->status = e->status;
                        worker->end_ms = e->time_ms;
                        break;
                    }
                }
                if (send_wait_log(worker->fd) == 0) {
                    continue;
                }
            } else if (!count && !worker->pid) {
                /* The application connects before the snapshot fork. It may
                 * exit or be signalled before any worker sends READY. Keep
                 * that lost coverage visible without inventing a failed
                 * generator. SO_PEERCRED identifies the connecting process,
                 * not the child PID subsequently supplied in READY.
                 */
                RegistrationAbort *abort = malloc(sizeof(*abort));
                fprintf(stderr, "AOT collection incomplete: peer_pid=%ld "
                        "disconnected before READY (no registered worker)\n",
                        (long)worker->peer_pid);
                if (!abort) {
                    perror("latx-aot-collect registration abort record");
                    failures++;
                } else {
                    *abort = (RegistrationAbort){ worker->peer_pid,
                                                  now_ms(), aborts };
                    aborts = abort;
                }
                close(worker->fd);
                worker->fd = -1;
                worker->peer_pid = 0;
                continue;
            } else if (!count && worker->released) {
                close(worker->fd);
                worker->fd = -1;
                continue;
            }
            fprintf(stderr, "AOT deferred worker failed: control connection\n");
            failures++;
            close(worker->fd);
            worker->fd = -1;
        }
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            Worker *worker = find_worker(workers, limit, pid);
            double time_ms = now_ms();
            if (pid == root) {
                root_done = true;
                root_status = status;
                root_exit_ms = time_ms;
            } else if (worker) {
                worker->reaped = true;
                worker->status = status;
                worker->end_ms = time_ms;
                if (!worker->released || exit_code(status)) {
                    fprintf(stderr, "AOT deferred worker failed: pid=%ld status=%d\n",
                            (long)pid, exit_code(status));
                    failures++;
                }
            } else {
                ExitRecord *e = malloc(sizeof(*e));
                if (!e) {
                    perror("latx-aot-collect exit record");
                    failures++;
                    goto finish;
                }
                *e = (ExitRecord){ pid, status, time_ms, exits };
                exits = e;
            }
        }
        for (int i = 0; i < limit; i++) {
            pending += workers[i].fd >= 0 && !workers[i].pid;
        }
        status = only_workers(workers, limit, &children);
        if (status < 0) {
            perror("latx-aot-collect children");
            failures++;
            break;
        }
        if (!released && root_done && status && !pending) {
            released = true;
            app_exit_ms = now_ms();
        }
        if (released) {
            /* Keep the existing generator concurrency and cache locking.
             * The controller changes only the application-exit barrier. */
            for (int i = 0; i < limit; i++) {
                Worker *next = &workers[i];
                if (!next->pid || next->released || next->reaped) {
                    continue;
                }
                next->released = true;
                next->start_ms = now_ms();
                if (next->fd < 0 || send(next->fd, "G", 1, MSG_NOSIGNAL) != 1) {
                    fprintf(stderr, "AOT deferred worker failed: release\n");
                    failures++;
                    if (next->fd >= 0) {
                        close(next->fd);
                        next->fd = -1;
                    }
                }
            }
        }
        if (root_done && !children && !pending) {
            break;
        }
    }
finish:
    if (report) {
        FILE *file = fopen(report, "w");
        if (file) {
            bool comma = false;
            bool root_snapshot = false;
            for (int i = 0; i < limit; i++) {
                if (workers[i].pid && workers[i].peer_pid == root) {
                    root_snapshot = true;
                    break;
                }
            }
            /* Compatibility fields below retain their protocol-only meaning.
             * A successful protocol cannot prove coverage of unobserved exits. */
            bool coverage_incomplete = failures || aborts ||
                (root_done && WIFSIGNALED(root_status) && !root_snapshot);
            fprintf(file, "{\"report_scope\":\"observed_collection_protocol\","
                    "\"coverage_status\":\"%s\",",
                    coverage_incomplete ? "incomplete" : "not_proven");
            fprintf(file, "\"app_status\":%d,\"root_exit_ms\":%.3f,\"app_exit_ms\":%.3f,\"all_exit_ms\":%.3f,\"failures\":%d,\"workers\":[",
                    root_done ? exit_code(root_status) : -1, root_exit_ms,
                    app_exit_ms, now_ms(), failures);
            for (int i = 0; i < limit; i++) {
                Worker *w = &workers[i];
                if (!w->pid) {
                    continue;
                }
                fprintf(file, "%s{\"pid\":%ld,\"ready_ms\":%.3f,\"start_ms\":%.3f,\"end_ms\":%.3f,\"status\":%d,\"released\":%s,\"reaped\":%s}",
                        comma ? "," : "", (long)w->pid, w->ready_ms,
                        w->start_ms, w->end_ms, w->reaped ? exit_code(w->status) : -1,
                        w->released ? "true" : "false", w->reaped ? "true" : "false");
                comma = true;
            }
            fprintf(file, "],\"collection_incomplete\":%s,"
                    "\"registration_aborts\":[",
                    failures || aborts ? "true" : "false");
            comma = false;
            for (RegistrationAbort *a = aborts; a; a = a->next) {
                fprintf(file, "%s{\"peer_pid\":%ld,\"time_ms\":%.3f,"
                        "\"reason\":\"eof_before_ready\"}",
                        comma ? "," : "", (long)a->peer_pid, a->time_ms);
                comma = true;
            }
            fprintf(file, "]}\n");
            if (fclose(file)) {
                perror("latx-aot-collect report");
            }
        } else {
            perror("latx-aot-collect report");
        }
    }
    for (int i = 0; i < limit; i++) {
        if (workers[i].fd >= 0) {
            close(workers[i].fd);
        }
    }
    close(listener);
    unlink(address.sun_path);
    rmdir(directory);
    while (exits) {
        ExitRecord *next = exits->next;
        free(exits);
        exits = next;
    }
    while (aborts) {
        RegistrationAbort *next = aborts->next;
        free(aborts);
        aborts = next;
    }
    free(workers);
    free(polls);
    return root_done ? exit_code(root_status) : 125;
}
