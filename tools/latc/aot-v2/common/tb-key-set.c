#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "lat-tb-key-set.h"
#include "lat-aot-v2.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static int key_compare(const void *left, const void *right)
{
    const LatTbKey *a = left;
    const LatTbKey *b = right;
    if (a->guest_rva != b->guest_rva) {
        return a->guest_rva < b->guest_rva ? -1 : 1;
    }
    return a->flags < b->flags ? -1 : a->flags > b->flags;
}

static int key_valid(const LatTbKey *key)
{
    return !key->reserved &&
        key->flags == (LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL);
}

void lat_tb_key_set_destroy(LatTbKeySet *set)
{
    if (!set) {
        return;
    }
    free(set->keys);
    memset(set, 0, sizeof(*set));
}

int lat_tb_key_set_sort_unique(LatTbKeySet *set,
                               char *error, size_t error_size)
{
    if (!set || (set->count && !set->keys) ||
        set->count > LAT_AOT_V2_TBSET_RECORD_LIMIT) {
        errno = EINVAL;
        return fail(error, error_size, "invalid TB key set");
    }
    for (size_t i = 0; i < set->count; i++) {
        if (!key_valid(&set->keys[i])) {
            errno = EINVAL;
            return fail(error, error_size, "TB key %zu is invalid", i);
        }
    }
    qsort(set->keys, set->count, sizeof(*set->keys), key_compare);
    size_t output = 0;
    for (size_t i = 0; i < set->count; i++) {
        if (!output || key_compare(&set->keys[output - 1],
                                   &set->keys[i])) {
            set->keys[output++] = set->keys[i];
        }
    }
    set->count = output;
    return 0;
}

int lat_tb_key_set_read_fd(int fd, const uint8_t expected_source[32],
                           LatTbKeySet *set, char *error, size_t error_size)
{
    if (fd < 0 || !set) {
        errno = EINVAL;
        return fail(error, error_size, "invalid TB key set arguments");
    }
    memset(set, 0, sizeof(*set));
    struct stat status;
    LatTbKeyFileHeader header;
    if (fstat(fd, &status) || !S_ISREG(status.st_mode) ||
        status.st_size < (off_t)sizeof(header) ||
        pread(fd, &header, sizeof(header), 0) != (ssize_t)sizeof(header)) {
        return fail(error, error_size, "cannot read TB key set header");
    }
    static const uint8_t magic[8] = LAT_TB_KEY_SET_MAGIC;
    if (memcmp(header.magic, magic, sizeof(magic)) ||
        header.version != LAT_TB_KEY_SET_VERSION ||
        header.header_size != sizeof(header) ||
        header.record_count > LAT_AOT_V2_TBSET_RECORD_LIMIT ||
        header.record_count > (SIZE_MAX / sizeof(LatTbKey)) ||
        (uint64_t)status.st_size != sizeof(header) +
            header.record_count * sizeof(LatTbKey) ||
        (expected_source && memcmp(header.source_sha256, expected_source, 32))) {
        errno = ENOEXEC;
        return fail(error, error_size, "TB key set header is invalid");
    }
    size_t bytes = (size_t)header.record_count * sizeof(LatTbKey);
    LatTbKey *keys = bytes ? malloc(bytes) : NULL;
    if (bytes && !keys) {
        return fail(error, error_size, "out of memory reading TB key set");
    }
    if (bytes && pread(fd, keys, bytes, sizeof(header)) != (ssize_t)bytes) {
        free(keys);
        return fail(error, error_size, "cannot read TB key records");
    }
    memcpy(set->source_sha256, header.source_sha256, 32);
    set->sequence = header.sequence;
    set->count = (size_t)header.record_count;
    set->keys = keys;
    if (lat_tb_key_set_sort_unique(set, error, error_size)) {
        lat_tb_key_set_destroy(set);
        return -1;
    }
    return 0;
}

int lat_tb_key_set_read_file(const char *path,
                             const uint8_t expected_source[32],
                             LatTbKeySet *set,
                             char *error, size_t error_size)
{
    int fd = path ? open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW) : -1;
    if (fd < 0) {
        return fail(error, error_size, "cannot open TB key set: %s",
                    strerror(errno));
    }
    int result = lat_tb_key_set_read_fd(fd, expected_source, set,
                                        error, error_size);
    close(fd);
    return result;
}

