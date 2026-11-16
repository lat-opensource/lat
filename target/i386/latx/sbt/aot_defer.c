/* SPDX-License-Identifier: GPL-2.0-only */
#include "qemu/osdep.h"
#include <sys/socket.h>
#include <sys/un.h>
#include "aot_defer.h"

int aot_defer_connect(const char *path)
{
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    struct ucred peer;
    socklen_t peer_size = sizeof(peer);
    int fd, saved;

    if (strlen(path) >= sizeof(address.sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(address.sun_path, path, strlen(path) + 1);
    fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return -1;
    }
    if (fd < 3) {
        int copy = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        saved = errno;
        close(fd);
        errno = saved;
        fd = copy;
        if (fd < 0) {
            return -1;
        }
    }
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peer, &peer_size) < 0) {
        goto fail;
    }
    if (peer.uid != geteuid()) {
        errno = EACCES;
        goto fail;
    }
    return fd;
fail:
    saved = errno;
    close(fd);
    errno = saved;
    return -1;
}

static int detach_application_fds(int control_fd)
{
    DIR *directory = opendir("/proc/self/fd");
    struct dirent *entry;
    int null_fd;

    if (!directory) {
        return -1;
    }
    while ((entry = readdir(directory))) {
        char *end;
        long fd = strtol(entry->d_name, &end, 10);
        if (!*entry->d_name || *end || fd < 0 || fd > INT_MAX ||
            fd == dirfd(directory) || fd == control_fd) {
            continue;
        }
        close(fd);
    }
    closedir(directory);
    null_fd = open("/dev/null", O_RDWR | O_CLOEXEC);
    if (null_fd < 0) {
        return -1;
    }
    for (int fd = 0; fd < 3; fd++) {
        if (dup2(null_fd, fd) < 0) {
            if (null_fd > 2) {
                close(null_fd);
            }
            return -1;
        }
    }
    if (null_fd > 2) {
        close(null_fd);
    }
    return 0;
}

int aot_defer_wait(int control_fd)
{
    AOTDeferReady ready = { .magic = AOT_DEFER_MAGIC, .pid = getpid() };
    char command;
    union {
        struct cmsghdr align;
        char bytes[CMSG_SPACE(sizeof(int))];
    } ancillary;
    struct iovec iov = { .iov_base = &command, .iov_len = 1 };
    struct msghdr message = { .msg_iov = &iov, .msg_iovlen = 1,
        .msg_control = ancillary.bytes, .msg_controllen = sizeof(ancillary) };
    struct cmsghdr *header;
    int log_fd;
    ssize_t count;

    if (detach_application_fds(control_fd) < 0) {
        return -1;
    }
    do {
        count = send(control_fd, &ready, sizeof(ready), MSG_NOSIGNAL);
    } while (count < 0 && errno == EINTR);
    if (count != sizeof(ready)) {
        return -1;
    }
    do {
        count = recvmsg(control_fd, &message, MSG_CMSG_CLOEXEC);
    } while (count < 0 && errno == EINTR);
    header = CMSG_FIRSTHDR(&message);
    if (count != 1 || command != 'W' ||
        (message.msg_flags & (MSG_CTRUNC | MSG_TRUNC)) || !header ||
        header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS ||
        header->cmsg_len != CMSG_LEN(sizeof(int))) {
        errno = EPROTO;
        return -1;
    }
    memcpy(&log_fd, CMSG_DATA(header), sizeof(log_fd));
    if (dup2(log_fd, STDERR_FILENO) < 0) {
        close(log_fd);
        return -1;
    }
    if (log_fd != STDERR_FILENO) {
        close(log_fd);
    }
    do {
        count = recv(control_fd, &command, 1, 0);
    } while (count < 0 && errno == EINTR);
    if (count != 1 || command != 'G') {
        errno = ECANCELED;
        return -1;
    }
    return 0;
}
