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
    fprintf(stderr,
            "usage: latu-runtime-manager status [--root ROOT]\n"
            "       latu-runtime-manager current [--abi ABI]"
            " [--program PROGRAM]\n"
            "       latu-runtime-manager list [--program PROGRAM]\n"
            "       latu-runtime-manager inspect-root [--abi ABI]"
            " [--] ROOT\n"
            "       latu-runtime-manager doctor [--abi ABI]"
            " [--program PROGRAM]\n");
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

static void string_buffer_reserve(StringBuffer *buffer, size_t extra)
{
    size_t required = buffer->length + extra + 1;

    if (required <= buffer->capacity) {
        return;
    }
    if (!buffer->capacity) {
        buffer->capacity = 128;
    }
    while (buffer->capacity < required) {
        if (buffer->capacity > SIZE_MAX / 2) {
            report_error("runtime information is too large");
            exit(2);
        }
        buffer->capacity *= 2;
    }
    buffer->data = g_realloc(buffer->data, buffer->capacity);
}

static void string_buffer_append_byte(StringBuffer *buffer, unsigned char byte)
{
    string_buffer_reserve(buffer, 1);
    buffer->data[buffer->length++] = byte;
    buffer->data[buffer->length] = '\0';
}

static bool string_buffer_append_codepoint(StringBuffer *buffer,
                                           uint32_t codepoint)
{
    if (!codepoint || codepoint > 0x10ffff ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
        return false;
    }
    if (codepoint <= 0x7f) {
        string_buffer_append_byte(buffer, codepoint);
    } else if (codepoint <= 0x7ff) {
        string_buffer_append_byte(buffer, 0xc0 | (codepoint >> 6));
        string_buffer_append_byte(buffer, 0x80 | (codepoint & 0x3f));
    } else if (codepoint <= 0xffff) {
        string_buffer_append_byte(buffer, 0xe0 | (codepoint >> 12));
        string_buffer_append_byte(buffer, 0x80 | ((codepoint >> 6) & 0x3f));
        string_buffer_append_byte(buffer, 0x80 | (codepoint & 0x3f));
    } else {
        string_buffer_append_byte(buffer, 0xf0 | (codepoint >> 18));
        string_buffer_append_byte(buffer, 0x80 | ((codepoint >> 12) & 0x3f));
        string_buffer_append_byte(buffer, 0x80 | ((codepoint >> 6) & 0x3f));
        string_buffer_append_byte(buffer, 0x80 | (codepoint & 0x3f));
    }
    return true;
}

static void json_skip_space(JsonParser *parser)
{
    while (parser->cursor < parser->end &&
           (*parser->cursor == ' ' || *parser->cursor == '\t' ||
            *parser->cursor == '\r' || *parser->cursor == '\n')) {
        parser->cursor++;
    }
}

static int json_hex_value(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

static bool json_parse_hex4(JsonParser *parser, uint32_t *value)
{
    uint32_t result = 0;

    if ((size_t)(parser->end - parser->cursor) < 4) {
        return false;
    }
    for (int i = 0; i < 4; i++) {
        int digit = json_hex_value(*parser->cursor++);

        if (digit < 0) {
            return false;
        }
        result = (result << 4) | digit;
    }
    *value = result;
    return true;
}

static char *json_parse_string(JsonParser *parser)
{
    StringBuffer result = { 0 };

    if (parser->cursor >= parser->end || *parser->cursor++ != '"') {
        return NULL;
    }

    while (parser->cursor < parser->end) {
        unsigned char byte = *parser->cursor++;

        if (byte == '"') {
            if (!result.data) {
                result.data = xstrdup("");
            }
            return result.data;
        }
        if (byte < 0x20) {
            break;
        }
        if (byte != '\\') {
            string_buffer_append_byte(&result, byte);
            continue;
        }
        if (parser->cursor >= parser->end) {
            break;
        }

        byte = *parser->cursor++;
        switch (byte) {
        case '"':
        case '\\':
        case '/':
            string_buffer_append_byte(&result, byte);
            break;
        case 'b':
            string_buffer_append_byte(&result, '\b');
            break;
        case 'f':
            string_buffer_append_byte(&result, '\f');
            break;
        case 'n':
            string_buffer_append_byte(&result, '\n');
            break;
        case 'r':
            string_buffer_append_byte(&result, '\r');
            break;
        case 't':
            string_buffer_append_byte(&result, '\t');
            break;
        case 'u': {
            uint32_t codepoint;

            if (!json_parse_hex4(parser, &codepoint)) {
                goto invalid;
            }
            if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                uint32_t low;

                if ((size_t)(parser->end - parser->cursor) < 6 ||
                    parser->cursor[0] != '\\' ||
                    parser->cursor[1] != 'u') {
                    goto invalid;
                }
                parser->cursor += 2;
                if (!json_parse_hex4(parser, &low) ||
                    low < 0xdc00 || low > 0xdfff) {
                    goto invalid;
                }
                codepoint = 0x10000 + ((codepoint - 0xd800) << 10) +
                            (low - 0xdc00);
            }
            if (!string_buffer_append_codepoint(&result, codepoint)) {
                goto invalid;
            }
            break;
        }
        default:
            goto invalid;
        }
    }

invalid:
    g_free(result.data);
    return NULL;
}

static bool json_parse_integer(JsonParser *parser, long *value)
{
    const char *start = parser->cursor;
    char *end;
    gint64 parsed;

    if (parser->cursor < parser->end && *parser->cursor == '-') {
        parser->cursor++;
    }
    if (parser->cursor >= parser->end ||
        *parser->cursor < '0' || *parser->cursor > '9') {
        parser->cursor = start;
        return false;
    }
    if (*parser->cursor == '0') {
        parser->cursor++;
    } else {
        while (parser->cursor < parser->end &&
               *parser->cursor >= '0' && *parser->cursor <= '9') {
            parser->cursor++;
        }
    }
    if (parser->cursor < parser->end &&
        (*parser->cursor == '.' || *parser->cursor == 'e' ||
         *parser->cursor == 'E')) {
        parser->cursor = start;
        return false;
    }

    errno = 0;
    parsed = g_ascii_strtoll(start, &end, 10);
    if (errno || end != parser->cursor || parsed < LONG_MIN ||
        parsed > LONG_MAX) {
        parser->cursor = start;
        return false;
    }
    *value = parsed;
    return true;
}

