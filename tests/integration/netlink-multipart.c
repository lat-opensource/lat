#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/auxv.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
static void check(int ok, const char *what)
{
    if (!ok) {
        fprintf(stderr, "FAIL %s errno=%d\n", what, errno);
        exit(1);
    }
}

static int create(void)
{
    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    struct sockaddr_nl addr = {.nl_family = AF_NETLINK};
    check(fd >= 0, "socket");
    check(!bind(fd, (void *)&addr, sizeof(addr)), "bind");
    return fd;
}

static void request(int fd)
{
    struct sockaddr_nl addr = {.nl_family = AF_NETLINK};
    struct {
        struct nlmsghdr h;
        struct ifinfomsg info;
    } req = {.h = {.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
                   .nlmsg_type = RTM_GETLINK,
                   .nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP,
                   .nlmsg_seq = 77},
             .info = {.ifi_family = AF_UNSPEC} };
    check(sendto(fd, &req, sizeof(req), 0, (void *)&addr, sizeof(addr)) ==
              sizeof(req),
          "send dump");
}

static int parse(char *data, int got, int *done)
{
    int n = 0, left = got;
    struct nlmsghdr *h = (void *)data;
    for (; NLMSG_OK(h, left); h = NLMSG_NEXT(h, left)) {
        check(h->nlmsg_seq == 77, "sequence");
        if (h->nlmsg_type == RTM_NEWLINK) {
            n++;
        }
        if (h->nlmsg_type == NLMSG_DONE) {
            *done = 1;
        }
        check(h->nlmsg_type != NLMSG_ERROR, "netlink error");
    }
    check(!left, "complete message");
    return n;
}

static ssize_t receive(int fd, char *data, size_t cap, int method)
{
    struct sockaddr_nl from = {0};
    socklen_t len = sizeof(from);
    ssize_t got;
    struct iovec vec[2] = { {data, cap / 2},
                            {data + cap / 2, cap - cap / 2} };
    struct msghdr m = {.msg_name = &from,
                       .msg_namelen = sizeof(from),
                       .msg_iov = vec,
                       .msg_iovlen = 2};
    if (method == 0) {
        got = recvfrom(fd, data, cap, 0, (void *)&from, &len);
        check(got < 0 || from.nl_family == AF_NETLINK, "recvfrom family");
    } else if (method == 1) {
        got = recvmsg(fd, &m, 0);
        check(got < 0 || !(m.msg_flags & MSG_TRUNC), "recvmsg no truncation");
        check(got < 0 || from.nl_family == AF_NETLINK, "recvmsg family");
    } else if (method == 2) {
        got = read(fd, data, cap);
    } else if (method == 3) {
        got = readv(fd, vec, 2);
    } else {
        struct mmsghdr mm = {.msg_hdr = m};
        int r = recvmmsg(fd, &mm, 1, 0, NULL);
        got = r < 0 ? r : mm.msg_len;
    }
    return got;
}

static void require_large_dump(int fd)
{
    char data[65536];
    ssize_t size = recv(fd, data, sizeof(data), MSG_PEEK);
    check(size > 0, "preflight dump");
    if (size <= 4096) {
        puts("SKIP: kernel did not produce a dump larger than 4 KiB");
        exit(77);
    }
}

static int drain(int fd, int method)
{
    char data[8192], peek[4096];
    int count = 0, done = 0;
    int ep = epoll_create1(EPOLL_CLOEXEC);
    struct epoll_event ev = {.events = EPOLLIN, .data.fd = fd};
    check(!epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev), "epoll add");
    while (!done) {
        check(epoll_wait(ep, &ev, 1, 2000) == 1, "epoll readable");
        ssize_t p = recv(fd, peek, sizeof(peek), MSG_PEEK | MSG_DONTWAIT);
        check(p >= 0, "peek");
        ssize_t got = receive(fd, data, 4096, method);
        check(got >= 0, "receive");
        check(p == got && !memcmp(data, peek, got), "peek agrees");
        count += parse(data, got, &done);
    }
    check(recv(fd, data, sizeof(data), MSG_DONTWAIT) < 0 && errno == EAGAIN,
          "queue drained");
    close(ep);
    return count;
}

