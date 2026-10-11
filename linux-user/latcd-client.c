#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "latcd-client.h"
#include "latcd-protocol.h"

#include <errno.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
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

static int exchange(const char *socket_path, int source_fd, int keys_fd,
                    uint32_t operation, uint32_t priority,
                    uint64_t request_id, uint64_t sequence,
                    const char *build_id, const uint8_t cache_identity[32],
                    char *error, size_t error_size)
{
    if (!socket_path || !*socket_path) {
        errno = EINVAL;
        return fail(error, error_size, "invalid latcd client arguments");
    }
    int socket_fd = socket(AF_UNIX,
                           SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (socket_fd < 0) {
        return fail(error, error_size, "cannot create latcd socket: %s",
                    strerror(errno));
    }
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    int directory_fd;
    if (latcd_socket_address(socket_path, &address, &directory_fd, error, error_size)) {
        close(socket_fd);
        return -1;
    }
    int connected = connect(socket_fd, (const void *)&address, sizeof(address));
    int connect_error = errno;
    if (directory_fd >= 0) close(directory_fd);
    if (connected) {
        int saved = connect_error;
        close(socket_fd);
        errno = saved;
        return fail(error, error_size, "cannot connect to latcd: %s",
                    strerror(saved));
    }
    struct ucred credentials;
    socklen_t credentials_size = sizeof(credentials);
    if (getsockopt(socket_fd, SOL_SOCKET, SO_PEERCRED, &credentials,
                   &credentials_size) ||
        credentials_size != sizeof(credentials) ||
        credentials.uid != geteuid()) {
        close(socket_fd);
        errno = EPERM;
        return fail(error, error_size, "latcd peer is not the current user");
    }
    LatcdRequestV2 request = {
        .magic = LATCD_REQUEST_MAGIC,
        .version = LATCD_PROTOCOL_VERSION,
        .size = sizeof(request),
        .operation = operation,
        .priority = priority,
        .request_id = request_id,
        .sequence = sequence,
    };
    if (latcd_send_request(socket_fd, source_fd, keys_fd, &request,
                           error, error_size)) {
        close(socket_fd);
        return -1;
    }
    struct pollfd event = { .fd = socket_fd, .events = POLLIN };
    int timeout = operation == LATCD_OP_SUBMIT_KEYS ||
                  operation == LATCD_OP_HELLO ? 1000 : 120000;
    int ready;
    do {
        ready = poll(&event, 1, timeout);
    } while (ready < 0 && errno == EINTR);
    if (ready <= 0 || !(event.revents & POLLIN)) {
        int saved = ready == 0 ? ETIMEDOUT :
                    (ready < 0 ? errno : ECONNRESET);
        close(socket_fd);
        errno = saved;
        return fail(error, error_size, "latcd did not finish request: %s",
                    strerror(saved));
    }
    LatcdResponseV2 response;
    int result = latcd_receive_response(socket_fd, &response,
                                        error, error_size);
    close(socket_fd);
    if (result) return -1;
    if (response.request_id != request_id) {
        errno = EPROTO;
        return fail(error, error_size, "latcd response id does not match");
    }
    if (response.status != LATCD_STATUS_OK) {
        errno = EIO;
        return fail(error, error_size, "latcd rejected request: %s",
                    response.message[0] ? response.message : "unknown error");
    }
    if (build_id && (strcmp(response.message, build_id) ||
        (cache_identity && memcmp(response.source_sha256, cache_identity, 32)))) {
        errno = EPROTO;
        return fail(error, error_size, "latcd build or cache identity mismatch");
    }
    return 0;
}

int latcd_client_hello(const char *socket_path, const char *build_id,
                        const uint8_t cache_identity[32],
                        char *error, size_t error_size)
{
    return exchange(socket_path, -1, -1, LATCD_OP_HELLO,
                    0, 1, 0, build_id, cache_identity, error, error_size);
}

int latcd_client_submit_keys_fd(const char *socket_path, int source_fd,
                                int keys_fd, uint32_t priority,
                                uint64_t request_id, uint64_t sequence,
                                char *error, size_t error_size)
{
    return exchange(socket_path, source_fd, keys_fd, LATCD_OP_SUBMIT_KEYS,
                    priority, request_id, sequence, NULL, NULL, error, error_size);
}

int latcd_client_flush_source(const char *socket_path, int source_fd,
                              uint64_t request_id,
                              char *error, size_t error_size)
{
    return exchange(socket_path, source_fd, -1, LATCD_OP_FLUSH_SOURCE,
                    LATCD_PRIORITY_STARTUP, request_id, 0, NULL, NULL,
                    error, error_size);
}

int latcd_client_flush_all(const char *socket_path, uint64_t request_id,
                           char *error, size_t error_size)
{
    return exchange(socket_path, -1, -1, LATCD_OP_FLUSH_ALL,
                    LATCD_PRIORITY_STARTUP, request_id, 0, NULL, NULL,
                    error, error_size);
}

int latcd_client_precompile_source(const char *socket_path, int source_fd,
                                   uint64_t request_id,
                                   char *error, size_t error_size)
{
    return exchange(socket_path, source_fd, -1,
                    LATCD_OP_PRECOMPILE_SOURCE, LATCD_PRIORITY_STARTUP,
                    request_id, 0, NULL, NULL, error, error_size);
}
