#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "precompile.h"
#include "latcd-protocol.h"

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#if GLIB_CHECK_VERSION(2, 64, 0)
#define LATCD_SPAWN_CHECK_EXIT_STATUS g_spawn_check_wait_status
#else
#define LATCD_SPAWN_CHECK_EXIT_STATUS g_spawn_check_exit_status
#endif

#define CACHE_NEW_MAGIC "glibc-ld.so.cache"
#define CACHE_NEW_VERSION "1.1"
#define CACHE_NEW_HEADER_SIZE 48u
#define CACHE_NEW_ENTRY_SIZE 24u
#define PRECOMPILE_MAX_FILE (UINT64_C(1) << 30)
#define PRECOMPILE_MAX_SYMLINKS 40u

typedef struct ElfDependencies {
    char *interpreter;
    GPtrArray *needed;
    GPtrArray *rpath;
    GPtrArray *runpath;
} ElfDependencies;

typedef struct AnalyzeContext {
    const char *compiler;
    const char *temporary_dir;
} AnalyzeContext;

static int fail(char *error, size_t error_size, const char *format, ...)
{
    if (error && error_size) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(error, error_size, format, arguments);
        va_end(arguments);
    }
    return -1;
}

static uint32_t read_u32(const unsigned char *data)
{
    uint32_t value;
    memcpy(&value, data, sizeof(value));
    return GUINT32_FROM_LE(value);
}

static char *root_join(const char *rootfs, const char *guest_path)
{
    return g_build_filename(rootfs, guest_path + 1, NULL);
}

static char *guest_normalize(const char *path)
{
    if (!path || path[0] != '/') {
        return NULL;
    }
    return g_canonicalize_filename(path, "/");
}

static int resolve_guest_path(const char *rootfs, const char *input,
                              char **guest_result, char **host_result,
                              char *error, size_t error_size)
{
    char *pending = guest_normalize(input);
    if (!pending) {
        return fail(error, error_size, "guest path must be absolute: %s",
                    input ? input : "(null)");
    }
    for (unsigned int link_count = 0;
         link_count <= PRECOMPILE_MAX_SYMLINKS; link_count++) {
        gchar **parts = g_strsplit(pending, "/", -1);
        GString *resolved = g_string_new("");
        bool restarted = false;
        for (size_t i = 0; parts[i]; i++) {
            if (!parts[i][0]) {
                continue;
            }
            g_string_append_c(resolved, '/');
            g_string_append(resolved, parts[i]);
            char *host = root_join(rootfs, resolved->str);
            struct stat status;
            if (lstat(host, &status)) {
                fail(error, error_size, "%s: %s", resolved->str,
                     strerror(errno));
                g_free(host);
                g_string_free(resolved, TRUE);
                g_strfreev(parts);
                g_free(pending);
                return -1;
            }
            if (S_ISLNK(status.st_mode)) {
                if (link_count == PRECOMPILE_MAX_SYMLINKS) {
                    fail(error, error_size, "%s: too many symbolic links",
                         resolved->str);
                    g_free(host);
                    g_string_free(resolved, TRUE);
                    g_strfreev(parts);
                    g_free(pending);
                    return -1;
                }
                char target[PATH_MAX + 1];
                ssize_t length = readlink(host, target, PATH_MAX);
                g_free(host);
                if (length < 0 || length == PATH_MAX) {
                    fail(error, error_size, "%s: cannot read symbolic link",
                         resolved->str);
                    g_string_free(resolved, TRUE);
                    g_strfreev(parts);
                    g_free(pending);
                    return -1;
                }
                target[length] = '\0';
                char *parent = g_path_get_dirname(resolved->str);
                GString *replacement = g_string_new(
                    target[0] == '/' ? target : parent);
                if (target[0] != '/') {
                    if (replacement->len > 1) g_string_append_c(replacement, '/');
                    g_string_append(replacement, target);
                }
                for (size_t j = i + 1; parts[j]; j++) {
                    if (parts[j][0]) {
                        g_string_append_c(replacement, '/');
                        g_string_append(replacement, parts[j]);
                    }
                }
                char *normalized = guest_normalize(replacement->str);
                g_string_free(replacement, TRUE);
                g_free(parent);
                g_free(pending);
                pending = normalized;
                g_string_free(resolved, TRUE);
                g_strfreev(parts);
                restarted = true;
                break;
            }
            g_free(host);
        }
        if (restarted) {
            continue;
        }
        if (!resolved->len) {
            g_string_assign(resolved, "/");
        }
        char *host = root_join(rootfs, resolved->str);
        struct stat status;
        if (stat(host, &status) || !S_ISREG(status.st_mode)) {
            fail(error, error_size, "%s: not a regular file", resolved->str);
            g_free(host);
            g_string_free(resolved, TRUE);
            g_strfreev(parts);
            g_free(pending);
            return -1;
        }
        *guest_result = g_string_free(resolved, FALSE);
        *host_result = host;
        g_strfreev(parts);
        g_free(pending);
        return 0;
    }
    g_free(pending);
    return fail(error, error_size, "cannot resolve guest path");
}

