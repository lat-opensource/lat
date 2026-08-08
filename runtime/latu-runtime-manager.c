/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"

#include <libgen.h>
#include <poll.h>

typedef enum TranslatorDirectoryStatus {
    TRANSLATOR_DIRECTORY_KNOWN,
    TRANSLATOR_DIRECTORY_UNKNOWN,
} TranslatorDirectoryStatus;

typedef enum TranslatorStatus {
    TRANSLATOR_PRESENT,
    TRANSLATOR_MISSING,
    TRANSLATOR_UNKNOWN,
} TranslatorStatus;

typedef enum QueryStatus {
    QUERY_SELECTED,
    QUERY_TRANSLATOR_MISSING,
    QUERY_TRANSLATOR_UNKNOWN,
    QUERY_TRANSLATOR_FAILED,
    QUERY_INVALID_OUTPUT,
} QueryStatus;

typedef enum InspectionStatus {
    INSPECTION_READY,
    INSPECTION_NOT_REQUIRED,
    INSPECTION_MISSING,
    INSPECTION_INVALID,
    INSPECTION_UNKNOWN,
} InspectionStatus;

typedef enum ReadinessStatus {
    READINESS_READY,
    READINESS_READY_WITH_HOST_FALLBACK,
    READINESS_UNAVAILABLE,
    READINESS_BROKEN,
    READINESS_UNKNOWN,
} ReadinessStatus;

typedef enum LoaderSource {
    LOADER_SOURCE_NONE,
    LOADER_SOURCE_RUNTIME_ROOT,
    LOADER_SOURCE_HOST_FALLBACK,
} LoaderSource;

typedef struct TranslatorLocation {
    char *directory;
    char *root;
    TranslatorDirectoryStatus directory_status;
} TranslatorLocation;

typedef struct RuntimeInfo {
    long schema_version;
    char *guest_abi;
    char *runtime_root;
    char *runtime_source;
} RuntimeInfo;

typedef struct RuntimeQuery {
    const char *guest_abi;
    TranslatorStatus translator_status;
    QueryStatus query_status;
    RuntimeInfo info;
} RuntimeQuery;

typedef struct RuntimeInspection {
    const char *guest_abi;
    const char *runtime_root;
    const char *loader_path;
    char *canonical_root;
    char *configured_loader;
    char *resolved_loader;
    bool configured_loader_selected;
    InspectionStatus status;
    const char *reason;
    char *effective_loader;
    InspectionStatus effective_status;
    const char *effective_reason;
    LoaderSource loader_source;
} RuntimeInspection;

typedef struct ProgramInfo {
    const char *path;
    const char *guest_abi;
    char *interpreter;
    bool is_static;
} ProgramInfo;

typedef struct LoaderCheck {
    char *resolved_path;
    InspectionStatus status;
    const char *reason;
} LoaderCheck;

typedef struct StringBuffer {
    char *data;
    size_t length;
    size_t capacity;
} StringBuffer;

typedef struct JsonParser {
    const char *cursor;
    const char *end;
} JsonParser;

#define RUNTIME_INFO_LIMIT (1024 * 1024)
#define RUNTIME_INFO_TIMEOUT_MS 10000

static const char *program_name = "latu-runtime-manager";

static void report_error(const char *format, ...)
{
    va_list arguments;

    fprintf(stderr, "%s: ", program_name);
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fputc('\n', stderr);
}

static void *xmalloc(size_t size)
{
    return g_malloc(size);
}

static char *xstrdup(const char *value)
{
    return g_strdup(value);
}

static char *path_join(const char *left, const char *right)
{
    size_t left_length = strlen(left);
    bool needs_separator = left_length && left[left_length - 1] != '/';
    size_t size = left_length + needs_separator + strlen(right) + 1;
    char *result = xmalloc(size);

    snprintf(result, size, "%s%s%s", left, needs_separator ? "/" : "",
             right);
    return result;
}

static char *absolute_path(const char *path)
{
    g_autofree char *current_directory = NULL;

    if (path[0] == '/') {
        return xstrdup(path);
    }
    current_directory = g_get_current_dir();
    return path_join(current_directory, path);
}

