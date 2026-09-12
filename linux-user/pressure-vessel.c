/* Steam pressure-vessel linux-user launch helpers. */

#include "qemu/osdep.h"
#include "qemu/envlist.h"
#include "qemu.h"
#include "qemu/path.h"
#include "qemu/pressure-vessel.h"
#include "pressure-vessel.h"
#include "elf.h"

#ifdef CONFIG_LATX

static char **pressure_vessel_payload;

/*
 * This is a lineage marker, not a Runtime discovery input.  It is written
 * only after the current executable has been verified as the Runtime's
 * pressure-vessel wrapper, then checked against the canonical Runtime path
 * in children that the wrapper launches.
 */
#define LATX_PRESSURE_VESSEL_RUNTIME_FILES_ENV \
    "LATX_PRESSURE_VESSEL_RUNTIME_FILES"

static bool latx_pressure_vessel_runtime_mark(envlist_t *envlist)
{
    const char *files = latx_pressure_vessel_runtime_files();
    g_autofree char *assignment = NULL;

    if (!files) {
        return false;
    }
    assignment = g_strdup_printf("%s=%s",
                                 LATX_PRESSURE_VESSEL_RUNTIME_FILES_ENV,
                                 files);
    return envlist_setenv(envlist, assignment) == 0;
}

#if defined(TARGET_X86_64)
static bool latx_pressure_vessel_webhelper(const char *pathname)
{
    const char *basename = strrchr(pathname, '/');

    basename = basename ? basename + 1 : pathname;
    return !strcmp(basename, "steamwebhelper_sniper_wrap.sh");
}

static bool latx_pressure_vessel_non_direct_mode(const char *arg)
{
    return !strcmp(arg, "--launcher") ||
           g_str_has_prefix(arg, "--launcher=") ||
           !strcmp(arg, "--help") ||
           g_str_has_prefix(arg, "--help=") ||
           !strcmp(arg, "--version") ||
           g_str_has_prefix(arg, "--version=") ||
           !strcmp(arg, "--version-only") ||
           g_str_has_prefix(arg, "--version-only=") ||
           !strcmp(arg, "--only-prepare") ||
           g_str_has_prefix(arg, "--only-prepare=") ||
           !strcmp(arg, "--test") ||
           g_str_has_prefix(arg, "--test=") ||
           !strcmp(arg, "--shell") ||
           g_str_has_prefix(arg, "--shell=") ||
           !strcmp(arg, "--shell-after") ||
           g_str_has_prefix(arg, "--shell-after=") ||
           !strcmp(arg, "--shell-fail") ||
           g_str_has_prefix(arg, "--shell-fail=") ||
           !strcmp(arg, "--shell-instead") ||
           g_str_has_prefix(arg, "--shell-instead=");
}

static bool latx_pressure_vessel_option_value(char ***arg)
{
    if (!(*arg)[1] || !strcmp((*arg)[1], "--")) {
        return false;
    }

    ++*arg;
    return true;
}

static void latx_pressure_vessel_apply_runtime_paths(
    envlist_t *envlist, const char *app_library_path,
    const char *old_xdg_data_dirs)
{
    const char *files = latx_pressure_vessel_runtime_files();
    g_autofree char *library_path = NULL;
    g_autofree char *assignment = NULL;

    if (!files) {
        return;
    }

    library_path = latx_pressure_vessel_runtime_make_library_path(
        app_library_path);
    if (library_path) {
        assignment = g_strdup_printf("LD_LIBRARY_PATH=%s", library_path);
        (void)envlist_setenv(envlist, assignment);
        g_clear_pointer(&assignment, g_free);
    }

    assignment = g_strdup_printf("XDG_DATA_DIRS=%s/share%s%s", files,
                                 old_xdg_data_dirs && old_xdg_data_dirs[0]
                                 ? ":" : "",
                                 old_xdg_data_dirs ? old_xdg_data_dirs : "");
    (void)envlist_setenv(envlist, assignment);
}