static void elf_dependencies_clear(ElfDependencies *dependencies)
{
    if (!dependencies) return;
    g_free(dependencies->interpreter);
    if (dependencies->needed) g_ptr_array_free(dependencies->needed, TRUE);
    if (dependencies->rpath) g_ptr_array_free(dependencies->rpath, TRUE);
    if (dependencies->runpath) g_ptr_array_free(dependencies->runpath, TRUE);
    memset(dependencies, 0, sizeof(*dependencies));
}

static char *elf_string(const unsigned char *data, size_t size,
                        uint64_t table, uint64_t table_size, uint64_t offset)
{
    if (table > size || table_size > size - table || offset >= table_size) {
        return NULL;
    }
    const unsigned char *start = data + table + offset;
    size_t remaining = (size_t)(table_size - offset);
    const unsigned char *end = memchr(start, '\0', remaining);
    return end ? g_strndup((const char *)start, end - start) : NULL;
}

static void append_paths(GPtrArray *array, const char *text)
{
    gchar **parts = g_strsplit(text, ":", -1);
    for (size_t i = 0; parts[i]; i++) {
        if (parts[i][0]) g_ptr_array_add(array, g_strdup(parts[i]));
    }
    g_strfreev(parts);
}

static int parse_elf_dependencies(const char *path, ElfDependencies *result,
                                  char digest[65], char *error,
                                  size_t error_size)
{
    memset(result, 0, sizeof(*result));
    result->needed = g_ptr_array_new_with_free_func(g_free);
    result->rpath = g_ptr_array_new_with_free_func(g_free);
    result->runpath = g_ptr_array_new_with_free_func(g_free);
    gchar *contents = NULL;
    gsize size = 0;
    if (!g_file_get_contents(path, &contents, &size, NULL) ||
        size < sizeof(Elf64_Ehdr) || size > PRECOMPILE_MAX_FILE) {
        elf_dependencies_clear(result);
        return fail(error, error_size, "%s: cannot read ELF", path);
    }
    const unsigned char *data = (const void *)contents;
    Elf64_Ehdr header;
    memcpy(&header, data, sizeof(header));
    if (memcmp(header.e_ident, ELFMAG, SELFMAG) ||
        header.e_ident[EI_CLASS] != ELFCLASS64 ||
        header.e_ident[EI_DATA] != ELFDATA2LSB ||
        header.e_machine != EM_X86_64 ||
        header.e_phentsize != sizeof(Elf64_Phdr) || !header.e_phnum ||
        header.e_phoff > size ||
        header.e_phnum > (size - header.e_phoff) / sizeof(Elf64_Phdr)) {
        g_free(contents);
        elf_dependencies_clear(result);
        return fail(error, error_size, "%s: invalid x86-64 ELF", path);
    }
    const char *checksum = g_compute_checksum_for_data(
        G_CHECKSUM_SHA256, data, size);
    g_strlcpy(digest, checksum, 65);
    g_free((gpointer)checksum);
    Elf64_Phdr dynamic = {0};
    bool have_dynamic = false;
    GArray *loads = g_array_new(FALSE, FALSE, sizeof(Elf64_Phdr));
    for (uint16_t i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr phdr;
        memcpy(&phdr, data + header.e_phoff + i * sizeof(phdr), sizeof(phdr));
        if (phdr.p_offset > size || phdr.p_filesz > size - phdr.p_offset) {
            g_array_free(loads, TRUE);
            g_free(contents);
            elf_dependencies_clear(result);
            return fail(error, error_size, "%s: invalid ELF segment", path);
        }
        if (phdr.p_type == PT_LOAD) {
            g_array_append_val(loads, phdr);
        } else if (phdr.p_type == PT_DYNAMIC) {
            dynamic = phdr;
            have_dynamic = true;
        } else if (phdr.p_type == PT_INTERP && phdr.p_filesz) {
            result->interpreter = elf_string(data, size, phdr.p_offset,
                                             phdr.p_filesz, 0);
            if (!result->interpreter) {
                g_array_free(loads, TRUE);
                g_free(contents);
                elf_dependencies_clear(result);
                return fail(error, error_size, "%s: invalid PT_INTERP", path);
            }
        }
    }
    if (!have_dynamic) {
        g_array_free(loads, TRUE);
        g_free(contents);
        return 0;
    }
    GArray *dynamic_entries = g_array_new(FALSE, FALSE, sizeof(Elf64_Dyn));
    for (uint64_t offset = 0; offset + sizeof(Elf64_Dyn) <= dynamic.p_filesz;
         offset += sizeof(Elf64_Dyn)) {
        Elf64_Dyn entry;
        memcpy(&entry, data + dynamic.p_offset + offset, sizeof(entry));
        if (entry.d_tag == DT_NULL) break;
        g_array_append_val(dynamic_entries, entry);
    }
    uint64_t string_vaddr = UINT64_MAX, string_size = 0;
    for (guint i = 0; i < dynamic_entries->len; i++) {
        Elf64_Dyn entry = g_array_index(dynamic_entries, Elf64_Dyn, i);
        if (entry.d_tag == DT_STRTAB) string_vaddr = entry.d_un.d_ptr;
        if (entry.d_tag == DT_STRSZ) string_size = entry.d_un.d_val;
    }
    uint64_t string_offset = UINT64_MAX;
    for (guint i = 0; i < loads->len && string_vaddr != UINT64_MAX; i++) {
        Elf64_Phdr phdr = g_array_index(loads, Elf64_Phdr, i);
        if (string_vaddr >= phdr.p_vaddr &&
            string_vaddr - phdr.p_vaddr < phdr.p_filesz) {
            string_offset = phdr.p_offset + string_vaddr - phdr.p_vaddr;
            break;
        }
    }
    if (string_offset == UINT64_MAX || string_offset > size ||
        string_size > size - string_offset) {
        g_array_free(dynamic_entries, TRUE);
        g_array_free(loads, TRUE);
        g_free(contents);
        elf_dependencies_clear(result);
        return fail(error, error_size, "%s: invalid dynamic string table",
                    path);
    }
    for (guint i = 0; i < dynamic_entries->len; i++) {
        Elf64_Dyn entry = g_array_index(dynamic_entries, Elf64_Dyn, i);
        if (entry.d_tag != DT_NEEDED && entry.d_tag != DT_RPATH &&
            entry.d_tag != DT_RUNPATH) continue;
        char *value = elf_string(data, size, string_offset, string_size,
                                 entry.d_un.d_val);
        if (!value) {
            g_array_free(dynamic_entries, TRUE);
            g_array_free(loads, TRUE);
            g_free(contents);
            elf_dependencies_clear(result);
            return fail(error, error_size, "%s: invalid dynamic string", path);
        }
        if (entry.d_tag == DT_NEEDED) {
            g_ptr_array_add(result->needed, value);
        } else {
            append_paths(entry.d_tag == DT_RPATH ? result->rpath :
                         result->runpath, value);
            g_free(value);
        }
    }
    g_array_free(dynamic_entries, TRUE);
    g_array_free(loads, TRUE);
    g_free(contents);
    return 0;
}

