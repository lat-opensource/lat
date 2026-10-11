#ifndef LAT_TB_KEY_SET_H
#define LAT_TB_KEY_SET_H

#include <stddef.h>
#include <stdint.h>

#define LAT_TB_KEY_SET_MAGIC "LATTBKS"
#define LAT_TB_KEY_SET_VERSION 1u

typedef struct LatTbKeyFileHeader {
    uint8_t magic[8];
    uint32_t version;
    uint32_t header_size;
    uint8_t source_sha256[32];
    uint64_t record_count;
    uint64_t sequence;
} LatTbKeyFileHeader;

typedef struct LatTbKey {
    uint64_t guest_rva;
    uint32_t flags;
    uint32_t reserved;
} LatTbKey;

typedef struct LatTbKeySet {
    uint8_t source_sha256[32];
    uint64_t sequence;
    size_t count;
    LatTbKey *keys;
} LatTbKeySet;

int lat_tb_key_set_read_fd(int fd, const uint8_t expected_source[32],
                           LatTbKeySet *set, char *error, size_t error_size);
int lat_tb_key_set_read_file(const char *path,
                             const uint8_t expected_source[32],
                             LatTbKeySet *set,
                             char *error, size_t error_size);
int lat_tb_key_set_write_fd(int fd, const LatTbKeySet *set,
                            char *error, size_t error_size);
int lat_tb_key_set_sort_unique(LatTbKeySet *set,
                               char *error, size_t error_size);
int lat_tb_key_set_union(const LatTbKeySet *left, const LatTbKeySet *right,
                         LatTbKeySet *result,
                         char *error, size_t error_size);
int lat_tb_key_set_difference(const LatTbKeySet *known,
                              const LatTbKeySet *published,
                              LatTbKeySet *result,
                              char *error, size_t error_size);
int lat_tb_key_set_contains(const LatTbKeySet *set,
                            uint64_t guest_rva, uint32_t flags);
void lat_tb_key_set_destroy(LatTbKeySet *set);

_Static_assert(sizeof(LatTbKeyFileHeader) == 64,
               "TB key file header size changed");
_Static_assert(sizeof(LatTbKey) == 16, "TB key record size changed");

#endif