static char **latx_pressure_vessel_direct_payload(const char *program,
                                                   char **target_argv,
                                                   envlist_t *envlist)
{
    static const char env_if_host_prefix[] = "--env-if-host=";
    static const char app_ld_prefix[] =
        "PRESSURE_VESSEL_APP_LD_LIBRARY_PATH=";
    static const char xdg_data_dirs_prefix[] = "XDG_DATA_DIRS=";
    static const char ld_preload_prefix[] = "LD_PRELOAD=";
    static const char ld_preload_option[] = "--ld-preload=";
    static const char ld_preloads_prefix[] = "--ld-preloads=";
    const char *app_library_path = NULL;
    const char *host_ld_preload = NULL;
    g_autofree char *old_xdg_data_dirs = NULL;
    GPtrArray *environment;
    GString *preloads = NULL;
    char **arg;
    char **payload = NULL;
    bool steam_webhelper;

    if (!latx_pressure_vessel_runtime_is_wrapper(program)) {
        return NULL;
    }

    old_xdg_data_dirs = g_strdup(envlist_getenv(envlist, "XDG_DATA_DIRS"));
    environment = g_ptr_array_new_with_free_func(g_free);
    for (arg = target_argv + 1; *arg && strcmp(*arg, "--"); arg++) {
        const char *assignment = NULL;
        const char *preload = NULL;

        if (!strcmp(*arg, "--env-if-host")) {
            if (!latx_pressure_vessel_option_value(&arg)) {
                goto out;
            }
            assignment = *arg;
        } else if (g_str_has_prefix(*arg, env_if_host_prefix)) {
            assignment = *arg + strlen(env_if_host_prefix);
        } else if (latx_pressure_vessel_non_direct_mode(*arg)) {
            goto out;
        } else if (!strcmp(*arg, "--ld-preload") ||
                   !strcmp(*arg, "--ld-preloads")) {
            if (!latx_pressure_vessel_option_value(&arg)) {
                goto out;
            }
            preload = *arg;
        } else if (g_str_has_prefix(*arg, ld_preload_option)) {
            preload = *arg + strlen(ld_preload_option);
        } else if (g_str_has_prefix(*arg, ld_preloads_prefix)) {
            preload = *arg + strlen(ld_preloads_prefix);
        } else if (!strcmp(*arg, "--variable-dir") ||
                   !strcmp(*arg, "--ld-audit") ||
                   !strcmp(*arg, "--ld-audits")) {
            if (!latx_pressure_vessel_option_value(&arg)) {
                goto out;
            }
        } else if (g_str_has_prefix(*arg, "--variable-dir=") ||
                   g_str_has_prefix(*arg, "--ld-audit=") ||
                   g_str_has_prefix(*arg, "--ld-audits=")) {
            /* These options are represented by their paired environment. */
        } else {
            goto out;
        }

        if (preload && preload[0]) {
            if (!preloads) {
                preloads = g_string_new(NULL);
            }
            if (preloads->len) {
                g_string_append_c(preloads, ':');
            }
            g_string_append(preloads, preload);
        }

        if (assignment) {
            if (!assignment[0] || !strchr(assignment, '=')) {
                goto out;
            }
            if (g_str_has_prefix(assignment, app_ld_prefix)) {
                app_library_path = assignment + strlen(app_ld_prefix);
            } else if (g_str_has_prefix(assignment, ld_preload_prefix)) {
                host_ld_preload = assignment + strlen(ld_preload_prefix);
            } else {
                if (g_str_has_prefix(assignment, xdg_data_dirs_prefix)) {
                    g_free(old_xdg_data_dirs);
                    old_xdg_data_dirs = g_strdup(assignment +
                        strlen(xdg_data_dirs_prefix));
                }
                g_ptr_array_add(environment, g_strdup(assignment));
            }
        }
    }
    if (!*arg || !arg[1]) {
        goto out;
    }

    payload = arg + 1;
    /* Runtime entry points can be host scripts as well as guest ELFs. */
    steam_webhelper = latx_pressure_vessel_webhelper(payload[0]);

    if (!steam_webhelper) {
        for (size_t i = 0; i < environment->len; i++) {
            (void)envlist_setenv(envlist, g_ptr_array_index(environment, i));
        }
        if (host_ld_preload) {
            g_autofree char *assignment = g_strdup_printf(
                "LD_PRELOAD=%s", host_ld_preload);

            (void)envlist_setenv(envlist, assignment);
        } else if (preloads && preloads->len) {
            g_autofree char *assignment = g_strdup_printf(
                "LD_PRELOAD=%s", preloads->str);

            (void)envlist_setenv(envlist, assignment);
        }
        latx_pressure_vessel_apply_runtime_paths(envlist, app_library_path,
                                                 old_xdg_data_dirs);
    }

out:
    if (preloads) {
        g_string_free(preloads, true);
    }
    g_ptr_array_free(environment, true);
    return payload;
}