static atomic_int waiting;
static void *blocked(void *arg)
{
    char data[4096];
    atomic_store(&waiting, 1);
    ssize_t got = recv(*(int *)arg, data, sizeof(data), 0);
    check(got > 0, "blocked receiver resumed");
    return NULL;
}

static int pass_socket(int channel, int fd)
{
    union {
        struct cmsghdr align;
        char bytes[CMSG_SPACE(sizeof(int))];
    } control = {0};
    char byte = 'x';
    struct iovec vec = {&byte, 1};
    struct msghdr msg = {.msg_iov = &vec, .msg_iovlen = 1,
                        .msg_control = control.bytes,
                        .msg_controllen = sizeof(control.bytes)};
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (fd >= 0) {
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
        memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));
        check(sendmsg(channel, &msg, 0) == 1, "send socket");
        return fd;
    }
    check(recvmsg(channel, &msg, 0) == 1, "receive socket");
    cmsg = CMSG_FIRSTHDR(&msg);
    check(cmsg && cmsg->cmsg_level == SOL_SOCKET &&
          cmsg->cmsg_type == SCM_RIGHTS, "socket control");
    memcpy(&fd, CMSG_DATA(cmsg), sizeof(fd));
    return fd;
}

static void check_remainder(int fd, size_t expected)
{
    char data[65536];
    ssize_t got = recv(fd, data, sizeof(data), MSG_DONTWAIT);
    check(got == expected, "no replay after socket handoff");
    int done = 0;
    check(parse(data, got, &done) > 0, "valid remaining messages");
}

static void expired_timeout(int fd, const char *mode)
{
    char data[8][65536];
    struct iovec vec[8];
    struct mmsghdr msg[8] = {0};
    struct { long tv_sec, tv_nsec; } timeout = {
        .tv_nsec = !strcmp(mode, "expired-timeout-tiny")
    };

    for (unsigned int i = 0; i < 8; i++) {
        vec[i] = (struct iovec){ data[i], sizeof(data[i]) };
        msg[i].msg_hdr.msg_iov = &vec[i];
        msg[i].msg_hdr.msg_iovlen = 1;
    }
    request(fd);
    check(syscall(SYS_recvmmsg, fd, msg, 8, MSG_DONTWAIT, &timeout) == 1,
          "expired timeout stops after first receive");
    check(!timeout.tv_sec && !timeout.tv_nsec, "expired timeout writeback");
    /* A second queued datagram must remain available after the call. */
    check(recv(fd, data[1], sizeof(data[1]), MSG_DONTWAIT) > 0,
          "expired timeout leaves next datagram queued");
}

static volatile sig_atomic_t alarm_seen;

static void receive_alarm(int sig)
{
    alarm_seen = 1;
}

static void timeout_snapshot(int fd, const char *mode)
{
    char data[8][65536];
    struct iovec vec[8];
    struct mmsghdr msg[8] = {0};
    struct { long tv_sec, tv_nsec; } timeout = { .tv_sec = 1 };
    struct timeval socket_timeout = { .tv_usec = 300000 };
    struct sigaction action = { .sa_handler = receive_alarm };
    bool interrupted = !strcmp(mode, "timeout-snapshot-signal");

    for (unsigned int i = 0; i < 8; i++) {
        vec[i] = (struct iovec){ data[i], sizeof(data[i]) };
        msg[i].msg_hdr.msg_iov = &vec[i];
        msg[i].msg_hdr.msg_iovlen = 1;
    }
    request(fd);
    if (interrupted) {
        check(!sigaction(SIGALRM, &action, NULL), "receive signal handler");
        ualarm(300000, 0);
    } else {
        check(!setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout,
                         sizeof(socket_timeout)), "snapshot socket timeout");
    }
    int got = syscall(SYS_recvmmsg, fd, msg, 8, 0, &timeout);
    if (interrupted) {
        ualarm(0, 0);
        check(alarm_seen, "receive interrupted after progress");
    }
    check(got > 0 && got < 8, "snapshot partial success");
    int done = 0, links = 0;
    for (int i = 0; i < got; i++) {
        links += parse(data[i], msg[i].msg_len, &done);
    }
    check(done && links > 0, "snapshot received complete dump");
    /* The failed wait must not subtract its 300 ms from the snapshot. */
    check(timeout.tv_sec == 0 && timeout.tv_nsec > 850000000 &&
          timeout.tv_nsec < 1000000000, "last successful timeout snapshot");
}

