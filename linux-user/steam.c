/* Steam linux-user launch helpers. */

#include "qemu/osdep.h"
#include "qemu/envlist.h"
#include "qemu.h"
#include "qemu/path.h"
#include "qemu/steam.h"
#include "steam.h"
#include "elf.h"
#if defined(CONFIG_LATX_KZT)
#include "kzt-runtime.h"
#endif

#ifdef CONFIG_LATX

static char **steam_payload;

static bool latx_steam_runtime_mark(envlist_t *envlist)
{
    const char *files = latx_steam_runtime_files();
    g_autofree char *assignment = NULL;

    if (!files) {
        return false;
    }
    assignment = g_strdup_printf("%s=%s",
                                 LATX_STEAM_RUNTIME_FILES_ENV,
                                 files);
    return envlist_setenv(envlist, assignment) == 0;
}

#if defined(TARGET_X86_64)
static bool latx_steam_webhelper(const char *pathname)
{
    const char *basename = strrchr(pathname, '/');

    basename = basename ? basename + 1 : pathname;
    return !strcmp(basename, "steamwebhelper_sniper_wrap.sh");
}

typedef enum {
    LATX_STEAM_TERMINAL_INVALID,
    LATX_STEAM_TERMINAL_NONE,
    LATX_STEAM_TERMINAL_AUTO,
    LATX_STEAM_TERMINAL_TTY,
    LATX_STEAM_TERMINAL_XTERM,
} LatxSteamTerminal;

static LatxSteamTerminal latx_steam_terminal_mode(const char *value)
{
    if (!value || !value[0] || !strcmp(value, "auto")) {
        return LATX_STEAM_TERMINAL_AUTO;
    }
    if (!strcmp(value, "none") || !strcmp(value, "no")) {
        return LATX_STEAM_TERMINAL_NONE;
    }
    if (!strcmp(value, "tty")) {
        return LATX_STEAM_TERMINAL_TTY;
    }
    if (!strcmp(value, "xterm")) {
        return LATX_STEAM_TERMINAL_XTERM;
    }
    return LATX_STEAM_TERMINAL_INVALID;
}

static bool latx_steam_shell_mode(const char *value, bool *active)
{
    if (!value || !value[0] || !strcmp(value, "none") ||
        !strcmp(value, "no")) {
        *active = false;
        return true;
    }
    if (!strcmp(value, "after") || !strcmp(value, "fail") ||
        !strcmp(value, "instead")) {
        *active = true;
        return true;
    }
    return false;
}

static bool latx_steam_boolean_value(const char *value, bool *result)
{
    if (!value || !value[0]) {
        return false;
    }
    if (!strcmp(value, "1") || !g_ascii_strcasecmp(value, "true") ||
        !g_ascii_strcasecmp(value, "yes")) {
        *result = true;
        return true;
    }
    if (!strcmp(value, "0") || !g_ascii_strcasecmp(value, "false") ||
        !g_ascii_strcasecmp(value, "no")) {
        *result = false;
        return true;
    }
    return false;
}

static bool latx_steam_non_direct_mode(const char *arg)
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
           g_str_has_prefix(arg, "--test=");
}

static bool latx_steam_option_value(char ***arg)
{
    if (!(*arg)[1] || !strcmp((*arg)[1], "--")) {
        return false;
    }

    ++*arg;
    return true;
}

static void latx_steam_append_loader_entry(GString **list, const char *value)
{
    if (!value || !value[0]) {
        return;
    }

    if (!*list) {
        *list = g_string_new(NULL);
    } else if ((*list)->len) {
        g_string_append_c(*list, ':');
    }
    g_string_append(*list, value);
}

static char *latx_steam_filter_game_overlay(const char *value)
{
    g_auto(GStrv) entries = g_strsplit_set(value, ": ", -1);
    GString *filtered = g_string_new(NULL);

    for (char **entry = entries; entry && *entry; entry++) {
        if (!(*entry)[0]) {
            continue;
        }
        if (g_str_has_suffix(*entry, "/gameoverlayrenderer.so")) {
            continue;
        }
        if (filtered->len) {
            g_string_append_c(filtered, ':');
        }
        g_string_append(filtered, *entry);
    }
    return g_string_free(filtered, false);
}

