/* Test-only override of the host CPU feature list; guest /proc is unaffected. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *fopen(const char *path, const char *mode)
{
    const char *features = getenv("LATX_TEST_HOST_FEATURES");
    FILE *(*real_fopen)(const char *, const char *) = dlsym(RTLD_NEXT, "fopen");

    if (features && !strcmp(path, "/proc/cpuinfo")) {
        return fmemopen((void *)features, strlen(features), "r");
    }
    return real_fopen(path, mode);
}