static void path_array_free(gpointer pointer)
{
    g_ptr_array_free(pointer, TRUE);
}

static GHashTable *load_ld_cache(const char *rootfs, char *error,
                                 size_t error_size)
{
    GHashTable *cache = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, path_array_free);
    char *path = g_build_filename(rootfs, "etc", "ld.so.cache", NULL);
    gchar *contents = NULL;
    gsize size = 0;
    if (!g_file_get_contents(path, &contents, &size, NULL)) {
        g_free(path);
        return cache;
    }
    g_free(path);
    const unsigned char *data = (const void *)contents;
    size_t magic_size = strlen(CACHE_NEW_MAGIC);
    size_t version_size = strlen(CACHE_NEW_VERSION);
    if (size < CACHE_NEW_HEADER_SIZE ||
        memcmp(data, CACHE_NEW_MAGIC, magic_size) ||
        memcmp(data + magic_size, CACHE_NEW_VERSION, version_size)) {
        g_free(contents);
        g_hash_table_destroy(cache);
        fail(error, error_size, "unsupported /etc/ld.so.cache format");
        return NULL;
    }
    uint32_t count = read_u32(data + 20);
    if (count > (size - CACHE_NEW_HEADER_SIZE) / CACHE_NEW_ENTRY_SIZE) {
        g_free(contents);
        g_hash_table_destroy(cache);
        fail(error, error_size, "invalid /etc/ld.so.cache entries");
        return NULL;
    }
    for (uint32_t i = 0; i < count; i++) {
        const unsigned char *entry = data + CACHE_NEW_HEADER_SIZE +
                                     i * CACHE_NEW_ENTRY_SIZE;
        uint32_t key_offset = read_u32(entry + 4);
        uint32_t value_offset = read_u32(entry + 8);
        if (key_offset >= size || value_offset >= size) continue;
        const char *key = (const char *)data + key_offset;
        const char *value = (const char *)data + value_offset;
        if (!memchr(key, '\0', size - key_offset) ||
            !memchr(value, '\0', size - value_offset) || value[0] != '/') {
            continue;
        }
        GPtrArray *paths = g_hash_table_lookup(cache, key);
        if (!paths) {
            paths = g_ptr_array_new_with_free_func(g_free);
            g_hash_table_insert(cache, g_strdup(key), paths);
        }
        g_ptr_array_add(paths, g_strdup(value));
    }
    g_free(contents);
    return cache;
}