static bool runtime_source_valid(const char *source)
{
    return !strcmp(source, "default") ||
           !strcmp(source, "system_config") ||
           !strcmp(source, "user_config") ||
           !strcmp(source, "environment") ||
           !strcmp(source, "command_line");
}

static bool utf8_valid(const char *value)
{
    const unsigned char *cursor = (const unsigned char *)value;

    while (*cursor) {
        size_t length;
        uint32_t codepoint;

        if (*cursor < 0x80) {
            cursor++;
            continue;
        }
        if ((*cursor & 0xe0) == 0xc0) {
            length = 2;
            codepoint = *cursor & 0x1f;
        } else if ((*cursor & 0xf0) == 0xe0) {
            length = 3;
            codepoint = *cursor & 0x0f;
        } else if ((*cursor & 0xf8) == 0xf0) {
            length = 4;
            codepoint = *cursor & 0x07;
        } else {
            return false;
        }
        for (size_t i = 1; i < length; i++) {
            if ((cursor[i] & 0xc0) != 0x80) {
                return false;
            }
            codepoint = (codepoint << 6) | (cursor[i] & 0x3f);
        }
        if ((length == 2 && codepoint < 0x80) ||
            (length == 3 && codepoint < 0x800) ||
            (length == 4 && codepoint < 0x10000) ||
            codepoint > 0x10ffff ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            return false;
        }
        cursor += length;
    }
    return true;
}

static bool runtime_info_parse(const char *json, size_t json_length,
                               const char *expected_abi, RuntimeInfo *info)
{
    JsonParser parser = { json, json + json_length };
    bool have_schema = false;
    bool have_abi = false;
    bool have_root = false;
    bool have_source = false;

    json_skip_space(&parser);
    if (parser.cursor >= parser.end || *parser.cursor++ != '{') {
        return false;
    }

    for (;;) {
        char *key;

        json_skip_space(&parser);
        if (parser.cursor < parser.end && *parser.cursor == '}') {
            parser.cursor++;
            break;
        }
        key = json_parse_string(&parser);
        if (!key) {
            goto invalid;
        }
        json_skip_space(&parser);
        if (parser.cursor >= parser.end || *parser.cursor++ != ':') {
            g_free(key);
            goto invalid;
        }
        json_skip_space(&parser);

        if (!strcmp(key, "schema_version") && !have_schema) {
            have_schema = json_parse_integer(&parser,
                                             &info->schema_version);
            if (!have_schema) {
                g_free(key);
                goto invalid;
            }
        } else if (!strcmp(key, "guest_abi") && !have_abi) {
            info->guest_abi = json_parse_string(&parser);
            have_abi = info->guest_abi != NULL;
            if (!have_abi) {
                g_free(key);
                goto invalid;
            }
        } else if (!strcmp(key, "runtime_root") && !have_root) {
            info->runtime_root = json_parse_string(&parser);
            have_root = info->runtime_root != NULL;
            if (!have_root) {
                g_free(key);
                goto invalid;
            }
        } else if (!strcmp(key, "runtime_source") && !have_source) {
            info->runtime_source = json_parse_string(&parser);
            have_source = info->runtime_source != NULL;
            if (!have_source) {
                g_free(key);
                goto invalid;
            }
        } else {
            g_free(key);
            goto invalid;
        }
        g_free(key);

        json_skip_space(&parser);
        if (parser.cursor < parser.end && *parser.cursor == ',') {
            parser.cursor++;
            json_skip_space(&parser);
            if (parser.cursor < parser.end && *parser.cursor == '}') {
                goto invalid;
            }
            continue;
        }
        if (parser.cursor < parser.end && *parser.cursor == '}') {
            parser.cursor++;
            break;
        }
        goto invalid;
    }

    json_skip_space(&parser);
    if (parser.cursor != parser.end || !have_schema || !have_abi ||
        !have_root || !have_source || info->schema_version != 1 ||
        strcmp(info->guest_abi, expected_abi) ||
        !runtime_source_valid(info->runtime_source) ||
        !utf8_valid(info->guest_abi) || !utf8_valid(info->runtime_root) ||
        !utf8_valid(info->runtime_source)) {
        goto invalid;
    }
    return true;

invalid:
    g_free(info->guest_abi);
    g_free(info->runtime_root);
    g_free(info->runtime_source);
    memset(info, 0, sizeof(*info));
    return false;
}

static char *read_translator_info(const char *translator,
                                  const char *program, int *exit_status,
                                  size_t *output_length)
{
    StringBuffer output = { 0 };
    int pipefd[2];
    pid_t child;
    int status;
    bool overflow = false;
    bool read_failed = false;
    bool timed_out = false;
    struct timespec start_time;
    int64_t deadline = -1;

    if (pipe(pipefd) < 0) {
        return NULL;
    }
    child = fork();
    if (child < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }
    if (child == 0) {
        int nullfd;

        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }
        close(pipefd[1]);
        nullfd = open("/dev/null", O_WRONLY);
        if (nullfd >= 0) {
            dup2(nullfd, STDERR_FILENO);
            close(nullfd);
        }
        if (program) {
            char *const args[] = {
                (char *)translator, (char *)"--runtime-info", (char *)"--",
                (char *)program, NULL,
            };
            execv(translator, args);
        } else {
            char *const args[] = {
                (char *)translator, (char *)"--runtime-info", NULL,
            };
            execv(translator, args);
        }
        _exit(127);
    }

    close(pipefd[1]);
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) == 0) {
        deadline = (int64_t)start_time.tv_sec * 1000 +
                   start_time.tv_nsec / 1000000 +
                   RUNTIME_INFO_TIMEOUT_MS;
    }
    for (;;) {
        char chunk[4096];
        struct pollfd pollfd = {
            .fd = pipefd[0],
            .events = POLLIN,
        };
        int timeout = RUNTIME_INFO_TIMEOUT_MS;
        int poll_status;
        ssize_t length;

        if (deadline >= 0) {
            struct timespec now;

            if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
                int64_t remaining = deadline -
                    ((int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000);

                if (remaining <= 0) {
                    timed_out = true;
                    break;
                }
                timeout = remaining > INT_MAX ? INT_MAX : (int)remaining;
            }
        }
        do {
            poll_status = poll(&pollfd, 1, timeout);
        } while (poll_status < 0 && errno == EINTR);
        if (!poll_status) {
            timed_out = true;
            break;
        }
        if (poll_status < 0) {
            read_failed = true;
            break;
        }

        do {
            length = read(pipefd[0], chunk, sizeof(chunk));
        } while (length < 0 && errno == EINTR);

        if (length < 0) {
            read_failed = true;
            break;
        }
        if (!length) {
            break;
        }
        if (overflow) {
            continue;
        }
        if (output.length + (size_t)length > RUNTIME_INFO_LIMIT) {
            g_free(output.data);
            output.data = NULL;
            output.length = 0;
            output.capacity = 0;
            overflow = true;
            break;
        }
        string_buffer_reserve(&output, length);
        memcpy(output.data + output.length, chunk, length);
        output.length += length;
        output.data[output.length] = '\0';
    }
    if (overflow || read_failed || timed_out) {
        kill(child, SIGKILL);
    }
    close(pipefd[0]);

    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            g_free(output.data);
            return NULL;
        }
    }
    if (WIFEXITED(status)) {
        *exit_status = WEXITSTATUS(status);
    } else {
        *exit_status = 128;
    }
    if (overflow || read_failed || timed_out) {
        g_free(output.data);
    }
    if (overflow) {
        *output_length = 0;
        return xstrdup("");
    }
    if (read_failed || timed_out) {
        *output_length = 0;
        return NULL;
    }
    if (!output.data) {
        output.data = xstrdup("");
    }
    *output_length = output.length;
    return output.data;
}

