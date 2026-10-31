/* SPDX-License-Identifier: GPL-2.0-only */
#define _GNU_SOURCE
#include <glib.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <linux/netlink.h>
#include <pthread.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "line %d: %s errno=%d\n", __LINE__, #x, errno); exit(1); \
} } while (0)
static int recvmsg_calls;
static ssize_t counted_recvmsg(int fd, struct msghdr *msg, int flags)
{
    recvmsg_calls++;
    return recvmsg(fd, msg, flags);
}
#define safe_recvmsg counted_recvmsg
#define safe_ppoll(fds, n, ts, mask, size) ppoll(fds, n, ts, mask)
static bool smul64_overflow(int64_t x, int64_t y, int64_t *ret)
{
    return __builtin_mul_overflow(x, y, ret);
}
static bool sadd64_overflow(int64_t x, int64_t y, int64_t *ret)
{
    return __builtin_add_overflow(x, y, ret);
}
typedef struct {
    struct NetlinkRecvShared {
        pthread_mutex_t mutex;
        size_t offset;
    } *shared;
    int error;
} NetlinkRecvState;
static int fd_trans_netlink_attach(int fd, NetlinkRecvState *state)
{
    CHECK(0); /* Fixture always provides an initialized shared mapping. */
    return -1;
}
/* The production receive function is inserted at this marker. */
/* INSERT_RECEIVER */

static int64_t milliseconds(void)
{
    struct timespec now;
    CHECK(!clock_gettime(CLOCK_MONOTONIC, &now));
    return now.tv_sec * 1000LL + now.tv_nsec / 1000000;
}

static void check_cursor_contention(int fd, NetlinkRecvState *state,
                                    struct msghdr *msg)
{
    int ready[2], status;
    char byte;
    int64_t start;
    struct timeval timeout = { .tv_usec = 100000 };

    CHECK(!pipe(ready));
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        close(ready[0]);
        CHECK(!pthread_mutex_lock(&state->shared->mutex));
        CHECK(write(ready[1], "x", 1) == 1);
        usleep(600000);
        CHECK(!pthread_mutex_unlock(&state->shared->mutex));
        _exit(0);
    }
    close(ready[1]);
    CHECK(read(ready[0], &byte, 1) == 1);
    close(ready[0]);
    start = milliseconds();
    CHECK(netlink_recvmsg(fd, msg, MSG_DONTWAIT, state) == -1);
    CHECK(errno == EAGAIN && milliseconds() - start < 200);
    CHECK(!fcntl(fd, F_SETFL, O_NONBLOCK));
    start = milliseconds();
    CHECK(netlink_recvmsg(fd, msg, 0, state) == -1);
    CHECK(errno == EAGAIN && milliseconds() - start < 200);
    CHECK(!fcntl(fd, F_SETFL, 0));
    CHECK(!setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
                      &timeout, sizeof(timeout)));
    start = milliseconds();
    CHECK(netlink_recvmsg(fd, msg, 0, state) == -1);
    CHECK(errno == EAGAIN && milliseconds() - start >= 50 &&
          milliseconds() - start < 400);
    timeout.tv_usec = 0;
    CHECK(!setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
                      &timeout, sizeof(timeout)));
    /* A blocking receiver still resumes and consumes exactly one slice. */
    CHECK(netlink_recvmsg(fd, msg, 0, state) == 3000);
    CHECK(state->shared->offset == 3000);
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(netlink_recvmsg(fd, msg, MSG_DONTWAIT, state) == 3000);
    CHECK(state->shared->offset == 0);
}

int main(void)
{
    int pair[2];
    char packet[6000] = {0}, data[8192], peek[8192];
    struct nlmsghdr *first = (void *)packet;
    struct nlmsghdr *second = (void *)(packet + 3000);
    NetlinkRecvState state = {0};
    pthread_mutexattr_t attr;
    struct iovec vec = {data, 4096};
    struct msghdr msg = {.msg_iov = &vec, .msg_iovlen = 1};
    struct pollfd event;
    char small[128] = {0};
    int status;

    CHECK(!socketpair(AF_UNIX, SOCK_DGRAM, 0, pair));
    state.shared = mmap(NULL, sizeof(*state.shared), PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    CHECK(state.shared != MAP_FAILED);
    CHECK(!pthread_mutexattr_init(&attr));
    CHECK(!pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED));
    CHECK(!pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST));
    CHECK(!pthread_mutex_init(&state.shared->mutex, &attr));
    pthread_mutexattr_destroy(&attr);
    first->nlmsg_len = second->nlmsg_len = 3000;
    first->nlmsg_flags = second->nlmsg_flags = NLM_F_MULTI;
    first->nlmsg_seq = 1;
    second->nlmsg_seq = 2;
    CHECK(send(pair[0], packet, sizeof(packet), 0) == sizeof(packet));
    check_cursor_contention(pair[1], &state, &msg);
    CHECK(send(pair[0], small, sizeof(small), 0) == sizeof(small));
    recvmsg_calls = 0;
    CHECK(netlink_recvmsg(pair[1], &msg, MSG_PEEK | MSG_DONTWAIT, &state) ==
          sizeof(small));
    CHECK(recvmsg_calls == 2); /* length probe plus direct peek */
    recvmsg_calls = 0;
    CHECK(netlink_recvmsg(pair[1], &msg, MSG_DONTWAIT, &state) ==
          sizeof(small));
    CHECK(recvmsg_calls == 2); /* length probe plus direct receive */
    CHECK(send(pair[0], packet, sizeof(packet), 0) == sizeof(packet));
    CHECK(netlink_recvmsg(pair[1], &msg, MSG_PEEK, &state) == 3000);
    memcpy(peek, data, 3000);
    CHECK(state.shared->offset == 0);
    CHECK(netlink_recvmsg(pair[1], &msg, 0, &state) == 3000);
    CHECK(!memcmp(data, peek, 3000) && state.shared->offset == 3000);
    event = (struct pollfd){.fd = pair[1], .events = POLLIN};
    CHECK(poll(&event, 1, 0) == 1);
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        vec.iov_len = sizeof(data);
        CHECK(netlink_recvmsg(pair[1], &msg, MSG_DONTWAIT, &state) == 3000);
        CHECK(((struct nlmsghdr *)data)->nlmsg_seq == 2);
        _exit(0);
    }
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(state.shared->offset == 0);
    CHECK(netlink_recvmsg(pair[1], &msg, MSG_DONTWAIT, &state) == -1);
    CHECK(errno == EAGAIN && !poll(&event, 1, 0));
    CHECK(send(pair[0], packet, sizeof(packet), 0) == sizeof(packet));
    vec.iov_len = 32;
    CHECK(netlink_recvmsg(pair[1], &msg, MSG_TRUNC, &state) == 6000);
    CHECK(msg.msg_flags & MSG_TRUNC);
    CHECK(!poll(&event, 1, 0));
    close(pair[0]);
    close(pair[1]);
    munmap(state.shared, sizeof(*state.shared));
    puts("kernel queue cursor: complete slices, peek, poll, fork, "
         "truncation passed");
    return 0;
}
