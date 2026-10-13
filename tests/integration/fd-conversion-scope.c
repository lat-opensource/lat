/*
 * An endian-only descriptor must not attach a conversion to an unrelated
 * descriptor with the same number in another thread's private file table.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <unistd.h>

static int target_fd, split, failed, fork_child;
static atomic_int ready, finish;
static int worker_error;

static void *worker(void *unused)
{
    int efd;

    (void)unused;
    if (split || failed) {
        int ret = unshare(CLONE_FILES | (failed ? CLONE_THREAD : 0));
        if (failed ? (ret != -1 || errno != EINVAL) : ret != 0) {
            worker_error = !failed && (errno == ENOSYS || errno == EPERM) ?
                           77 : 10;
            goto done;
        }
    }
    efd = eventfd(7, EFD_CLOEXEC);
    if (efd < 0 || dup2(efd, target_fd) < 0) {
        worker_error = 11;
        goto done;
    }
    close(efd);
    if (fork_child) {
        pid_t pid = fork();
        int status;
        if (!pid) {
            int copy = dup(target_fd);
            _exit(copy < 0 || close(copy));
        }
        if (pid < 0 || waitpid(pid, &status, 0) != pid || status) {
            worker_error = 12;
            goto done;
        }
    }
done:
    atomic_store(&ready, 1);
    while (!atomic_load(&finish)) {
        sched_yield();
    }
    return NULL;
}

int main(int argc, char **argv)
{
    pthread_t thread;
    char byte = 0;
    ssize_t result;
    int error;

    if (argc != 2 || (strcmp(argv[1], "shared") &&
                      strcmp(argv[1], "failed") &&
                      strcmp(argv[1], "split") &&
                      strcmp(argv[1], "split-fork"))) {
        return 2;
    }
    split = !strcmp(argv[1], "split") || !strcmp(argv[1], "split-fork");
    failed = !strcmp(argv[1], "failed");
    fork_child = !strcmp(argv[1], "split-fork");
    alarm(10);
    target_fd = open("/dev/zero", O_RDONLY);
    if (target_fd < 0 || pthread_create(&thread, NULL, worker, NULL)) {
        return 2;
    }
    while (!atomic_load(&ready)) {
        sched_yield();
    }
    errno = 0;
    result = read(target_fd, &byte, 1);
    error = errno;
    atomic_store(&finish, 1);
    if (pthread_join(thread, NULL)) {
        return 6;
    }
    printf("%s: read=%zd errno=%d worker=%d\n", argv[1], result, error,
           worker_error);
    return worker_error ? worker_error :
           (split ? result != 1 : result != -1 || error != EINVAL);
}
