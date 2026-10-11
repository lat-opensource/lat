#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

extern int semantic_startup_apply(int value, int (*callback)(int));
extern int semantic_preload_marker(void) __attribute__((weak));

int semantic_hook(void)
{
    return 10;
}

static int semantic_callback(int value)
{
    return value * 2;
}

static void *required_symbol(void *handle, const char *name)
{
    dlerror();
    void *symbol = dlsym(handle, name);
    return dlerror() ? NULL : symbol;
}

static int loop_count_from_env(void)
{
    const char *value = getenv("LATC_M3_CROSS_MODULE_LOOPS");
    int result = 0;

    if (!value) {
        return 0;
    }
    if (!*value) {
        return -1;
    }
    for (; *value; value++) {
        if (*value < '0' || *value > '9' || result > 1000000) {
            return -1;
        }
        result = result * 10 + (*value - '0');
    }
    return result;
}

int main(void)
{
    void *plugin = dlopen(getenv("LATC_SEMANTICS_PLUGIN"),
                          RTLD_NOW | RTLD_LOCAL);
    if (!plugin) {
        fprintf(stderr, "plugin dlopen failed: %s\n", dlerror());
        return 2;
    }
    int (*tls_add)(int) = required_symbol(plugin, "semantic_tls_add");
    int (*ifunc)(void) = required_symbol(plugin, "semantic_ifunc");
    int (*version2)(void) = required_symbol(plugin, "semantic_version");
    int (*version1)(void) = plugin ?
        dlvsym(plugin, "semantic_version", "LATC_1.0") : NULL;
    int (*interposed)(void) = required_symbol(plugin, "semantic_interposed");
    int (*pingpong)(int) = required_symbol(plugin, "semantic_pingpong");
    if (!tls_add || !ifunc || !version1 || !version2 || !interposed ||
        !pingpong || !semantic_preload_marker) {
        fprintf(stderr,
                "missing dynamic semantic symbol tls=%p ifunc=%p v1=%p "
                "v2=%p interposed=%p pingpong=%p preload=%p\n",
                (void *)tls_add, (void *)ifunc, (void *)version1,
                (void *)version2, (void *)interposed, (void *)pingpong,
                (void *)semantic_preload_marker);
        return 2;
    }
    int startup = semantic_startup_apply(5, semantic_callback);
    int tls_first = tls_add(3);
    int tls_second = tls_add(2);
    int ifunc_value = ifunc();
    int v1 = version1();
    int v2 = version2();
    int hook = interposed();
    int preload = semantic_preload_marker();
    int loops = loop_count_from_env();
    int pingpong_sum = pingpong(loops);
    printf("startup=%d tls=%d,%d ifunc=%d versions=%d,%d hook=%d preload=%d\n",
           startup, tls_first, tls_second, ifunc_value, v1, v2, hook, preload);
    return startup == 11 && tls_first == 7 && tls_second == 9 &&
           ifunc_value == 23 && v1 == 31 && v2 == 32 && hook == 10 &&
           preload == 77 && loops >= 0 && pingpong_sum == loops * 10 ? 0 : 3;
}
