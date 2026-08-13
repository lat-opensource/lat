/*
 * Steam pressure-vessel compatibility helpers.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/envlist.h"
#include "pressure-vessel.h"

#if defined(CONFIG_LATX) && defined(TARGET_X86_64)

static bool latx_steam_webhelper(const char *pathname)
{
    const char *basename = strrchr(pathname, '/');

    basename = basename ? basename + 1 : pathname;
    return !strcmp(basename, "steamwebhelper_sniper_wrap.sh");
}

static char *latx_pressure_vessel_runtime_files(void)
{
    const char *runtime_base = getenv("PRESSURE_VESSEL_RUNTIME_BASE");
    const char *runtime = getenv("PRESSURE_VESSEL_RUNTIME");
    char *runtime_files;

    if (!runtime_base || runtime_base[0] != '/' || !runtime || !runtime[0]
        || strchr(runtime, '/')) {
        return NULL;
    }

    runtime_files = g_build_filename(runtime_base, runtime, "files", NULL);
    if (!g_file_test(runtime_files, G_FILE_TEST_IS_DIR)) {
        g_free(runtime_files);
        return NULL;
    }

    return runtime_files;
}

static void latx_pressure_vessel_runtime_paths(envlist_t *envlist,
                                               const char *runtime_files,
                                               const char *app_ld_path,
                                               const char *old_xdg_data_dirs)
{
    static const char *const library_dirs[] = {
        "lib/x86_64-linux-gnu",
        "lib/i386-linux-gnu",
        "lib",
        "lib64",
        "lib32",
    };
    GString *library_path;
    char *assignment;
    size_t i;

    library_path = g_string_new(NULL);
    for (i = 0; i < G_N_ELEMENTS(library_dirs); i++) {
        char *directory = g_build_filename(runtime_files, library_dirs[i],
                                           NULL);

        if (g_file_test(directory, G_FILE_TEST_IS_DIR)) {
            if (library_path->len) {
                g_string_append_c(library_path, ':');
            }
            g_string_append(library_path, directory);
        }
        g_free(directory);
    }
    if (app_ld_path && app_ld_path[0]) {
        if (library_path->len) {
            g_string_append_c(library_path, ':');
        }
        g_string_append(library_path, app_ld_path);
    }
    if (library_path->len) {
        assignment = g_strdup_printf("LD_LIBRARY_PATH=%s", library_path->str);
        (void)envlist_setenv(envlist, assignment);
        g_free(assignment);
    }
    g_string_free(library_path, true);

    assignment = g_strdup_printf("XDG_DATA_DIRS=%s/share%s%s", runtime_files,
                                 old_xdg_data_dirs && old_xdg_data_dirs[0]
                                 ? ":" : "",
                                 old_xdg_data_dirs ? old_xdg_data_dirs : "");
    (void)envlist_setenv(envlist, assignment);
    g_free(assignment);
}

char **latx_pressure_vessel_prepare(const char *program, char **target_argv,
                                    envlist_t *envlist)
{
    static const char env_if_host_prefix[] = "--env-if-host=";
    static const char app_ld_prefix[] =
        "PRESSURE_VESSEL_APP_LD_LIBRARY_PATH=";
    static const char xdg_data_dirs_prefix[] = "XDG_DATA_DIRS=";
    static const char ld_preload_prefix[] = "LD_PRELOAD=";
    static const char ld_preload_option[] = "--ld-preload=";
    static const char ld_preloads_prefix[] = "--ld-preloads=";
    const char *basename = strrchr(program, '/');
    const char *app_ld_path = NULL;
    const char *host_ld_preload = NULL;
    const char *old_xdg_data_dirs = getenv("XDG_DATA_DIRS");
    char *runtime_files = NULL;
    GPtrArray *environment;
    GString *preloads = NULL;
    char **arg;
    char **payload = NULL;
    bool steam_webhelper;
    bool launcher = false;

    basename = basename ? basename + 1 : program;
    if (strcmp(basename, "pressure-vessel-wrap")) {
        return NULL;
    }

    environment = g_ptr_array_new_with_free_func(g_free);
    for (arg = target_argv + 1; *arg && strcmp(*arg, "--"); arg++) {
        const char *assignment = NULL;
        const char *preload = NULL;

        if (!strcmp(*arg, "--env-if-host")) {
            if (!arg[1]) {
                goto out;
            }
            assignment = *++arg;
        } else if (g_str_has_prefix(*arg, env_if_host_prefix)) {
            assignment = *arg + strlen(env_if_host_prefix);
        } else if (!strcmp(*arg, "--launcher") ||
                   g_str_has_prefix(*arg, "--launcher=")) {
            launcher = true;
        } else if (!strcmp(*arg, "--ld-preload") ||
                   !strcmp(*arg, "--ld-preloads")) {
            if (!arg[1] || !strcmp(arg[1], "--")) {
                goto out;
            }
            preload = *++arg;
        } else if (g_str_has_prefix(*arg, ld_preload_option)) {
            preload = *arg + strlen(ld_preload_option);
        } else if (g_str_has_prefix(*arg, ld_preloads_prefix)) {
            preload = *arg + strlen(ld_preloads_prefix);
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
                app_ld_path = assignment + strlen(app_ld_prefix);
            } else if (g_str_has_prefix(assignment, ld_preload_prefix)) {
                host_ld_preload = assignment + strlen(ld_preload_prefix);
            } else {
                if (g_str_has_prefix(assignment, xdg_data_dirs_prefix)) {
                    old_xdg_data_dirs = assignment +
                        strlen(xdg_data_dirs_prefix);
                }
                g_ptr_array_add(environment, g_strdup(assignment));
            }
        }
    }
    if (!*arg || !arg[1] || launcher) {
        goto out;
    }

    steam_webhelper = latx_steam_webhelper(arg[1]);
    runtime_files = latx_pressure_vessel_runtime_files();
    if (!runtime_files && !steam_webhelper) {
        goto out;
    }

    /* The WebHelper wrapper is a host script and retains its original env. */
    if (!steam_webhelper) {
        for (size_t i = 0; i < environment->len; i++) {
            (void)envlist_setenv(envlist, g_ptr_array_index(environment, i));
        }
        if (host_ld_preload) {
            char *assignment = g_strdup_printf("LD_PRELOAD=%s",
                                                host_ld_preload);

            (void)envlist_setenv(envlist, assignment);
            g_free(assignment);
        } else if (preloads && preloads->len) {
            char *assignment = g_strdup_printf("LD_PRELOAD=%s", preloads->str);

            (void)envlist_setenv(envlist, assignment);
            g_free(assignment);
        }
    }
    if (runtime_files) {
        latx_pressure_vessel_runtime_paths(envlist, runtime_files, app_ld_path,
                                           old_xdg_data_dirs);
    }
    payload = arg + 1;

out:
    g_free(runtime_files);
    if (preloads) {
        g_string_free(preloads, true);
    }
    g_ptr_array_free(environment, true);
    return payload;
}

#endif