static RuntimeQuery query_runtime(const TranslatorLocation *location,
                                  const char *guest_abi,
                                  const char *program)
{
    RuntimeQuery query = { .guest_abi = guest_abi };
    const char *name = !strcmp(guest_abi, "x86_64") ?
                       "latx-x86_64" : "latx-i386";
    char *translator = NULL;
    char *output;
    int exit_status;
    size_t output_length;

    query.translator_status = resolve_translator(location, name, &translator);
    if (query.translator_status == TRANSLATOR_MISSING) {
        query.query_status = QUERY_TRANSLATOR_MISSING;
        return query;
    }
    if (query.translator_status == TRANSLATOR_UNKNOWN) {
        query.query_status = QUERY_TRANSLATOR_UNKNOWN;
        return query;
    }

    output = read_translator_info(translator, program, &exit_status,
                                  &output_length);
    g_free(translator);
    if (!output || exit_status != 0) {
        g_free(output);
        query.query_status = QUERY_TRANSLATOR_FAILED;
        return query;
    }
    if (!runtime_info_parse(output, output_length, guest_abi, &query.info)) {
        g_free(output);
        query.query_status = QUERY_INVALID_OUTPUT;
        return query;
    }
    g_free(output);
    query.query_status = QUERY_SELECTED;
    return query;
}

static void runtime_info_free(RuntimeInfo *info)
{
    g_free(info->guest_abi);
    g_free(info->runtime_root);
    g_free(info->runtime_source);
    memset(info, 0, sizeof(*info));
}

