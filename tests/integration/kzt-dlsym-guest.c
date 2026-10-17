/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { \
    if (!(c)) { \
        fprintf(stderr, "FAIL:%d: %s\n", __LINE__, #c); \
        exit(1); \
    } \
} while (0)

static void *handle;
static int *main_tls;

static void *lookup(const char *name)
{
    void *result;

    dlerror();
    result = dlsym(handle, name);
    CHECK(result != NULL);
    CHECK(dlerror() == NULL);
    return result;
}

static void *check_thread_tls(void *unused)
{
    int *tls = lookup("probe_public_tls");
    int *(*address)(void) = lookup("probe_public_tls_address");

    (void)unused;
    CHECK(tls == address());
    CHECK(tls != main_tls);
    CHECK(*tls == 23);
    *tls = 99;
    return NULL;
}

int main(int argc, char **argv)
{
    int (*normal)(void);
    int (*indirect)(void);
    int (*marker)(void);
    pthread_t thread;
    const char *error;
    void *result;

    CHECK(argc == 3);
    handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return 1;
    }

    /* Warm a registered wrapper's Guest link_map before the NULL lookup. */
    normal = lookup("probe_normal");
    CHECK(normal() == 42);
    indirect = lookup("probe_ifunc");
    CHECK(indirect == normal);
    CHECK(indirect() == 42);
    CHECK(*(int *)lookup("probe_data") == 37);

    main_tls = lookup("probe_public_tls");
    CHECK(*main_tls == 23);
    *main_tls = 71;
    CHECK(pthread_create(&thread, NULL, check_thread_tls, NULL) == 0);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(*main_tls == 71);

    dlerror();
    result = dlsym(handle, "probe_null_ifunc");
    CHECK(result == NULL);
    dlerror();

    CHECK(dlsym(handle, "probe_symbol_does_not_exist") == NULL);
    error = dlerror();
    CHECK(error != NULL && error[0] != '\0');
    CHECK(dlerror() == NULL);
    CHECK(lookup("probe_normal") == (void *)normal);

    marker = lookup("XSupportsLocale");
    CHECK(marker() == atoi(argv[2]));
    printf("PASS: dlsym values and TLS; endpoint=%s\n",
           atoi(argv[2]) == 61 ? "native-wrapper" : "guest");
    CHECK(dlclose(handle) == 0);
    return 0;
}
