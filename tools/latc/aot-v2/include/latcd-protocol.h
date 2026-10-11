#ifndef LATCD_PROTOCOL_H
#define LATCD_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/un.h>

#define LATCD_REQUEST_MAGIC UINT32_C(0x4c415444)
#define LATCD_RESPONSE_MAGIC UINT32_C(0x4c415452)
#define LATCD_PROTOCOL_VERSION 2u
#define LATCD_RESPONSE_MESSAGE_SIZE 192u

enum LatcdPriority {
    LATCD_PRIORITY_BACKGROUND = 0,
    LATCD_PRIORITY_LIBRARY = 100,
    LATCD_PRIORITY_STARTUP = 200,
};

enum LatcdOperation {
    LATCD_OP_SUBMIT_KEYS = 1,
    LATCD_OP_FLUSH_SOURCE = 2,
    LATCD_OP_FLUSH_ALL = 3,
    LATCD_OP_PRECOMPILE_SOURCE = 4,
    LATCD_OP_HELLO = 5,
    LATCD_OP_SUBMIT_STATIC_KEYS = 6,
};

enum LatcdStatus {
    LATCD_STATUS_OK = 0,
    LATCD_STATUS_BAD_REQUEST = 1,
    LATCD_STATUS_BAD_SOURCE = 2,
    LATCD_STATUS_COMPILE_FAILED = 3,
    LATCD_STATUS_INVALID_MODULE = 4,
    LATCD_STATUS_IO_ERROR = 5,
    LATCD_STATUS_QUEUE_FULL = 6,
    LATCD_STATUS_NEGATIVE_CACHE = 7,
};

typedef struct LatcdRequestV2 {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t operation;
    uint32_t priority;
    uint64_t request_id;
    uint64_t sequence;
} LatcdRequestV2;

typedef struct LatcdResponseV2 {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    int32_t status;
    uint32_t reserved;
    uint64_t request_id;
    uint64_t accepted_sequence;
    uint64_t published_sequence;
    uint8_t source_sha256[32];
    char message[LATCD_RESPONSE_MESSAGE_SIZE];
} LatcdResponseV2;

int latcd_send_request(int socket_fd, int source_fd, int keys_fd,
                       const LatcdRequestV2 *request,
                       char *error, size_t error_size);
int latcd_receive_request(int socket_fd, LatcdRequestV2 *request,
                          int *source_fd, int *keys_fd,
                          char *error, size_t error_size);
int latcd_send_response(int socket_fd, const LatcdResponseV2 *response,
                        char *error, size_t error_size);
int latcd_receive_response(int socket_fd, LatcdResponseV2 *response,
                           char *error, size_t error_size);

/* Keep directory_fd open through bind/connect when a pathname exceeds sun_path. */
int latcd_socket_address(const char *path, struct sockaddr_un *address,
                         int *directory_fd, char *error, size_t error_size);

_Static_assert(sizeof(LatcdRequestV2) == 32,
               "latcd request ABI size changed");
_Static_assert(sizeof(LatcdResponseV2) == 264,
               "latcd response ABI size changed");

#endif