static int64_t milliseconds(void)
{
    struct timespec now;
    check(!clock_gettime(CLOCK_MONOTONIC, &now), "monotonic clock");
    return now.tv_sec * 1000LL + now.tv_nsec / 1000000;
}

static void socket_timeout(int fd, const char *mode)
{
    char data[16][65536];
    struct iovec vec[16];
    struct mmsghdr msg[16] = {0};
    /* SYS_recvmmsg uses the guest long layout, including on i386. */
    struct { long tv_sec, tv_nsec; } timeout = { .tv_sec = 1 };
    struct timeval socket_timeout = { .tv_usec = 100000 };
    int flags = 0;
    bool progress = !strcmp(mode, "socket-timeout-data") ||
                    !strcmp(mode, "socket-timeout-waitforone");
    bool immediate = !strcmp(mode, "socket-timeout-dontwait") ||
                     !strcmp(mode, "socket-timeout-nonblock");
    bool call_first = !strcmp(mode, "socket-timeout-call-first");
    bool waitforone = !strcmp(mode, "socket-timeout-waitforone");

    if (immediate || waitforone) {
        socket_timeout.tv_sec = 1;
        socket_timeout.tv_usec = 0;
    }
    if (call_first) {
        socket_timeout.tv_sec = 1;
        socket_timeout.tv_usec = 0;
        timeout.tv_sec = 0;
        timeout.tv_nsec = 100000000;
    }
    check(!setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout,
                     sizeof(socket_timeout)), "recvmmsg socket timeout");
    if (!strcmp(mode, "socket-timeout-dontwait")) {
        flags = MSG_DONTWAIT;
    } else if (!strcmp(mode, "socket-timeout-nonblock")) {
        check(!fcntl(fd, F_SETFL, O_NONBLOCK), "recvmmsg nonblock");
    } else if (waitforone) {
        flags = MSG_WAITFORONE;
    }
    for (unsigned int i = 0; i < 16; i++) {
        vec[i] = (struct iovec){ data[i], sizeof(data[i]) };
        msg[i].msg_hdr.msg_iov = &vec[i];
        msg[i].msg_hdr.msg_iovlen = 1;
    }
    if (progress) {
        request(fd);
    }
    int64_t start = milliseconds();
    int got = syscall(SYS_recvmmsg, fd, msg, progress ? 16 : 1,
                      flags, &timeout);
    int error = errno;
    int64_t elapsed = milliseconds() - start;

    if (progress) {
        check(got > 0 && got < 16, "recvmmsg partial progress");
        int done = 0, links = 0;
        for (int i = 0; i < got; i++) {
            links += parse(data[i], msg[i].msg_len, &done);
        }
        check(done && links > 0, "recvmmsg complete dump before timeout");
        check(timeout.tv_sec == 0 && timeout.tv_nsec > 0 &&
              timeout.tv_nsec <= 1000000000, "progress timeout writeback");
    } else if (call_first) {
        check(got == 0 && timeout.tv_sec == 0 && timeout.tv_nsec == 0,
              "recvmmsg call timeout first");
    } else {
        check(got == -1 && error == EAGAIN, "recvmmsg socket EAGAIN");
        check(timeout.tv_sec == 1 && timeout.tv_nsec == 0,
              "initial EAGAIN preserves timeout");
    }
    check(elapsed < 800, "recvmmsg respects shorter timeout");
    if (!immediate && !waitforone) {
        check(elapsed >= 50, "recvmmsg did wait for timeout");
    }
}