static char *find_program_in_path(const char *name)
{
    const char *path = getenv("PATH");
    const char *start;

    if (!path) {
        return NULL;
    }

    for (start = path; ; ) {
        const char *end = strchr(start, ':');
        size_t length = end ? (size_t)(end - start) : strlen(start);
        const char *directory = length ? start : ".";
        size_t directory_length = length ? length : 1;
        size_t size = directory_length + 1 + strlen(name) + 1;
        char *candidate = xmalloc(size);

        snprintf(candidate, size, "%.*s/%s", (int)directory_length,
                 directory, name);
        if (access(candidate, X_OK) == 0) {
            return candidate;
        }
        g_free(candidate);

        if (!end) {
            break;
        }
        start = end + 1;
    }
    return NULL;
}

static void usage(void)
{
    fprintf(stderr, "usage: latu-runtime-manager status [--root ROOT]\n");
    exit(2);
}

static bool path_is_within(const char *root, const char *path)
{
    size_t length;

    if (!strcmp(root, "/")) {
        return true;
    }

    length = strlen(root);
    return !strncmp(root, path, length) &&
           (path[length] == '\0' || path[length] == '/');
}

static char *resolve_manager_path(const char *argv0)
{
    char *candidate = NULL;
    char *resolved;
    char proc_path[PATH_MAX + 1];
    ssize_t length;

    length = readlink("/proc/self/exe", proc_path, sizeof(proc_path) - 1);
    if (length >= 0) {
        proc_path[length] = '\0';
        candidate = xstrdup(proc_path);
    } else if (strchr(argv0, '/')) {
        candidate = xstrdup(argv0);
    } else {
        candidate = find_program_in_path(argv0);
        if (!candidate) {
            report_error("cannot locate the manager executable");
            return NULL;
        }
    }

    resolved = realpath(candidate, NULL);
    g_free(candidate);
    if (!resolved) {
        report_error("cannot resolve the manager executable");
        return NULL;
    }

    return resolved;
}

static bool locate_sibling_translators(const char *argv0,
                                       TranslatorLocation *location)
{
    char *manager_path = resolve_manager_path(argv0);
    char *manager_path_copy;
    char *directory;
    char *resolved;

    if (!manager_path) {
        return false;
    }

    manager_path_copy = xstrdup(manager_path);
    directory = dirname(manager_path_copy);
    resolved = realpath(directory, NULL);
    g_free(manager_path);
    g_free(manager_path_copy);
    if (!resolved) {
        report_error("cannot locate the manager installation directory");
        return false;
    }

    location->directory = resolved;
    location->root = xstrdup("");
    location->directory_status = TRANSLATOR_DIRECTORY_KNOWN;
    return true;
}

static bool path_exists_without_following(const char *path)
{
    struct stat statbuf;

    return lstat(path, &statbuf) == 0;
}

static bool missing_symlink_is_confined(const char *path, const char *root)
{
    char *current = xstrdup(path);

    for (int link_count = 0; link_count < 40; link_count++) {
        char target[PATH_MAX + 1];
        ssize_t target_length;
        char *current_copy = NULL;
        char *candidate = NULL;
        char *candidate_directory_copy = NULL;
        char *candidate_name_copy = NULL;
        char *resolved_directory = NULL;
        char *resolved_target = NULL;
        struct stat statbuf;
        int stat_status;
        bool target_missing;

        target_length = readlink(current, target, sizeof(target) - 1);
        if (target_length < 0 || target_length == sizeof(target) - 1) {
            g_free(current);
            return false;
        }
        target[target_length] = '\0';
        if (target[0] == '/') {
            candidate = xstrdup(target);
        } else {
            current_copy = xstrdup(current);
            candidate = path_join(dirname(current_copy), target);
        }

        candidate_directory_copy = xstrdup(candidate);
        candidate_name_copy = xstrdup(candidate);
        resolved_directory = realpath(dirname(candidate_directory_copy), NULL);
        if (resolved_directory) {
            resolved_target = path_join(resolved_directory,
                                        basename(candidate_name_copy));
        }
        g_free(current_copy);
        g_free(candidate);
        g_free(candidate_directory_copy);
        g_free(candidate_name_copy);
        g_free(resolved_directory);
        if (!resolved_target ||
            (root[0] && !path_is_within(root, resolved_target))) {
            g_free(resolved_target);
            g_free(current);
            return false;
        }

        stat_status = lstat(resolved_target, &statbuf);
        target_missing = stat_status < 0 &&
                         (errno == ENOENT || errno == ENOTDIR);
        if (target_missing) {
            g_free(resolved_target);
            g_free(current);
            return true;
        }
        if (stat_status < 0 || !S_ISLNK(statbuf.st_mode)) {
            g_free(resolved_target);
            g_free(current);
            return false;
        }

        g_free(current);
        current = resolved_target;
    }

    g_free(current);
    return false;
}

