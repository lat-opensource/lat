// SPDX-License-Identifier: GPL-2.0-or-later
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/statvfs.h>
#include <unistd.h>

#define PR_GET_IO_FLUSHER 58

typedef int (*prctl_fn)(int, unsigned long, unsigned long, unsigned long,
                        unsigned long);

int prctl(int option, ...)
{
    static prctl_fn real_prctl;
    unsigned long arg1;
    unsigned long arg2;
    unsigned long arg3;
    unsigned long arg4;
    va_list ap;

    if (option == PR_GET_IO_FLUSHER) {
        return 0;
    }
    if (!real_prctl) {
        real_prctl = (prctl_fn)dlsym(RTLD_NEXT, "prctl");
    }
    va_start(ap, option);
    arg1 = va_arg(ap, unsigned long);
    arg2 = va_arg(ap, unsigned long);
    arg3 = va_arg(ap, unsigned long);
    arg4 = va_arg(ap, unsigned long);
    va_end(ap);
    return real_prctl(option, arg1, arg2, arg3, arg4);
}

int fstatvfs(int fd, struct statvfs *buf)
{
    static int (*real_fstatvfs)(int, struct statvfs *);
    int ret;

    if (!real_fstatvfs) {
        real_fstatvfs = (int (*)(int, struct statvfs *))
                        dlsym(RTLD_NEXT, "fstatvfs");
    }
    ret = real_fstatvfs(fd, buf);
    if (ret == 0) {
        buf->f_flag &= ~ST_NOEXEC;
    }
    return ret;
}

int faccessat(int dirfd, const char *pathname, int mode, int flags)
{
    static int (*real_faccessat)(int, const char *, int, int);

    if (!pathname[0] && (flags & AT_EMPTY_PATH)) {
        return 0;
    }
    if (!real_faccessat) {
        real_faccessat = (int (*)(int, const char *, int, int))
                         dlsym(RTLD_NEXT, "faccessat");
    }
    return real_faccessat(dirfd, pathname, mode, flags);
}
