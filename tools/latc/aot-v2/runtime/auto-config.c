#define _GNU_SOURCE
#include "auto-config.h"
#include "latcd-client.h"
#include "latc-build-id.h"

#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <pwd.h>
#include <spawn.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/auxv.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

static char *mode, *cache, *socket_path, *module, *autostart;
static char *state, *daemon_path, *compiler_path, *runner_path, *runtime_path;
static bool enabled, managed, can_start;
static int64_t retry_after;

bool lat_aot_config_option(const char *key, const char *value)
{
    char **slot = NULL;
    if (!strcmp(key, "LATX_AOT_V2")) slot = &mode;
    else if (!strcmp(key, "LATX_AOT_V2_CACHE_DIR")) slot = &cache;
    else if (!strcmp(key, "LATX_AOT_V2_LATCD_SOCKET")) slot = &socket_path;
    else if (!strcmp(key, "LATX_AOT_V2_MODULE")) slot = &module;
    else if (!strcmp(key, "LATX_AOT_V2_AUTOSTART")) slot = &autostart;
    if (!slot) return false;
    g_free(*slot);
    *slot = g_strdup(value);
    return true;
}

static bool private_directory(const char *path)
{
    if (!path || path[0] != '/') return false;
    char **parts = g_strsplit(path, "/", -1);
    int fd = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    bool valid = fd >= 0;
    bool private_ancestor = false;
    for (unsigned int i = 0; valid && parts[i]; i++) {
        if (!*parts[i]) continue;
        if (!strcmp(parts[i], ".") || !strcmp(parts[i], "..")) {
            valid = false;
            break;
        }
        if (mkdirat(fd, parts[i], 0700) && errno != EEXIST) {
            valid = false;
            break;
        }
        int next = openat(fd, parts[i], O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        struct stat st;
        struct statvfs filesystem;
        bool readonly = next >= 0 && !fstatvfs(next, &filesystem) &&
                        (filesystem.f_flag & ST_RDONLY);
        valid = next >= 0 && !fstat(next, &st) &&
            (readonly || st.st_uid == 0 || st.st_uid == geteuid()) &&
            (private_ancestor || !(st.st_mode & 0022) || (st.st_mode & S_ISVTX));
        /* A private user-owned ancestor prevents other users reaching children. */
        if (valid && st.st_uid == geteuid() && !(st.st_mode & 0077))
            private_ancestor = true;
        close(fd);
        fd = next;
    }
    struct stat st;
    valid = valid && !fstat(fd, &st) && st.st_uid == geteuid() &&
            !(st.st_mode & 0077) && (st.st_mode & 0700) == 0700;
    if (fd >= 0) close(fd);
    g_strfreev(parts);
    return valid;
}

static char *xdg_path(const char *variable, const char *home, const char *fallback)
{
    const char *value = getenv(variable);
    return value && value[0] == '/' ? g_strdup(value) :
        g_build_filename(home, fallback, NULL);
}

void lat_aot_config_environment(void)
{
    const char *keys[] = { "LATX_AOT_V2", "LATX_AOT_V2_CACHE_DIR",
        "LATX_AOT_V2_LATCD_SOCKET", "LATX_AOT_V2_MODULE",
        "LATX_AOT_V2_AUTOSTART", NULL };
    for (int i = 0; keys[i]; i++) {
        const char *value = getenv(keys[i]);
        if (value) lat_aot_config_option(keys[i], value);
    }
}

int lat_aot_config_init(int *legacy, bool explicit_legacy, const char *bindir,
                        const char *libdir, char *error, size_t error_size)
{
    if ((mode && strcmp(mode, "0") && strcmp(mode, "1")) ||
        (autostart && strcmp(autostart, "0") && strcmp(autostart, "1"))) {
        snprintf(error, error_size, "AOT v2 enable/autostart values must be 0 or 1");
        return -1;
    }
    enabled = mode ? !strcmp(mode, "1") :
        ((cache && *cache) || (socket_path && *socket_path) || (module && *module));
    if (!enabled) return 0;
    if (explicit_legacy && *legacy) {
        snprintf(error, error_size, "LATX_AOT and AOT v2 cannot both be enabled");
        return -1;
    }
    *legacy = 0;
    if (getauxval(AT_SECURE) || getuid() != geteuid() || getgid() != getegid()) {
        enabled = false;
        return 0;
    }
    managed = !(socket_path && *socket_path) && !(module && *module);
    can_start = managed && (!autostart || !strcmp(autostart, "1"));
    if (managed && cache && !*cache) g_clear_pointer(&cache, g_free);
    if (module && !*module) g_clear_pointer(&module, g_free);
    const char *home = getenv("HOME");
    struct passwd *pw = NULL;
    if (!home || home[0] != '/') {
        pw = getpwuid(geteuid());
        home = pw ? pw->pw_dir : NULL;
    }
    if (!home || home[0] != '/') {
        managed = false;
        return 0;
    }
    char *cache_root = xdg_path("XDG_CACHE_HOME", home, ".cache");
    char *state_root = xdg_path("XDG_STATE_HOME", home, ".local/state");
    if (!cache && !module && managed) {
        cache = g_build_filename(cache_root, "lat-aot-v2", LATC_BUILD_ID, "live", NULL);
    }
    state = g_build_filename(state_root, "lat-aot-v2", LATC_BUILD_ID, NULL);
    g_free(cache_root);
    if (managed && private_directory(state) && private_directory(cache)) {
        char *canonical = realpath(cache, NULL);
        char *digest = canonical ? g_compute_checksum_for_string(
            G_CHECKSUM_SHA256, canonical, -1) : NULL;
        free(canonical);
        const char *runtime = getenv("XDG_RUNTIME_DIR");
        char *directory = runtime && private_directory(runtime) ?
            g_build_filename(runtime, "lat-aot-v2", NULL) :
            g_build_filename(state_root, "lat-aot-v2", "run", NULL);
        if (digest && private_directory(directory)) {
            char *identity = g_strconcat(LATC_BUILD_ID, digest, NULL);
            char *socket_digest = g_compute_checksum_for_string(
                G_CHECKSUM_SHA256, identity, -1);
            char *name = g_strdup_printf("%.20s.sock", socket_digest);
            g_free(socket_digest);
            g_free(identity);
            g_free(socket_path);
            socket_path = g_build_filename(directory, name, NULL);
            g_free(name);
        }
        g_free(digest);
        g_free(directory);
    }
    g_free(state_root);
    if (!socket_path) {
        managed = false;
        g_clear_pointer(&socket_path, g_free);
    }
    daemon_path = g_build_filename(bindir, "latcd", NULL);
    compiler_path = g_build_filename(bindir, "latc", NULL);
    runner_path = g_build_filename(bindir, "latx-x86_64", NULL);
    runtime_path = g_strdup(libdir);
    return 0;
}

const char *lat_aot_cache_path(void) { return enabled ? cache : NULL; }
const char *lat_aot_socket_path(void) { return enabled ? socket_path : NULL; }
const char *lat_aot_module_path(void) { return enabled ? module : NULL; }

static bool hello(void)
{
    char error[256];
    uint8_t digest[32];
    bool check_cache = cache && *cache;
    char *canonical = check_cache ? realpath(cache, NULL) : NULL;
    if (check_cache && !canonical) return false;
    if (canonical) {
        GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
        g_checksum_update(checksum, (const guchar *)canonical, strlen(canonical));
        gsize size = sizeof(digest);
        g_checksum_get_digest(checksum, digest, &size);
        g_checksum_free(checksum);
        free(canonical);
    }
    return !latcd_client_hello(socket_path, LATC_BUILD_ID,
                              check_cache ? digest : NULL, error, sizeof(error));
}

bool lat_aot_ensure_daemon(void)
{
    if (!enabled || !socket_path) return false;
    if (!managed) return hello();
    if (retry_after > g_get_monotonic_time()) return false;
    if (hello()) return true;
    if (!can_start) return false;
    if (errno != ENOENT && errno != ECONNREFUSED) return false;
    retry_after = g_get_monotonic_time() + 5 * G_USEC_PER_SEC;
    char *lock_path = g_strconcat(socket_path, ".lock", NULL);
    int lock = open(lock_path, O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    g_free(lock_path);
    struct stat st;
    if (lock < 0) return false;
    if (fstat(lock, &st) || !S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
        st.st_nlink != 1 || (st.st_mode & 0077)) {
        close(lock);
        return false;
    }
    int64_t deadline = g_get_monotonic_time() + 7 * G_USEC_PER_SEC;
    while (flock(lock, LOCK_EX | LOCK_NB)) {
        if ((errno != EWOULDBLOCK && errno != EINTR) ||
            g_get_monotonic_time() >= deadline) {
            close(lock);
            return false;
        }
        if (hello()) {
            retry_after = 0;
            close(lock);
            return true;
        }
        g_usleep(25000);
    }
    bool ready = hello();
    if (!ready && (errno == ENOENT || errno == ECONNREFUSED)) {
        char *log_path = g_build_filename(state, "daemon.log", NULL);
        int log = open(log_path, O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC | O_NOFOLLOW, 0600);
        g_free(log_path);
        if (log >= 0 && !fstat(log, &st) && S_ISREG(st.st_mode) &&
            st.st_uid == geteuid() && st.st_nlink == 1 && !(st.st_mode & 0077)) {
            posix_spawn_file_actions_t actions;
            posix_spawn_file_actions_init(&actions);
            posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
            posix_spawn_file_actions_addopen(&actions, 1, "/dev/null", O_WRONLY, 0);
            posix_spawn_file_actions_adddup2(&actions, log, 2);
            posix_spawnattr_t attributes;
            posix_spawnattr_init(&attributes);
            sigset_t empty;
            sigemptyset(&empty);
            posix_spawnattr_setsigmask(&attributes, &empty);
            sigset_t defaults;
            sigfillset(&defaults);
            posix_spawnattr_setsigdefault(&attributes, &defaults);
            posix_spawnattr_setflags(&attributes,
                                    POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
            char *arguments[] = { daemon_path, "--serve", "--daemonize",
                "--socket", socket_path, "--cache-dir", cache, "--latc", compiler_path,
                "--runner", runner_path, "--runtime-dir", runtime_path,
                "--idle-seconds", "60", NULL };
            char *temporary = g_build_filename(state, "tmp", NULL);
            char *tmpenv = g_strconcat("TMPDIR=", temporary, NULL);
            char *loaderenv = g_strconcat("LD_LIBRARY_PATH=", runtime_path, NULL);
            char *environment[] = { "PATH=/usr/bin:/bin", "LANG=C", "LC_ALL=C",
                "LATX_AOT=0", "LATX_AOT_V2=0", "LATX_AOT_V2_AUTOSTART=0",
                tmpenv, loaderenv, NULL };
            pid_t child;
            if (private_directory(temporary) &&
                !posix_spawn(&child, daemon_path, &actions, &attributes, arguments, environment)) {
                int status;
                for (int i = 0; i < 200; i++) {
                    pid_t result = waitpid(child, &status, WNOHANG);
                    if (result == child || (result < 0 && errno != EINTR)) break;
                    g_usleep(25000);
                    if (i == 199) {
                        kill(child, SIGKILL);
                        while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
                    }
                }
                deadline = g_get_monotonic_time() + 2 * G_USEC_PER_SEC;
                while (!(ready = hello()) && g_get_monotonic_time() < deadline) {
                    if (errno != ENOENT && errno != ECONNREFUSED) break;
                    g_usleep(25000);
                }
            }
            g_free(loaderenv);
            g_free(tmpenv);
            g_free(temporary);
            posix_spawn_file_actions_destroy(&actions);
            posix_spawnattr_destroy(&attributes);
        }
        if (log >= 0) close(log);
    }
    close(lock);
    if (ready) retry_after = 0;
    return ready;
}