static bool locate_rooted_translators(const char *input_root,
                                      TranslatorLocation *location)
{
    char *directory;
    char *usr;
    char *root;
    char *resolved;
    struct stat statbuf;

    root = realpath(input_root, NULL);
    if (!root || stat(root, &statbuf) < 0 || !S_ISDIR(statbuf.st_mode)) {
        report_error("invalid root directory: %s", input_root);
        g_free(root);
        return false;
    }

    usr = path_join(root, "usr");
    directory = path_join(usr, "bin");
    location->root = root;
    location->directory_status = TRANSLATOR_DIRECTORY_KNOWN;

    resolved = realpath(directory, NULL);
    if (resolved) {
        if (stat(resolved, &statbuf) == 0 && S_ISDIR(statbuf.st_mode) &&
            path_is_within(root, resolved)) {
            location->directory = resolved;
        } else {
            location->directory = xstrdup(directory);
            location->directory_status = TRANSLATOR_DIRECTORY_UNKNOWN;
            g_free(resolved);
        }
        g_free(directory);
        g_free(usr);
        return true;
    }

    location->directory = directory;
    if (path_exists_without_following(directory)) {
        location->directory_status = TRANSLATOR_DIRECTORY_UNKNOWN;
        g_free(usr);
        return true;
    }

    if (stat(usr, &statbuf) == 0 && S_ISDIR(statbuf.st_mode) &&
        access(usr, X_OK) < 0) {
        location->directory_status = TRANSLATOR_DIRECTORY_UNKNOWN;
    }
    g_free(usr);

    return true;
}

static const char *translator_status_name(TranslatorStatus status)
{
    switch (status) {
    case TRANSLATOR_PRESENT:
        return "present";
    case TRANSLATOR_MISSING:
        return "missing";
    case TRANSLATOR_UNKNOWN:
        return "unknown";
    }
    abort();
}

static TranslatorStatus resolve_translator(const TranslatorLocation *location,
                                           const char *name,
                                           char **resolved_path)
{
    char *path;
    struct stat statbuf;
    char resolved[PATH_MAX + 1];

    if (location->directory_status == TRANSLATOR_DIRECTORY_UNKNOWN) {
        return TRANSLATOR_UNKNOWN;
    }

    path = path_join(location->directory, name);
    if (lstat(path, &statbuf) == 0 && S_ISLNK(statbuf.st_mode)) {
        if (!realpath(path, resolved)) {
            int resolve_errno = errno;

            if ((resolve_errno == ENOENT || resolve_errno == ENOTDIR) &&
                missing_symlink_is_confined(path, location->root)) {
                g_free(path);
                return TRANSLATOR_MISSING;
            }
            g_free(path);
            return TRANSLATOR_UNKNOWN;
        }
        if (location->root[0] &&
            !path_is_within(location->root, resolved)) {
            g_free(path);
            return TRANSLATOR_UNKNOWN;
        }
        g_free(path);
        path = xstrdup(resolved);
    }

    if (stat(path, &statbuf) == 0 && S_ISREG(statbuf.st_mode) &&
        access(path, X_OK) == 0) {
        if (resolved_path) {
            *resolved_path = path;
        } else {
            g_free(path);
        }
        return TRANSLATOR_PRESENT;
    }
    g_free(path);
    return TRANSLATOR_MISSING;
}

int main(int argc, char **argv)
{
    TranslatorLocation location = { 0 };
    bool located;
    TranslatorStatus status;

    if (strrchr(argv[0], '/')) {
        program_name = strrchr(argv[0], '/') + 1;
    } else if (argv[0][0]) {
        program_name = argv[0];
    }

    if (argc == 2 && !strcmp(argv[1], "status")) {
        located = locate_sibling_translators(argv[0], &location);
    } else if (argc == 4 && !strcmp(argv[1], "status") &&
               !strcmp(argv[2], "--root") && argv[3][0]) {
        located = locate_rooted_translators(argv[3], &location);
    } else {
        usage();
    }
    if (!located) {
        return 2;
    }

    status = resolve_translator(&location, "latx-x86_64", NULL);
    printf("translator_x86_64=%s\n", translator_status_name(status));
    status = resolve_translator(&location, "latx-i386", NULL);
    printf("translator_i386=%s\n", translator_status_name(status));

    g_free(location.directory);
    g_free(location.root);
    return 0;
}