#endif

#if defined(TARGET_I386) && !defined(TARGET_X86_64)
static bool latx_pressure_vessel_elf_is_i386(const char *path)
{
    unsigned char ident[EI_NIDENT];
    ssize_t count;
    int fd;

    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    do {
        count = read(fd, ident, sizeof(ident));
    } while (count < 0 && errno == EINTR);
    close(fd);

    return count == sizeof(ident) && !memcmp(ident, ELFMAG, SELFMAG) &&
           ident[EI_CLASS] == ELFCLASS32;
}

static bool latx_pressure_vessel_i386_library_dir(const char *guest_dir)
{
    g_autofree char *prefixed = NULL;
    GDir *entries;
    const char *entry;
    bool found = false;

    if (!guest_dir || guest_dir[0] != '/') {
        return false;
    }
    prefixed = path_get_prefixed(guest_dir);
    entries = g_dir_open(prefixed, 0, NULL);
    if (!entries) {
        return false;
    }
    while ((entry = g_dir_read_name(entries))) {
        g_autofree char *file = NULL;

        if (!g_str_has_prefix(entry, "libc.so") &&
            !g_str_has_prefix(entry, "libc-")) {
            continue;
        }
        file = g_build_filename(prefixed, entry, NULL);
        if (latx_pressure_vessel_elf_is_i386(file)) {
            found = true;
            break;
        }
    }
    g_dir_close(entries);
    return found;
}

static bool latx_pressure_vessel_guest_path_present(const char *library_path,
                                                    const char *guest_path)
{
    g_auto(GStrv) directories = NULL;

    if (!library_path || !guest_path) {
        return false;
    }
    directories = g_strsplit(library_path, ":", -1);
    for (char **directory = directories; *directory; directory++) {
        g_autofree char *directory_host = NULL;
        g_autofree char *guest_host = NULL;
        g_autofree char *directory_real = NULL;
        g_autofree char *guest_real = NULL;

        if (!strcmp(*directory, guest_path)) {
            return true;
        }
        if ((*directory)[0] != '/' || guest_path[0] != '/') {
            continue;
        }
        directory_host = path_get_prefixed(*directory);
        guest_host = path_get_prefixed(guest_path);
        directory_real = realpath(directory_host, NULL);
        guest_real = realpath(guest_host, NULL);
        if (directory_real && guest_real && !strcmp(directory_real,
                                                     guest_real)) {
            return true;
        }
    }
    return false;
}

static bool latx_pressure_vessel_path_is_within(const char *parent,
                                                const char *path)
{
    size_t length;

    if (!parent || !path) {
        return false;
    }
    length = strlen(parent);
    return !strncmp(parent, path, length) &&
           (path[length] == '\0' || path[length] == '/');
}