static char *replace_token(char *value, const char *token,
                           const char *replacement)
{
    char *position;
    while ((position = strstr(value, token))) {
        GString *result = g_string_new_len(value, position - value);
        g_string_append(result, replacement);
        g_string_append(result, position + strlen(token));
        g_free(value);
        value = g_string_free(result, FALSE);
    }
    return value;
}

static char *expand_search_path(const char *path, const char *origin)
{
    char *expanded = g_strdup(path);
    expanded = replace_token(expanded, "${ORIGIN}", origin);
    expanded = replace_token(expanded, "$ORIGIN", origin);
    expanded = replace_token(expanded, "${LIB}", "lib64");
    expanded = replace_token(expanded, "$LIB", "lib64");
    expanded = replace_token(expanded, "${PLATFORM}", "x86_64");
    expanded = replace_token(expanded, "$PLATFORM", "x86_64");
    char *absolute = expanded[0] == '/' ? g_strdup(expanded) :
                     g_build_filename(origin, expanded, NULL);
    g_free(expanded);
    char *normalized = guest_normalize(absolute);
    g_free(absolute);
    return normalized;
}

static int dependency_candidate(const char *rootfs, const char *candidate,
                                char **guest, char **host)
{
    char ignored[1];
    return resolve_guest_path(rootfs, candidate, guest, host,
                              ignored, sizeof(ignored));
}

