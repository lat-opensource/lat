#ifndef LATC_LAT_FALLBACK_H
#define LATC_LAT_FALLBACK_H

#include <stddef.h>
#include <stdint.h>

#define LAT_FALLBACK_ABI_VERSION 1u
#define LAT_FALLBACK_SONAME "liblat.so.1"

typedef struct LatFallbackContext LatFallbackContext;

typedef struct LatX86StateV1 {
    uint64_t gpr[16];
    uint64_t rip;
    uint64_t rflags;
    uint64_t fs_base;
    uint64_t gs_base;
    uint32_t mxcsr;
    uint16_t fp_control;
    uint16_t fp_status;
    uint16_t fp_tag;
    uint16_t reserved;
    uint8_t st[8][16];
    uint8_t xmm[16][16];
    uint8_t tail_reserved[4];
} LatX86StateV1;

typedef int (*LatFallbackIsCompiledV1)(void *opaque, uint64_t guest_pc);

typedef struct LatFallbackConfigV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *lat_build_id;
    uint64_t guest_base;
    LatFallbackIsCompiledV1 is_compiled;
    void *is_compiled_opaque;
} LatFallbackConfigV1;

typedef struct LatFallbackApiV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *lat_build_id;
    int (*create)(const LatFallbackConfigV1 *config,
                  LatFallbackContext **context, char *error,
                  size_t error_size);
    int (*run_until_compiled)(LatFallbackContext *context,
                              LatX86StateV1 *state, char *error,
                              size_t error_size);
    void (*destroy)(LatFallbackContext *context);
} LatFallbackApiV1;

typedef const LatFallbackApiV1 *(*LatFallbackGetApiV1)(void);

/* Exported by liblat.so.1. */
const LatFallbackApiV1 *lat_fallback_get_api_v1(void);

#endif
