#define _GNU_SOURCE

#include "fallback-loader.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int set_error(char *error, size_t error_size, const char *message)
{
    if (error && error_size) {
        snprintf(error, error_size, "%s", message);
    }
    return -1;
}

int lat_fallback_loader_open(LatFallbackLoader *loader,
                             const char *expected_build_id,
                             char *error, size_t error_size)
{
    const char *path;
    LatFallbackGetApiV1 get_api;
    const LatFallbackApiV1 *api;

    if (!loader || !expected_build_id || !expected_build_id[0]) {
        return set_error(error, error_size,
                         "invalid fallback loader arguments");
    }
    if (loader->handle) {
        return 0;
    }

    path = getenv("LATC_LIBLAT");
    if (!path || !path[0]) {
        path = LAT_FALLBACK_SONAME;
    }
    loader->handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!loader->handle) {
        return set_error(error, error_size, dlerror());
    }
    dlerror();
    *(void **)(&get_api) = dlsym(loader->handle, "lat_fallback_get_api_v1");
    const char *dlsym_error = dlerror();
    if (dlsym_error) {
        char saved_error[256];
        snprintf(saved_error, sizeof(saved_error), "%s", dlsym_error);
        lat_fallback_loader_close(loader);
        return set_error(error, error_size, saved_error);
    }
    api = get_api();
    if (!api || api->abi_version != LAT_FALLBACK_ABI_VERSION ||
        api->struct_size < sizeof(*api) || !api->lat_build_id ||
        !api->create || !api->run_until_compiled || !api->destroy) {
        lat_fallback_loader_close(loader);
        return set_error(error, error_size,
                         "liblat.so.1 has an incompatible ABI");
    }
    if (strcmp(api->lat_build_id, expected_build_id) != 0) {
        lat_fallback_loader_close(loader);
        return set_error(error, error_size,
                         "liblat.so.1 build ID does not match the native image");
    }
    loader->api = api;
    return 0;
}

void lat_fallback_loader_close(LatFallbackLoader *loader)
{
    if (!loader) {
        return;
    }
    loader->api = NULL;
    if (loader->handle) {
        dlclose(loader->handle);
        loader->handle = NULL;
    }
}
