/* Steam pressure-vessel Runtime path helpers. */

#include "qemu/osdep.h"
#include "qemu/envlist.h"
#include "qemu/path.h"
#include "qemu/pressure-vessel.h"

static char *runtime_base;
static char *runtime_files;
static char *runtime_library_path;
static bool runtime_active;
static bool runtime_library_path_active;

static const char *const runtime_x86_64_dirs[] = {
    "lib/x86_64-linux-gnu",
    "usr/lib/x86_64-linux-gnu",
};

static const char *const runtime_i386_dirs[] = {
    "lib/i386-linux-gnu",
    "usr/lib/i386-linux-gnu",
};

static const char *const runtime_library_dirs[] = {
    "lib/x86_64-linux-gnu",
    "lib/i386-linux-gnu",
    "usr/lib/x86_64-linux-gnu",
    "usr/lib/i386-linux-gnu",
    "lib",
    "lib64",
    "lib32",
    "usr/lib",
    "usr/lib64",
    "usr/lib32",
};

static char *pressure_vessel_canonical_path(const char *path)
{
    return path ? realpath(path, NULL) : NULL;
}

static bool pressure_vessel_path_is_within(const char *parent,
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

static bool pressure_vessel_runtime_path_is_active(const char *files,
                                                   const char *library_path)
{
    g_auto(GStrv) directories = NULL;

    if (!files || !library_path) {
        return false;
    }
    directories = g_strsplit(library_path, ":", -1);
    for (char **directory = directories; *directory; directory++) {
        g_autofree char *normalized = NULL;

        if ((*directory)[0] != '/') {
            continue;
        }
        normalized = pressure_vessel_canonical_path(*directory);
        if (normalized && pressure_vessel_path_is_within(files, normalized)) {
            return true;
        }
    }
    return false;
}

static bool pressure_vessel_runtime_name_is_valid(const char *runtime)
{
    return runtime && runtime[0] && strcmp(runtime, ".") &&
           strcmp(runtime, "..") && !strchr(runtime, '/');
}

static bool pressure_vessel_runtime_has_library(const char *files,
                                                const char *relative)
{
    g_autofree char *directory = g_build_filename(files, relative, NULL);
    GDir *entries;
    const char *entry;
    bool found = false;

    entries = g_dir_open(directory, 0, NULL);
    if (!entries) {
        return false;
    }
    while ((entry = g_dir_read_name(entries))) {
        g_autofree char *file = g_build_filename(directory, entry, NULL);

        if (g_file_test(file, G_FILE_TEST_IS_REGULAR)) {
            found = true;
            break;
        }
    }
    g_dir_close(entries);
    return found;
}

static bool pressure_vessel_runtime_files_are_valid(const char *files)
{
    g_autofree char *reference = NULL;
    size_t i;

    if (!files || !g_file_test(files, G_FILE_TEST_IS_DIR)) {
        return false;
    }
    reference = g_build_filename(files, ".ref", NULL);
    if (!g_file_test(reference, G_FILE_TEST_EXISTS)) {
        return false;
    }

    for (i = 0; i < G_N_ELEMENTS(runtime_library_dirs); i++) {
        if (pressure_vessel_runtime_has_library(files,
                                                runtime_library_dirs[i])) {
            return true;
        }
    }
    return false;
}

static char *pressure_vessel_runtime_from_environment(const char *base,
                                                       const char *runtime)
{
    char *files;
    char *canonical;

    if (!base || base[0] != '/' ||
        !pressure_vessel_runtime_name_is_valid(runtime)) {
        return NULL;
    }
    files = g_build_filename(base, runtime, "files", NULL);
    if (!pressure_vessel_runtime_files_are_valid(files)) {
        g_free(files);
        return NULL;
    }
    canonical = pressure_vessel_canonical_path(files);
    g_free(files);
    return canonical;
}

static char *pressure_vessel_runtime_from_library_path(const char *library_path)
{
    g_auto(GStrv) directories = NULL;

    if (!library_path) {
        return NULL;
    }

    directories = g_strsplit(library_path, ":", -1);
    for (char **directory = directories; *directory; directory++) {
        char *candidate;

        if ((*directory)[0] != '/') {
            continue;
        }
        candidate = pressure_vessel_canonical_path(*directory);
        while (candidate) {
            char *parent;

            if (pressure_vessel_runtime_files_are_valid(candidate)) {
                return candidate;
            }
            parent = g_path_get_dirname(candidate);
            if (!strcmp(parent, candidate)) {
                g_free(parent);
                g_free(candidate);
                break;
            }
            g_free(candidate);
            candidate = parent;
        }
    }
    return NULL;
}

static char *pressure_vessel_runtime_base_from_files(const char *files)
{
    g_autofree char *runtime = NULL;
    g_autofree char *base = NULL;

    if (!files) {
        return NULL;
    }
    runtime = g_path_get_dirname(files);
    base = g_path_get_dirname(runtime);
    return pressure_vessel_canonical_path(base);
}

bool latx_pressure_vessel_runtime_configure(const envlist_t *envlist,
                                            const char *expected_files)
{
    const char *base;
    const char *runtime;

    g_clear_pointer(&runtime_base, g_free);
    g_clear_pointer(&runtime_files, g_free);
    g_clear_pointer(&runtime_library_path, g_free);
    runtime_active = false;
    runtime_library_path_active = false;

    if (!envlist) {
        return false;
    }

    runtime_library_path = g_strdup(envlist_getenv(envlist,
                                                    "LD_LIBRARY_PATH"));
    base = envlist_getenv(envlist, "PRESSURE_VESSEL_RUNTIME_BASE");
    runtime = envlist_getenv(envlist, "PRESSURE_VESSEL_RUNTIME");
    runtime_files = pressure_vessel_runtime_from_environment(base, runtime);
    if (runtime_files) {
        runtime_base = pressure_vessel_canonical_path(base);
        if (!runtime_base) {
            g_clear_pointer(&runtime_files, g_free);
        }
    }
    if (!runtime_files) {
        runtime_files = pressure_vessel_runtime_from_library_path(
            runtime_library_path);
    }
    if (runtime_files && !runtime_base) {
        runtime_base = pressure_vessel_runtime_base_from_files(runtime_files);
    }
    runtime_library_path_active = pressure_vessel_runtime_path_is_active(
        runtime_files, runtime_library_path);
    runtime_active = runtime_files && expected_files &&
                     !strcmp(runtime_files, expected_files);
    return runtime_active;
}

const char *latx_pressure_vessel_runtime_files(void)
{
    return runtime_files;
}

const char *latx_pressure_vessel_runtime_library_path(void)
{
    return runtime_active ? runtime_library_path : NULL;
}

char *latx_pressure_vessel_runtime_make_library_path(const char *suffix)
{
    GString *library_path;
    size_t i;

    if (!runtime_files) {
        return NULL;
    }

    library_path = g_string_new(NULL);
    for (i = 0; i < G_N_ELEMENTS(runtime_library_dirs); i++) {
        g_autofree char *directory = g_build_filename(runtime_files,
                                                       runtime_library_dirs[i],
                                                       NULL);

        if (!g_file_test(directory, G_FILE_TEST_IS_DIR)) {
            continue;
        }
        if (library_path->len) {
            g_string_append_c(library_path, ':');
        }
        g_string_append(library_path, directory);
    }
    if (suffix && suffix[0]) {
        if (library_path->len) {
            g_string_append_c(library_path, ':');
        }
        g_string_append(library_path, suffix);
    }
    if (!library_path->len) {
        g_string_free(library_path, true);
        return NULL;
    }
    return g_string_free(library_path, false);
}

bool latx_pressure_vessel_runtime_is_library_path(const char *path)
{
    g_autofree char *normalized = NULL;

    if (!runtime_active || !runtime_files || !path || path[0] != '/') {
        return false;
    }
    normalized = pressure_vessel_canonical_path(path);
    if (!normalized) {
        return false;
    }
    return pressure_vessel_path_is_within(runtime_files, normalized);
}

bool latx_pressure_vessel_runtime_is_i386_library_path(const char *path)
{
    g_autofree char *normalized = NULL;
    size_t i;

    if (!runtime_active || !runtime_files || !path || path[0] != '/') {
        return false;
    }
    normalized = pressure_vessel_canonical_path(path);
    if (!normalized) {
        return false;
    }
    for (i = 0; i < G_N_ELEMENTS(runtime_i386_dirs); i++) {
        g_autofree char *directory = g_build_filename(runtime_files,
                                                       runtime_i386_dirs[i],
                                                       NULL);

        if (pressure_vessel_path_is_within(directory, normalized)) {
            return true;
        }
    }
    return false;
}

bool latx_pressure_vessel_runtime_is_wrapper(const char *program)
{
    g_autofree char *resolved = NULL;
    g_autofree char *wrapper_dir = NULL;
    const char *basename;

    if (!runtime_base || !program) {
        return false;
    }
    basename = strrchr(program, '/');
    basename = basename ? basename + 1 : program;
    if (strcmp(basename, "pressure-vessel-wrap")) {
        return false;
    }

    resolved = pressure_vessel_canonical_path(program);
    if (!resolved) {
        return false;
    }
    wrapper_dir = g_build_filename(runtime_base, "pressure-vessel", NULL);
    return pressure_vessel_path_is_within(wrapper_dir, resolved);
}

static char *pressure_vessel_runtime_regular_file(const char *relative)
{
    char *file;

    file = g_build_filename(runtime_files, relative, NULL);
    if (g_file_test(file, G_FILE_TEST_IS_REGULAR)) {
        return file;
    }
    g_free(file);
    return NULL;
}

static char *pressure_vessel_runtime_versioned_file(const char *directory,
                                                    const char *name)
{
    GDir *entries;
    const char *entry;
    char *file = NULL;
    size_t length = strlen(name);

    entries = g_dir_open(directory, 0, NULL);
    if (!entries) {
        return NULL;
    }

    while ((entry = g_dir_read_name(entries))) {
        char *candidate;

        if (strncmp(entry, name, length) || entry[length] != '.') {
            continue;
        }
        candidate = g_build_filename(directory, entry, NULL);
        if (!g_file_test(candidate, G_FILE_TEST_IS_REGULAR)) {
            g_free(candidate);
            continue;
        }
        if (file) {
            g_free(candidate);
            g_clear_pointer(&file, g_free);
            break;
        }
        file = candidate;
    }
    g_dir_close(entries);
    return file;
}

static char *pressure_vessel_runtime_library_file(
    const char *const *directories, size_t directory_count,
    const char *relative)
{
    bool allow_versioned_fallback = !strchr(relative, '/');
    size_t i;

    for (i = 0; i < directory_count; i++) {
        g_autofree char *directory = g_build_filename(runtime_files,
                                                       directories[i], NULL);
        char *file;

        if (!g_file_test(directory, G_FILE_TEST_IS_DIR)) {
            continue;
        }
        file = g_build_filename(directory, relative, NULL);
        if (g_file_test(file, G_FILE_TEST_IS_REGULAR)) {
            return file;
        }
        g_free(file);
        if (allow_versioned_fallback) {
            file = pressure_vessel_runtime_versioned_file(directory, relative);
            if (file) {
                return file;
            }
        }
    }
    return NULL;
}

static bool pressure_vessel_relative_path_is_safe(const char *relative)
{
    const char *component = relative;

    if (!relative || !relative[0] || relative[0] == '/') {
        return false;
    }
    while (*component) {
        size_t length = strcspn(component, "/");

        if (!length || (length == 1 && component[0] == '.') ||
            (length == 2 && component[0] == '.' && component[1] == '.')) {
            return false;
        }
        component += length;
        if (*component == '/') {
            component++;
        }
    }
    return true;
}

static bool pressure_vessel_runtime_has_dri(const char *const *directories,
                                            size_t directory_count)
{
    size_t i;

    for (i = 0; i < directory_count; i++) {
        g_autofree char *dri = g_build_filename(runtime_files, directories[i],
                                                 "dri", NULL);

        if (g_file_test(dri, G_FILE_TEST_IS_DIR)) {
            return true;
        }
    }
    return false;
}

static char *pressure_vessel_prefix_graphics_library(
    const char *const *directories, size_t directory_count,
    const char *prefix_library_dir, const char *prefix_dri_dir,
    const char *name)
{
    g_autofree char *prefix_dri = NULL;
    g_autofree char *guest_file = NULL;
    char *file;

    if (strcmp(name, "libEGL_mesa.so.0") &&
        strcmp(name, "libGLX_mesa.so.0")) {
        return NULL;
    }
    if (pressure_vessel_runtime_has_dri(directories, directory_count)) {
        return NULL;
    }
    prefix_dri = path_get_prefixed(prefix_dri_dir);
    if (!g_file_test(prefix_dri, G_FILE_TEST_IS_DIR)) {
        return NULL;
    }

    guest_file = g_build_filename(prefix_library_dir, name, NULL);
    file = path_get_prefixed(guest_file);
    if (g_file_test(file, G_FILE_TEST_IS_REGULAR)) {
        return file;
    }
    g_free(file);
    return NULL;
}

char *latx_pressure_vessel_runtime_resolve_path(const char *name)
{
    static const struct {
        const char *guest_dir;
        const char *const *runtime_dirs;
        size_t runtime_dir_count;
        const char *prefix_library_dir;
        const char *prefix_dri_dir;
    } libraries[] = {
        { "/lib/x86_64-linux-gnu/", runtime_x86_64_dirs,
          G_N_ELEMENTS(runtime_x86_64_dirs), "/usr/lib",
          "/usr/lib/xorg/modules/dri" },
        { "/usr/lib/x86_64-linux-gnu/", runtime_x86_64_dirs,
          G_N_ELEMENTS(runtime_x86_64_dirs), "/usr/lib",
          "/usr/lib/xorg/modules/dri" },
        { "/lib/i386-linux-gnu/", runtime_i386_dirs,
          G_N_ELEMENTS(runtime_i386_dirs), "/usr/lib",
          "/usr/lib/xorg/modules/dri" },
        { "/usr/lib/i386-linux-gnu/", runtime_i386_dirs,
          G_N_ELEMENTS(runtime_i386_dirs), "/usr/lib",
          "/usr/lib/xorg/modules/dri" },
    };
    size_t i;

    if (!runtime_active || !runtime_files || !name ||
        !runtime_library_path_active) {
        return NULL;
    }

    if (!strcmp(name, "/etc/ld.so.cache")) {
        return pressure_vessel_runtime_regular_file("etc/ld.so.cache");
    }
    if (!strcmp(name, "/lib/locale/locale-archive") ||
        !strcmp(name, "/usr/lib/locale/locale-archive")) {
        char *archive = pressure_vessel_runtime_regular_file(
            "lib/locale/locale-archive");

        if (!archive) {
            archive = pressure_vessel_runtime_regular_file(
                "usr/lib/locale/locale-archive");
        }
        return archive;
    }
    if (!strcmp(name, "/lib64/ld-linux-x86-64.so.2")) {
        return pressure_vessel_runtime_library_file(
            runtime_x86_64_dirs, G_N_ELEMENTS(runtime_x86_64_dirs),
            "ld-linux-x86-64.so.2");
    }
    if (!strcmp(name, "/lib/ld-linux.so.2")) {
        return pressure_vessel_runtime_library_file(
            runtime_i386_dirs, G_N_ELEMENTS(runtime_i386_dirs),
            "ld-linux.so.2");
    }

    for (i = 0; i < G_N_ELEMENTS(libraries); i++) {
        const char *relative;
        char *file;

        if (!g_str_has_prefix(name, libraries[i].guest_dir)) {
            continue;
        }
        relative = name + strlen(libraries[i].guest_dir);
        if (!pressure_vessel_relative_path_is_safe(relative)) {
            return NULL;
        }
        file = pressure_vessel_runtime_library_file(
            libraries[i].runtime_dirs, libraries[i].runtime_dir_count,
            relative);
        if (file) {
            return file;
        }
        if (!strchr(relative, '/')) {
            return pressure_vessel_prefix_graphics_library(
                libraries[i].runtime_dirs, libraries[i].runtime_dir_count,
                libraries[i].prefix_library_dir, libraries[i].prefix_dri_dir,
                relative);
        }
        return NULL;
    }
    return NULL;
}
