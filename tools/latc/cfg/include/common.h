#ifndef COMMON_H
#define COMMON_H

/*
 * Small process-wide utilities shared by the analysis modules.
 *
 * These helpers intentionally fail fast: this is an offline inspection tool,
 * so malformed input and allocation failures are reported with a clear message
 * instead of being propagated through every parser call.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Fatal diagnostics. These functions do not return. */
void die(const char *msg);
void die_errno(const char *msg);

/* Allocation helpers that never return NULL. */
void *xmalloc(size_t n);
char *xstrdup(const char *s);

/* Bounds and little-endian readers for ELF and x86 byte streams. */
bool range_ok(size_t file_size, uint64_t off, uint64_t size);
int32_t rd_i32(const uint8_t *p);
uint32_t rd32(const uint8_t *p);
uint64_t rd64(const uint8_t *p);

/* DWARF-style LEB128 readers. *off is advanced past the consumed bytes. */
uint64_t read_uleb(const uint8_t *buf, size_t size, size_t *off);
int64_t read_sleb(const uint8_t *buf, size_t size, size_t *off);

#endif
