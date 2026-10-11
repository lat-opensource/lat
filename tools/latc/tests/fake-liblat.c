#include "lat-fallback.h"

#include <stddef.h>

struct LatFallbackContext {
    int unused;
};

static int fake_create(const LatFallbackConfigV1 *config,
                       LatFallbackContext **context, char *error,
                       size_t error_size)
{
    (void)config;
    (void)context;
    (void)error;
    (void)error_size;
    return 0;
}

static int fake_run(LatFallbackContext *context, LatX86StateV1 *state,
                    char *error, size_t error_size)
{
    (void)context;
    (void)state;
    (void)error;
    (void)error_size;
    return 0;
}

static void fake_destroy(LatFallbackContext *context)
{
    (void)context;
}

const LatFallbackApiV1 *lat_fallback_get_api_v1(void)
{
    static const LatFallbackApiV1 api = {
        .abi_version = LAT_FALLBACK_ABI_VERSION,
        .struct_size = sizeof(api),
        .lat_build_id = LAT_FAKE_BUILD_ID,
        .create = fake_create,
        .run_until_compiled = fake_run,
        .destroy = fake_destroy,
    };
    return &api;
}
