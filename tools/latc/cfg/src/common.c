#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "common.h"

/*
 * Shared utility implementation.
 *
 * These helpers keep parser modules focused on binary-format logic instead of
 * repeating fatal error and byte-reader boilerplate.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void die(const char *msg)
{
    fprintf(stderr, "error: %s\n", msg);
    exit(1);
}

void die_errno(const char *msg)
{
    fprintf(stderr, "error: %s: %s\n", msg, strerror(errno));
    exit(1);
}

void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) {
        die_errno("malloc");
    }
    return p;
}

char *xstrdup(const char *s)
{
    char *p = strdup(s ? s : "");
    if (!p) {
        die_errno("strdup");
    }
    return p;
}

bool range_ok(size_t file_size, uint64_t off, uint64_t size)
{
    return off <= file_size && size <= file_size - off;
}

int32_t rd_i32(const uint8_t *p)
{
    uint32_t v = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                 ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    return (int32_t)v;
}

uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint64_t rd64(const uint8_t *p)
{
    return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
}

uint64_t read_uleb(const uint8_t *buf, size_t size, size_t *off)
{
    uint64_t result = 0;
    unsigned shift = 0;
    while (*off < size) {
        uint8_t b = buf[(*off)++];
        result |= (uint64_t)(b & 0x7f) << shift;
        if ((b & 0x80) == 0) {
            break;
        }
        shift += 7;
        if (shift >= 64) {
            break;
        }
    }
    return result;
}

int64_t read_sleb(const uint8_t *buf, size_t size, size_t *off)
{
    int64_t result = 0;
    unsigned shift = 0;
    uint8_t b = 0;
    while (*off < size) {
        b = buf[(*off)++];
        result |= (int64_t)(b & 0x7f) << shift;
        shift += 7;
        if ((b & 0x80) == 0) {
            break;
        }
        if (shift >= 64) {
            break;
        }
    }
    if (shift < 64 && (b & 0x40)) {
        result |= -((int64_t)1 << shift);
    }
    return result;
}