static void json_write_string(const char *value)
{
    const unsigned char *cursor = (const unsigned char *)value;

    putchar('"');
    while (*cursor) {
        switch (*cursor) {
        case '"':
            fputs("\\\"", stdout);
            cursor++;
            break;
        case '\\':
            fputs("\\\\", stdout);
            cursor++;
            break;
        case '\b':
            fputs("\\b", stdout);
            cursor++;
            break;
        case '\f':
            fputs("\\f", stdout);
            cursor++;
            break;
        case '\n':
            fputs("\\n", stdout);
            cursor++;
            break;
        case '\r':
            fputs("\\r", stdout);
            cursor++;
            break;
        case '\t':
            fputs("\\t", stdout);
            cursor++;
            break;
        default:
            if (*cursor < 0x20) {
                printf("\\u%04x", *cursor++);
            } else if (*cursor < 0x80) {
                putchar(*cursor++);
            } else {
                size_t length;
                uint32_t codepoint;
                bool valid = true;

                if ((*cursor & 0xe0) == 0xc0) {
                    length = 2;
                    codepoint = *cursor & 0x1f;
                } else if ((*cursor & 0xf0) == 0xe0) {
                    length = 3;
                    codepoint = *cursor & 0x0f;
                } else if ((*cursor & 0xf8) == 0xf0) {
                    length = 4;
                    codepoint = *cursor & 0x07;
                } else {
                    fputs("\\ufffd", stdout);
                    cursor++;
                    break;
                }
                for (size_t i = 1; i < length; i++) {
                    if ((cursor[i] & 0xc0) != 0x80) {
                        valid = false;
                        break;
                    }
                    codepoint = (codepoint << 6) | (cursor[i] & 0x3f);
                }
                if ((length == 2 && codepoint < 0x80) ||
                    (length == 3 && codepoint < 0x800) ||
                    (length == 4 && codepoint < 0x10000) ||
                    codepoint > 0x10ffff ||
                    (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
                    valid = false;
                }
                if (!valid) {
                    fputs("\\ufffd", stdout);
                    cursor++;
                    break;
                }
                fwrite(cursor, 1, length, stdout);
                cursor += length;
            }
        }
    }
    putchar('"');
}

static const char *query_status_name(QueryStatus status)
{
    switch (status) {
    case QUERY_SELECTED:
        return "selected";
    case QUERY_TRANSLATOR_MISSING:
        return "translator_missing";
    case QUERY_TRANSLATOR_UNKNOWN:
        return "translator_unknown";
    case QUERY_TRANSLATOR_FAILED:
        return "translator_query_failed";
    case QUERY_INVALID_OUTPUT:
        return "invalid_runtime_info";
    }
    abort();
}

static void print_runtime_query(const RuntimeQuery *query)
{
    fputs("{\"schema_version\":1,\"guest_abi\":", stdout);
    json_write_string(query->guest_abi);
    fputs(",\"translator_status\":", stdout);
    json_write_string(translator_status_name(query->translator_status));
    fputs(",\"query_status\":", stdout);
    json_write_string(query_status_name(query->query_status));
    fputs(",\"runtime_root\":", stdout);
    if (query->query_status == QUERY_SELECTED) {
        json_write_string(query->info.runtime_root);
    } else {
        fputs("null", stdout);
    }
    fputs(",\"runtime_source\":", stdout);
    if (query->query_status == QUERY_SELECTED) {
        json_write_string(query->info.runtime_source);
    } else {
        fputs("null", stdout);
    }
    fputs("}\n", stdout);
}

static int query_exit_status(const RuntimeQuery *query)
{
    switch (query->query_status) {
    case QUERY_SELECTED:
        return 0;
    case QUERY_TRANSLATOR_MISSING:
        return 1;
    case QUERY_TRANSLATOR_UNKNOWN:
    case QUERY_TRANSLATOR_FAILED:
    case QUERY_INVALID_OUTPUT:
        return 2;
    }
    abort();
}

static bool guest_abi_valid(const char *guest_abi)
{
    return !strcmp(guest_abi, "x86_64") || !strcmp(guest_abi, "i386");
}

static const char *default_loader_path(const char *guest_abi)
{
    return !strcmp(guest_abi, "x86_64") ?
           "/lib64/ld-linux-x86-64.so.2" : "/lib/ld-linux.so.2";
}

static uint16_t read_le16(const unsigned char *value)
{
    return value[0] | ((uint16_t)value[1] << 8);
}

static uint32_t read_le32(const unsigned char *value)
{
    return value[0] | ((uint32_t)value[1] << 8) |
           ((uint32_t)value[2] << 16) | ((uint32_t)value[3] << 24);
}

static uint64_t read_le64(const unsigned char *value)
{
    return read_le32(value) | ((uint64_t)read_le32(value + 4) << 32);
}

static bool loader_elf_valid(int fd, const char *guest_abi)
{
    unsigned char header[64];
    uint64_t program_offset;
    uint16_t program_entry_size;
    uint16_t program_count;
    uint16_t machine;
    size_t header_size;
    unsigned char expected_class;
    bool have_load = false;
    ssize_t length;
    struct stat statbuf;

    do {
        length = pread(fd, header, sizeof(header), 0);
    } while (length < 0 && errno == EINTR);

    if (!strcmp(guest_abi, "x86_64")) {
        header_size = 64;
        expected_class = 2;
    } else {
        header_size = 52;
        expected_class = 1;
    }
    if (length < 0 || (size_t)length < header_size ||
        memcmp(header, "\177ELF", 4) || header[4] != expected_class ||
        header[5] != 1 || header[6] != 1) {
        return false;
    }
    machine = read_le16(header + 18);
    if ((read_le16(header + 16) != 2 && read_le16(header + 16) != 3) ||
        (expected_class == 2 ? machine != 62 :
         (machine != 3 && machine != 6))) {
        return false;
    }

    if (expected_class == 2) {
        program_offset = read_le64(header + 32);
        program_entry_size = read_le16(header + 54);
        program_count = read_le16(header + 56);
        if (read_le16(header + 52) != 64 || program_entry_size != 56) {
            return false;
        }
    } else {
        program_offset = read_le32(header + 28);
        program_entry_size = read_le16(header + 42);
        program_count = read_le16(header + 44);
        if (read_le16(header + 40) != 52 || program_entry_size != 32) {
            return false;
        }
    }
    if (!program_count || program_offset > INT64_MAX ||
        fstat(fd, &statbuf) < 0 || statbuf.st_size < 0 ||
        program_offset > (uint64_t)statbuf.st_size ||
        program_count > ((uint64_t)statbuf.st_size - program_offset) /
                        program_entry_size) {
        return false;
    }

    for (uint16_t i = 0; i < program_count; i++) {
        unsigned char program_type[4];
        uint64_t offset = program_offset +
                          (uint64_t)i * program_entry_size;

        if (offset > INT64_MAX) {
            return false;
        }
        do {
            length = pread(fd, program_type, sizeof(program_type), offset);
        } while (length < 0 && errno == EINTR);
        if (length != sizeof(program_type)) {
            return false;
        }
        if (read_le32(program_type) == 1) {
            have_load = true;
        }
    }
    return have_load;
}

static bool pread_exact(int fd, void *buffer, size_t size, uint64_t offset)
{
    ssize_t length;

    if (offset > INT64_MAX) {
        return false;
    }
    do {
        length = pread(fd, buffer, size, offset);
    } while (length < 0 && errno == EINTR);
    return length == size;
}

static bool inspect_program(const char *path, ProgramInfo *program,
                            const char **reason)
{
    unsigned char header[64];
    struct stat statbuf;
    uint64_t program_offset;
    uint16_t program_entry_size;
    uint16_t program_count;
    uint16_t machine;
    unsigned char elf_class;
    bool have_load = false;
    int fd;

    memset(program, 0, sizeof(*program));
    program->path = path;
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        *reason = errno == ENOENT || errno == ENOTDIR ?
                  "program_not_found" : "program_uninspectable";
        return false;
    }
    if (fstat(fd, &statbuf) < 0 || !S_ISREG(statbuf.st_mode) ||
        statbuf.st_size < 0 ||
        !pread_exact(fd, header, sizeof(header), 0) ||
        memcmp(header, "\177ELF", 4) || header[5] != 1 || header[6] != 1) {
        *reason = "program_invalid_elf";
        close(fd);
        return false;
    }

    elf_class = header[4];
    machine = read_le16(header + 18);
    if (elf_class == 2 && machine == 62) {
        program->guest_abi = "x86_64";
        program_offset = read_le64(header + 32);
        program_entry_size = read_le16(header + 54);
        program_count = read_le16(header + 56);
        if (read_le16(header + 52) != 64 || program_entry_size != 56) {
            goto invalid;
        }
    } else if (elf_class == 1 && (machine == 3 || machine == 6)) {
        program->guest_abi = "i386";
        program_offset = read_le32(header + 28);
        program_entry_size = read_le16(header + 42);
        program_count = read_le16(header + 44);
        if (read_le16(header + 40) != 52 || program_entry_size != 32) {
            goto invalid;
        }
    } else {
        goto invalid;
    }

    if ((read_le16(header + 16) != 2 && read_le16(header + 16) != 3) ||
        !program_count || program_offset > (uint64_t)statbuf.st_size ||
        program_count > ((uint64_t)statbuf.st_size - program_offset) /
                        program_entry_size) {
        goto invalid;
    }

    for (uint16_t i = 0; i < program_count; i++) {
        unsigned char program_header[56];
        uint64_t offset = program_offset +
                          (uint64_t)i * program_entry_size;
        uint32_t type;

        if (!pread_exact(fd, program_header, program_entry_size, offset)) {
            goto invalid;
        }
        type = read_le32(program_header);
        if (type == 1) {
            have_load = true;
        } else if (type == 3) {
            uint64_t interpreter_offset;
            uint64_t interpreter_size;
            char *interpreter;

            if (program->interpreter) {
                goto invalid;
            }
            if (elf_class == 2) {
                interpreter_offset = read_le64(program_header + 8);
                interpreter_size = read_le64(program_header + 32);
            } else {
                interpreter_offset = read_le32(program_header + 4);
                interpreter_size = read_le32(program_header + 16);
            }
            if (interpreter_size < 2 || interpreter_size > PATH_MAX ||
                interpreter_offset > (uint64_t)statbuf.st_size ||
                interpreter_size >
                    (uint64_t)statbuf.st_size - interpreter_offset) {
                goto invalid;
            }
            interpreter = xmalloc(interpreter_size);
            if (!pread_exact(fd, interpreter, interpreter_size,
                             interpreter_offset) ||
                interpreter[interpreter_size - 1] != '\0' ||
                memchr(interpreter, '\0', interpreter_size - 1)) {
                g_free(interpreter);
                goto invalid;
            }
            program->interpreter = interpreter;
        }
    }
    close(fd);
    if (!have_load) {
        goto invalid_closed;
    }
    program->is_static = !program->interpreter;
    return true;

invalid:
    close(fd);
invalid_closed:
    g_free(program->interpreter);
    memset(program, 0, sizeof(*program));
    *reason = "program_invalid_elf";
    return false;
}

