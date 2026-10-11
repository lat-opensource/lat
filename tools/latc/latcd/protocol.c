#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "latcd-protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int fail(char *error, size_t error_size, const char *format, ...)
{
    if (error && error_size) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(error, error_size, format, arguments);
        va_end(arguments);
    }
    return -1;
}

int latcd_socket_address(const char *path, struct sockaddr_un *address,
                         int *directory_fd, char *error, size_t error_size)
{
    *directory_fd = -1;
    memset(address, 0, sizeof(*address));
    address->sun_family = AF_UNIX;
    if (!path || !*path) {
        errno = EINVAL;
        return fail(error, error_size, "empty socket path");
    }
    if (strlen(path) < sizeof(address->sun_path)) {
        strcpy(address->sun_path, path);
        return 0;
    }
    const char *slash = strrchr(path, '/');
    if (!slash || !slash[1]) {
        errno = ENAMETOOLONG;
        return fail(error, error_size, "invalid long socket path");
    }
    char *parent = strndup(path, slash - path);
    if (!parent) return fail(error, error_size, "cannot allocate socket parent");
    int fd = open(parent, O_PATH | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    free(parent);
    if (fd < 0) return fail(error, error_size, "cannot open socket parent: %s", strerror(errno));
    int length = snprintf(address->sun_path, sizeof(address->sun_path),
                          "/proc/self/fd/%d/%s", fd, slash + 1);
    if (length < 0 || (size_t)length >= sizeof(address->sun_path)) {
        close(fd);
        errno = ENAMETOOLONG;
        return fail(error, error_size, "socket basename is too long");
    }
    *directory_fd = fd;
    return 0;
}

static int expected_descriptors(const LatcdRequestV2 *request)
{
    if (request->operation == LATCD_OP_SUBMIT_KEYS) return 2;
    if (request->operation == LATCD_OP_SUBMIT_STATIC_KEYS) return 2;
    if (request->operation == LATCD_OP_FLUSH_SOURCE) return 1;
    if (request->operation == LATCD_OP_FLUSH_ALL) return 0;
    if (request->operation == LATCD_OP_PRECOMPILE_SOURCE) return 1;
    if (request->operation == LATCD_OP_HELLO) return 0;
    return -1;
}

static int request_header_valid(const LatcdRequestV2 *request)
{
    return request->magic == LATCD_REQUEST_MAGIC &&
           request->version == LATCD_PROTOCOL_VERSION &&
           request->size == sizeof(*request) &&
           expected_descriptors(request) >= 0;
}

int latcd_send_request(int socket_fd, int source_fd, int keys_fd,
                       const LatcdRequestV2 *request,
                       char *error, size_t error_size)
{
    int count = request ? expected_descriptors(request) : -1;
    if (socket_fd < 0 || !request || !request_header_valid(request) ||
        (count >= 1 && source_fd < 0) || (count == 2 && keys_fd < 0)) {
        errno = EINVAL;
        return fail(error, error_size, "invalid latcd request arguments");
    }
    struct iovec iov = { .iov_base = (void *)request,
                         .iov_len = sizeof(*request) };
    union {
        struct cmsghdr align;
        unsigned char bytes[CMSG_SPACE(sizeof(int) * 2)];
    } control = {0};
    struct msghdr message = { .msg_iov = &iov, .msg_iovlen = 1 };
    int descriptors[2] = { source_fd, keys_fd };
    if (count) {
        message.msg_control = control.bytes;
        message.msg_controllen = CMSG_SPACE(sizeof(int) * count);
        struct cmsghdr *header = CMSG_FIRSTHDR(&message);
        header->cmsg_level = SOL_SOCKET;
        header->cmsg_type = SCM_RIGHTS;
        header->cmsg_len = CMSG_LEN(sizeof(int) * count);
        memcpy(CMSG_DATA(header), descriptors, sizeof(int) * count);
    }
    ssize_t sent;
    do {
        sent = sendmsg(socket_fd, &message, MSG_NOSIGNAL);
    } while (sent < 0 && errno == EINTR);
    if (sent != sizeof(*request)) {
        return fail(error, error_size, "cannot send latcd request: %s",
                    sent < 0 ? strerror(errno) : "short packet");
    }
    return 0;
}

int latcd_receive_request(int socket_fd, LatcdRequestV2 *request,
                          int *source_fd, int *keys_fd,
                          char *error, size_t error_size)
{
    if (socket_fd < 0 || !request || !source_fd || !keys_fd) {
        errno = EINVAL;
        return fail(error, error_size, "invalid latcd receive arguments");
    }
    *source_fd = -1;
    *keys_fd = -1;
    struct iovec iov = { .iov_base = request, .iov_len = sizeof(*request) };
    union {
        struct cmsghdr align;
        unsigned char bytes[CMSG_SPACE(sizeof(int) * 4)];
    } control = {0};
    struct msghdr message = {
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = control.bytes,
        .msg_controllen = sizeof(control.bytes),
    };
    ssize_t received;
    do {
        received = recvmsg(socket_fd, &message, MSG_CMSG_CLOEXEC);
    } while (received < 0 && errno == EINTR);
    int descriptors[4];
    size_t descriptor_count = 0;
    for (struct cmsghdr *header = CMSG_FIRSTHDR(&message); header;
         header = CMSG_NXTHDR(&message, header)) {
        if (header->cmsg_level != SOL_SOCKET ||
            header->cmsg_type != SCM_RIGHTS ||
            header->cmsg_len < CMSG_LEN(sizeof(int))) continue;
        size_t count = (header->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        const int *fds = (const void *)CMSG_DATA(header);
        for (size_t i = 0; i < count && descriptor_count < 4; i++) {
            descriptors[descriptor_count++] = fds[i];
        }
    }
    int expected = request_header_valid(request) ?
                   expected_descriptors(request) : -1;
    if (received != sizeof(*request) ||
        (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) ||
        expected < 0 || descriptor_count != (size_t)expected) {
        for (size_t i = 0; i < descriptor_count; i++) close(descriptors[i]);
        errno = EPROTO;
        return fail(error, error_size,
                    "latcd request header or descriptor count is invalid");
    }
    if (expected >= 1) *source_fd = descriptors[0];
    if (expected == 2) *keys_fd = descriptors[1];
    return 0;
}

int latcd_send_response(int socket_fd, const LatcdResponseV2 *response,
                        char *error, size_t error_size)
{
    ssize_t sent;
    do {
        sent = send(socket_fd, response, sizeof(*response), MSG_NOSIGNAL);
    } while (sent < 0 && errno == EINTR);
    if (sent != sizeof(*response)) {
        return fail(error, error_size, "cannot send latcd response: %s",
                    sent < 0 ? strerror(errno) : "short packet");
    }
    return 0;
}

int latcd_receive_response(int socket_fd, LatcdResponseV2 *response,
                           char *error, size_t error_size)
{
    ssize_t received;
    do {
        received = recv(socket_fd, response, sizeof(*response), 0);
    } while (received < 0 && errno == EINTR);
    if (received != sizeof(*response) ||
        response->magic != LATCD_RESPONSE_MAGIC ||
        response->version != LATCD_PROTOCOL_VERSION ||
        response->size != sizeof(*response)) {
        errno = EPROTO;
        return fail(error, error_size, "invalid latcd response packet");
    }
    response->message[sizeof(response->message) - 1] = '\0';
    return 0;
}