static char *latx_pressure_vessel_find_i386_sysroot(
    const envlist_t *envlist, const char *library_path)
{
    const char *const environment_paths[] = {
        envlist_getenv(envlist, "SYSTEM_LD_LIBRARY_PATH"),
        library_path,
    };
    const char *const loader_paths[] = {
        "/lib/ld-linux.so.2",
        "/usr/lib/ld-linux.so.2",
    };
    g_autofree char *prefix = realpath(interp_prefix, NULL);
    size_t i;

    for (i = 0; i < G_N_ELEMENTS(environment_paths); i++) {
        g_auto(GStrv) directories = NULL;

        if (!environment_paths[i]) {
            continue;
        }
        directories = g_strsplit(environment_paths[i], ":", -1);
        for (char **directory = directories; *directory; directory++) {
            if (latx_pressure_vessel_i386_library_dir(*directory)) {
                return g_strdup(*directory);
            }
        }
    }

    if (!prefix) {
        return NULL;
    }
    for (i = 0; i < G_N_ELEMENTS(loader_paths); i++) {
        g_autofree char *prefixed = path_get_prefixed(loader_paths[i]);
        g_autofree char *resolved = realpath(prefixed, NULL);
        const char *relative;
        g_autofree char *guest_dir = NULL;

        if (!resolved || !latx_pressure_vessel_elf_is_i386(resolved) ||
            !latx_pressure_vessel_path_is_within(prefix, resolved)) {
            continue;
        }
        relative = resolved + strlen(prefix);
        guest_dir = g_path_get_dirname(relative);
        if (latx_pressure_vessel_i386_library_dir(guest_dir)) {
            return g_steal_pointer(&guest_dir);
        }
    }
    return NULL;
}

static void latx_pressure_vessel_append_i386_sysroot(envlist_t *envlist)
{
    const char *library_path = latx_pressure_vessel_runtime_library_path();
    g_auto(GStrv) directories = NULL;
    g_autofree char *sysroot = NULL;
    g_autofree char *assignment = NULL;
    bool runtime_seen = false;

    if (!library_path) {
        return;
    }

    directories = g_strsplit(library_path, ":", -1);
    for (char **directory = directories; *directory; directory++) {
        if (!(*directory)[0]) {
            continue;
        }
        runtime_seen |= latx_pressure_vessel_runtime_is_i386_library_path(
            *directory);
    }
    if (!runtime_seen) {
        return;
    }

    sysroot = latx_pressure_vessel_find_i386_sysroot(envlist, library_path);
    if (!sysroot || latx_pressure_vessel_guest_path_present(library_path,
                                                              sysroot)) {
        return;
    }
    assignment = g_strdup_printf("LD_LIBRARY_PATH=%s:%s", library_path,
                                 sysroot);
    (void)envlist_setenv(envlist, assignment);
}
#endif

void latx_pressure_vessel_prepare(const char *program, char **target_argv,
                                  envlist_t *envlist)
{
    const char *basename = program ? strrchr(program, '/') : NULL;
    const char *expected_files;
    bool wrapper_name;

    pressure_vessel_payload = NULL;

    expected_files = envlist_getenv(envlist,
                                    LATX_PRESSURE_VESSEL_RUNTIME_FILES_ENV);
    wrapper_name = !g_strcmp0(basename ? basename + 1 : program,
                              "pressure-vessel-wrap");

    /* Avoid Runtime discovery for launches outside the pressure-vessel chain. */
    if (!wrapper_name && !expected_files) {
        return;
    }

    /* Discovery alone must not override generic guest path resolution. */
    latx_pressure_vessel_runtime_configure(envlist, NULL);
    if (latx_pressure_vessel_runtime_is_wrapper(program)) {
        if (!latx_pressure_vessel_runtime_mark(envlist)) {
            return;
        }
    } else if (!expected_files) {
        return;
    }

#if defined(TARGET_X86_64)
    pressure_vessel_payload = latx_pressure_vessel_direct_payload(
        program, target_argv, envlist);
#endif

    expected_files = envlist_getenv(envlist,
                                    LATX_PRESSURE_VESSEL_RUNTIME_FILES_ENV);
    if (!latx_pressure_vessel_runtime_configure(envlist, expected_files)) {
        return;
    }

#if defined(TARGET_I386) && !defined(TARGET_X86_64)
    latx_pressure_vessel_append_i386_sysroot(envlist);
    (void)latx_pressure_vessel_runtime_configure(envlist, expected_files);
#endif
}

void latx_pressure_vessel_exec_payload(char **target_environ)
{
    if (!pressure_vessel_payload) {
        return;
    }
    execve(pressure_vessel_payload[0], pressure_vessel_payload,
           target_environ);
    fprintf(stderr, "cannot launch pressure-vessel payload %s: %s\n",
            pressure_vessel_payload[0], strerror(errno));
}

#endif