static void program_info_free(ProgramInfo *program)
{
    g_free(program->interpreter);
    memset(program, 0, sizeof(*program));
}

static LoaderCheck inspect_loader_file(const char *guest_abi,
                                       const char *path,
                                       const char *confinement_root)
{
    LoaderCheck check = {
        .status = INSPECTION_UNKNOWN,
        .reason = "loader_uninspectable",
    };
    struct stat statbuf;
    bool entry_exists;
    int resolve_errno;
    int fd;

    check.resolved_path = realpath(path, NULL);
    if (!check.resolved_path) {
        resolve_errno = errno;
        entry_exists = path_exists_without_following(path);
        if (!entry_exists &&
            (resolve_errno == ENOENT || resolve_errno == ENOTDIR)) {
            check.status = INSPECTION_MISSING;
            check.reason = "loader_not_found";
        } else if (entry_exists &&
                   (resolve_errno == ENOENT || resolve_errno == ENOTDIR)) {
            check.status = INSPECTION_INVALID;
            check.reason = "loader_symlink_broken";
        }
        return check;
    }
    if (!utf8_valid(check.resolved_path)) {
        g_free(check.resolved_path);
        check.resolved_path = NULL;
        check.reason = "loader_path_not_utf8";
        return check;
    }
    if (confinement_root &&
        !path_is_within(confinement_root, check.resolved_path)) {
        check.reason = "loader_escapes_root";
        return check;
    }

    fd = open(check.resolved_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        check.status = errno == EACCES ?
                       INSPECTION_INVALID : INSPECTION_UNKNOWN;
        check.reason = errno == EACCES ?
                       "loader_permission_denied" : "loader_uninspectable";
        return check;
    }
    if (fstat(fd, &statbuf) < 0) {
        close(fd);
        return check;
    }
    if (!S_ISREG(statbuf.st_mode)) {
        close(fd);
        check.status = INSPECTION_INVALID;
        check.reason = "loader_not_regular";
        return check;
    }
    if (!loader_elf_valid(fd, guest_abi)) {
        close(fd);
        check.status = INSPECTION_INVALID;
        check.reason = "loader_invalid_elf";
        return check;
    }
    close(fd);
    check.status = INSPECTION_READY;
    check.reason = "ready";
    return check;
}

static RuntimeInspection inspect_runtime_root(const char *guest_abi,
                                              const char *input_root,
                                              const char *loader_path)
{
    RuntimeInspection inspection = {
        .guest_abi = guest_abi,
        .runtime_root = input_root,
        .loader_path = loader_path,
        .status = INSPECTION_UNKNOWN,
        .reason = "root_uninspectable",
        .effective_status = INSPECTION_UNKNOWN,
        .effective_reason = "not_checked",
    };
    g_autofree char *configured_root = NULL;
    const char *relative_loader = loader_path[0] == '/' ?
                                  loader_path + 1 : loader_path;
    LoaderCheck check;
    struct stat statbuf;

    if (!input_root[0]) {
        inspection.status = INSPECTION_MISSING;
        inspection.reason = "runtime_root_empty";
        return inspection;
    }
    configured_root = absolute_path(input_root);
    inspection.configured_loader = loader_path[0] == '/' ?
                                   path_join(configured_root,
                                             relative_loader) :
                                   xstrdup(loader_path);
    inspection.configured_loader_selected =
        loader_path[0] == '/' &&
        access(inspection.configured_loader, F_OK) == 0;

    inspection.canonical_root = realpath(input_root, NULL);
    if (!inspection.canonical_root) {
        if (errno == ENOENT || errno == ENOTDIR) {
            inspection.status = INSPECTION_MISSING;
            inspection.reason = "root_not_found";
        }
        return inspection;
    }
    if (!utf8_valid(inspection.canonical_root)) {
        g_free(inspection.canonical_root);
        inspection.canonical_root = NULL;
        inspection.status = INSPECTION_UNKNOWN;
        inspection.reason = "root_path_not_utf8";
        return inspection;
    }
    inspection.runtime_root = inspection.canonical_root;
    if (stat(inspection.canonical_root, &statbuf) < 0) {
        return inspection;
    }
    if (!S_ISDIR(statbuf.st_mode)) {
        inspection.status = INSPECTION_INVALID;
        inspection.reason = "root_not_directory";
        return inspection;
    }

    check = inspect_loader_file(guest_abi, inspection.configured_loader,
                                inspection.canonical_root);
    inspection.resolved_loader = check.resolved_path;
    inspection.status = check.status;
    inspection.reason = check.reason;
    return inspection;
}