static void large_timeout(int fd, const char *mode)
{
    char data[65536];
    struct { long tv_sec, tv_nsec; } timeout = { .tv_sec = 1 };
    struct iovec vec = { data, sizeof(data) };
    struct mmsghdr msg = { .msg_hdr = { .msg_iov = &vec, .msg_iovlen = 1 } };

    if (!strcmp(mode, "vlen0")) {
        check(syscall(SYS_recvmmsg, fd, NULL, 0, 0, &timeout) == 0 &&
              timeout.tv_sec == 1 && timeout.tv_nsec == 0,
              "empty recvmmsg vector");
    } else if (!strcmp(mode, "vlen0-bad-timeout")) {
        check(syscall(SYS_recvmmsg, fd, NULL, 0, 0, (void *)1) == -1 &&
              errno == EFAULT, "empty vector validates timeout");
    } else if (!strcmp(mode, "huge-timeout")) {
        timeout.tv_sec = LONG_MAX;
        request(fd);
        check(syscall(SYS_recvmmsg, fd, &msg, 1, 0, &timeout) == 1,
              "huge timeout receives queued datagram");
        check(timeout.tv_sec > 1000000000, "huge timeout does not collapse");
    } else {
        struct timeval socket_timeout = { .tv_sec = LONG_MAX };
        int64_t start;
        int got;

        check(!setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout,
                         sizeof(socket_timeout)), "huge socket timeout");
        timeout.tv_sec = 0;
        timeout.tv_nsec = 100000000;
        start = milliseconds();
        got = syscall(SYS_recvmmsg, fd, &msg, 1, 0, &timeout);
        check(got == 0 && milliseconds() - start >= 50 &&
              milliseconds() - start < 800,
              "huge socket timeout preserves call deadline");
    }
}

static void bad_iovec(int fd, const char *mode)
{
    char data[65536];
    struct iovec vec[2] = { { data, sizeof(data) }, { (void *)1, 4096 } };
    struct mmsghdr msg[2] = {
        { .msg_hdr = { .msg_iov = &vec[0], .msg_iovlen = 1 } },
        { .msg_hdr = { .msg_iov = &vec[1], .msg_iovlen = 1 },
          .msg_len = 0x12345678 },
    };
    bool partial = !strcmp(mode, "mmsg-partial-bad-iov");
    bool batch = !strncmp(mode, "mmsg-", 5);
    ssize_t got;

    request(fd);
    ssize_t queued = recv(fd, data, sizeof(data), MSG_PEEK);
    check(queued > 0, "bad iovec queued datagram");
    if (!partial) {
        if (strstr(mode, "bad-buffer")) {
            vec[0].iov_base = (void *)1;
        } else {
            msg[0].msg_hdr.msg_iov = (void *)1;
        }
    }
    if (batch) {
        got = syscall(SYS_recvmmsg, fd, msg, partial ? 2 : 1,
                      MSG_DONTWAIT, NULL);
    } else {
        got = recvmsg(fd, &msg[0].msg_hdr, MSG_DONTWAIT);
    }
    if (partial) {
        check(got == 1 && msg[0].msg_len == queued,
              "bad later iovec preserves completed receive");
        check(msg[1].msg_len == 0x12345678,
              "bad later iovec leaves its length unchanged");
    } else {
        check(got == -1 && errno == EFAULT, "bad iovec returns EFAULT");
        check(recv(fd, data, sizeof(data), MSG_PEEK | MSG_DONTWAIT) ==
                  queued, "bad iovec does not advance cursor");
    }
}

static void empty_iovec(int fd, const char *mode)
{
    char data[65536];
    struct iovec vec = { .iov_base = (void *)1, .iov_len = 0 };
    struct mmsghdr msg = {0};
    bool zero = strstr(mode, "zero-iov") != NULL;
    bool trunc = strstr(mode, "trunc") != NULL;
    bool peek = strstr(mode, "peek") != NULL;
    bool batch = !strncmp(mode, "mmsg-", 5);
    int flags = MSG_DONTWAIT | (trunc ? MSG_TRUNC : 0) |
                (peek ? MSG_PEEK : 0);
    ssize_t got;

    if (zero) {
        msg.msg_hdr.msg_iov = &vec;
        msg.msg_hdr.msg_iovlen = 1;
    }
    request(fd);
    ssize_t queued = recv(fd, data, sizeof(data), MSG_PEEK);
    check(queued > 0, "empty iovec queued datagram");
    if (batch) {
        got = syscall(SYS_recvmmsg, fd, &msg, 1, flags, NULL);
        check(got == 1, "empty iovec batch receive");
        got = msg.msg_len;
    } else {
        got = recvmsg(fd, &msg.msg_hdr, flags);
    }
    check(got == (trunc ? queued : 0) &&
          (msg.msg_hdr.msg_flags & MSG_TRUNC), "empty iovec truncation");
    got = recv(fd, data, sizeof(data), MSG_DONTWAIT);
    if (peek) {
        check(got == queued, "empty iovec peek did not consume");
    } else {
        check(got > 0 && ((struct nlmsghdr *)data)->nlmsg_type == NLMSG_DONE,
              "empty iovec consumed only the first datagram");
    }
}

