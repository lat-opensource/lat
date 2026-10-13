/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

static long directory_syscall;
static char identity[16];
static int proc_fd, ordinary_fd, target_fd;
static int task_source;
static atomic_int stop;

static int find_identity_fd(void)
{
    char exe[4096], target[4096], path[64];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe));

    if (len <= 0) {
        return -1;
    }
    for (int fd = 3; fd < 256; fd++) {
        snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
        if (readlink(path, target, sizeof(target)) == len &&
            !memcmp(exe, target, len)) {
            snprintf(identity, sizeof(identity), "%d", fd);
            return fd;
        }
    }
    return -1;
}

/* One batch must contain both the marker and the ordinary numeric filename. */
static int names(const unsigned char *buf, long len)
{
    int name_offset = directory_syscall == SYS_getdents64 ? 19 :
                      2 * sizeof(long) + 2;
    int reclen_offset = directory_syscall == SYS_getdents64 ? 16 :
                        2 * sizeof(long);
    int found = 0;

    for (long offset = 0; offset < len; ) {
        unsigned short reclen;
        const char *name = (const char *)buf + offset + name_offset;

        if (len - offset <= name_offset) {
            return -1;
        }
        memcpy(&reclen, buf + offset + reclen_offset, sizeof(reclen));
        if (reclen <= name_offset || reclen > len - offset ||
            !memchr(name, 0, reclen - name_offset)) {
            return -1;
        }
        found |= !strcmp(name, identity) ? 1 : 0;
        found |= !strcmp(name, "ordinary-marker") ? 2 : 0;
        offset += reclen;
    }
    return found;
}

static void *direct_tid(void *unused)
{
    unsigned char buf[1024];
    char path[64];
    long len;
    int fd, result = 0;

    (void)unused;

    snprintf(path, sizeof(path), "/proc/%ld/fd", syscall(SYS_gettid));
    fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        return (void *)(intptr_t)11;
    }
    while ((len = syscall(directory_syscall, fd, buf, sizeof(buf))) > 0) {
        if (names(buf, len) != 0) {
            result = 12;
            break;
        }
    }
    close(fd);
    return (void *)(intptr_t)(len < 0 ? 13 : result);
}

static void *replace_directory(void *unused)
{
    (void)unused;
    while (!atomic_load(&stop)) {
        if (dup2(proc_fd, target_fd) < 0 ||
            dup2(ordinary_fd, target_fd) < 0) {
            return (void *)(intptr_t)31;
        }
        sched_yield();
    }
    return NULL;
}

static int replacement_race(const char *directory, int private_fd)
{
    unsigned char buf[8192];
    char path[512];
    const char *files[] = { identity, "ordinary-marker" };
    pthread_t thread;
    void *worker_result;
    int observed = 0, result = 0;

    for (unsigned int i = 0; i < 2; i++) {
        int fd;

        if (snprintf(path, sizeof(path), "%s/%s", directory, files[i]) >=
            (int)sizeof(path)) {
            return 30;
        }
        fd = open(path, O_WRONLY | O_CREAT, 0600);
        if (fd < 0 || close(fd)) {
            return 30;
        }
    }
    proc_fd = open(task_source ? "/proc/self/task" : "/proc/self/fd",
                   O_RDONLY | O_DIRECTORY);
    ordinary_fd = open(directory, O_RDONLY | O_DIRECTORY);
    target_fd = private_fd + 64;
    if (proc_fd < 0 || ordinary_fd < 0 ||
        dup2(proc_fd, target_fd) < 0 ||
        pthread_create(&thread, NULL, replace_directory, NULL)) {
        return 31;
    }
    for (int i = 0; i < 200000; i++) {
        long len;
        int found;

        lseek(target_fd, 0, SEEK_SET);
        len = syscall(directory_syscall, target_fd, buf, sizeof(buf));
        found = len < 0 ? -1 : names(buf, len);

        if (found < 0 || found == 2) {
            result = 33;
            break;
        }
        observed += found == 3;
    }
    atomic_store(&stop, 1);
    pthread_join(thread, &worker_result);
    return result ? result : (worker_result || !observed ? 34 : 0);
}

static int task_directory_has_pid(pid_t pid)
{
    unsigned char buf[1024];
    char path[64];
    int fd, found = 0;
    long len;

    snprintf(identity, sizeof(identity), "%d", pid);
    snprintf(path, sizeof(path), "/proc/%d/task", pid);
    fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        return 54;
    }
    while ((len = syscall(directory_syscall, fd, buf, sizeof(buf))) > 0) {
        int present = names(buf, len);

        if (present < 0) {
            close(fd);
            return 55;
        }
        found |= present & 1;
    }
    close(fd);
    return len < 0 || !found ? 56 : 0;
}

static int fork_task(void)
{
    pid_t child = fork();
    int status;

    if (child < 0) {
        return 51;
    }
    if (!child) {
        _exit(task_directory_has_pid(getpid()));
    }
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status)) {
        return 58;
    }
    return WEXITSTATUS(status);
}

static int other_task(void)
{
    int ready[2], finish[2], status, result = 0;
    char byte = 0;
    pid_t child;

    if (pipe(ready) || pipe(finish)) {
        return 50;
    }
    child = fork();
    if (child < 0) {
        return 51;
    }
    if (!child) {
        close(ready[0]);
        close(finish[1]);
        byte = 'R';
        if (write(ready[1], &byte, 1) != 1 ||
            read(finish[0], &byte, 1) != 1 || byte != 'F') {
            _exit(52);
        }
        _exit(0);
    }
    close(ready[1]);
    close(finish[0]);
    if (read(ready[0], &byte, 1) != 1 || byte != 'R') {
        return 53;
    }
    result = task_directory_has_pid(child);
    if (waitpid(child, &status, WNOHANG) != 0) {
        return 57;
    }
    byte = 'F';
    if (write(finish[1], &byte, 1) != 1 ||
        waitpid(child, &status, 0) != child || status) {
        return 58;
    }
    return result;
}

int main(int argc, char **argv)
{
    pthread_t thread;
    void *result;
    int private_fd;

    if (argc != 4) {
        return 2;
    }
    alarm(25);
    directory_syscall = !strcmp(argv[2], "64") ? SYS_getdents64 : SYS_getdents;
    if (!strcmp(argv[1], "other")) {
        return other_task();
    }
    if (!strcmp(argv[1], "fork-self")) {
        return fork_task();
    }
    private_fd = find_identity_fd();
    if (private_fd < 0) {
        return 10;
    }
    task_source = !strcmp(argv[1], "task-race");
    if (task_source) {
        /* No real task has TID zero; an ordinary filename must survive. */
        strcpy(identity, "0");
    }
    if (task_source || !strcmp(argv[1], "race")) {
        return replacement_race(argv[3], private_fd);
    }
    if (pthread_create(&thread, NULL, direct_tid, NULL) ||
        pthread_join(thread, &result)) {
        return 11;
    }
    return (intptr_t)result;
}