static void resolve_effective_loader(RuntimeInspection *inspection)
{
    LoaderCheck check;

    if (inspection->status == INSPECTION_NOT_REQUIRED) {
        inspection->effective_status = INSPECTION_NOT_REQUIRED;
        inspection->effective_reason = "static_program";
        return;
    }
    if (inspection->configured_loader_selected) {
        inspection->loader_source = LOADER_SOURCE_RUNTIME_ROOT;
        inspection->effective_status = inspection->status;
        inspection->effective_reason = inspection->reason;
        inspection->effective_loader =
            xstrdup(inspection->resolved_loader ?
                    inspection->resolved_loader :
                    inspection->configured_loader);
        return;
    }

    inspection->loader_source = LOADER_SOURCE_HOST_FALLBACK;
    check = inspect_loader_file(inspection->guest_abi,
                                inspection->loader_path, NULL);
    inspection->effective_status = check.status;
    inspection->effective_reason = check.reason;
    inspection->effective_loader = check.resolved_path;
}

static const char *inspection_status_name(InspectionStatus status)
{
    switch (status) {
    case INSPECTION_READY:
        return "ready";
    case INSPECTION_NOT_REQUIRED:
        return "not_required";
    case INSPECTION_MISSING:
        return "missing";
    case INSPECTION_INVALID:
        return "invalid";
    case INSPECTION_UNKNOWN:
        return "unknown";
    }
    abort();
}

static int inspection_exit_status(const RuntimeInspection *inspection)
{
    switch (inspection->status) {
    case INSPECTION_READY:
    case INSPECTION_NOT_REQUIRED:
        return 0;
    case INSPECTION_MISSING:
    case INSPECTION_INVALID:
        return 1;
    case INSPECTION_UNKNOWN:
        return 2;
    }
    abort();
}

static void print_runtime_inspection(const RuntimeInspection *inspection)
{
    fputs("{\"schema_version\":1,\"guest_abi\":", stdout);
    json_write_string(inspection->guest_abi);
    fputs(",\"runtime_root\":", stdout);
    if (inspection->runtime_root) {
        json_write_string(inspection->runtime_root);
    } else {
        fputs("null", stdout);
    }
    fputs(",\"loader_path\":", stdout);
    json_write_string(inspection->loader_path);
    fputs(",\"resolved_loader\":", stdout);
    if (inspection->resolved_loader) {
        json_write_string(inspection->resolved_loader);
    } else {
        fputs("null", stdout);
    }
    fputs(",\"inspection_status\":", stdout);
    json_write_string(inspection_status_name(inspection->status));
    fputs(",\"reason\":", stdout);
    json_write_string(inspection->reason);
    fputs("}\n", stdout);
}

static void runtime_inspection_free(RuntimeInspection *inspection)
{
    g_free(inspection->canonical_root);
    g_free(inspection->configured_loader);
    g_free(inspection->resolved_loader);
    g_free(inspection->effective_loader);
    memset(inspection, 0, sizeof(*inspection));
}

static int inspect_root_command(int argc, char **argv)
{
    const char *guest_abi = NULL;
    const char *root = NULL;
    int status = 0;

    for (int i = 0; i < argc; ) {
        if (!strcmp(argv[i], "--abi") && i + 1 < argc && !guest_abi) {
            guest_abi = argv[i + 1];
            i += 2;
        } else if (!strcmp(argv[i], "--") && !root && i + 2 == argc) {
            root = argv[i + 1];
            i += 2;
        } else if (argv[i][0] != '-' && !root) {
            root = argv[i++];
        } else {
            usage();
        }
    }
    if (!root || !root[0] || (guest_abi && !guest_abi_valid(guest_abi))) {
        usage();
    }
    if (!utf8_valid(root)) {
        report_error("runtime root is not valid UTF-8");
        for (int i = 0; i < 2; i++) {
            const char *abi = i ? "i386" : "x86_64";
            RuntimeInspection inspection = {
                .guest_abi = abi,
                .runtime_root = NULL,
                .loader_path = i ? "/lib/ld-linux.so.2" :
                               "/lib64/ld-linux-x86-64.so.2",
                .status = INSPECTION_UNKNOWN,
                .reason = "root_path_not_utf8",
            };

            if (!guest_abi || !strcmp(guest_abi, abi)) {
                print_runtime_inspection(&inspection);
            }
        }
        return 2;
    }

    if (!guest_abi || !strcmp(guest_abi, "x86_64")) {
        RuntimeInspection inspection = inspect_runtime_root(
            "x86_64", root, default_loader_path("x86_64"));

        print_runtime_inspection(&inspection);
        status = inspection_exit_status(&inspection);
        runtime_inspection_free(&inspection);
    }
    if (!guest_abi || !strcmp(guest_abi, "i386")) {
        RuntimeInspection inspection = inspect_runtime_root(
            "i386", root, default_loader_path("i386"));
        int i386_status;

        print_runtime_inspection(&inspection);
        i386_status = inspection_exit_status(&inspection);
        if (i386_status > status) {
            status = i386_status;
        }
        runtime_inspection_free(&inspection);
    }
    return status;
}

static const char *readiness_status_name(ReadinessStatus status)
{
    switch (status) {
    case READINESS_READY:
        return "ready";
    case READINESS_READY_WITH_HOST_FALLBACK:
        return "ready_with_host_fallback";
    case READINESS_UNAVAILABLE:
        return "unavailable";
    case READINESS_BROKEN:
        return "broken";
    case READINESS_UNKNOWN:
        return "unknown";
    }
    abort();
}

static const char *loader_source_name(LoaderSource source)
{
    switch (source) {
    case LOADER_SOURCE_NONE:
        return "none";
    case LOADER_SOURCE_RUNTIME_ROOT:
        return "runtime_root";
    case LOADER_SOURCE_HOST_FALLBACK:
        return "host_fallback";
    }
    abort();
}