static int write_all_at(int fd, const void *data, size_t size, off_t offset)
{
    const unsigned char *cursor = data;
    while (size) {
        ssize_t written = pwrite(fd, cursor, size, offset);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return -1;
        }
        cursor += written;
        size -= (size_t)written;
        offset += written;
    }
    return 0;
}

int lat_tb_key_set_write_fd(int fd, const LatTbKeySet *set,
                            char *error, size_t error_size)
{
    if (fd < 0 || !set || (set->count && !set->keys) ||
        set->count > LAT_AOT_V2_TBSET_RECORD_LIMIT) {
        errno = EINVAL;
        return fail(error, error_size, "invalid TB key set output");
    }
    for (size_t i = 0; i < set->count; i++) {
        if (!key_valid(&set->keys[i]) ||
            (i && key_compare(&set->keys[i - 1], &set->keys[i]) >= 0)) {
            errno = EINVAL;
            return fail(error, error_size,
                        "TB key set output is not sorted and unique");
        }
    }
    LatTbKeyFileHeader header = {
        .magic = LAT_TB_KEY_SET_MAGIC,
        .version = LAT_TB_KEY_SET_VERSION,
        .header_size = sizeof(header),
        .record_count = set->count,
        .sequence = set->sequence,
    };
    memcpy(header.source_sha256, set->source_sha256, 32);
    size_t bytes = set->count * sizeof(*set->keys);
    if (ftruncate(fd, 0) ||
        write_all_at(fd, &header, sizeof(header), 0) ||
        (bytes && write_all_at(fd, set->keys, bytes, sizeof(header)))) {
        return fail(error, error_size, "cannot write TB key set: %s",
                    strerror(errno));
    }
    return 0;
}

static int sources_match(const LatTbKeySet *left, const LatTbKeySet *right)
{
    return !memcmp(left->source_sha256, right->source_sha256, 32);
}

int lat_tb_key_set_union(const LatTbKeySet *left, const LatTbKeySet *right,
                         LatTbKeySet *result,
                         char *error, size_t error_size)
{
    if (!left || !right || !result || !sources_match(left, right) ||
        left->count > LAT_AOT_V2_TBSET_RECORD_LIMIT - right->count) {
        errno = EINVAL;
        return fail(error, error_size, "cannot union TB key sets");
    }
    memset(result, 0, sizeof(*result));
    memcpy(result->source_sha256, left->source_sha256, 32);
    result->sequence = left->sequence > right->sequence ?
                       left->sequence : right->sequence;
    result->count = left->count + right->count;
    result->keys = result->count ? malloc(result->count * sizeof(LatTbKey)) :
                                   NULL;
    if (result->count && !result->keys) {
        return fail(error, error_size, "out of memory merging TB key sets");
    }
    memcpy(result->keys, left->keys, left->count * sizeof(LatTbKey));
    memcpy(result->keys + left->count, right->keys,
           right->count * sizeof(LatTbKey));
    if (lat_tb_key_set_sort_unique(result, error, error_size)) {
        lat_tb_key_set_destroy(result);
        return -1;
    }
    return 0;
}

int lat_tb_key_set_difference(const LatTbKeySet *known,
                              const LatTbKeySet *published,
                              LatTbKeySet *result,
                              char *error, size_t error_size)
{
    if (!known || !published || !result || !sources_match(known, published)) {
        errno = EINVAL;
        return fail(error, error_size, "cannot subtract TB key sets");
    }
    memset(result, 0, sizeof(*result));
    memcpy(result->source_sha256, known->source_sha256, 32);
    result->sequence = known->sequence;
    result->keys = known->count ? malloc(known->count * sizeof(LatTbKey)) :
                                  NULL;
    if (known->count && !result->keys) {
        return fail(error, error_size,
                    "out of memory subtracting TB key sets");
    }
    size_t i = 0, j = 0;
    while (i < known->count) {
        while (j < published->count &&
               key_compare(&published->keys[j], &known->keys[i]) < 0) {
            j++;
        }
        if (j == published->count ||
            key_compare(&known->keys[i], &published->keys[j])) {
            result->keys[result->count++] = known->keys[i];
        }
        i++;
    }
    return 0;
}

int lat_tb_key_set_contains(const LatTbKeySet *set,
                            uint64_t guest_rva, uint32_t flags)
{
    if (!set) {
        return 0;
    }
    LatTbKey wanted = { .guest_rva = guest_rva, .flags = flags };
    return bsearch(&wanted, set->keys, set->count, sizeof(*set->keys),
                   key_compare) != NULL;
}