static int resolve_needed(const char *rootfs, GHashTable *cache,
                          const char *origin, const ElfDependencies *elf,
                          const char *name, char **guest, char **host,
                          char *error, size_t error_size)
{
    if (strchr(name, '/')) {
        char *candidate = name[0] == '/' ? g_strdup(name) :
                          g_build_filename(origin, name, NULL);
        int result = dependency_candidate(rootfs, candidate, guest, host);
        g_free(candidate);
        if (!result) return 0;
        return fail(error, error_size, "cannot resolve DT_NEEDED %s", name);
    }
    GPtrArray *search = elf->runpath->len ? elf->runpath : elf->rpath;
    for (guint i = 0; i < search->len; i++) {
        char *directory = expand_search_path(g_ptr_array_index(search, i),
                                             origin);
        char *candidate = g_build_filename(directory, name, NULL);
        g_free(directory);
        int result = dependency_candidate(rootfs, candidate, guest, host);
        g_free(candidate);
        if (!result) return 0;
    }
    GPtrArray *cached = g_hash_table_lookup(cache, name);
    for (guint i = 0; cached && i < cached->len; i++) {
        if (!dependency_candidate(rootfs, g_ptr_array_index(cached, i),
                                  guest, host)) return 0;
    }
    static const char *const defaults[] = {
        "/lib64", "/usr/lib64", "/lib/x86_64-linux-gnu",
        "/usr/lib/x86_64-linux-gnu", "/lib", "/usr/lib",
    };
    for (size_t i = 0; i < G_N_ELEMENTS(defaults); i++) {
        char *candidate = g_build_filename(defaults[i], name, NULL);
        int result = dependency_candidate(rootfs, candidate, guest, host);
        g_free(candidate);
        if (!result) return 0;
    }
    return fail(error, error_size, "cannot resolve DT_NEEDED %s", name);
}

void latcd_precompile_dependency_free(gpointer pointer)
{
    LatcdPrecompileDependency *dependency = pointer;
    if (!dependency) return;
    g_free(dependency->guest_path);
    g_free(dependency->host_path);
    g_free(dependency->tbset_path);
    g_free(dependency->analysis_error);
    g_free(dependency);
}