static ReadinessStatus doctor_readiness(const RuntimeQuery *query,
                                        const RuntimeInspection *inspection)
{
    switch (query->query_status) {
    case QUERY_TRANSLATOR_MISSING:
        return READINESS_UNAVAILABLE;
    case QUERY_TRANSLATOR_UNKNOWN:
    case QUERY_TRANSLATOR_FAILED:
    case QUERY_INVALID_OUTPUT:
        return READINESS_UNKNOWN;
    case QUERY_SELECTED:
        break;
    }

    switch (inspection->effective_status) {
    case INSPECTION_READY:
        return inspection->loader_source == LOADER_SOURCE_HOST_FALLBACK ?
               READINESS_READY_WITH_HOST_FALLBACK : READINESS_READY;
    case INSPECTION_NOT_REQUIRED:
        return READINESS_READY;
    case INSPECTION_MISSING:
    case INSPECTION_INVALID:
        return READINESS_BROKEN;
    case INSPECTION_UNKNOWN:
        return READINESS_UNKNOWN;
    }
    abort();
}

static void print_doctor_result(const RuntimeQuery *query,
                                const RuntimeInspection *inspection,
                                ReadinessStatus readiness,
                                const ProgramInfo *program)
{
    const char *loader_path = inspection ? inspection->loader_path :
                              default_loader_path(query->guest_abi);
    const char *reason;

    if (query->query_status != QUERY_SELECTED) {
        reason = query_status_name(query->query_status);
    } else if (inspection->status == INSPECTION_NOT_REQUIRED) {
        reason = "static_program";
    } else if (readiness == READINESS_READY_WITH_HOST_FALLBACK) {
        reason = "host_fallback";
    } else {
        reason = inspection->effective_reason;
    }

    fputs("{\"schema_version\":1,\"guest_abi\":", stdout);
    json_write_string(query->guest_abi);
    fputs(",\"translator_status\":", stdout);
    json_write_string(translator_status_name(query->translator_status));
    fputs(",\"query_status\":", stdout);
    json_write_string(query_status_name(query->query_status));
    fputs(",\"runtime_root\":", stdout);
    if (query->query_status == QUERY_SELECTED) {
        json_write_string(query->info.runtime_root);
    } else {
        fputs("null", stdout);
    }
    fputs(",\"runtime_source\":", stdout);
    if (query->query_status == QUERY_SELECTED) {
        json_write_string(query->info.runtime_source);
    } else {
        fputs("null", stdout);
    }
    fputs(",\"loader_path\":", stdout);
    if (loader_path) {
        json_write_string(loader_path);
    } else {
        fputs("null", stdout);
    }
    fputs(",\"configured_loader\":", stdout);
    if (inspection && inspection->configured_loader) {
        json_write_string(inspection->configured_loader);
    } else {
        fputs("null", stdout);
    }
    fputs(",\"resolved_loader\":", stdout);
    if (inspection && inspection->resolved_loader) {
        json_write_string(inspection->resolved_loader);
    } else {
        fputs("null", stdout);
    }
    fputs(",\"inspection_status\":", stdout);
    if (inspection) {
        json_write_string(inspection_status_name(inspection->status));
    } else {
        json_write_string("not_checked");
    }
    fputs(",\"readiness\":", stdout);
    json_write_string(readiness_status_name(readiness));
    fputs(",\"reason\":", stdout);
    json_write_string(reason);
    fputs(",\"runtime_root_status\":", stdout);
    if (inspection) {
        json_write_string(inspection_status_name(inspection->status));
    } else {
        json_write_string("not_checked");
    }
    fputs(",\"runtime_root_reason\":", stdout);
    if (inspection) {
        json_write_string(inspection->reason);
    } else {
        json_write_string("not_checked");
    }
    fputs(",\"effective_loader\":", stdout);
    if (inspection && inspection->effective_loader) {
        json_write_string(inspection->effective_loader);
    } else {
        fputs("null", stdout);
    }
    fputs(",\"effective_loader_status\":", stdout);
    if (inspection) {
        json_write_string(inspection_status_name(
            inspection->effective_status));
    } else {
        json_write_string("not_checked");
    }
    fputs(",\"loader_source\":", stdout);
    json_write_string(inspection ?
                      loader_source_name(inspection->loader_source) : "none");
    fputs(",\"program\":", stdout);
    if (program) {
        json_write_string(program->path);
    } else {
        fputs("null", stdout);
    }
    fputs(",\"program_status\":", stdout);
    json_write_string(program ?
                      (program->is_static ? "static" : "dynamic") :
                      "not_checked");
    fputs(",\"program_reason\":null", stdout);
    fputs("}\n", stdout);
}

static void print_program_failure(const char *program, const char *reason)
{
    fputs("{\"schema_version\":1,\"guest_abi\":null,"
          "\"translator_status\":\"not_checked\","
          "\"query_status\":\"not_checked\",\"runtime_root\":null,"
          "\"runtime_source\":null,\"loader_path\":null,"
          "\"configured_loader\":null,\"resolved_loader\":null,"
          "\"inspection_status\":\"not_checked\","
          "\"readiness\":\"unknown\",\"reason\":", stdout);
    json_write_string(reason);
    fputs(",\"runtime_root_status\":\"not_checked\","
          "\"runtime_root_reason\":\"not_checked\","
          "\"effective_loader\":null,"
          "\"effective_loader_status\":\"not_checked\","
          "\"loader_source\":\"none\",\"program\":", stdout);
    json_write_string(program);
    fputs(",\"program_status\":\"invalid\",\"program_reason\":", stdout);
    json_write_string(reason);
    fputs("}\n", stdout);
}

