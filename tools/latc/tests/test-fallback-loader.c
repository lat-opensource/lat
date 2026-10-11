#define _GNU_SOURCE

#include "fallback-loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    LatFallbackLoader loader = {0};
    char error[256] = {0};

    if (argc != 3) {
        fprintf(stderr, "usage: %s GOOD_LIB BAD_LIB\n", argv[0]);
        return 2;
    }
    if (loader.handle || loader.api) {
        return 1;
    }
    setenv("LATC_LIBLAT", argv[1], 1);
    if (lat_fallback_loader_open(&loader, "test-build", error,
                                 sizeof(error)) != 0 || !loader.api) {
        fprintf(stderr, "good library rejected: %s\n", error);
        return 1;
    }
    lat_fallback_loader_close(&loader);
    if (loader.handle || loader.api) {
        return 1;
    }

    setenv("LATC_LIBLAT", argv[2], 1);
    if (lat_fallback_loader_open(&loader, "test-build", error,
                                 sizeof(error)) == 0 ||
        !strstr(error, "build ID")) {
        fprintf(stderr, "bad library accepted: %s\n", error);
        return 1;
    }
    puts("test-fallback-loader: PASS");
    return 0;
}
