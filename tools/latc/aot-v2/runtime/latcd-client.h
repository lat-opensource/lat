#ifndef LATCD_CLIENT_H
#define LATCD_CLIENT_H

#include <stddef.h>
#include <stdint.h>

int latcd_client_hello(const char *socket_path, const char *build_id,
                        const uint8_t cache_identity[32],
                        char *error, size_t error_size);

int latcd_client_submit_keys_fd(const char *socket_path, int source_fd,
                                int keys_fd, uint32_t priority,
                                uint64_t request_id, uint64_t sequence,
                                char *error, size_t error_size);
int latcd_client_flush_source(const char *socket_path, int source_fd,
                              uint64_t request_id,
                              char *error, size_t error_size);
int latcd_client_flush_all(const char *socket_path, uint64_t request_id,
                           char *error, size_t error_size);
int latcd_client_precompile_source(const char *socket_path, int source_fd,
                                   uint64_t request_id,
                                   char *error, size_t error_size);

#endif