static ReadinessStatus doctor_one(const TranslatorLocation *location,
                                  const char *guest_abi,
                                  const ProgramInfo *program)
{
    RuntimeQuery query = query_runtime(location, guest_abi,
                                       program ? program->path : NULL);
    RuntimeInspection inspection;
    RuntimeInspection *inspection_ptr = NULL;
    ReadinessStatus readiness;

    if (query.query_status == QUERY_SELECTED) {
        if (program && program->is_static) {
            inspection = (RuntimeInspection) {
                .guest_abi = guest_abi,
                .runtime_root = query.info.runtime_root,
                .status = INSPECTION_NOT_REQUIRED,
                .reason = "static_program",
                .effective_status = INSPECTION_NOT_REQUIRED,
                .effective_reason = "static_program",
            };
        } else {
            inspection = inspect_runtime_root(
                guest_abi, query.info.runtime_root,
                program ? program->interpreter :
                default_loader_path(guest_abi));
            resolve_effective_loader(&inspection);
        }
        inspection_ptr = &inspection;
    }
    readiness = doctor_readiness(&query, inspection_ptr);
    print_doctor_result(&query, inspection_ptr, readiness, program);

    if (inspection_ptr) {
        runtime_inspection_free(inspection_ptr);
    }
    runtime_info_free(&query.info);
    return readiness;
}

static int doctor_command(const TranslatorLocation *location, int argc,
                          char **argv)
{
    const char *guest_abi = NULL;
    const char *program_path = NULL;
    const char *program_reason = NULL;
    ProgramInfo program = { 0 };
    ProgramInfo *program_ptr = NULL;
    bool any_ready = false;
    bool any_broken = false;
    bool any_unknown = false;
    int status;

    for (int i = 0; i < argc; ) {
        if (!strcmp(argv[i], "--abi") && i + 1 < argc && !guest_abi) {
            guest_abi = argv[i + 1];
        } else if (!strcmp(argv[i], "--program") && i + 1 < argc &&
                   !program_path && argv[i + 1][0]) {
            program_path = argv[i + 1];
        } else {
            usage();
        }
        i += 2;
    }
    if (guest_abi && !guest_abi_valid(guest_abi)) {
        usage();
    }
    if (program_path) {
        if (!inspect_program(program_path, &program, &program_reason)) {
            print_program_failure(program_path, program_reason);
            report_error("cannot inspect program '%s': %s",
                         program_path, program_reason);
            return 2;
        }
        program_ptr = &program;
        if (guest_abi && strcmp(guest_abi, program.guest_abi)) {
            report_error("program ABI %s does not match requested ABI %s",
                         program.guest_abi, guest_abi);
            program_info_free(&program);
            return 2;
        }
        guest_abi = program.guest_abi;
    }

    for (int i = 0; i < 2; i++) {
        const char *abi = i ? "i386" : "x86_64";
        ReadinessStatus readiness;

        if (guest_abi && strcmp(guest_abi, abi)) {
            continue;
        }
        readiness = doctor_one(location, abi, program_ptr);
        switch (readiness) {
        case READINESS_READY:
        case READINESS_READY_WITH_HOST_FALLBACK:
            any_ready = true;
            break;
        case READINESS_UNAVAILABLE:
            break;
        case READINESS_BROKEN:
            any_broken = true;
            break;
        case READINESS_UNKNOWN:
            any_unknown = true;
            break;
        }
    }

    status = any_unknown ? 2 : any_broken || !any_ready ? 1 : 0;
    program_info_free(&program);
    return status;
}

static int query_both_runtimes(const TranslatorLocation *location,
                               const char *program)
{
    RuntimeQuery x86_64_query = query_runtime(location, "x86_64", program);
    RuntimeQuery i386_query = query_runtime(location, "i386", program);
    int x86_64_status;
    int i386_status;
    int status;

    print_runtime_query(&x86_64_query);
    print_runtime_query(&i386_query);
    x86_64_status = query_exit_status(&x86_64_query);
    i386_status = query_exit_status(&i386_query);
    status = x86_64_status == 2 || i386_status == 2 ? 2 :
             x86_64_status == 0 || i386_status == 0 ? 0 : 1;
    runtime_info_free(&x86_64_query.info);
    runtime_info_free(&i386_query.info);
    return status;
}

static int current_command(const TranslatorLocation *location, int argc,
                           char **argv)
{
    const char *guest_abi = NULL;
    const char *program = NULL;
    RuntimeQuery query;
    int status;

    for (int i = 0; i < argc; ) {
        if (!strcmp(argv[i], "--abi") && i + 1 < argc && !guest_abi) {
            guest_abi = argv[i + 1];
        } else if (!strcmp(argv[i], "--program") && i + 1 < argc &&
                   !program && argv[i + 1][0]) {
            program = argv[i + 1];
        } else {
            usage();
        }
        i += 2;
    }
    if (guest_abi && !guest_abi_valid(guest_abi)) {
        usage();
    }

    if (!guest_abi) {
        return query_both_runtimes(location, program);
    }

    query = query_runtime(location, guest_abi, program);
    print_runtime_query(&query);
    status = query_exit_status(&query);
    runtime_info_free(&query.info);
    return status;
}

static int list_command(const TranslatorLocation *location, int argc,
                        char **argv)
{
    const char *program = NULL;
    if (argc == 2 && !strcmp(argv[0], "--program") && argv[1][0]) {
        program = argv[1];
    } else if (argc) {
        usage();
    }

    return query_both_runtimes(location, program);
}

int main(int argc, char **argv)
{
    TranslatorLocation location = { 0 };
    bool located = false;
    int status;

    if (strrchr(argv[0], '/')) {
        program_name = strrchr(argv[0], '/') + 1;
    } else if (argv[0][0]) {
        program_name = argv[0];
    }

    if (argc >= 2 && !strcmp(argv[1], "inspect-root")) {
        return inspect_root_command(argc - 2, argv + 2);
    }

    if (argc >= 2 &&
        (!strcmp(argv[1], "current") || !strcmp(argv[1], "list") ||
         !strcmp(argv[1], "doctor"))) {
        if (!locate_sibling_translators(argv[0], &location)) {
            return 2;
        }

        if (!strcmp(argv[1], "current")) {
            status = current_command(&location, argc - 2, argv + 2);
        } else if (!strcmp(argv[1], "list")) {
            status = list_command(&location, argc - 2, argv + 2);
        } else {
            status = doctor_command(&location, argc - 2, argv + 2);
        }
        g_free(location.directory);
        g_free(location.root);
        return status;
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

    printf("translator_x86_64=%s\n",
           translator_status_name(resolve_translator(
               &location, "latx-x86_64", NULL)));
    printf("translator_i386=%s\n",
           translator_status_name(resolve_translator(
               &location, "latx-i386", NULL)));

    g_free(location.directory);
    g_free(location.root);
    return 0;
}
