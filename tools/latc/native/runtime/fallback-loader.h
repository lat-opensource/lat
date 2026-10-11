#ifndef LATC_FALLBACK_LOADER_H
#define LATC_FALLBACK_LOADER_H

#include "lat-fallback.h"

#include <stddef.h>

typedef struct LatFallbackLoader {
    void *handle;
    const LatFallbackApiV1 *api;
} LatFallbackLoader;

int lat_fallback_loader_open(LatFallbackLoader *loader,
                             const char *expected_build_id,
                             char *error, size_t error_size);
void lat_fallback_loader_close(LatFallbackLoader *loader);

#endif