int latcd_precompile_collect(const char *rootfs_input, const char *guest_path,
                             GPtrArray **dependencies,
                             char *error, size_t error_size)
{
    if (!rootfs_input || !guest_path || !dependencies) {
        return fail(error, error_size, "invalid precompile arguments");
    }
    char *rootfs = realpath(rootfs_input, NULL);
    if (!rootfs) {
        return fail(error, error_size, "cannot resolve rootfs: %s",
                    strerror(errno));
    }
    struct stat root_status;
    if (stat(rootfs, &root_status) || !S_ISDIR(root_status.st_mode)) {
        g_free(rootfs);
        return fail(error, error_size, "x86 rootfs is not a directory");
    }
    GHashTable *cache = load_ld_cache(rootfs, error, error_size);
    if (!cache) {
        g_free(rootfs);
        return -1;
    }
    GPtrArray *result = g_ptr_array_new_with_free_func(
        latcd_precompile_dependency_free);
    GQueue queue = G_QUEUE_INIT;
    g_queue_push_tail(&queue, g_strdup(guest_path));
    GHashTable *seen_paths = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, NULL);
    GHashTable *seen_digests = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, NULL);
    int status = 0;
    while (!g_queue_is_empty(&queue)) {
        char *queued = g_queue_pop_head(&queue);
        char *guest = NULL, *host = NULL;
        if (resolve_guest_path(rootfs, queued, &guest, &host,
                               error, error_size)) {
            g_free(queued);
            status = -1;
            break;
        }
        g_free(queued);
        if (g_hash_table_contains(seen_paths, guest)) {
            g_free(guest);
            g_free(host);
            continue;
        }
        g_hash_table_add(seen_paths, g_strdup(guest));
        ElfDependencies elf;
        char digest[65];
        if (parse_elf_dependencies(host, &elf, digest,
                                   error, error_size)) {
            g_free(guest);
            g_free(host);
            status = -1;
            break;
        }
        if (g_hash_table_contains(seen_digests, digest)) {
            elf_dependencies_clear(&elf);
            g_free(guest);
            g_free(host);
            continue;
        }
        g_hash_table_add(seen_digests, g_strdup(digest));
        LatcdPrecompileDependency *dependency = g_new0(
            LatcdPrecompileDependency, 1);
        dependency->guest_path = guest;
        dependency->host_path = host;
        g_strlcpy(dependency->source_sha256, digest, sizeof(digest));
        g_ptr_array_add(result, dependency);
        if (elf.interpreter) {
            g_queue_push_tail(&queue, g_strdup(elf.interpreter));
        }
        char *origin = g_path_get_dirname(guest);
        for (guint i = 0; i < elf.needed->len; i++) {
            char *needed_guest = NULL, *needed_host = NULL;
            const char *name = g_ptr_array_index(elf.needed, i);
            if (resolve_needed(rootfs, cache, origin, &elf, name,
                               &needed_guest, &needed_host,
                               error, error_size)) {
                status = -1;
                break;
            }
            g_free(needed_host);
            g_queue_push_tail(&queue, needed_guest);
        }
        g_free(origin);
        elf_dependencies_clear(&elf);
        if (status) break;
    }
    while (!g_queue_is_empty(&queue)) g_free(g_queue_pop_head(&queue));
    g_hash_table_destroy(seen_digests);
    g_hash_table_destroy(seen_paths);
    g_hash_table_destroy(cache);
    g_free(rootfs);
    if (status) {
        g_ptr_array_free(result, TRUE);
        return -1;
    }
    *dependencies = result;
    return 0;
}

static void analyze_dependency(gpointer data, gpointer user_data)
{
    LatcdPrecompileDependency *dependency = data;
    const AnalyzeContext *context = user_data;
    dependency->tbset_path = g_strdup_printf(
        "%s/%s.tbset", context->temporary_dir,
        dependency->source_sha256);
    const char *arguments[] = {
        context->compiler, "emit-static-tbset",
        dependency->host_path, "-o", dependency->tbset_path, NULL,
    };
    gchar *standard_output = NULL, *standard_error = NULL;
    gint wait_status = 0;
    GError *gerror = NULL;
    if (!g_spawn_sync(NULL, (char **)arguments, NULL, 0, NULL, NULL,
                      &standard_output, &standard_error,
                      &wait_status, &gerror) ||
        !LATCD_SPAWN_CHECK_EXIT_STATUS(wait_status, &gerror)) {
        dependency->analysis_status = 1;
        dependency->analysis_error = g_strdup(
            gerror ? gerror->message : standard_error);
        g_clear_error(&gerror);
    } else {
        const char *key_count = strstr(standard_output, "static_tb_keys=");
        if (!key_count) {
            dependency->analysis_status = 1;
            dependency->analysis_error = g_strdup(
                "latc did not report static_tb_keys");
        } else {
            dependency->static_tb_keys = g_ascii_strtoull(
                key_count + strlen("static_tb_keys="), NULL, 10);
        }
    }
    g_free(standard_error);
    g_free(standard_output);
}

