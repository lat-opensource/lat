#define _GNU_SOURCE
#include "auto-config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void run_case(int number)
{
    unsetenv("LATX_AOT_V2");
    unsetenv("LATX_AOT_V2_CACHE_DIR");
    unsetenv("LATX_AOT_V2_LATCD_SOCKET");
    unsetenv("LATX_AOT_V2_MODULE");
    unsetenv("LATX_AOT_V2_AUTOSTART");
    int legacy = 1;
    bool explicit = false;
    if (number != 0) setenv("LATX_AOT_V2", "1", 1);
    if (number == 2) explicit = true;
    if (number == 3) legacy = 0;
    if (number == 4) setenv("LATX_AOT_V2", "0", 1);
    if (number == 5) setenv("LATX_AOT_V2", "invalid", 1);
    if (number == 6) {
        setenv("LATX_AOT_V2_CACHE_DIR", "/unused/cache", 1);
        setenv("LATX_AOT_V2_LATCD_SOCKET", "/unused/socket", 1);
    }
    if (number == 8) {
        unsetenv("LATX_AOT_V2");
        setenv("LATX_AOT_V2_CACHE_DIR", "/unused/cache", 1);
        setenv("LATX_AOT_V2_LATCD_SOCKET", "/unused/socket", 1);
    }
    if (number == 9) {
        setenv("LATX_AOT_V2", "0", 1);
        setenv("LATX_AOT_V2_CACHE_DIR", "/unused/cache", 1);
        setenv("LATX_AOT_V2_LATCD_SOCKET", "/unused/socket", 1);
    }
    if (number == 10) {
        legacy = 0;
        explicit = true;
    }
    if (number == 11) {
        setenv("LATX_AOT_V2_CACHE_DIR", "", 1);
        setenv("LATX_AOT_V2_LATCD_SOCKET", "/unused/socket", 1);
    }
    setenv("LATX_AOT_V2_AUTOSTART", "0", 1);
    lat_aot_config_environment();
    if (number == 7) lat_aot_config_option("LATX_AOT_V2", "0");
    char error[256];
    int rc = lat_aot_config_init(&legacy, explicit, "/usr/bin", "/usr/lib", error, sizeof(error));
    assert((rc != 0) == (number == 2 || number == 5));
    if (!rc) {
        bool v2 = number != 0 && number != 4 && number != 7 && number != 9;
        assert(legacy == (v2 ? 0 : 1));
        assert((lat_aot_cache_path() != NULL) == v2);
        if (number == 6) {
            assert(!strcmp(lat_aot_cache_path(), "/unused/cache"));
            assert(!strcmp(lat_aot_socket_path(), "/unused/socket"));
        }
        if (number == 11) {
            assert(!*lat_aot_cache_path());
            assert(!strcmp(lat_aot_socket_path(), "/unused/socket"));
        }
        if (v2 && number != 6 && number != 8 && number != 11) {
            assert(lat_aot_socket_path());
            assert(strlen(lat_aot_socket_path()) < 108);
            assert(!lat_aot_ensure_daemon());
        }
    }
}

int main(void)
{
    const char *temporary = getenv("TMPDIR");
    assert(temporary);
    char *home = malloc(strlen(temporary) + 32);
    assert(home);
    sprintf(home, "%s/lat-auto-config-XXXXXX", temporary);
    assert(mkdtemp(home));
    setenv("HOME", home, 1);
    unsetenv("XDG_CACHE_HOME");
    unsetenv("XDG_STATE_HOME");
    unsetenv("XDG_RUNTIME_DIR");
    for (int i = 0; i < 12; i++) {
        pid_t child = fork();
        assert(child >= 0);
        if (!child) { run_case(i); _exit(0); }
        int status;
        assert(waitpid(child, &status, 0) == child);
        assert(WIFEXITED(status) && !WEXITSTATUS(status));
    }
    puts("auto-config mode matrix passed");
    return 0;
}
