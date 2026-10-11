#define _GNU_SOURCE

#include "latcd-client.h"
#include "latcd-protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

static int fail(const char *message)
{
    fprintf(stderr, "test-latcd-client: %s\n", message);
    return 1;
}

int main(void)
{
    char directory[] = "/tmp/test-latcd-client.XXXXXX";
    if (!mkdtemp(directory) || chmod(directory, 0700)) {
        return fail("cannot create socket directory");
    }
    char path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    snprintf(path, sizeof(path), "%s/socket", directory);
    int server = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    snprintf(address.sun_path, sizeof(address.sun_path), "%s", path);
    if (server < 0 || bind(server, (const void *)&address, sizeof(address)) ||
        listen(server, 1)) {
        perror("test-latcd-client: cannot create test listener");
        return 1;
    }
    int source = open("/dev/null", O_RDONLY | O_CLOEXEC);
    if (source < 0) {
        return fail("cannot open test source");
    }
    pid_t submitter = fork();
    if (submitter < 0) {
        return fail("cannot fork submitter");
    }
    if (!submitter) {
        char error[128] = {0};
        int result = latcd_client_submit_keys_fd(path, source, source,
            LATCD_PRIORITY_STARTUP, 42, 7, error, sizeof(error));
        if (result) fprintf(stderr, "test-latcd-client: %s\n", error);
        _exit(result ? 1 : 0);
    }
    int client = accept4(server, NULL, NULL, SOCK_CLOEXEC);
    LatcdRequestV2 request;
    char control[CMSG_SPACE(sizeof(int) * 2)] = {0};
    struct iovec iov = { .iov_base = &request, .iov_len = sizeof(request) };
    struct msghdr message = {
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = control,
        .msg_controllen = sizeof(control),
    };
    if (client < 0 || recvmsg(client, &message, MSG_CMSG_CLOEXEC) !=
                      sizeof(request) ||
        request.magic != LATCD_REQUEST_MAGIC || request.request_id != 42 ||
        request.priority != LATCD_PRIORITY_STARTUP ||
        request.operation != LATCD_OP_SUBMIT_KEYS || request.sequence != 7) {
        return fail("received request does not match submission");
    }
    struct cmsghdr *header = CMSG_FIRSTHDR(&message);
    int received_fds[2] = {-1, -1};
    if (!header || header->cmsg_level != SOL_SOCKET ||
        header->cmsg_type != SCM_RIGHTS) {
        return fail("submission did not carry an FD");
    }
    memcpy(received_fds, CMSG_DATA(header), sizeof(received_fds));
    if (fcntl(received_fds[0], F_GETFL) < 0 ||
        fcntl(received_fds[1], F_GETFL) < 0) {
        return fail("received FDs are invalid");
    }
    LatcdResponseV2 response = {
        .magic = LATCD_RESPONSE_MAGIC,
        .version = LATCD_PROTOCOL_VERSION,
        .size = sizeof(response),
        .status = LATCD_STATUS_OK,
        .request_id = request.request_id,
    };
    if (send(client, &response, sizeof(response), MSG_NOSIGNAL) !=
        sizeof(response)) {
        return fail("cannot acknowledge submission");
    }
    close(received_fds[0]);
    close(received_fds[1]);
    close(client);
    int submit_status = 0;
    if (waitpid(submitter, &submit_status, 0) != submitter ||
        !WIFEXITED(submit_status) || WEXITSTATUS(submit_status)) {
        return fail("submission was not acknowledged");
    }
    pid_t precompiler = fork();
    if (precompiler < 0) {
        return fail("cannot fork precompiler");
    }
    if (!precompiler) {
        char error[128] = {0};
        int result = latcd_client_precompile_source(path, source, 44,
                                                     error, sizeof(error));
        if (result) fprintf(stderr, "test-latcd-client: %s\n", error);
        _exit(result ? 1 : 0);
    }
    client = accept4(server, NULL, NULL, SOCK_CLOEXEC);
    int precompile_source = -1, no_keys = -1;
    char receive_error[128] = {0};
    if (client < 0 || latcd_receive_request(client, &request,
                                             &precompile_source, &no_keys,
                                             receive_error,
                                             sizeof(receive_error)) ||
        request.operation != LATCD_OP_PRECOMPILE_SOURCE ||
        request.request_id != 44 || precompile_source < 0 || no_keys >= 0) {
        return fail("received request does not match precompile");
    }
    response.request_id = request.request_id;
    if (send(client, &response, sizeof(response), MSG_NOSIGNAL) !=
        sizeof(response)) {
        return fail("cannot acknowledge precompile");
    }
    close(precompile_source);
    close(client);
    if (waitpid(precompiler, &submit_status, 0) != precompiler ||
        !WIFEXITED(submit_status) || WEXITSTATUS(submit_status)) {
        return fail("precompile was not acknowledged");
    }
    for (int variant = 0; variant < 3; variant++) {
        pid_t greeter = fork();
        if (greeter < 0) return fail("cannot fork handshake client");
        if (!greeter) {
            char error[128] = {0};
            uint8_t digest[32] = { 7 };
            int result = latcd_client_hello(path, "test-build", digest,
                                            error, sizeof(error));
            _exit(variant ? !(result && errno == EPROTO) : !!result);
        }
        client = accept4(server, NULL, NULL, SOCK_CLOEXEC);
        int no_source = -1, no_keys = -1;
        char error[128];
        if (client < 0 || latcd_receive_request(client, &request,
                &no_source, &no_keys, error, sizeof(error)) ||
            request.operation != LATCD_OP_HELLO || no_source >= 0 || no_keys >= 0)
            return fail("invalid handshake request");
        response.request_id = request.request_id;
        snprintf(response.message, sizeof(response.message), "%s",
                 variant == 1 ? "wrong-build" : "test-build");
        memset(response.source_sha256, 0, sizeof(response.source_sha256));
        response.source_sha256[0] = variant == 2 ? 8 : 7;
        if (send(client, &response, sizeof(response), MSG_NOSIGNAL) != sizeof(response))
            return fail("cannot acknowledge handshake");
        close(client);
        if (waitpid(greeter, &submit_status, 0) != greeter ||
            !WIFEXITED(submit_status) || WEXITSTATUS(submit_status))
            return fail("handshake identity validation failed");
    }
    close(source);
    close(server);
    unlink(path);
    rmdir(directory);

    char error[128] = {0};
    errno = 0;
    if (!latcd_client_submit_keys_fd(path, STDIN_FILENO, STDIN_FILENO,
                                     LATCD_PRIORITY_LIBRARY,
                                     43, 8, error, sizeof(error)) ||
        (errno != ENOENT && errno != ECONNREFUSED)) {
        return fail("missing daemon was not reported immediately");
    }
    puts("latcd acknowledged client tests: PASS");
    return 0;
}