static void receive_flags(int fd, const char *mode)
{
    char data[65536];
    union {
        struct cmsghdr align;
        char bytes[CMSG_SPACE(sizeof(struct nl_pktinfo))];
    } control;
    struct iovec vec = { data, 32 };
    struct mmsghdr msg = {
        .msg_hdr = { .msg_iov = &vec, .msg_iovlen = 1 },
    };
    bool ancillary = strstr(mode, "control-flags") != NULL;
    bool batch = !strncmp(mode, "mmsg-", 5);
    int got;

    if (ancillary) {
        int enabled = 1;
        check(!setsockopt(fd, SOL_NETLINK, NETLINK_PKTINFO,
                         &enabled, sizeof(enabled)), "enable packet info");
        vec.iov_len = sizeof(data);
        msg.msg_hdr.msg_control = &control;
        msg.msg_hdr.msg_controllen = CMSG_LEN(0);
        msg.msg_hdr.msg_flags = MSG_TRUNC;
    }
    request(fd);
    if (batch) {
        got = syscall(SYS_recvmmsg, fd, &msg, 1, MSG_DONTWAIT, NULL);
        check(got == 1, "flags first batch receive");
    } else {
        got = recvmsg(fd, &msg.msg_hdr, MSG_DONTWAIT);
        check(got > 0, "flags first receive");
    }
    if (ancillary) {
        check(!(msg.msg_hdr.msg_flags & MSG_TRUNC) &&
              (msg.msg_hdr.msg_flags & MSG_CTRUNC),
              "fresh ancillary truncation replaces stale payload flags");
        return;
    }
    check(msg.msg_hdr.msg_flags & MSG_TRUNC, "first receive truncated");
    vec.iov_len = sizeof(data);
    if (batch) {
        got = syscall(SYS_recvmmsg, fd, &msg, 1, MSG_DONTWAIT, NULL);
        check(got == 1 && msg.msg_len > 0, "flags second batch receive");
    } else {
        got = recvmsg(fd, &msg.msg_hdr, MSG_DONTWAIT);
        check(got > 0, "flags second receive");
    }
    check(((struct nlmsghdr *)data)->nlmsg_type == NLMSG_DONE,
          "flags second datagram complete");
    check(!msg.msg_hdr.msg_flags, "reused header has only current flags");
}

static void empty_vector_timeout(int fd)
{
    struct { long tv_sec, tv_nsec; } timeout[] = {
        { -1, 0 }, { 0, -1 }, { 0, 1000000000L },
    };
    struct mmsghdr msg = {0};
    int sockets[2] = { fd, socket(AF_INET, SOCK_DGRAM, 0) };

    check(sockets[1] >= 0, "empty vector UDP socket");
    for (unsigned int s = 0; s < 2; s++) {
        for (unsigned int i = 0; i < 3; i++) {
            check(syscall(SYS_recvmmsg, sockets[s], &msg, 0,
                          MSG_DONTWAIT, &timeout[i]) == -1 &&
                  errno == EINVAL, "empty vector invalid timeout value");
        }
        struct { long tv_sec, tv_nsec; } valid = { 1, 2 };
        check(!syscall(SYS_recvmmsg, sockets[s], NULL, 0,
                       MSG_DONTWAIT, &valid) &&
              valid.tv_sec == 1 && valid.tv_nsec == 2,
              "empty vector preserves valid timeout");
        check(syscall(SYS_recvmmsg, sockets[s], NULL, 0,
                      MSG_DONTWAIT, (void *)1) == -1 && errno == EFAULT,
              "empty vector invalid timeout pointer");
    }
    close(sockets[1]);
}