static int send_request(const char *socket_path, uint32_t operation,
                        const char *source_path, const char *tbset_path,
                        uint32_t priority, LatcdResponseV2 *response,
                        char *error, size_t error_size)
{
    int source = source_path ? open(source_path, O_RDONLY | O_CLOEXEC) : -1;
    int tbset = tbset_path ? open(tbset_path, O_RDONLY | O_CLOEXEC) : -1;
    int client = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    int needed = operation == LATCD_OP_SUBMIT_KEYS ||
                 operation == LATCD_OP_SUBMIT_STATIC_KEYS ? 2 : 1;
    if ((needed >= 1 && source < 0) || (needed == 2 && tbset < 0) ||
        client < 0) {
        fail(error, error_size, "cannot open precompile request: %s",
             strerror(errno));
        goto failed;
    }
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    int directory_fd;
    if (latcd_socket_address(socket_path, &address, &directory_fd,
                             error, error_size)) {
        goto failed;
    }
    int connected = connect(client, (const void *)&address, sizeof(address));
    int connect_error = errno;
    if (directory_fd >= 0) close(directory_fd);
    errno = connect_error;
    if (connected) {
        fail(error, error_size, "cannot connect to latcd: %s",
             strerror(errno));
        goto failed;
    }
    LatcdRequestV2 request = {
        .magic = LATCD_REQUEST_MAGIC,
        .version = LATCD_PROTOCOL_VERSION,
        .size = sizeof(request),
        .operation = operation,
        .priority = priority,
        .request_id = ((uint64_t)getpid() << 32) ^ g_get_monotonic_time(),
    };
    int result = latcd_send_request(client, source, tbset, &request,
                                    error, error_size) ||
                 latcd_receive_response(client, response, error, error_size);
    close(source);
    if (tbset >= 0) close(tbset);
    close(client);
    return result;
failed:
    if (source >= 0) close(source);
    if (tbset >= 0) close(tbset);
    if (client >= 0) close(client);
    return -1;
}

static void remove_temporary(const char *directory, GPtrArray *dependencies)
{
    for (guint i = 0; i < dependencies->len; i++) {
        LatcdPrecompileDependency *dependency =
            g_ptr_array_index(dependencies, i);
        if (dependency->tbset_path) unlink(dependency->tbset_path);
    }
    rmdir(directory);
}