static void latx_steam_apply_loader_list(envlist_t *envlist, const char *name,
                                         const char *override,
                                         bool has_override,
                                         const GString *additions,
                                         bool remove_game_overlay)
{
    const char *base = has_override ? override : envlist_getenv(envlist, name);
    g_autofree char *value = NULL;
    g_autofree char *assignment = NULL;

    if (!has_override && (!additions || !additions->len) &&
        !remove_game_overlay) {
        return;
    }
    if (base && base[0] && additions && additions->len) {
        value = g_strdup_printf("%s:%s", base, additions->str);
    } else if (additions && additions->len) {
        value = g_strdup(additions->str);
    } else if (base) {
        value = g_strdup(base);
    } else if (has_override) {
        value = g_strdup("");
    }

    if (!value) {
        return;
    }
    if (remove_game_overlay) {
        g_autofree char *filtered = latx_steam_filter_game_overlay(value);

        g_free(g_steal_pointer(&value));
        value = g_steal_pointer(&filtered);
    }
    assignment = g_strdup_printf("%s=%s", name, value);
    (void)envlist_setenv(envlist, assignment);
}

static void latx_steam_apply_runtime_paths(
    envlist_t *envlist, const char *payload_path, const char *app_library_path,
    const char *old_xdg_data_dirs)
{
    const char *files = latx_steam_runtime_files();
    const char *app_path = app_library_path && app_library_path[0] ?
                           app_library_path : NULL;
    g_autofree char *application_paths = NULL;
    g_autofree char *library_path = NULL;
    g_autofree char *assignment = NULL;
    g_autofree char *payload_dir = NULL;
    bool steam_payload = latx_steam_program(payload_path);
    if (!files) {
        return;
    }

    if (steam_payload) {
        /*
         * Steam's own payload must lead the search so its versioned libraries
         * remain a coherent set ahead of the generic guest sysroot.
         */
        payload_dir = g_path_get_dirname(payload_path);
        application_paths = g_strdup_printf("%s%s%s:/usr/lib64:/usr/lib",
                                            payload_dir, app_path ? ":" : "",
                                            app_path ? app_path : "");
    } else {
        /* Applications keep the established sysroot and Runtime priority. */
        application_paths = g_strdup("/usr/lib64:/usr/lib");
    }
    library_path = latx_steam_runtime_make_library_path(application_paths);
    if (library_path && app_path && !steam_payload) {
        g_autofree char *runtime_path = g_steal_pointer(&library_path);

        library_path = g_strconcat(runtime_path, ":", app_path, NULL);
    }
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

static char **latx_steam_direct_payload(const char *program,
                                        char **target_argv,
                                        envlist_t *envlist)
{
    static const char env_if_host_prefix[] = "--env-if-host=";
    static const char app_ld_prefix[] =
        "PRESSURE_VESSEL_APP_LD_LIBRARY_PATH=";
    static const char xdg_data_dirs_prefix[] = "XDG_DATA_DIRS=";
    static const char ld_preload_prefix[] = "LD_PRELOAD=";
    static const char ld_audit_prefix[] = "LD_AUDIT=";
    static const char host_ld_preload_option[] = "--host-ld-preload=";
    static const char ld_preload_option[] = "--ld-preload=";
    static const char ld_preloads_prefix[] = "--ld-preloads=";
    static const char ld_audit_option[] = "--ld-audit=";
    static const char ld_audits_prefix[] = "--ld-audits=";
    const char *app_library_path = NULL;
    const char *host_ld_preload = NULL;
    const char *host_ld_audit = NULL;
    g_autofree char *old_xdg_data_dirs = NULL;
    GPtrArray *environment;
    GString *preloads = NULL;
    GString *audits = NULL;
    bool has_host_ld_preload = false;
    bool has_host_ld_audit = false;
    bool batch =
        !g_strcmp0(envlist_getenv(envlist, "PRESSURE_VESSEL_BATCH"), "1");
    bool shell;
    bool remove_game_overlay =
        !g_strcmp0(envlist_getenv(envlist,
                                 "PRESSURE_VESSEL_REMOVE_GAME_OVERLAY"), "1");
    LatxSteamTerminal terminal = latx_steam_terminal_mode(
        envlist_getenv(envlist, "PRESSURE_VESSEL_TERMINAL"));
    char **arg;
    char **payload = NULL;
    bool steam_webhelper;

    if (!latx_steam_runtime_is_wrapper(program)) {
        return NULL;
    }

    old_xdg_data_dirs = g_strdup(envlist_getenv(envlist, "XDG_DATA_DIRS"));
    environment = g_ptr_array_new_with_free_func(g_free);
    if (terminal == LATX_STEAM_TERMINAL_INVALID ||
        !latx_steam_shell_mode(
            envlist_getenv(envlist, "PRESSURE_VESSEL_SHELL"), &shell)) {
        goto out;
    }
    for (arg = target_argv + 1; *arg && strcmp(*arg, "--"); arg++) {
        const char *assignment = NULL;
        const char *preload = NULL;

        if (!strcmp(*arg, "--env-if-host")) {
            if (!latx_steam_option_value(&arg)) {
                goto out;
            }
            assignment = *arg;
        } else if (g_str_has_prefix(*arg, env_if_host_prefix)) {
            assignment = *arg + strlen(env_if_host_prefix);
        } else if (latx_steam_non_direct_mode(*arg)) {
            goto out;
        } else if (!strcmp(*arg, "--batch")) {
            batch = true;
        } else if (g_str_has_prefix(*arg, "--batch=")) {
            if (!latx_steam_boolean_value(*arg + strlen("--batch="),
                                          &batch)) {
                goto out;
            }
        } else if (!strcmp(*arg, "--shell")) {
            if (!latx_steam_option_value(&arg) ||
                !latx_steam_shell_mode(*arg, &shell)) {
                goto out;
            }
        } else if (g_str_has_prefix(*arg, "--shell=")) {
            if (!latx_steam_shell_mode(*arg + strlen("--shell="), &shell)) {
                goto out;
            }
        } else if (!strcmp(*arg, "--shell-after") ||
                   g_str_has_prefix(*arg, "--shell-after=") ||
                   !strcmp(*arg, "--shell-fail") ||
                   g_str_has_prefix(*arg, "--shell-fail=") ||
                   !strcmp(*arg, "--shell-instead") ||
                   g_str_has_prefix(*arg, "--shell-instead=")) {
            shell = true;
        } else if (!strcmp(*arg, "--terminal")) {
            if (!latx_steam_option_value(&arg)) {
                goto out;
            }
            terminal = latx_steam_terminal_mode(*arg);
            if (terminal == LATX_STEAM_TERMINAL_INVALID) {
                goto out;
            }
        } else if (g_str_has_prefix(*arg, "--terminal=")) {
            terminal = latx_steam_terminal_mode(*arg + strlen("--terminal="));
            if (terminal == LATX_STEAM_TERMINAL_INVALID) {
                goto out;
            }
        } else if (!strcmp(*arg, "--tty") ||
                   g_str_has_prefix(*arg, "--tty=")) {
            terminal = LATX_STEAM_TERMINAL_TTY;
        } else if (!strcmp(*arg, "--xterm") ||
                   g_str_has_prefix(*arg, "--xterm=")) {
            terminal = LATX_STEAM_TERMINAL_XTERM;
        } else if (!strcmp(*arg, "--remove-game-overlay")) {
            remove_game_overlay = true;
        } else if (g_str_has_prefix(*arg, "--remove-game-overlay=")) {
            if (!latx_steam_boolean_value(
                    *arg + strlen("--remove-game-overlay="),
                    &remove_game_overlay)) {
                goto out;
            }
        } else if (!strcmp(*arg, "--keep-game-overlay")) {
            remove_game_overlay = false;
        } else if (g_str_has_prefix(*arg, "--keep-game-overlay=")) {
            bool keep;

            if (!latx_steam_boolean_value(*arg +
                                          strlen("--keep-game-overlay="),
                                          &keep)) {
                goto out;
            }
            remove_game_overlay = !keep;
        } else if (!strcmp(*arg, "--host-ld-preload")) {
            if (!latx_steam_option_value(&arg)) {
                goto out;
            }
            preload = *arg;
        } else if (g_str_has_prefix(*arg, host_ld_preload_option)) {
            preload = *arg + strlen(host_ld_preload_option);
        } else if (!strcmp(*arg, "--ld-preload") ||
                   !strcmp(*arg, "--ld-preloads")) {
            if (!latx_steam_option_value(&arg)) {
                goto out;
            }
            preload = *arg;
        } else if (g_str_has_prefix(*arg, ld_preload_option)) {
            preload = *arg + strlen(ld_preload_option);
        } else if (g_str_has_prefix(*arg, ld_preloads_prefix)) {
            preload = *arg + strlen(ld_preloads_prefix);
        } else if (!strcmp(*arg, "--ld-audit") ||
                   !strcmp(*arg, "--ld-audits")) {
            if (!latx_steam_option_value(&arg)) {
                goto out;
            }
            latx_steam_append_loader_entry(&audits, *arg);
        } else if (g_str_has_prefix(*arg, ld_audit_option)) {
            latx_steam_append_loader_entry(
                &audits, *arg + strlen(ld_audit_option));
        } else if (g_str_has_prefix(*arg, ld_audits_prefix)) {
            latx_steam_append_loader_entry(
                &audits, *arg + strlen(ld_audits_prefix));
        } else if (!strcmp(*arg, "--variable-dir")) {
            if (!latx_steam_option_value(&arg)) {
                goto out;
            }
        } else {
            /*
             * Direct execution creates no container. Options that only adjust
             * container setup have no equivalent here; environment and loader
             * options with direct equivalents are handled above. Skipping the
             * remaining wrapper options keeps new Runtime releases compatible
             * without treating each container-only addition as a new failure.
             */
        }

        if (preload && preload[0]) {
            latx_steam_append_loader_entry(&preloads, preload);
        }

        if (assignment) {
            if (!assignment[0] || !strchr(assignment, '=')) {
                goto out;
            }
            if (g_str_has_prefix(assignment, app_ld_prefix)) {
                app_library_path = assignment + strlen(app_ld_prefix);
            } else if (g_str_has_prefix(assignment, ld_preload_prefix)) {
                host_ld_preload = assignment + strlen(ld_preload_prefix);
                has_host_ld_preload = true;
            } else if (g_str_has_prefix(assignment, ld_audit_prefix)) {
                host_ld_audit = assignment + strlen(ld_audit_prefix);
                has_host_ld_audit = true;
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
    if (batch) {
        shell = false;
        terminal = LATX_STEAM_TERMINAL_NONE;
    } else if (terminal == LATX_STEAM_TERMINAL_AUTO) {
        terminal = shell ? LATX_STEAM_TERMINAL_XTERM :
                           LATX_STEAM_TERMINAL_NONE;
    }
    if (terminal == LATX_STEAM_TERMINAL_TTY ||
        terminal == LATX_STEAM_TERMINAL_XTERM ||
        (terminal == LATX_STEAM_TERMINAL_NONE && shell)) {
        goto out;
    }

    payload = arg + 1;
    /* Runtime entry points can be host scripts as well as guest ELFs. */
    steam_webhelper = latx_steam_webhelper(payload[0]);

    if (!steam_webhelper) {
        for (size_t i = 0; i < environment->len; i++) {
            (void)envlist_setenv(envlist, g_ptr_array_index(environment, i));
        }
        latx_steam_apply_loader_list(envlist, "LD_PRELOAD", host_ld_preload,
                                     has_host_ld_preload, preloads,
                                     remove_game_overlay);
        latx_steam_apply_loader_list(envlist, "LD_AUDIT", host_ld_audit,
                                     has_host_ld_audit, audits, false);
        latx_steam_apply_runtime_paths(envlist, payload[0], app_library_path,
                                       old_xdg_data_dirs);
    }

out:
    if (preloads) {
        g_string_free(preloads, true);
    }
    if (audits) {
        g_string_free(audits, true);
    }
    g_ptr_array_free(environment, true);
    return payload;
}

#endif

#if defined(TARGET_I386) && !defined(TARGET_X86_64)
static bool latx_steam_elf_is_i386(const char *path)
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

static bool latx_steam_i386_library_dir(const char *guest_dir)
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
        if (latx_steam_elf_is_i386(file)) {
            found = true;
            break;
        }
    }
    g_dir_close(entries);
    return found;
}

static bool latx_steam_guest_path_present(const char *library_path,
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

static bool latx_steam_path_is_within(const char *parent,
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

static char *latx_steam_find_i386_sysroot(
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
            if (latx_steam_i386_library_dir(*directory)) {
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

        if (!resolved || !latx_steam_elf_is_i386(resolved) ||
            !latx_steam_path_is_within(prefix, resolved)) {
            continue;
        }
        relative = resolved + strlen(prefix);
        guest_dir = g_path_get_dirname(relative);
        if (latx_steam_i386_library_dir(guest_dir)) {
            return g_steal_pointer(&guest_dir);
        }
    }
    return NULL;
}

static void latx_steam_append_i386_sysroot(envlist_t *envlist)
{
    const char *library_path = latx_steam_runtime_library_path();
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
        runtime_seen |= latx_steam_runtime_is_i386_library_path(
            *directory);
    }
    if (!runtime_seen) {
        return;
    }

    sysroot = latx_steam_find_i386_sysroot(envlist, library_path);
    if (!sysroot || latx_steam_guest_path_present(library_path, sysroot)) {
        return;
    }
    assignment = g_strdup_printf("LD_LIBRARY_PATH=%s:%s", library_path,
                                 sysroot);
    (void)envlist_setenv(envlist, assignment);
}
#endif

void latx_steam_prepare(const char *program, char **target_argv,
                        envlist_t *envlist)
{
    const char *basename = program ? strrchr(program, '/') : NULL;
    const char *expected_files;
    bool steam_program;
    bool wrapper_name;

    steam_payload = NULL;

    expected_files = envlist_getenv(envlist,
                                    LATX_STEAM_RUNTIME_FILES_ENV);
    basename = basename ? basename + 1 : program;
    wrapper_name = !g_strcmp0(basename, "pressure-vessel-wrap");
    steam_program = latx_steam_program(program);
    latx_steam_context_configure(envlist,
                                 wrapper_name || steam_program ||
                                 expected_files != NULL);
    /* Avoid Runtime discovery for launches outside the pressure-vessel chain. */
    if (!wrapper_name && !expected_files) {
        return;
    }

    /* Discovery alone must not override generic guest path resolution. */
    latx_steam_runtime_configure(envlist, NULL);
    if (latx_steam_runtime_is_wrapper(program)) {
        if (!latx_steam_runtime_mark(envlist)) {
            return;
        }
    } else if (!expected_files) {
        return;
    }

#if defined(TARGET_X86_64)
    steam_payload = latx_steam_direct_payload(
        program, target_argv, envlist);
#endif

    expected_files = envlist_getenv(envlist,
                                    LATX_STEAM_RUNTIME_FILES_ENV);
    if (!latx_steam_runtime_configure(envlist, expected_files)) {
        return;
    }

#if defined(TARGET_I386) && !defined(TARGET_X86_64)
    latx_steam_append_i386_sysroot(envlist);
    (void)latx_steam_runtime_configure(envlist, expected_files);
#endif
}

static bool latx_steam_runtime_is_sniper(void)
{
    const char *files = latx_steam_runtime_files();
    g_autofree char *contents = NULL;
    g_autofree char *os_release = NULL;

    if (!files) {
        return false;
    }
    os_release = g_build_filename(files, "lib", "os-release", NULL);
    return g_file_get_contents(os_release, &contents, NULL, NULL) &&
           strstr(contents, "VERSION_CODENAME=sniper");
}

static char **latx_steam_sniper_argv(char **payload)
{
    const char *files = latx_steam_runtime_files();
    g_autofree char *bwrap = NULL;
    g_autofree char *os_release = NULL;
    g_autofree char **argv = NULL;
    size_t payload_count;

    if (!files) {
        return NULL;
    }
    os_release = g_build_filename(files, "lib", "os-release", NULL);
    if (!g_file_test(os_release, G_FILE_TEST_IS_REGULAR)) {
        return NULL;
    }
    bwrap = g_find_program_in_path("bwrap");
    if (!bwrap) {
        return NULL;
    }

    /*
     * The direct payload path intentionally bypasses pressure-vessel, but
     * sniper-aware launchers inspect the container's /etc/os-release before
     * they start the game.  Reproduce only that mount-namespace contract.
     */
    payload_count = g_strv_length(payload);
    argv = g_new0(char *, payload_count + 12);
    argv[0] = g_strdup(bwrap);
    argv[1] = (char *)"--bind";
    argv[2] = (char *)"/";
    argv[3] = (char *)"/";
    argv[4] = (char *)"--dev-bind";
    argv[5] = (char *)"/dev";
    argv[6] = (char *)"/dev";
    argv[7] = (char *)"--ro-bind";
    argv[8] = g_strdup(os_release);
    argv[9] = (char *)"/etc/os-release";
    argv[10] = (char *)"--";
    memcpy(argv + 11, payload, (payload_count + 1) * sizeof(*payload));
    return g_steal_pointer(&argv);
}

void latx_steam_exec_payload(char **target_environ)
{
    g_autofree char **argv = NULL;

    if (!steam_payload) {
        return;
    }

    if (latx_steam_runtime_is_sniper()) {
        argv = latx_steam_sniper_argv(steam_payload);
        if (argv) {
            execve(argv[0], argv, target_environ);
        }
    }
    execve(steam_payload[0], steam_payload,
           target_environ);
    fprintf(stderr, "cannot launch pressure-vessel payload %s: %s\n",
            steam_payload[0], strerror(errno));
}

#endif
