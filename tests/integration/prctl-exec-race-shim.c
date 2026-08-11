/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

static int blocked;

static ssize_t intercept_pread(int fd, void *buffer, size_t count,
                               off64_t offset)
{
    const char *target = getenv("LATX_EXEC_RACE_TARGET");
    const char *ready = getenv("LATX_EXEC_RACE_READY");
    const char *release = getenv("LATX_EXEC_RACE_RELEASE");
    struct stat fd_stat, target_stat;
    ssize_t ret;
    int marker;
    int i;

    ret = syscall(SYS_pread64, fd, buffer, count, offset);
    if (ret <= 0 || offset || !target || !ready || !release ||
        fstat(fd, &fd_stat) || stat(target, &target_stat) ||
        fd_stat.st_dev != target_stat.st_dev ||
        fd_stat.st_ino != target_stat.st_ino ||
        !__sync_bool_compare_and_swap(&blocked, 0, 1)) {
        return ret;
    }

    marker = open(ready, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (marker >= 0) {
        close(marker);
    }
    for (i = 0; i < 10000; i++) {
        if (access(release, F_OK) == 0) {
            break;
        }
        usleep(1000);
    }
    return ret;
}

ssize_t pread(int fd, void *buffer, size_t count, off_t offset)
{
    return intercept_pread(fd, buffer, count, offset);
}

ssize_t pread64(int fd, void *buffer, size_t count, off64_t offset)
{
    return intercept_pread(fd, buffer, count, offset);
}