int main(int argc, char **argv)
{
    check(argc >= 2, "mode");
    if (!strcmp(argv[1], "after-exec")) {
        check(argc == 4, "exec arguments");
        check_remainder(atoi(argv[2]), (size_t)atol(argv[3]));
        puts("PASS after-exec");
        return 0;
    }
    int fd = create();
    char data[65536];
    if (!strcmp(argv[1], "vlen0-invalid-timeout")) {
        empty_vector_timeout(fd);
    } else if (strstr(argv[1], "flags-reuse") ||
               strstr(argv[1], "control-flags")) {
        receive_flags(fd, argv[1]);
    } else if (strstr(argv[1], "empty-iov") || strstr(argv[1], "zero-iov")) {
        empty_iovec(fd, argv[1]);
    } else if (!strcmp(argv[1], "recv-bad-iov") ||
        !strcmp(argv[1], "recv-bad-buffer") ||
        !strcmp(argv[1], "mmsg-bad-iov") ||
        !strcmp(argv[1], "mmsg-bad-buffer") ||
        !strcmp(argv[1], "mmsg-partial-bad-iov")) {
        bad_iovec(fd, argv[1]);
    } else if (!strncmp(argv[1], "vlen0", strlen("vlen0")) ||
        !strncmp(argv[1], "huge-", strlen("huge-"))) {
        large_timeout(fd, argv[1]);
    } else if (!strncmp(argv[1], "timeout-snapshot",
                        strlen("timeout-snapshot"))) {
        timeout_snapshot(fd, argv[1]);
    } else if (!strncmp(argv[1], "expired-timeout",
                        strlen("expired-timeout"))) {
        expired_timeout(fd, argv[1]);
    } else if (!strncmp(argv[1], "socket-timeout", strlen("socket-timeout"))) {
        socket_timeout(fd, argv[1]);
    } else if (!strcmp(argv[1], "directory-lock")) {
        char path[80];
        snprintf(path, sizeof(path), "/dev/shm/latx-netlink-%lu",
                 (unsigned long)geteuid());
        check(!mkdir(path, 0700) || errno == EEXIST, "cursor directory");
        int directory = open(path, O_RDONLY | O_DIRECTORY);
        check(directory >= 0 && !flock(directory, LOCK_EX), "directory lock");
        check(!fcntl(fd, F_SETFL, O_NONBLOCK), "nonblocking socket");
        check(recv(fd, data, 4096, 0) == -1 && errno == EAGAIN,
              "nonblocking receive ignores collector lock");
        request(fd);
        check(drain(fd, 1) > 0, "receive under collector lock");
        check(!flock(directory, LOCK_UN), "unlock directory");
        close(directory);
    } else if (!strcmp(argv[1], "rights") ||
               !strcmp(argv[1], "rights-process") ||
        !strcmp(argv[1], "exec")) {
        request(fd);
        ssize_t total = recv(fd, data, sizeof(data), MSG_PEEK);
        if (total <= 4096) {
            puts("SKIP: multipart dump did not exceed 4 KiB");
            return 77;
        }
        ssize_t first = recv(fd, data, 4096, 0);
        check(first > 0 && first < total, "first handoff slice");
        size_t expected = total - first;
        if (!strcmp(argv[1], "exec")) {
            const char *emulator = getenv("LATX_TEST_EMULATOR");
            char number[32], remaining[32];
            check(emulator != NULL, "emulator for exec");
            snprintf(number, sizeof(number), "%d", fd);
            snprintf(remaining, sizeof(remaining), "%zu", expected);
            execl(emulator, emulator, "-L",
                  getenv("LATX_TEST_SYSROOT") ? getenv("LATX_TEST_SYSROOT") :
                  "/usr/gnemul/latx-x86_64", argv[0], "after-exec",
                  number, remaining, NULL);
            check(0, "exec translator");
        }
        int channels[2];
        check(!socketpair(AF_UNIX, SOCK_DGRAM, 0, channels), "handoff pair");
        if (!strcmp(argv[1], "rights-process")) {
            pid_t child = fork();
            check(child >= 0, "handoff fork");
            if (!child) {
                close(fd);
                close(channels[0]);
                int received = pass_socket(channels[1], -1);
                check_remainder(received, expected);
                close(received);
                _exit(0);
            }
            close(channels[1]);
            pass_socket(channels[0], fd);
            close(fd);
            int status;
            check(waitpid(child, &status, 0) == child && WIFEXITED(status) &&
                  WEXITSTATUS(status) == 0, "handoff child result");
            close(channels[0]);
            puts("PASS rights-process");
            return 0;
        }
        pass_socket(channels[0], fd);
        int received = pass_socket(channels[1], -1);
        check_remainder(received, expected);
        check(recv(fd, data, sizeof(data), MSG_DONTWAIT) > 0,
              "original alias advances to next datagram");
        close(received);
        close(channels[0]);
        close(channels[1]);
    } else if (!strcmp(argv[1], "nonblock")) {
        pthread_t t;
        check(!pthread_create(&t, NULL, blocked, &fd), "thread");
        while (!atomic_load(&waiting)) {
            sched_yield();
        }
        usleep(100000);
        check(recv(fd, data, 4096, MSG_DONTWAIT) < 0 && errno == EAGAIN,
              "concurrent DONTWAIT");
        request(fd);
        check(!pthread_join(t, NULL), "thread join");
    } else if (!strcmp(argv[1], "timeout")) {
        struct timeval tv = {.tv_usec = 100000};
        check(!setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)),
              "set timeout");
        check(recv(fd, data, 4096, 0) < 0 && errno == EAGAIN,
              "receive timeout");
    } else if (!strcmp(argv[1], "fcntl-nonblock")) {
        check(!fcntl(fd, F_SETFL, O_NONBLOCK), "set nonblock");
        check(recv(fd, data, 4096, 0) < 0 && errno == EAGAIN,
              "O_NONBLOCK receive");
    } else if (!strcmp(argv[1], "peek-trunc")) {
        request(fd);
        require_large_dump(fd);
        ssize_t first = recv(fd, data, 4096, MSG_PEEK | MSG_TRUNC);
        check(first > 0 && first <= 4096, "peek complete slice");
        char again[4096];
        ssize_t second = recv(fd, again, sizeof(again), MSG_PEEK);
        check(first == second && !memcmp(data, again, first), "repeated peek");
        check(recv(fd, again, sizeof(again), 0) == first &&
                  !memcmp(data, again, first),
              "consume peek slice");
        check(drain(fd, 1) > 0, "peek remainder");
    } else if (!strcmp(argv[1], "switch")) {
        request(fd);
        require_large_dump(fd);
        ssize_t first = recv(fd, data, 4096, 0);
        check(first > 0, "first slice");
        ssize_t rest = receive(fd, data, sizeof(data), 1);
        int done = 0;
        check(rest > 0 && parse(data, rest, &done) > 0,
              "large recvmsg after small recvfrom");
    } else if (!strcmp(argv[1], "null")) {
        request(fd);
        check(recv(fd, NULL, 4096, MSG_DONTWAIT) < 0 && errno == EFAULT,
              "NULL receive");
        check(recv(fd, data, sizeof(data), MSG_PEEK) > 0,
              "NULL did not consume");
    } else if (!strcmp(argv[1], "many")) {
        int sockets[80];
        for (int i = 0; i < 80; i++) {
            sockets[i] = create();
            request(sockets[i]);
            check(recv(sockets[i], data, 4096, 0) > 0, "many first");
        }
        for (int i = 0; i < 80; i++) {
            check(drain(sockets[i], 1) >= 0, "many drain");
            close(sockets[i]);
        }
    } else if (!strcmp(argv[1], "reuse")) {
        for (int i = 0; i < 40; i++) {
            request(fd);
            check(recv(fd, data, 4096, 0) > 0, "reuse first");
            close(fd);
            fd = create();
            request(fd);
            check(drain(fd, 1) > 0, "reuse whole dump");
        }
    } else if (!strcmp(argv[1], "small")) {
        request(fd);
        struct iovec v = {data, 32};
        struct msghdr m = {.msg_iov = &v, .msg_iovlen = 1};
        ssize_t got = recvmsg(fd, &m, MSG_TRUNC);
        check(got > 32 && (m.msg_flags & MSG_TRUNC), "small truncation");
    } else if (!strcmp(argv[1], "bad-timeout")) {
        struct iovec v = {data, sizeof(data)};
        struct mmsghdr mm = { .msg_hdr = {.msg_iov = &v, .msg_iovlen = 1} };
        check(syscall(SYS_recvmmsg, fd, &mm, 1, MSG_DONTWAIT,
                      (void *)1) == -1 &&
              errno == EFAULT, "invalid timeout pointer");
    } else if (!strcmp(argv[1], "readonly-timeout") ||
               !strcmp(argv[1], "readonly-timeout-data") ||
               !strcmp(argv[1], "readonly-timeout-nonblock")) {
        size_t page = getauxval(AT_PAGESZ);
        struct { long tv_sec, tv_nsec; } *ts =
            mmap(NULL, page, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        struct iovec v = {data, sizeof(data)};
        struct mmsghdr mm = { .msg_hdr = {.msg_iov = &v, .msg_iovlen = 1} };
        check(ts != MAP_FAILED, "timeout mapping");
        ts->tv_sec = 0;
        ts->tv_nsec = 1000000;
        check(!mprotect(ts, page, PROT_READ), "readonly timeout");
        if (!strcmp(argv[1], "readonly-timeout-data")) {
            request(fd);
        }
        int nonblock = !strcmp(argv[1], "readonly-timeout-nonblock");
        check(syscall(SYS_recvmmsg, fd, &mm, 1,
                      nonblock ? MSG_DONTWAIT : 0, ts) == -1 &&
              errno == (nonblock ? EAGAIN : EFAULT),
              "readonly timeout writeback");
        munmap(ts, page);
    } else if (!strcmp(argv[1], "multi-timeout")) {
        struct mmsghdr mm = {0};
        struct iovec v = {data, sizeof(data)};
        mm.msg_hdr.msg_iov = &v;
        mm.msg_hdr.msg_iovlen = 1;
        struct timespec ts = {.tv_nsec = 100000000};
        check(recvmmsg(fd, &mm, 1, 0, &ts) == 0, "recvmmsg timeout");
    } else if (!strcmp(argv[1], "fork")) {
        request(fd);
        require_large_dump(fd);
        ssize_t got = recv(fd, data, 4096, 0);
        check(got > 0, "first");
        int done = 0;
        int count = parse(data, got, &done);
        struct pollfd p = {.fd = fd, .events = POLLIN};
        check(poll(&p, 1, 0) == 1, "remaining readable");
        pid_t child = fork();
        check(child >= 0, "fork");
        if (!child) {
            int n = drain(fd, 1);
            printf("child remaining links=%d\n", n);
            fflush(stdout);
            _exit(n > 0 ? 0 : 1);
        }
        int status;
        check(waitpid(child, &status, 0) == child && WIFEXITED(status) &&
                  WEXITSTATUS(status) == 0,
              "child drain");
        check(recv(fd, data, 4096, MSG_DONTWAIT) < 0 && errno == EAGAIN,
              "parent no duplicate");
        check(count > 0, "first count");
    } else if (!strcmp(argv[1], "dup")) {
        request(fd);
        require_large_dump(fd);
        ssize_t got = recv(fd, data, 4096, 0);
        check(got > 0, "first");
        int alias = dup(fd);
        check(alias >= 0, "dup");
        close(fd);
        fd = alias;
        check(drain(fd, 1) > 0, "alias remainder");
    } else {
        int method = atoi(argv[1]);
        request(fd);
        require_large_dump(fd);
        int count = drain(fd, method);
        check(count > 0, "link count");
        printf("links=%d ", count);
    }
    close(fd);
    printf("PASS %s\n", argv[1]);
    return 0;
}