int latcd_run_precompile(const char *socket_path, const char *rootfs,
                         const char *compiler, const char *guest_path,
                         uint32_t jobs)
{
    int64_t started = g_get_monotonic_time();
    char error[512] = {0};
    GPtrArray *dependencies = NULL;
    if (latcd_precompile_collect(rootfs, guest_path, &dependencies,
                                 error, sizeof(error))) {
        fprintf(stderr, "latcd: %s\n", error);
        return 1;
    }
    if (access(compiler, X_OK)) {
        fprintf(stderr, "latcd: compiler is not executable: %s\n", compiler);
        g_ptr_array_free(dependencies, TRUE);
        return 1;
    }
    GError *gerror = NULL;
    char *temporary = g_dir_make_tmp("latc-precompile-XXXXXX", &gerror);
    if (!temporary) {
        fprintf(stderr, "latcd: cannot create temporary directory: %s\n",
                gerror ? gerror->message : "unknown error");
        g_clear_error(&gerror);
        g_ptr_array_free(dependencies, TRUE);
        return 1;
    }
    AnalyzeContext context = { .compiler = compiler,
                               .temporary_dir = temporary };
    GThreadPool *pool = g_thread_pool_new(analyze_dependency, &context,
                                           jobs, FALSE, &gerror);
    if (!pool) {
        fprintf(stderr, "latcd: cannot create analysis workers: %s\n",
                gerror ? gerror->message : "unknown error");
        g_clear_error(&gerror);
        rmdir(temporary);
        g_free(temporary);
        g_ptr_array_free(dependencies, TRUE);
        return 1;
    }
    for (guint i = 0; i < dependencies->len; i++) {
        g_thread_pool_push(pool, g_ptr_array_index(dependencies, i), NULL);
    }
    g_thread_pool_free(pool, FALSE, TRUE);
    bool analysis_failed = false;
    uint64_t total_keys = 0;
    for (guint i = 0; i < dependencies->len; i++) {
        LatcdPrecompileDependency *dependency =
            g_ptr_array_index(dependencies, i);
        total_keys += dependency->static_tb_keys;
        if (dependency->analysis_status) {
            analysis_failed = true;
            fprintf(stderr, "latcd: static analysis failed for %s: %s\n",
                    dependency->guest_path,
                    dependency->analysis_error ? dependency->analysis_error :
                    "unknown error");
        }
    }
    if (analysis_failed) {
        remove_temporary(temporary, dependencies);
        g_free(temporary);
        g_ptr_array_free(dependencies, TRUE);
        return 1;
    }
    uint64_t cache_hits = 0, compiled = 0, failed_count = 0;
    GArray *submitted = g_array_sized_new(FALSE, TRUE, sizeof(bool),
                                          dependencies->len);
    GArray *hit = g_array_sized_new(FALSE, TRUE, sizeof(bool),
                                    dependencies->len);
    g_array_set_size(submitted, dependencies->len);
    g_array_set_size(hit, dependencies->len);
    for (guint i = 0; i < dependencies->len; i++) {
        LatcdPrecompileDependency *dependency =
            g_ptr_array_index(dependencies, i);
        LatcdResponseV2 response = {0};
        uint32_t priority = i == 0 ? LATCD_PRIORITY_STARTUP :
                                    LATCD_PRIORITY_LIBRARY;
        error[0] = '\0';
        if (send_request(socket_path, LATCD_OP_SUBMIT_STATIC_KEYS,
                         dependency->host_path, dependency->tbset_path,
                         priority, &response, error, sizeof(error)) ||
            response.status != LATCD_STATUS_OK) {
            fprintf(stderr, "latcd: submit failed for %s: %s\n",
                    dependency->guest_path,
                    error[0] ? error : response.message);
            failed_count++;
            continue;
        }
        g_array_index(submitted, bool, i) = true;
        bool cache_hit = g_str_has_prefix(response.message, "cache hit:");
        g_array_index(hit, bool, i) = cache_hit;
        cache_hits += cache_hit;
        printf("dependency=%s source_sha256=%s static_tb_keys=%" PRIu64
               " submit=%s\n", dependency->guest_path,
               dependency->source_sha256, dependency->static_tb_keys,
               cache_hit ? "cache-hit" : "accepted");
    }
    for (guint i = 0; i < dependencies->len; i++) {
        if (!g_array_index(submitted, bool, i)) continue;
        LatcdPrecompileDependency *dependency =
            g_ptr_array_index(dependencies, i);
        LatcdResponseV2 response = {0};
        error[0] = '\0';
        if (send_request(socket_path, LATCD_OP_FLUSH_SOURCE,
                         dependency->host_path, NULL,
                         LATCD_PRIORITY_LIBRARY, &response,
                         error, sizeof(error)) ||
            response.status != LATCD_STATUS_OK) {
            fprintf(stderr, "latcd: flush failed for %s: %s\n",
                    dependency->guest_path,
                    error[0] ? error : response.message);
            failed_count++;
        } else if (!g_array_index(hit, bool, i)) {
            compiled++;
        }
    }
    printf("precompile_elfs=%u\nstatic_tb_keys=%" PRIu64
           "\ncache_hits=%" PRIu64 "\ncompiled=%" PRIu64
           "\nfailed=%" PRIu64 "\nelapsed_ms=%" PRIi64 "\n",
           dependencies->len, total_keys, cache_hits, compiled,
           failed_count, (g_get_monotonic_time() - started) / 1000);
    g_array_free(submitted, TRUE);
    g_array_free(hit, TRUE);
    remove_temporary(temporary, dependencies);
    g_free(temporary);
    g_ptr_array_free(dependencies, TRUE);
    return failed_count ? 1 : 0;
}
