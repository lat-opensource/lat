#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "latcd-protocol.h"
#include "lat-aot-v2.h"
#include "lat-tb-key-set.h"
#include "latc-build-id.h"
#include "module-inspect.h"
#include "precompile.h"

#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>

static long superseded_grace_seconds(void);
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#if GLIB_CHECK_VERSION(2, 64, 0)
#define LATCD_SPAWN_CHECK_EXIT_STATUS g_spawn_check_wait_status
#else
#define LATCD_SPAWN_CHECK_EXIT_STATUS g_spawn_check_exit_status
#endif

#define LATCD_DEFAULT_MAX_INPUT (UINT64_C(1) << 30)
#define LATCD_DEFAULT_MAX_JOBS 64u
#define LATCD_DEFAULT_MAX_QUEUE_BYTES (UINT64_C(4) << 30)
#define LATCD_DEFAULT_MAX_CACHE_BYTES (UINT64_C(16) << 30)
#define LATCD_AUTO_WORKERS_MAX 8u
#define LATCD_MAX_WORKERS 32u
#define LATCD_DEFAULT_MAX_NEGATIVE 128u
#define LATCD_DEFAULT_NEGATIVE_MS 30000u
#define LATCD_DEFAULT_CPU_SECONDS 60u
#define LATCD_DEFAULT_ADDRESS_SPACE (UINT64_C(1) << 40)
#define LATCD_DEFAULT_FILE_SIZE (UINT64_C(2) << 30)
#define LATCD_DEFAULT_OPEN_FILES 256u
#define LATCD_BATCH_QUIET_US (INT64_C(100) * 1000)
#define LATCD_BATCH_MAX_US (INT64_C(500) * 1000)
#define LATCD_SUPERSEDED_DIRNAME ".superseded"

typedef struct LatcdConfig {
    const char *socket_path;
    const char *cache_dir;
    const char *compiler;
    const char *runner;
    const char *runtime_dir;
    const char *x86_rootfs;
    const char *stats_path;
    uint64_t max_input;
    uint64_t address_space_limit;
    uint64_t file_size_limit;
    uint64_t max_queue_bytes;
    uint64_t max_cache_bytes;
    uint32_t max_jobs;
    uint32_t workers;
    uint32_t max_negative;
    uint32_t negative_ms;
    uint32_t cpu_seconds;
    uint32_t open_files;
    bool flush_only;
    uint32_t idle_seconds;
} LatcdConfig;

typedef struct LatcdCurrentManifest {
    char *module;
    char *native;
    char *tbset;
    uint64_t published_sequence;
} LatcdCurrentManifest;

static void current_manifest_clear(LatcdCurrentManifest *manifest);
static int current_manifest_read(const LatcdConfig *config, const char *hex,
                                 LatcdCurrentManifest *manifest);
static int persisted_manifest_valid(const LatcdConfig *config,
                                    const char source_key[65],
                                    const uint8_t digest[32]);
static int cached_module_inspect(const char *path, const uint8_t digest[32],
                                 LatAotModuleInfoV2 *info);
static _Atomic unsigned int active_precompiles;

static uint32_t default_worker_count(void)
{
    long online = sysconf(_SC_NPROCESSORS_ONLN);
    if (online < 1) {
        return 1;
    }
    return MIN((uint32_t)online, LATCD_AUTO_WORKERS_MAX);
}

typedef struct LatcdJob {
    char *snapshot;
    char *tbset;
    bool tbset_canonical;
    bool static_tbset;
    bool running;
    bool dirty;
    char key[130];
    char source_key[65];
    uint8_t digest[32];
    uint32_t priority;
    uint64_t sequence;
    uint64_t accepted_sequence;
    uint64_t compile_sequence;
    uint64_t source_size;
    int64_t batch_started_at_us;
    int64_t ready_at_us;
} LatcdJob;

typedef struct LatcdSourceState {
    uint64_t accepted_sequence;
    uint64_t published_sequence;
    uint64_t failed_sequence;
} LatcdSourceState;

typedef struct LatcdNegativeEntry {
    uint32_t failures;
    int status;
    int64_t retry_at_us;
} LatcdNegativeEntry;

typedef struct LatcdStaticBarrier {
    char source_key[65];
    uint64_t first_sequence;
    uint64_t last_sequence;
    bool failed;
} LatcdStaticBarrier;

typedef struct LatcdService {
    const LatcdConfig *config;
    pthread_mutex_t lock;
    pthread_cond_t ready;
    GPtrArray *queue;
    GHashTable *active;
    GHashTable *running_sources;
    GHashTable *negative;
    GHashTable *source_states;
    GHashTable *static_barriers;
    pthread_t *workers;
    int stopping;
    uint64_t next_sequence;
    uint64_t requests;
    uint64_t queued;
    uint64_t deduplicated;
    uint64_t cache_hits;
    uint64_t compiled;
    uint64_t failed;
    uint64_t negative_hits;
    uint64_t queue_full;
    uint64_t queued_bytes;
} LatcdService;

typedef struct LatcdWorker {
    LatcdService *service;
    uint32_t index;
} LatcdWorker;

typedef struct LatcdPrecompileRequest {
    const LatcdConfig *config;
    int client_fd;
    int source_fd;
    uint64_t request_id;
} LatcdPrecompileRequest;

static volatile sig_atomic_t stop_requested;
static volatile sig_atomic_t compiler_process_groups[LATCD_MAX_WORKERS];
static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;

static LatcdSourceState *source_state_locked(LatcdService *service,
                                             const char *source_key);

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

static int sync_directory(const char *path, char *error, size_t error_size);

static void digest_hex(const uint8_t digest[32], char hex[65])
{
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < 32; i++) {
        hex[i * 2] = digits[digest[i] >> 4];
        hex[i * 2 + 1] = digits[digest[i] & 15];
    }
    hex[64] = '\0';
}

static void expected_codegen_id(uint8_t digest[32])
{
    GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(checksum, (const guchar *)LATC_BUILD_ID,
                      strlen(LATC_BUILD_ID));
    gsize length = 32;
    g_checksum_get_digest(checksum, digest, &length);
    g_checksum_free(checksum);
}

static int read_program_build_id(const char *label, const char *path,
                                 const char *argument,
                                 const char *runtime_dir,
                                 char actual[65], char *error,
                                 size_t error_size)
{
    int output[2];
    if (pipe2(output, O_CLOEXEC | O_NONBLOCK)) {
        return fail(error, error_size, "cannot probe %s %s: %s", label, path,
                    strerror(errno));
    }
    pid_t child = fork();
    if (child == 0) {
        dup2(output[1], STDOUT_FILENO);
        close(output[0]);
        close(output[1]);
        if (runtime_dir) {
            setenv("LD_LIBRARY_PATH", runtime_dir, 1);
        }
        execl(path, path, argument, (char *)NULL);
        _exit(127);
    }
    close(output[1]);
    if (child < 0) {
        close(output[0]);
        return fail(error, error_size, "cannot start %s %s: %s", label, path,
                    strerror(errno));
    }

    char buffer[128] = {0};
    size_t used = 0;
    int status = 0;
    int64_t deadline = g_get_monotonic_time() + 2 * G_TIME_SPAN_SECOND;
    for (;;) {
        struct pollfd poll_fd = { .fd = output[0], .events = POLLIN | POLLHUP };
        int64_t remaining = deadline - g_get_monotonic_time();
        if (remaining <= 0) {
            kill(child, SIGKILL);
            waitpid(child, &status, 0);
            close(output[0]);
            return fail(error, error_size, "%s identity probe timed out: %s",
                        label, path);
        }
        poll(&poll_fd, 1, MIN(50, (int)(remaining / 1000)));
        ssize_t count;
        while (used < sizeof(buffer) - 1 &&
               (count = read(output[0], buffer + used,
                             sizeof(buffer) - 1 - used)) > 0) {
            used += count;
        }
        pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child) {
            while (used < sizeof(buffer) - 1 &&
                   (count = read(output[0], buffer + used,
                                 sizeof(buffer) - 1 - used)) > 0) {
                used += count;
            }
            break;
        }
        if (waited < 0 && errno != EINTR) {
            close(output[0]);
            return fail(error, error_size, "cannot wait for %s identity: %s",
                        label, strerror(errno));
        }
    }
    close(output[0]);
    buffer[used] = '\0';
    g_strchomp(buffer);
    if (!WIFEXITED(status) || WEXITSTATUS(status) || strlen(buffer) != 64) {
        return fail(error, error_size,
                    "%s identity probe failed for %s (status=%d, output=%s)",
                    label, path, WIFEXITED(status) ? WEXITSTATUS(status) : -1,
                    buffer[0] ? buffer : "<empty>");
    }
    memcpy(actual, buffer, 65);
    return 0;
}

static int validate_toolchain(const LatcdConfig *config, char *error,
                              size_t error_size)
{
    char actual[65];
    if (read_program_build_id("latc", config->compiler, "build-id",
                              config->runtime_dir, actual, error, error_size)) {
        return -1;
    }
    if (strcmp(actual, LATC_BUILD_ID)) {
        return fail(error, error_size,
                    "latc build identity mismatch: %s expected=%s actual=%s",
                    config->compiler, LATC_BUILD_ID, actual);
    }
    if (read_program_build_id("runner", config->runner, "--latc-build-id",
                              config->runtime_dir, actual, error, error_size)) {
        return -1;
    }
    if (strcmp(actual, LATC_BUILD_ID)) {
        return fail(error, error_size,
                    "runner build identity mismatch: %s expected=%s actual=%s",
                    config->runner, LATC_BUILD_ID, actual);
    }

    char *runtime_path = g_build_filename(
        config->runtime_dir, LAT_AOT_V2_RUNTIME_SONAME, NULL);
    void *runtime = dlopen(runtime_path, RTLD_NOW | RTLD_LOCAL);
    if (!runtime) {
        fail(error, error_size, "cannot load AOT runtime %s: %s", runtime_path,
             dlerror());
        g_free(runtime_path);
        return -1;
    }
    uint32_t (*abi_version)(void) = dlsym(runtime,
        "lat_aot_runtime_abi_version");
    const char *(*build_id)(void) = dlsym(runtime,
        "lat_aot_runtime_build_id");
    if (!abi_version || !build_id || abi_version() != LAT_AOT_V2_ABI_VERSION) {
        dlclose(runtime);
        int result = fail(error, error_size,
                          "AOT runtime ABI mismatch: %s expected=%u",
                          runtime_path, LAT_AOT_V2_ABI_VERSION);
        g_free(runtime_path);
        return result;
    }
    const char *runtime_id = build_id();
    if (!runtime_id || strcmp(runtime_id, LATC_BUILD_ID)) {
        char mismatch[65] = {0};
        g_strlcpy(mismatch, runtime_id ? runtime_id : "<missing>",
                  sizeof(mismatch));
        dlclose(runtime);
        int result = fail(error, error_size,
                          "AOT runtime build identity mismatch: %s expected=%s actual=%s",
                          runtime_path, LATC_BUILD_ID, mismatch);
        g_free(runtime_path);
        return result;
    }
    dlclose(runtime);
    g_free(runtime_path);
    return 0;
}

static void signal_stop(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static int ensure_private_directory(const char *path, char *error,
                                    size_t error_size)
{
    if (g_mkdir_with_parents(path, 0700)) {
        return fail(error, error_size, "cannot create %s: %s", path,
                    strerror(errno));
    }
    struct stat status;
    if (lstat(path, &status) || !S_ISDIR(status.st_mode) ||
        status.st_uid != geteuid()) {
        return fail(error, error_size, "%s is not a user-owned directory",
                    path);
    }
    if (chmod(path, 0700)) {
        return fail(error, error_size, "cannot protect %s: %s", path,
                    strerror(errno));
    }
    return 0;
}

static int acquire_cache_owner(const LatcdConfig *config, char *error,
                               size_t error_size)
{
    if (ensure_private_directory(config->cache_dir, error, error_size)) {
        return -1;
    }
    char *path = g_build_filename(config->cache_dir, ".latcd.lock", NULL);
    int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) {
        fail(error, error_size, "cannot open cache owner lock %s: %s", path,
             strerror(errno));
        g_free(path);
        return -1;
    }
    struct stat status;
    if (fstat(fd, &status) || !S_ISREG(status.st_mode) ||
        status.st_uid != geteuid() || status.st_nlink != 1 ||
        fchmod(fd, 0600)) {
        fail(error, error_size,
             "cache owner lock is not a private user-owned file: %s", path);
        close(fd);
        g_free(path);
        return -1;
    }
    if (flock(fd, LOCK_EX | LOCK_NB)) {
        if (errno == EWOULDBLOCK) {
            fail(error, error_size,
                 "cache is already owned by another latcd: %s",
                 config->cache_dir);
        } else {
            fail(error, error_size, "cannot lock cache %s: %s",
                 config->cache_dir, strerror(errno));
        }
        close(fd);
        g_free(path);
        return -1;
    }
    char *owner = g_strdup_printf("pid=%ld\nsocket=%s\nbuild_id=%s\n",
                                  (long)getpid(), config->socket_path, LATC_BUILD_ID);
    size_t length = strlen(owner);
    if (ftruncate(fd, 0) || pwrite(fd, owner, length, 0) != (ssize_t)length ||
        fsync(fd)) {
        fail(error, error_size, "cannot record cache owner in %s: %s", path,
             strerror(errno));
        close(fd);
        g_free(owner);
        g_free(path);
        return -1;
    }
    g_free(owner);
    g_free(path);
    return fd;
}

static int validate_x86_elf(int fd, uint64_t file_size, char *error,
                            size_t error_size)
{
    Elf64_Ehdr header;
    if (file_size < sizeof(header) ||
        pread(fd, &header, sizeof(header), 0) != sizeof(header)) {
        return fail(error, error_size, "source is too small for an ELF header");
    }
    if (memcmp(header.e_ident, ELFMAG, SELFMAG) ||
        header.e_ident[EI_CLASS] != ELFCLASS64 ||
        header.e_ident[EI_DATA] != ELFDATA2LSB ||
        header.e_machine != EM_X86_64 ||
        (header.e_type != ET_EXEC && header.e_type != ET_DYN) ||
        header.e_phentsize != sizeof(Elf64_Phdr) || !header.e_phnum ||
        header.e_phoff > file_size ||
        header.e_phnum > (file_size - header.e_phoff) / sizeof(Elf64_Phdr)) {
        return fail(error, error_size,
                    "source is not a supported x86-64 executable ELF");
    }
    int executable_load = 0;
    for (uint16_t i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr program;
        off_t offset = header.e_phoff + (off_t)i * sizeof(program);
        if (pread(fd, &program, sizeof(program), offset) != sizeof(program) ||
            program.p_offset > file_size ||
            program.p_filesz > file_size - program.p_offset) {
            return fail(error, error_size, "source ELF program headers are invalid");
        }
        if (program.p_type == PT_LOAD && (program.p_flags & PF_X)) {
            executable_load = 1;
        }
    }
    if (!executable_load) {
        return fail(error, error_size, "source ELF has no executable PT_LOAD");
    }
    return 0;
}

static int metadata_equal(const struct stat *before, const struct stat *after)
{
    return before->st_dev == after->st_dev &&
           before->st_ino == after->st_ino &&
           before->st_size == after->st_size &&
           before->st_mtim.tv_sec == after->st_mtim.tv_sec &&
           before->st_mtim.tv_nsec == after->st_mtim.tv_nsec &&
           before->st_ctim.tv_sec == after->st_ctim.tv_sec &&
           before->st_ctim.tv_nsec == after->st_ctim.tv_nsec;
}

static int snapshot_source(int source_fd, const char *temporary_dir,
                           uint64_t max_input, char **snapshot_path,
                           uint8_t digest[32], struct stat *identity,
                           char *error, size_t error_size)
{
    struct stat before;
    int flags = fcntl(source_fd, F_GETFL);
    if (flags < 0 || (flags & O_ACCMODE) != O_RDONLY ||
        fstat(source_fd, &before) || !S_ISREG(before.st_mode) ||
        before.st_size < 0 || (uint64_t)before.st_size > max_input) {
        return fail(error, error_size,
                    "source descriptor must be a bounded read-only regular file");
    }
    if (validate_x86_elf(source_fd, before.st_size, error, error_size)) {
        return -1;
    }
    char *path = g_build_filename(temporary_dir, "source-XXXXXX", NULL);
    int output = g_mkstemp_full(path, O_RDWR | O_CLOEXEC, 0600);
    if (output < 0) {
        g_free(path);
        return fail(error, error_size, "cannot create source snapshot: %s",
                    strerror(errno));
    }
    GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
    unsigned char buffer[128 * 1024];
    off_t offset = 0;
    int result = -1;
    while ((uint64_t)offset < (uint64_t)before.st_size) {
        size_t wanted = sizeof(buffer);
        if (wanted > (uint64_t)before.st_size - (uint64_t)offset) {
            wanted = before.st_size - offset;
        }
        ssize_t count = pread(source_fd, buffer, wanted, offset);
        if (count <= 0) {
            fail(error, error_size, "cannot read source descriptor: %s",
                 count < 0 ? strerror(errno) : "unexpected end of file");
            goto out;
        }
        ssize_t written = 0;
        while (written < count) {
            ssize_t part = write(output, buffer + written, count - written);
            if (part < 0 && errno == EINTR) {
                continue;
            }
            if (part <= 0) {
                fail(error, error_size, "cannot write source snapshot: %s",
                     strerror(errno));
                goto out;
            }
            written += part;
        }
        g_checksum_update(checksum, buffer, count);
        offset += count;
    }
    struct stat after;
    if (fstat(source_fd, &after) || !metadata_equal(&before, &after)) {
        fail(error, error_size, "source changed while creating snapshot");
        goto out;
    }
    if (fsync(output)) {
        fail(error, error_size, "cannot sync source snapshot: %s",
             strerror(errno));
        goto out;
    }
    gsize digest_size = 32;
    g_checksum_get_digest(checksum, digest, &digest_size);
    if (identity) {
        *identity = before;
    }
    *snapshot_path = path;
    path = NULL;
    result = 0;
out:
    g_checksum_free(checksum);
    close(output);
    if (path) {
        unlink(path);
        g_free(path);
    }
    return result;
}

static int hash_source(int source_fd, uint64_t max_input, uint8_t digest[32],
                       char *error, size_t error_size)
{
    struct stat before;
    int flags = fcntl(source_fd, F_GETFL);
    if (flags < 0 || (flags & O_ACCMODE) != O_RDONLY ||
        fstat(source_fd, &before) || !S_ISREG(before.st_mode) ||
        before.st_size < 0 || (uint64_t)before.st_size > max_input) {
        return fail(error, error_size,
                    "source descriptor must be a bounded read-only regular file");
    }
    if (validate_x86_elf(source_fd, before.st_size, error, error_size)) {
        return -1;
    }
    GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
    unsigned char buffer[128 * 1024];
    off_t offset = 0;
    while ((uint64_t)offset < (uint64_t)before.st_size) {
        size_t wanted = sizeof(buffer);
        if (wanted > (uint64_t)before.st_size - (uint64_t)offset) {
            wanted = before.st_size - offset;
        }
        ssize_t count = pread(source_fd, buffer, wanted, offset);
        if (count <= 0) {
            g_checksum_free(checksum);
            return fail(error, error_size,
                        "cannot read source descriptor: %s",
                        count < 0 ? strerror(errno) : "unexpected end of file");
        }
        g_checksum_update(checksum, buffer, count);
        offset += count;
    }
    struct stat after;
    if (fstat(source_fd, &after) || !metadata_equal(&before, &after)) {
        g_checksum_free(checksum);
        return fail(error, error_size, "source changed while hashing");
    }
    gsize digest_size = 32;
    g_checksum_get_digest(checksum, digest, &digest_size);
    g_checksum_free(checksum);
    return 0;
}

static void publish_source_identity(const LatcdConfig *config,
                                    const struct stat *status,
                                    const uint8_t digest[32])
{
    char error[128];
    char *directory = g_build_filename(config->cache_dir, ".identities", NULL);
    if (ensure_private_directory(directory, error, sizeof(error))) {
        g_free(directory);
        return;
    }
    char *name = g_strdup_printf(
        "%llx-%llx-%llx-%llx-%lx-%llx-%lx.sha256",
        (unsigned long long)status->st_dev,
        (unsigned long long)status->st_ino,
        (unsigned long long)status->st_size,
        (unsigned long long)status->st_mtim.tv_sec,
        (unsigned long)status->st_mtim.tv_nsec,
        (unsigned long long)status->st_ctim.tv_sec,
        (unsigned long)status->st_ctim.tv_nsec);
    char *final = g_build_filename(directory, name, NULL);
    char *temporary = g_strdup_printf("%s.tmp.%ld", final, (long)getpid());
    int fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC |
                  O_NOFOLLOW, 0600);
    if (fd >= 0) {
        static const char digits[] = "0123456789abcdef";
        char text[65];
        for (size_t i = 0; i < 32; i++) {
            text[i * 2] = digits[digest[i] >> 4];
            text[i * 2 + 1] = digits[digest[i] & 15];
        }
        text[64] = '\n';
        if (write(fd, text, sizeof(text)) == sizeof(text) && !fsync(fd)) {
            close(fd);
            fd = -1;
            if (!rename(temporary, final)) {
                sync_directory(directory, error, sizeof(error));
            }
        }
        if (fd >= 0) close(fd);
        unlink(temporary);
    }
    g_free(temporary);
    g_free(final);
    g_free(name);
    g_free(directory);
}

static int snapshot_tbset(int tbset_fd, const char *source_path,
                          const char *temporary_dir,
                          const uint8_t source_digest[32],
                          char **snapshot_path,
                          char *error, size_t error_size)
{
    struct stat status;
    int flags = fcntl(tbset_fd, F_GETFL);
    if (flags < 0 || (flags & O_ACCMODE) != O_RDONLY ||
        fstat(tbset_fd, &status) || !S_ISREG(status.st_mode) ||
        status.st_size <= 0 || status.st_size > 64 * 1024 * 1024) {
        return fail(error, error_size,
                    "TB set descriptor must be a bounded read-only regular file");
    }
    int source_fd = open(source_path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    Elf64_Ehdr header;
    if (source_fd < 0 ||
        pread(source_fd, &header, sizeof(header), 0) != sizeof(header) ||
        header.e_phentsize != sizeof(Elf64_Phdr) || !header.e_phnum ||
        header.e_phnum > 4096) {
        if (source_fd >= 0) close(source_fd);
        return fail(error, error_size, "cannot validate TB set source ELF");
    }
    size_t phdr_size = header.e_phnum * sizeof(Elf64_Phdr);
    Elf64_Phdr *phdrs = g_malloc(phdr_size);
    if (!phdrs || pread(source_fd, phdrs, phdr_size, header.e_phoff) !=
                  (ssize_t)phdr_size) {
        g_free(phdrs);
        close(source_fd);
        return fail(error, error_size, "cannot read TB set source segments");
    }
    close(source_fd);
    uint64_t load_base = UINT64_MAX;
    for (uint16_t i = 0; i < header.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD && phdrs[i].p_memsz &&
            phdrs[i].p_vaddr < load_base) {
            load_base = phdrs[i].p_vaddr;
        }
    }
    if (load_base == UINT64_MAX) {
        g_free(phdrs);
        return fail(error, error_size,
                    "TB set source ELF has no loadable segments");
    }

    LatTbKeySet set;
    if (lat_tb_key_set_read_fd(tbset_fd, source_digest, &set,
                               error, error_size)) {
        g_free(phdrs);
        return -1;
    }
    for (size_t record = 0; record < set.count; record++) {
        uint64_t rva = set.keys[record].guest_rva;
        int executable = 0;
        for (uint16_t i = 0; i < header.e_phnum; i++) {
            uint64_t segment_rva = phdrs[i].p_vaddr - load_base;
            if (phdrs[i].p_type == PT_LOAD && (phdrs[i].p_flags & PF_X) &&
                phdrs[i].p_vaddr >= load_base && rva >= segment_rva &&
                rva - segment_rva < phdrs[i].p_memsz) {
                executable = 1;
                break;
            }
        }
        if (!executable) {
            lat_tb_key_set_destroy(&set);
            g_free(phdrs);
            return fail(error, error_size,
                        "TB key record %zu is outside executable segments",
                        record);
        }
    }
    g_free(phdrs);
    char *path = g_build_filename(temporary_dir, "tbset-XXXXXX", NULL);
    int output_fd = g_mkstemp_full(path, O_RDWR | O_CLOEXEC, 0600);
    if (output_fd < 0) {
        unlink(path);
        g_free(path);
        lat_tb_key_set_destroy(&set);
        return fail(error, error_size, "cannot snapshot TB set: %s",
                    strerror(errno));
    }
    int result = lat_tb_key_set_write_fd(output_fd, &set,
                                          error, error_size) ||
                 fsync(output_fd);
    if (close(output_fd)) result = -1;
    output_fd = -1;
    lat_tb_key_set_destroy(&set);
    if (result) {
        if (output_fd >= 0) close(output_fd);
        unlink(path);
        g_free(path);
        if (!error[0]) fail(error, error_size,
                            "cannot persist TB key set snapshot");
        return -1;
    }
    *snapshot_path = path;
    return 0;
}

static int merge_tbset(const LatcdConfig *config, const uint8_t digest[32],
                       const char *incoming, char **canonical, bool *changed,
                       char *error, size_t error_size)
{
    if (changed) *changed = false;
    char source_hex[65];
    digest_hex(digest, source_hex);
    char *directory = g_build_filename(config->cache_dir, ".tbsets", NULL);
    if (ensure_private_directory(directory, error, error_size)) {
        g_free(directory);
        return -1;
    }
    char *name = g_strdup_printf("%s.tbset", source_hex);
    char *final = g_build_filename(directory, name, NULL);
    g_free(name);
    LatTbKeySet incoming_set = {0};
    LatTbKeySet existing = {0};
    LatTbKeySet merged = {0};
    pthread_mutex_lock(&cache_lock);
    int result = lat_tb_key_set_read_file(incoming, digest, &incoming_set,
                                           error, error_size);
    if (!result && lat_tb_key_set_read_file(final, digest, &existing,
                                             error, error_size)) {
        if (errno != ENOENT) {
            result = -1;
        } else {
            memset(&existing, 0, sizeof(existing));
            memcpy(existing.source_sha256, digest, 32);
            error[0] = '\0';
        }
    }
    uint64_t original_count = existing.count;
    uint64_t original_sequence = existing.sequence;
    if (!result) {
        LatcdCurrentManifest manifest = {0};
        if (!current_manifest_read(config, source_hex, &manifest) &&
            *manifest.tbset &&
            persisted_manifest_valid(config, source_hex, digest)) {
            char *published = g_build_filename(config->cache_dir,
                                                manifest.tbset, NULL);
            LatTbKeySet published_set = {0}, complete = {0};
            result = lat_tb_key_set_read_file(published, digest, &published_set,
                                              error, error_size);
            if (!result) {
                result = lat_tb_key_set_union(&existing, &published_set,
                                               &complete, error, error_size);
            }
            if (!result) {
                lat_tb_key_set_destroy(&existing);
                existing = complete;
            }
            lat_tb_key_set_destroy(&published_set);
            g_free(published);
        }
        current_manifest_clear(&manifest);
    }
    if (!result) {
        result = lat_tb_key_set_union(&existing, &incoming_set, &merged,
                                      error, error_size);
    }
    char *temporary = NULL;
    if (!result) {
        bool content_changed = merged.count != original_count;
        merged.sequence = content_changed ? original_sequence + 1 :
                                            original_sequence;
        if (changed) *changed = content_changed;
        if (!content_changed) goto merged_done;
        temporary = g_build_filename(directory, "tbset-XXXXXX", NULL);
        int fd = g_mkstemp_full(temporary, O_RDWR | O_CLOEXEC, 0600);
        if (fd < 0) {
            result = fail(error, error_size, "cannot create canonical TB set");
        } else {
            if (lat_tb_key_set_write_fd(fd, &merged, error, error_size) ||
                fsync(fd)) result = -1;
            if (close(fd)) result = -1;
            if (!result && rename(temporary, final)) {
                result = -1;
            } else if (!result) {
                result = sync_directory(directory, error, error_size);
            }
            if (result && !error[0]) {
                fail(error, error_size, "cannot publish canonical TB set: %s",
                     strerror(errno));
            }
        }
    }
merged_done:
    if (result && temporary) unlink(temporary);
    pthread_mutex_unlock(&cache_lock);
    lat_tb_key_set_destroy(&incoming_set);
    lat_tb_key_set_destroy(&existing);
    lat_tb_key_set_destroy(&merged);
    g_free(temporary);
    g_free(directory);
    if (result) {
        g_free(final);
        return -1;
    }
    *canonical = final;
    return 0;
}

static int prepare_compile_delta(const LatcdConfig *config,
                                 const uint8_t digest[32],
                                 const char *known_path, char **delta_path,
                                 bool *empty, char *error, size_t error_size)
{
    LatTbKeySet known = {0};
    LatTbKeySet delta = {0};
    *delta_path = NULL;
    *empty = false;
    int result = lat_tb_key_set_read_file(known_path, digest, &known,
                                           error, error_size);
    if (!result) {
        char source_hex[65];
        digest_hex(digest, source_hex);
        LatcdCurrentManifest manifest = {0};
        char *module_path = NULL;
        char *native_path = NULL;
        char *published_path = NULL;
        LatAotModuleInfoV2 info;
        pthread_mutex_lock(&cache_lock);
        bool current_valid = !current_manifest_read(
            config, source_hex, &manifest) && *manifest.tbset;
        if (current_valid) {
            module_path = g_build_filename(config->cache_dir,
                                            manifest.module, NULL);
            native_path = g_build_filename(config->cache_dir,
                                            manifest.native, NULL);
            published_path = g_build_filename(config->cache_dir,
                                               manifest.tbset, NULL);
            current_valid = cached_module_inspect(module_path, digest, &info) &&
                !access(native_path, R_OK) && !access(published_path, R_OK);
        }
        if (current_valid) {
            result = lat_aot_v2_module_missing_tbset_file(
                module_path, known_path, &info, &delta, error, error_size);
        } else {
            delta = known;
            memset(&known, 0, sizeof(known));
        }
        pthread_mutex_unlock(&cache_lock);
        g_free(published_path);
        g_free(native_path);
        g_free(module_path);
        current_manifest_clear(&manifest);
    }
    if (!result && !delta.count) {
        *empty = true;
    } else if (!result) {
        char *temporary_dir = g_build_filename(config->cache_dir, ".tmp", NULL);
        char *path = g_build_filename(temporary_dir, "delta-XXXXXX", NULL);
        int fd = g_mkstemp_full(path, O_RDWR | O_CLOEXEC, 0600);
        g_free(temporary_dir);
        delta.sequence = known.sequence;
        if (fd < 0 || lat_tb_key_set_write_fd(fd, &delta,
                                               error, error_size) ||
            (!config->flush_only && fsync(fd))) {
            if (fd >= 0) close(fd);
            unlink(path);
            g_free(path);
            result = -1;
        } else {
            close(fd);
            *delta_path = path;
        }
    }
    lat_tb_key_set_destroy(&known);
    lat_tb_key_set_destroy(&delta);
    return result;
}

/* Keep failed static discoveries out of the runtime collection, while
 * retaining keys already collected or published for this ELF. */
static int prepare_static_tbset(const LatcdConfig *config,
                                const uint8_t digest[32], const char *incoming,
                                char **output, char *error, size_t error_size)
{
    char hex[65];
    digest_hex(digest, hex);
    char *name = g_strdup_printf("%s.tbset", hex);
    char *known = g_build_filename(config->cache_dir, ".tbsets", name, NULL);
    LatcdCurrentManifest manifest = {0};
    LatTbKeySet complete = {0};
    int result = lat_tb_key_set_read_file(incoming, digest, &complete,
                                         error, error_size);
    pthread_mutex_lock(&cache_lock);
    char *published = NULL;
    if (!current_manifest_read(config, hex, &manifest) && *manifest.tbset &&
        persisted_manifest_valid(config, hex, digest)) {
        published = g_build_filename(config->cache_dir, manifest.tbset, NULL);
    }
    const char *paths[] = { known, published };
    for (size_t i = 0; !result && i < G_N_ELEMENTS(paths); i++) {
        if (!paths[i]) continue;
        LatTbKeySet existing = {0}, merged = {0};
        if (lat_tb_key_set_read_file(paths[i], digest, &existing,
                                      error, error_size)) {
            if (errno != ENOENT) result = -1;
            else error[0] = '\0';
        } else {
            result = lat_tb_key_set_union(&complete, &existing, &merged,
                                          error, error_size);
            if (!result) {
                lat_tb_key_set_destroy(&complete);
                complete = merged;
            }
        }
        lat_tb_key_set_destroy(&existing);
    }
    pthread_mutex_unlock(&cache_lock);
    if (!result) {
        char *path = g_build_filename(config->cache_dir, ".tmp",
                                      "static-XXXXXX", NULL);
        int fd = g_mkstemp_full(path, O_RDWR | O_CLOEXEC, 0600);
        if (fd < 0 || lat_tb_key_set_write_fd(fd, &complete,
                                              error, error_size)) {
            result = -1;
        }
        if (fd >= 0 && close(fd)) result = -1;
        if (result) {
            unlink(path);
            g_free(path);
        } else {
            *output = path;
        }
    }
    lat_tb_key_set_destroy(&complete);
    current_manifest_clear(&manifest);
    g_free(published);
    g_free(known);
    g_free(name);
    return result;
}

static int run_compiler(const LatcdConfig *config, uint32_t worker_index,
                        const char *source, const char *tbset,
                        bool static_tbset,
                        const char *base_native, const char *output_native,
                        const char *output, const uint8_t digest[32],
                        char *error, size_t error_size)
{
    if (validate_toolchain(config, error, error_size)) {
        return -1;
    }
    const char *arguments[] = {
        config->compiler, "compile-module", source,
        "-o", output, "--runner", config->runner,
        "--runtime-dir", config->runtime_dir, NULL, NULL, NULL, NULL,
    };
    if (tbset) {
        arguments[9] = "--tbset";
        arguments[10] = tbset;
        if (static_tbset) arguments[11] = "--static-tbset";
    }
    char *library_path = g_strdup_printf("LD_LIBRARY_PATH=%s",
                                         config->runtime_dir);
    const char *compile_tmpdir = getenv("LATCD_COMPILE_TMPDIR");
    if (!compile_tmpdir || !*compile_tmpdir) {
        compile_tmpdir = access("/dev/shm", W_OK | X_OK) == 0 ?
                         "/dev/shm" : "/tmp";
    }
    char *temporary_path = g_strdup_printf("TMPDIR=%s", compile_tmpdir);
    char *guest_prefix = config->x86_rootfs ?
        g_strdup_printf("LAT_LD_PREFIX=%s", config->x86_rootfs) : NULL;
    const char *compile_timing = getenv("LATCD_COMPILE_TIMING") ?
        "LATC_COMPILE_TIMING=1" : NULL;
    long online = sysconf(_SC_NPROCESSORS_ONLN);
    uint32_t aot_thread_count = online > 0 ?
        MAX(1u, MIN(8u, (uint32_t)online / config->workers)) : 1u;
    struct stat tbset_status;
    if (aot_thread_count == 1 && tbset &&
        !stat(tbset, &tbset_status) &&
        tbset_status.st_size >= 512 * 1024) {
        aot_thread_count = 2;
    }
    char *aot_threads = g_strdup_printf("LATC_AOT_THREADS=%u",
                                         aot_thread_count);
    char *native_base = base_native ?
        g_strdup_printf("LATC_NATIVE_BASE=%s", base_native) : NULL;
    char *native_output = g_strdup_printf("LATC_NATIVE_OUTPUT=%s",
                                           output_native);
    char digest_text[65];
    digest_hex(digest, digest_text);
    char *trusted_digest = g_strdup_printf("LATC_TRUSTED_SOURCE_SHA256=%s",
                                            digest_text);
    const char *environment[13];
    size_t environment_count = 0;
    environment[environment_count++] = "PATH=/usr/bin:/bin";
    environment[environment_count++] = "LANG=C";
    environment[environment_count++] = "LC_ALL=C";
    environment[environment_count++] = library_path;
    environment[environment_count++] = temporary_path;
    environment[environment_count++] = aot_threads;
    environment[environment_count++] = trusted_digest;
    if (guest_prefix) environment[environment_count++] = guest_prefix;
    if (native_base) environment[environment_count++] = native_base;
    environment[environment_count++] = native_output;
    if (compile_timing) environment[environment_count++] = compile_timing;
    environment[environment_count++] = "LATC_SKIP_FINAL_INSPECT=1";
    environment[environment_count] = NULL;
    pid_t child = fork();
    if (child == 0) {
        if (setpgid(0, 0)) {
            _exit(126);
        }
        struct rlimit cpu = { config->cpu_seconds, config->cpu_seconds };
        struct rlimit memory = {
            config->address_space_limit, config->address_space_limit,
        };
        struct rlimit file = {
            config->file_size_limit, config->file_size_limit,
        };
        struct rlimit descriptors = { config->open_files, config->open_files };
        if (setrlimit(RLIMIT_CPU, &cpu) ||
            setrlimit(RLIMIT_AS, &memory) ||
            setrlimit(RLIMIT_FSIZE, &file) ||
            setrlimit(RLIMIT_NOFILE, &descriptors)) {
            _exit(126);
        }
        execve(config->compiler, (char *const *)arguments,
               (char *const *)environment);
        _exit(127);
    }
    if (child < 0) {
        g_free(library_path);
        g_free(temporary_path);
        g_free(guest_prefix);
        g_free(aot_threads);
        g_free(native_base);
        g_free(native_output);
        g_free(trusted_digest);
        return fail(error, error_size, "cannot start compiler: %s",
                    strerror(errno));
    }
    setpgid(child, child);
    compiler_process_groups[worker_index] = child;
    if (stop_requested) {
        kill(-child, SIGTERM);
    }
    int status = 0;
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    /*
     * Compiler shell scripts can exit after SIGTERM while a grandchild keeps
     * translating in the compiler's process group.  Do not lose the PGID
     * when the leader is reaped: terminate any remaining descendants before
     * marking this worker idle.
     */
    if (!kill(-child, 0) || errno == EPERM) {
        struct timespec grace = { .tv_nsec = 100 * 1000 * 1000 };
        kill(-child, SIGTERM);
        nanosleep(&grace, NULL);
        kill(-child, SIGKILL);
    }
    compiler_process_groups[worker_index] = 0;
    g_free(library_path);
    g_free(temporary_path);
    g_free(guest_prefix);
    g_free(aot_threads);
    g_free(native_base);
    g_free(native_output);
    g_free(trusted_digest);
    if (waited < 0) {
        return fail(error, error_size, "cannot wait for compiler: %s",
                    strerror(errno));
    }
    if (!WIFEXITED(status)) {
        return fail(error, error_size, "compiler terminated by signal");
    }
    if (WEXITSTATUS(status)) {
        return fail(error, error_size, "compiler failed with exit status %d",
                    WEXITSTATUS(status));
    }
    return 0;
}

static int sync_file(const char *path, char *error, size_t error_size)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0 || fsync(fd)) {
        int saved = errno;
        if (fd >= 0) {
            close(fd);
        }
        return fail(error, error_size, "cannot sync module: %s",
                    strerror(saved));
    }
    close(fd);
    return 0;
}

static int snapshot_cached_file(const char *source, const char *temporary_dir,
                                uint64_t max_size, char **snapshot,
                                char *error, size_t error_size)
{
    *snapshot = NULL;
    int input = open(source, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    struct stat status;
    if (input < 0 || fstat(input, &status) || !S_ISREG(status.st_mode) ||
        status.st_uid != geteuid() || status.st_nlink != 1 ||
        (status.st_mode & 0222) || status.st_size <= 0 ||
        (uint64_t)status.st_size > max_size) {
        int saved = errno;
        if (input >= 0) close(input);
        errno = saved ? saved : ENOEXEC;
        return fail(error, error_size,
                    "cannot open canonical native image: %s",
                    strerror(errno));
    }
    char *path = g_build_filename(temporary_dir, "base-native-XXXXXX", NULL);
    int output = g_mkstemp_full(path, O_RDWR | O_CLOEXEC, 0600);
    if (output < 0) {
        close(input);
        g_free(path);
        return fail(error, error_size,
                    "cannot create canonical native snapshot: %s",
                    strerror(errno));
    }
    unsigned char buffer[128 * 1024];
    int result = 0;
    for (;;) {
        ssize_t count = read(input, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) {
            result = fail(error, error_size,
                          "cannot read canonical native image: %s",
                          strerror(errno));
            break;
        }
        if (!count) break;
        ssize_t offset = 0;
        while (offset < count) {
            ssize_t written = write(output, buffer + offset, count - offset);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) {
                result = fail(error, error_size,
                              "cannot write canonical native snapshot: %s",
                              strerror(errno));
                break;
            }
            offset += written;
        }
        if (result) break;
    }
    if (close(input) && !result) {
        result = fail(error, error_size,
                      "cannot close canonical native image: %s",
                      strerror(errno));
    }
    if (close(output) && !result) {
        result = fail(error, error_size,
                      "cannot close canonical native snapshot: %s",
                      strerror(errno));
    }
    if (result) {
        unlink(path);
        g_free(path);
        return -1;
    }
    *snapshot = path;
    return 0;
}

static int sync_directory(const char *path, char *error, size_t error_size)
{
    int directory = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0 || fsync(directory)) {
        int saved = errno;
        if (directory >= 0) {
            close(directory);
        }
        return fail(error, error_size, "cannot sync cache directory: %s",
                    strerror(saved));
    }
    close(directory);
    return 0;
}

static int owned_module_inspect(const char *path, LatAotModuleInfoV2 *info)
{
    struct stat status;
    return !lstat(path, &status) && S_ISREG(status.st_mode) &&
           status.st_uid == geteuid() && status.st_nlink == 1 &&
           !(status.st_mode & 0222) &&
           !lat_aot_v2_module_inspect_file(path, info, NULL, 0);
}

static int cached_module_inspect(const char *path, const uint8_t digest[32],
                                 LatAotModuleInfoV2 *info)
{
    uint8_t codegen_id[32];
    expected_codegen_id(codegen_id);
    return owned_module_inspect(path, info) &&
           !memcmp(info->note.source_sha256, digest, 32) &&
           !memcmp(info->note.codegen_id, codegen_id, 32);
}

typedef struct LatcdCacheEntry {
    char *module_path;
    char *native_path;
    char *tbset_path;
    char *index_path;
    char *module_name;
    uint64_t size;
    struct timespec modified;
} LatcdCacheEntry;

static void cache_entry_free(gpointer opaque)
{
    LatcdCacheEntry *entry = opaque;
    g_free(entry->module_path);
    g_free(entry->native_path);
    g_free(entry->tbset_path);
    g_free(entry->index_path);
    g_free(entry->module_name);
    g_free(entry);
}

static int cache_entry_compare(gconstpointer left, gconstpointer right)
{
    const LatcdCacheEntry *a = *(LatcdCacheEntry *const *)left;
    const LatcdCacheEntry *b = *(LatcdCacheEntry *const *)right;
    if (a->modified.tv_sec != b->modified.tv_sec) {
        return a->modified.tv_sec < b->modified.tv_sec ? -1 : 1;
    }
    if (a->modified.tv_nsec != b->modified.tv_nsec) {
        return a->modified.tv_nsec < b->modified.tv_nsec ? -1 : 1;
    }
    return strcmp(a->module_path, b->module_path);
}

static bool cache_module_name(const char *name, char digest_text[65])
{
    size_t length = strlen(name);
    if (length != 67 && length != 132) {
        return false;
    }
    if (strcmp(name + length - 3, ".so") ||
        (length == 132 && name[64] != '-')) {
        return false;
    }
    size_t hex_length = length == 67 ? 64 : 129;
    for (size_t i = 0; i < hex_length; i++) {
        if (i == 64) {
            continue;
        }
        if (!g_ascii_isxdigit(name[i])) {
            return false;
        }
    }
    memcpy(digest_text, name, 64);
    digest_text[64] = '\0';
    return true;
}

static bool cache_generated_name(const char *name)
{
    size_t length = strlen(name);
    bool suffix = g_str_has_suffix(name, ".so") ||
                  g_str_has_suffix(name, ".native") ||
                  g_str_has_suffix(name, ".tbset");
    if (!suffix || length < 68 || name[64] != '-') return false;
    for (size_t i = 0; i < 64; i++) {
        if (!g_ascii_isxdigit(name[i])) return false;
    }
    return true;
}

static bool cache_index_points_to(const char *path, const char *module_name)
{
    gchar *contents = NULL;
    gsize size = 0;
    if (!g_file_get_contents(path, &contents, &size, NULL)) {
        return false;
    }
    char *expected = g_strdup_printf("\"%s\"", module_name);
    bool matches = strstr(contents, expected) != NULL;
    g_free(expected);
    g_free(contents);
    return matches;
}

static bool cache_grace_elapsed(time_t now, time_t then, long grace)
{
    return now >= then && (uint64_t)(now - then) >= (uint64_t)grace;
}

static uint64_t cache_add_regular_file_size(uint64_t total, const char *path)
{
    struct stat status;
    if (lstat(path, &status) || !S_ISREG(status.st_mode) ||
        status.st_size < 0) {
        return total;
    }
    uint64_t size = (uint64_t)status.st_size;
    return size > UINT64_MAX - total ? UINT64_MAX : total + size;
}

static uint64_t superseded_generations_size(const LatcdConfig *config,
                                            uint64_t total)
{
    char *root = g_build_filename(config->cache_dir,
                                  LATCD_SUPERSEDED_DIRNAME, NULL);
    GDir *directory = g_dir_open(root, 0, NULL);
    if (!directory) {
        g_free(root);
        return total;
    }
    const char *batch_name;
    while ((batch_name = g_dir_read_name(directory))) {
        char *batch = g_build_filename(root, batch_name, NULL);
        GDir *batch_directory = g_dir_open(batch, 0, NULL);
        if (batch_directory) {
            const char *name;
            while ((name = g_dir_read_name(batch_directory))) {
                char *path = g_build_filename(batch, name, NULL);
                total = cache_add_regular_file_size(total, path);
                g_free(path);
            }
            g_dir_close(batch_directory);
        }
        g_free(batch);
    }
    g_dir_close(directory);
    g_free(root);
    return total;
}

static bool cache_current_generation_name(const LatcdConfig *config,
                                          const char *name)
{
    if (!cache_generated_name(name)) {
        return false;
    }
    char source_hex[65];
    memcpy(source_hex, name, 64);
    source_hex[64] = '\0';
    LatcdCurrentManifest manifest = {0};
    int manifest_result = current_manifest_read(config, source_hex, &manifest);
    if (manifest_result) {
        current_manifest_clear(&manifest);
        /* A malformed or unreadable current manifest is not evidence that
         * generated files are orphaned.  Preserve them until the manifest
         * can be read again; only ENOENT means no current generation. */
        return manifest_result > 0 ? false : true;
    }
    bool current = !strcmp(name, manifest.module) ||
                   !strcmp(name, manifest.native) ||
                   (*manifest.tbset && !strcmp(name, manifest.tbset));
    current_manifest_clear(&manifest);
    return current;
}

static void prune_superseded_generations(const LatcdConfig *config)
{
    char *root = g_build_filename(config->cache_dir,
                                  LATCD_SUPERSEDED_DIRNAME, NULL);
    GDir *directory = g_dir_open(root, 0, NULL);
    if (!directory) {
        g_free(root);
        return;
    }
    long grace = superseded_grace_seconds();
    time_t now = time(NULL);
    const char *batch_name;
    while ((batch_name = g_dir_read_name(directory))) {
        char *batch = g_build_filename(root, batch_name, NULL);
        struct stat status;
        if (lstat(batch, &status) || !S_ISDIR(status.st_mode)) {
            g_free(batch);
            continue;
        }
        if (now < status.st_mtime) {
            struct timespec times[2] = {
                { .tv_nsec = UTIME_OMIT },
                { .tv_nsec = UTIME_NOW },
            };
            if (utimensat(AT_FDCWD, batch, times, AT_SYMLINK_NOFOLLOW)) {
                fprintf(stderr,
                        "latcd: cannot normalize superseded batch time %s: %s\n",
                        batch, strerror(errno));
                g_free(batch);
                continue;
            }
            status.st_mtime = now;
        }
        if (!cache_grace_elapsed(now, status.st_mtime, grace)) {
            g_free(batch);
            continue;
        }
        GDir *batch_directory = g_dir_open(batch, 0, NULL);
        if (batch_directory) {
            const char *name;
            while ((name = g_dir_read_name(batch_directory))) {
                char *path = g_build_filename(batch, name, NULL);
                if (unlink(path) && errno != ENOENT) {
                    fprintf(stderr,
                            "latcd: cannot remove superseded file %s: %s\n",
                            path, strerror(errno));
                }
                g_free(path);
            }
            g_dir_close(batch_directory);
        }
        if (rmdir(batch) && errno != ENOENT) {
            fprintf(stderr, "latcd: cannot remove superseded batch %s: %s\n",
                    batch, strerror(errno));
        }
        g_free(batch);
    }
    g_dir_close(directory);
    g_free(root);
}

static char *create_superseded_batch(const LatcdConfig *config,
                                     const char *source_hex)
{
    char *root = g_build_filename(config->cache_dir,
                                  LATCD_SUPERSEDED_DIRNAME, NULL);
    if (mkdir(root, 0700) && errno != EEXIST) {
        fprintf(stderr, "latcd: cannot create superseded directory %s: %s\n",
                root, strerror(errno));
        g_free(root);
        return NULL;
    }
    time_t now = time(NULL);
    for (unsigned int attempt = 0; attempt < 1000; attempt++) {
        char *batch = g_strdup_printf("%s/%s-%lld-%ld-%u", root, source_hex,
                                      (long long)now, (long)getpid(), attempt);
        if (!mkdir(batch, 0700)) {
            g_free(root);
            return batch;
        }
        if (errno != EEXIST) {
            fprintf(stderr, "latcd: cannot create superseded batch %s: %s\n",
                    batch, strerror(errno));
            g_free(batch);
            g_free(root);
            return NULL;
        }
        g_free(batch);
    }
    g_free(root);
    return NULL;
}

static void reconcile_superseded_generations(const LatcdConfig *config)
{
    GDir *directory = g_dir_open(config->cache_dir, 0, NULL);
    if (!directory) {
        return;
    }
    char *batch = NULL;
    unsigned int moved = 0;
    const char *name;
    while ((name = g_dir_read_name(directory))) {
        if (!cache_generated_name(name) ||
            cache_current_generation_name(config, name)) {
            continue;
        }
        if (!batch) {
            batch = create_superseded_batch(config, "reconcile");
        }
        if (!batch) {
            break;
        }
        char *path = g_build_filename(config->cache_dir, name, NULL);
        char *retired = g_build_filename(batch, name, NULL);
        if (rename(path, retired)) {
            if (errno != ENOENT) {
                fprintf(stderr, "latcd: cannot reconcile superseded file %s: %s\n",
                        path, strerror(errno));
            }
        } else {
            moved++;
        }
        g_free(retired);
        g_free(path);
    }
    g_dir_close(directory);
    if (batch && !moved) {
        rmdir(batch);
    }
    g_free(batch);
}

static int cache_make_room(const LatcdConfig *config, uint64_t incoming,
                           const char *incoming_name, char *error,
                           size_t error_size)
{
    if (incoming > config->max_cache_bytes) {
        return fail(error, error_size,
                    "compiled module exceeds cache capacity");
    }
    reconcile_superseded_generations(config);
    prune_superseded_generations(config);
    GError *directory_error = NULL;
    GDir *directory = g_dir_open(config->cache_dir, 0, &directory_error);
    if (!directory) {
        int result = fail(error, error_size, "cannot scan cache: %s",
                          directory_error ? directory_error->message :
                          "unknown error");
        g_clear_error(&directory_error);
        return result;
    }
    uint64_t total = superseded_generations_size(config, 0);
    GPtrArray *entries = g_ptr_array_new_with_free_func(cache_entry_free);
    const char *name;
    while ((name = g_dir_read_name(directory))) {
        char digest_text[65];
        bool is_module = cache_module_name(name, digest_text);
        if (!is_module && !cache_generated_name(name)) {
            continue;
        }
        char *module_path = g_build_filename(config->cache_dir, name, NULL);
        struct stat status;
        if (lstat(module_path, &status) || !S_ISREG(status.st_mode) ||
            status.st_size < 0) {
            g_free(module_path);
            continue;
        }
        if ((uint64_t)status.st_size > UINT64_MAX - total) {
            total = UINT64_MAX;
        } else {
            total += status.st_size;
        }
        if (!is_module) {
            g_free(module_path);
            continue;
        }
        uint8_t digest[32];
        for (size_t i = 0; i < 32; i++) {
            int high = g_ascii_xdigit_value(digest_text[i * 2]);
            int low = g_ascii_xdigit_value(digest_text[i * 2 + 1]);
            digest[i] = (high << 4) | low;
        }
        if (!strcmp(name, incoming_name)) {
            g_free(module_path);
            continue;
        }
        /* Keep the currently published generation for this source until the
         * new manifest becomes the commit point. */
        if (!strncmp(name, incoming_name, 64)) {
            g_free(module_path);
            continue;
        }
        LatAotModuleInfoV2 info;
        if (owned_module_inspect(module_path, &info) &&
            !memcmp(info.note.source_sha256, digest, 32)) {
            LatcdCurrentManifest manifest = {0};
            if (current_manifest_read(config, digest_text, &manifest) ||
                strcmp(manifest.module, name)) {
                current_manifest_clear(&manifest);
                g_free(module_path);
                continue;
            }
            LatcdCacheEntry *entry = g_new0(LatcdCacheEntry, 1);
            entry->module_path = module_path;
            entry->index_path = g_strdup_printf("%s/%s.current",
                                                config->cache_dir,
                                                digest_text);
            entry->module_name = g_strdup(name);
            entry->size = status.st_size;
            entry->native_path = g_build_filename(config->cache_dir,
                                                    manifest.native, NULL);
            if (*manifest.tbset) {
                entry->tbset_path = g_build_filename(config->cache_dir,
                                                       manifest.tbset, NULL);
            }
            struct stat auxiliary;
            if (!lstat(entry->native_path, &auxiliary) &&
                S_ISREG(auxiliary.st_mode) && auxiliary.st_size > 0) {
                entry->size += auxiliary.st_size;
            }
            if (entry->tbset_path &&
                !lstat(entry->tbset_path, &auxiliary) &&
                S_ISREG(auxiliary.st_mode) && auxiliary.st_size > 0) {
                entry->size += auxiliary.st_size;
            }
            entry->modified = status.st_mtim;
            g_ptr_array_add(entries, entry);
            current_manifest_clear(&manifest);
        } else {
            g_free(module_path);
        }
    }
    g_dir_close(directory);
    g_ptr_array_sort(entries, cache_entry_compare);
    for (guint i = 0;
         incoming > config->max_cache_bytes - MIN(total,
                                                   config->max_cache_bytes) &&
         i < entries->len;
         i++) {
        LatcdCacheEntry *entry = g_ptr_array_index(entries, i);
        if (cache_index_points_to(entry->index_path, entry->module_name) &&
            !unlink(entry->index_path)) {
            unlink(entry->module_path);
            if (entry->native_path) unlink(entry->native_path);
            if (entry->tbset_path) unlink(entry->tbset_path);
            total = total > entry->size ? total - entry->size : 0;
        }
    }
    int result = 0;
    if (total > config->max_cache_bytes - incoming) {
        result = fail(error, error_size,
                      "cache capacity is occupied by non-evictable files");
    }
    g_ptr_array_free(entries, TRUE);
    return result;
}

static void current_manifest_clear(LatcdCurrentManifest *manifest)
{
    g_free(manifest->module);
    g_free(manifest->native);
    g_free(manifest->tbset);
    memset(manifest, 0, sizeof(*manifest));
}

static int publish_current_index(const LatcdConfig *config, const char *hex,
                                 const char *module_name,
                                 const char *native_name,
                                 const char *tbset_name,
                                 const LatAotModuleInfoV2 *info,
                                 uint64_t published_sequence,
                                 char *error, size_t error_size)
{
    char *temporary_dir = g_build_filename(config->cache_dir, ".tmp", NULL);
    char *temporary = g_build_filename(temporary_dir, "current-XXXXXX", NULL);
    int fd = g_mkstemp_full(temporary, O_RDWR | O_CLOEXEC, 0600);
    if (fd < 0) {
        g_free(temporary_dir);
        g_free(temporary);
        return fail(error, error_size, "cannot create current index: %s",
                    strerror(errno));
    }
    char codegen[65];
    digest_hex(info->note.codegen_id, codegen);
    GString *contents = g_string_new(NULL);
    g_string_append_printf(contents,
        "{\"version\":2,\"module\":\"%s\",\"native\":\"%s\","
        "\"tbset\":\"%s\",\"source_sha256\":\"%s\","
        "\"codegen_id\":\"%s\","
        "\"published_sequence\":%" PRIu64 "}\n",
        module_name, native_name, tbset_name, hex, codegen,
        published_sequence);
    ssize_t written = write(fd, contents->str, contents->len);
    int result = 0;
    if (written != (ssize_t)contents->len || fchmod(fd, 0444) || fsync(fd)) {
        result = fail(error, error_size, "cannot write current index: %s",
                      strerror(errno));
    }
    g_string_free(contents, TRUE);
    if (close(fd) && !result) {
        result = fail(error, error_size, "cannot close current index: %s",
                      strerror(errno));
    }
    char *final = g_strdup_printf("%s/%s.current", config->cache_dir, hex);
    if (!result && rename(temporary, final)) {
        result = fail(error, error_size, "cannot publish current index: %s",
                      strerror(errno));
    }
    if (result) {
        unlink(temporary);
    }
    g_free(final);
    g_free(temporary);
    g_free(temporary_dir);
    return result;
}

static char *manifest_string(const char *contents, const char *field)
{
    char *prefix = g_strdup_printf("\"%s\":\"", field);
    const char *begin = strstr(contents, prefix);
    g_free(prefix);
    if (!begin) return NULL;
    begin = strchr(begin, ':') + 2;
    const char *end = strchr(begin, '\"');
    if (!end || end - begin >= 192 ||
        memchr(begin, '/', end - begin)) return NULL;
    return g_strndup(begin, end - begin);
}

static int current_manifest_read(const LatcdConfig *config, const char *hex,
                                 LatcdCurrentManifest *manifest)
{
    char *path = g_strdup_printf("%s/%s.current", config->cache_dir, hex);
    gchar *contents = NULL;
    gsize size = 0;
    GError *read_error = NULL;
    if (!g_file_get_contents(path, &contents, &size, &read_error)) {
        int missing = read_error && read_error->domain == G_FILE_ERROR &&
                      read_error->code == G_FILE_ERROR_NOENT;
        g_clear_error(&read_error);
        g_free(path);
        errno = missing ? ENOENT : EIO;
        return missing ? 1 : -1;
    }
    g_free(path);
    if (!size || size >= 4096 || !strstr(contents, "\"version\":2")) {
        g_free(contents);
        errno = ENOEXEC;
        return -1;
    }
    manifest->module = manifest_string(contents, "module");
    manifest->native = manifest_string(contents, "native");
    manifest->tbset = manifest_string(contents, "tbset");
    const char *sequence = strstr(contents, "\"published_sequence\":");
    const char *sequence_value = sequence ? strchr(sequence, ':') + 1 : NULL;
    char *end = NULL;
    manifest->published_sequence = sequence ?
        g_ascii_strtoull(sequence_value, &end, 10) : 0;
    int result = !manifest->module || !manifest->native || !manifest->tbset ||
        strlen(manifest->module) <= 3 ||
        strcmp(manifest->module + strlen(manifest->module) - 3, ".so") ||
        strlen(manifest->native) <= 7 ||
        strcmp(manifest->native + strlen(manifest->native) - 7, ".native") ||
        (*manifest->tbset && (strlen(manifest->tbset) <= 6 ||
         strcmp(manifest->tbset + strlen(manifest->tbset) - 6, ".tbset"))) ||
        strncmp(manifest->module, hex, 64) ||
        strncmp(manifest->native, hex, 64) ||
        (*manifest->tbset && strncmp(manifest->tbset, hex, 64)) ||
        !sequence || end == sequence_value || (*end != ',' && *end != '}');
    g_free(contents);
    if (result) {
        current_manifest_clear(manifest);
        errno = ENOEXEC;
    }
    return result;
}

/*
 * 被取代的模块代在宽限期内不得删除：运行中的进程可能仍映射并执行它们。
 * 默认 600 秒，可用 LATCD_SUPERSEDED_GRACE_SECONDS 调整（0 表示立即删除）。
 */
static long superseded_grace_seconds(void)
{
    const char *text = getenv("LATCD_SUPERSEDED_GRACE_SECONDS");
    if (text && *text) {
        char *end = NULL;
        errno = 0;
        long parsed = strtol(text, &end, 10);
        if (!errno && end && *end == '\0' && parsed >= 0 &&
            parsed <= INT_MAX) {
            return parsed;
        }
    }
    return 600;
}

static void remove_superseded_generation(const LatcdConfig *config,
                                         const char *source_hex,
                                         const char *module_name,
                                         const char *native_name,
                                         const char *tbset_name)
{
    GDir *directory = g_dir_open(config->cache_dir, 0, NULL);
    if (!directory) return;
    char prefix[66];
    snprintf(prefix, sizeof(prefix), "%s-", source_hex);
    char *batch = NULL;
    unsigned int moved = 0;
    const char *name;
    while ((name = g_dir_read_name(directory))) {
        bool generated = g_str_has_suffix(name, ".so") ||
                         g_str_has_suffix(name, ".native") ||
                         g_str_has_suffix(name, ".tbset");
        if (!generated || !g_str_has_prefix(name, prefix) ||
            !strcmp(name, module_name) || !strcmp(name, native_name) ||
            !strcmp(name, tbset_name)) {
            continue;
        }
        char *path = g_build_filename(config->cache_dir, name, NULL);
        if (!batch) {
            batch = create_superseded_batch(config, source_hex);
        }
        if (!batch) {
            g_free(path);
            break;
        }
        char *retired = g_build_filename(batch, name, NULL);
        if (rename(path, retired)) {
            if (errno != ENOENT) {
                fprintf(stderr, "latcd: cannot retire superseded file %s: %s\n",
                        path, strerror(errno));
            }
        } else {
            moved++;
        }
        g_free(retired);
        g_free(path);
    }
    g_dir_close(directory);
    if (batch && !moved) {
        rmdir(batch);
    }
    g_free(batch);
    prune_superseded_generations(config);
}

/* Publish a newer complete observed-key set when the current module already
 * contains every key.  Return 1 if the current generation changed or stopped
 * being usable and the caller must fall back to a full compile. */
static int publish_covered_tbset(const LatcdConfig *config,
                                 const char *complete_tbset,
                                 const uint8_t digest[32],
                                 uint64_t published_sequence,
                                 LatcdResponseV2 *response)
{
    char error[sizeof(response->message)] = {0};
    gchar *tbset_data = NULL;
    gsize tbset_size = 0;
    char *temporary_dir = NULL;
    char *temporary = NULL;
    char *final_tbset = NULL;
    char *tbset_name = NULL;
    char *module_path = NULL;
    char *native_path = NULL;
    bool tbset_staged = false;
    int result = -1;
    memcpy(response->source_sha256, digest, 32);
    if (!g_file_get_contents(complete_tbset, &tbset_data,
                             &tbset_size, NULL)) {
        fail(error, sizeof(error), "cannot read complete TB set");
        goto out;
    }
    char source_hex[65];
    digest_hex(digest, source_hex);
    GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(checksum, (const guchar *)tbset_data, tbset_size);
    tbset_name = g_strdup_printf("%s-%s.tbset", source_hex,
                                  g_checksum_get_string(checksum));
    g_checksum_free(checksum);
    final_tbset = g_build_filename(config->cache_dir, tbset_name, NULL);
    temporary_dir = g_build_filename(config->cache_dir, ".tmp", NULL);
    temporary = g_build_filename(temporary_dir, "published-XXXXXX", NULL);
    int fd = g_mkstemp_full(temporary, O_RDWR | O_CLOEXEC, 0600);
    bool stage_failed = fd < 0;
    gsize offset = 0;
    while (!stage_failed && offset < tbset_size) {
        ssize_t written = write(fd, tbset_data + offset,
                                tbset_size - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) {
            stage_failed = true;
        } else {
            offset += written;
        }
    }
    if (!stage_failed && (fchmod(fd, 0444) || fsync(fd))) {
        stage_failed = true;
    }
    if (fd >= 0 && close(fd)) stage_failed = true;
    if (stage_failed) {
        fail(error, sizeof(error), "cannot stage complete TB set: %s",
             strerror(errno));
        goto out;
    }

    pthread_mutex_lock(&cache_lock);
    LatcdCurrentManifest manifest = {0};
    LatAotModuleInfoV2 info;
    bool covered = !current_manifest_read(config, source_hex, &manifest) &&
                   *manifest.tbset;
    if (covered) {
        module_path = g_build_filename(config->cache_dir,
                                        manifest.module, NULL);
        native_path = g_build_filename(config->cache_dir,
                                        manifest.native, NULL);
        covered = cached_module_inspect(module_path, digest, &info) &&
                  !access(native_path, R_OK) &&
                  !lat_aot_v2_module_validate_tbset_file(
                      module_path, complete_tbset, error, sizeof(error));
    }
    if (!covered) {
        current_manifest_clear(&manifest);
        pthread_mutex_unlock(&cache_lock);
        result = 1;
        goto out;
    }
    if (strcmp(manifest.tbset, tbset_name)) {
        if (cache_make_room(config, tbset_size, manifest.module,
                            error, sizeof(error)) ||
            rename(temporary, final_tbset)) {
            if (!error[0]) {
                fail(error, sizeof(error),
                     "cannot publish complete TB set: %s", strerror(errno));
            }
            current_manifest_clear(&manifest);
            pthread_mutex_unlock(&cache_lock);
            goto out;
        }
        temporary[0] = '\0';
        tbset_staged = true;
        if (sync_directory(config->cache_dir, error, sizeof(error))) {
            current_manifest_clear(&manifest);
            pthread_mutex_unlock(&cache_lock);
            goto out;
        }
        const char *failpoint = getenv("LATCD_TEST_FAIL_BEFORE_MANIFEST");
        if (failpoint && *failpoint && !access(failpoint, F_OK)) {
            unlink(failpoint);
            fail(error, sizeof(error),
                 "injected failure before manifest commit");
            current_manifest_clear(&manifest);
            pthread_mutex_unlock(&cache_lock);
            goto out;
        }
    }
    if (publish_current_index(config, source_hex, manifest.module,
                              manifest.native, tbset_name, &info,
                              published_sequence, error, sizeof(error))) {
        current_manifest_clear(&manifest);
        pthread_mutex_unlock(&cache_lock);
        goto out;
    }
    tbset_staged = false;
    char sync_error[128] = {0};
    if (sync_directory(config->cache_dir, sync_error,
                       sizeof(sync_error))) {
        fprintf(stderr,
                "latcd: committed covered-TB manifest sync failed: %s\n",
                sync_error);
    }
    remove_superseded_generation(config, source_hex, manifest.module,
                                 manifest.native, tbset_name);
    utimensat(AT_FDCWD, module_path, NULL, AT_SYMLINK_NOFOLLOW);
    snprintf(error, sizeof(error), "published covered TB set: %s",
             module_path);
    current_manifest_clear(&manifest);
    pthread_mutex_unlock(&cache_lock);
    response->status = LATCD_STATUS_OK;
    result = 0;
out:
    if (result < 0) {
        response->status = LATCD_STATUS_IO_ERROR;
        g_strlcpy(response->message, error[0] ? error : "unknown error",
                  sizeof(response->message));
    } else if (!result) {
        g_strlcpy(response->message, error, sizeof(response->message));
    }
    if (temporary && *temporary) unlink(temporary);
    if (tbset_staged) unlink(final_tbset);
    g_free(native_path);
    g_free(module_path);
    g_free(tbset_name);
    g_free(final_tbset);
    g_free(temporary);
    g_free(temporary_dir);
    g_free(tbset_data);
    return result;
}

static int publish_snapshot(const LatcdConfig *config, uint32_t worker_index,
                            const char *snapshot, const char *compile_tbset,
                            const char *complete_tbset,
                            bool static_tbset,
                            const uint8_t digest[32],
                            uint64_t published_sequence,
                            LatcdResponseV2 *response)
{
    char error[sizeof(response->message)] = {0};
    char *temporary_dir = g_build_filename(config->cache_dir, ".tmp", NULL);
    char *module = NULL;
    char *native = NULL;
    char *tbset_copy = NULL;
    char *final_module = NULL;
    char *final_native = NULL;
    char *final_tbset = NULL;
    char *base_native = NULL;
    char *module_name = NULL;
    char *native_name = NULL;
    char *tbset_name = NULL;
    gchar *tbset_data = NULL;
    gsize tbset_size = 0;
    bool module_staged = false;
    bool native_staged = false;
    bool tbset_staged = false;
    int status = LATCD_STATUS_IO_ERROR;
    memcpy(response->source_sha256, digest, 32);
    char hex[65];
    digest_hex(digest, hex);
    char tbset_hex[65] = {0};
    if (complete_tbset) {
        if (!g_file_get_contents(complete_tbset, &tbset_data,
                                 &tbset_size, NULL)) {
            fail(error, sizeof(error), "cannot read validated TB set");
            goto out;
        }
        GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
        g_checksum_update(checksum, (const guchar *)tbset_data, tbset_size);
        if (static_tbset) {
            /* A checked compile must stage a fresh generation even when
             * its keys match the currently published generation. */
            g_checksum_update(checksum, (const guchar *)snapshot,
                              strlen(snapshot));
        }
        g_strlcpy(tbset_hex, g_checksum_get_string(checksum),
                  sizeof(tbset_hex));
        g_checksum_free(checksum);
    }
    module_name = complete_tbset ?
        g_strdup_printf("%s-%s.so", hex, tbset_hex) :
        g_strdup_printf("%s.so", hex);
    native_name = complete_tbset ?
        g_strdup_printf("%s-%s.native", hex, tbset_hex) :
        g_strdup_printf("%s.native", hex);
    tbset_name = complete_tbset ?
        g_strdup_printf("%s-%s.tbset", hex, tbset_hex) : g_strdup("");
    final_module = g_build_filename(config->cache_dir, module_name, NULL);
    final_native = g_build_filename(config->cache_dir, native_name, NULL);
    if (complete_tbset) {
        final_tbset = g_build_filename(config->cache_dir, tbset_name, NULL);
    }

    LatAotModuleInfoV2 info;
    pthread_mutex_lock(&cache_lock);
    bool cache_hit = !static_tbset &&
        cached_module_inspect(final_module, digest, &info) &&
        !access(final_native, R_OK) &&
        (!complete_tbset || !access(final_tbset, R_OK));
    if (cache_hit && complete_tbset) {
        cache_hit = !lat_aot_v2_module_inspect_and_validate_tbset_file(
            final_module, complete_tbset, &info, error, sizeof(error));
    }
    if (cache_hit) {
        int publish_result = publish_current_index(
            config, hex, module_name, native_name, tbset_name, &info,
            published_sequence, error, sizeof(error));
        if (publish_result) {
            pthread_mutex_unlock(&cache_lock);
            goto out;
        }
        char sync_error[128] = {0};
        if (sync_directory(config->cache_dir, sync_error,
                           sizeof(sync_error))) {
            fprintf(stderr,
                    "latcd: committed cache-hit manifest sync failed: %s\n",
                    sync_error);
        }
        remove_superseded_generation(config, hex, module_name, native_name,
                                     tbset_name);
        utimensat(AT_FDCWD, final_module, NULL, AT_SYMLINK_NOFOLLOW);
        pthread_mutex_unlock(&cache_lock);
        snprintf(error, sizeof(error), "cache hit: %s", final_module);
        status = LATCD_STATUS_OK;
        goto out;
    }
    if (!static_tbset && complete_tbset && compile_tbset &&
        strcmp(compile_tbset, complete_tbset)) {
        LatcdCurrentManifest previous = {0};
        if (!current_manifest_read(config, hex, &previous) &&
            persisted_manifest_valid(config, hex, digest)) {
            char *cached_base = g_build_filename(config->cache_dir,
                                                  previous.native, NULL);
            if (snapshot_cached_file(cached_base, temporary_dir,
                                     config->file_size_limit, &base_native,
                                     error, sizeof(error))) {
                g_free(cached_base);
                current_manifest_clear(&previous);
                pthread_mutex_unlock(&cache_lock);
                goto out;
            }
            g_free(cached_base);
        }
        current_manifest_clear(&previous);
    }
    pthread_mutex_unlock(&cache_lock);
    if (!base_native) compile_tbset = complete_tbset;

    module = g_build_filename(temporary_dir, "module-XXXXXX", NULL);
    int placeholder = g_mkstemp_full(module, O_RDWR | O_CLOEXEC, 0600);
    if (placeholder < 0) {
        fail(error, sizeof(error), "cannot reserve module path: %s",
             strerror(errno));
        goto out;
    }
    close(placeholder);
    unlink(module);
    native = g_build_filename(temporary_dir, "native-XXXXXX", NULL);
    placeholder = g_mkstemp_full(native, O_RDWR | O_CLOEXEC, 0600);
    if (placeholder < 0) {
        fail(error, sizeof(error), "cannot reserve native image path: %s",
             strerror(errno));
        goto out;
    }
    close(placeholder);
    unlink(native);
    int compiler_result = run_compiler(config, worker_index, snapshot,
                                       compile_tbset, static_tbset,
                                       base_native, native,
                                       module, digest, error, sizeof(error));
    if (compiler_result) {
        status = LATCD_STATUS_COMPILE_FAILED;
        goto out;
    }
    uint8_t codegen_id[32];
    expected_codegen_id(codegen_id);
    int inspect_result = complete_tbset ?
        lat_aot_v2_module_inspect_and_validate_tbset_file(
            module, complete_tbset, &info, error, sizeof(error)) :
        lat_aot_v2_module_inspect_file(module, &info,
                                       error, sizeof(error));
    if (inspect_result || memcmp(info.note.source_sha256, digest, 32) ||
        memcmp(info.note.codegen_id, codegen_id, 32)) {
        if (!error[0]) {
            snprintf(error, sizeof(error),
                     "compiled module source or codegen identity mismatch");
        }
        status = LATCD_STATUS_INVALID_MODULE;
        goto out;
    }
    if (chmod(module, 0444) || chmod(native, 0444)) {
        fail(error, sizeof(error), "cannot protect compiled output: %s",
             strerror(errno));
        goto out;
    }
    if (sync_file(module, error, sizeof(error)) ||
        sync_file(native, error, sizeof(error))) {
        goto out;
    }
    struct stat module_status, native_status;
    if (lstat(module, &module_status) || !S_ISREG(module_status.st_mode) ||
        lstat(native, &native_status) || !S_ISREG(native_status.st_mode)) {
        fail(error, sizeof(error), "cannot inspect compiled output: %s",
             strerror(errno));
        goto out;
    }
    if (complete_tbset) {
        tbset_copy = g_build_filename(temporary_dir, "published-XXXXXX", NULL);
        int fd = g_mkstemp_full(tbset_copy, O_RDWR | O_CLOEXEC, 0600);
        bool stage_failed = fd < 0;
        if (!stage_failed &&
            (write(fd, tbset_data, tbset_size) != (ssize_t)tbset_size ||
             fchmod(fd, 0444) || fsync(fd))) {
            stage_failed = true;
        }
        if (fd >= 0 && close(fd)) stage_failed = true;
        if (stage_failed) {
            fail(error, sizeof(error), "cannot stage published TB set: %s",
                 strerror(errno));
            goto out;
        }
    }

    pthread_mutex_lock(&cache_lock);
    uint64_t incoming = (uint64_t)module_status.st_size +
                        (uint64_t)native_status.st_size + tbset_size;
    if (cache_make_room(config, incoming, module_name,
                        error, sizeof(error))) {
        pthread_mutex_unlock(&cache_lock);
        goto out;
    }
    if (!rename(module, final_module)) module_staged = true;
    if (module_staged && !rename(native, final_native)) native_staged = true;
    if (native_staged && complete_tbset &&
        !rename(tbset_copy, final_tbset)) tbset_staged = true;
    if (!module_staged || !native_staged ||
        (complete_tbset && !tbset_staged)) {
        if (!error[0]) {
            fail(error, sizeof(error), "cannot publish compiled files: %s",
                 strerror(errno));
        }
        pthread_mutex_unlock(&cache_lock);
        goto out;
    }
    module[0] = native[0] = '\0';
    if (tbset_copy) tbset_copy[0] = '\0';
    if (sync_directory(config->cache_dir, error, sizeof(error))) {
        pthread_mutex_unlock(&cache_lock);
        goto out;
    }
    const char *failpoint = getenv("LATCD_TEST_FAIL_BEFORE_MANIFEST");
    if (failpoint && *failpoint && !access(failpoint, F_OK)) {
        unlink(failpoint);
        fail(error, sizeof(error), "injected failure before manifest commit");
        pthread_mutex_unlock(&cache_lock);
        goto out;
    }
    int publish_result = publish_current_index(
        config, hex, module_name, native_name, tbset_name, &info,
        published_sequence, error, sizeof(error));
    if (publish_result) {
        pthread_mutex_unlock(&cache_lock);
        goto out;
    }
    module_staged = native_staged = tbset_staged = false;
    char sync_error[128] = {0};
    if (sync_directory(config->cache_dir, sync_error,
                       sizeof(sync_error))) {
        fprintf(stderr, "latcd: committed manifest directory sync failed: %s\n",
                sync_error);
    }
    remove_superseded_generation(config, hex, module_name, native_name,
                                 tbset_name);
    pthread_mutex_unlock(&cache_lock);
    snprintf(error, sizeof(error), "published: %s", final_module);
    status = LATCD_STATUS_OK;
out:
    response->status = status;
    g_strlcpy(response->message, error[0] ? error : "unknown error",
              sizeof(response->message));
    if (module && *module) unlink(module);
    if (native && *native) unlink(native);
    if (tbset_copy && *tbset_copy) unlink(tbset_copy);
    if (module_staged) unlink(final_module);
    if (native_staged) unlink(final_native);
    if (tbset_staged) unlink(final_tbset);
    g_free(tbset_data);
    if (base_native) unlink(base_native);
    g_free(base_native);
    g_free(final_tbset);
    g_free(final_native);
    g_free(final_module);
    g_free(tbset_name);
    g_free(native_name);
    g_free(module_name);
    g_free(tbset_copy);
    g_free(native);
    g_free(module);
    g_free(temporary_dir);
    return status ? -1 : 0;
}

static int process_request(const LatcdConfig *config, int source_fd,
                           int tbset_fd, bool static_tbset,
                           LatcdResponseV2 *response)
{
    char error[sizeof(response->message)] = {0};
    char *temporary_dir = g_build_filename(config->cache_dir, ".tmp", NULL);
    char *snapshot = NULL;
    char *tbset = NULL;
    char *canonical = NULL;
    int result = -1;
    if (ensure_private_directory(config->cache_dir, error, sizeof(error)) ||
        ensure_private_directory(temporary_dir, error, sizeof(error))) {
        response->status = LATCD_STATUS_IO_ERROR;
        goto out;
    }
    struct stat identity;
    if (snapshot_source(source_fd, temporary_dir, config->max_input, &snapshot,
                        response->source_sha256, &identity,
                        error, sizeof(error))) {
        response->status = LATCD_STATUS_BAD_SOURCE;
        goto out;
    }
    publish_source_identity(config, &identity, response->source_sha256);
    if (tbset_fd >= 0 && snapshot_tbset(
            tbset_fd, snapshot, temporary_dir, response->source_sha256,
            &tbset, error, sizeof(error))) {
        response->status = LATCD_STATUS_BAD_REQUEST;
        goto out;
    }
    if (tbset && static_tbset &&
        prepare_static_tbset(config, response->source_sha256, tbset,
                             &canonical, error, sizeof(error))) {
        response->status = LATCD_STATUS_BAD_REQUEST;
        goto out;
    }
    if (tbset && !static_tbset &&
        merge_tbset(config, response->source_sha256, tbset,
                               &canonical, NULL, error, sizeof(error))) {
        response->status = LATCD_STATUS_BAD_REQUEST;
        goto out;
    }
    result = publish_snapshot(config, 0, snapshot,
                              canonical ? canonical : tbset,
                              canonical ? canonical : tbset,
                              static_tbset,
                              response->source_sha256, 0,
                              response);
out:
    if (result && !response->message[0]) {
        g_strlcpy(response->message, error[0] ? error : "unknown error",
                  sizeof(response->message));
    }
    if (snapshot) {
        unlink(snapshot);
    }
    g_free(snapshot);
    if (tbset) unlink(tbset);
    g_free(tbset);
    g_free(canonical);
    g_free(temporary_dir);
    return result;
}

static void job_free(LatcdJob *job)
{
    if (!job) {
        return;
    }
    if (job->snapshot) {
        unlink(job->snapshot);
    }
    if (job->tbset && !job->tbset_canonical) {
        unlink(job->tbset);
    }
    g_free(job->snapshot);
    g_free(job->tbset);
    g_free(job);
}

static int cache_contains(const LatcdConfig *config, const uint8_t digest[32])
{
    char hex[65];
    digest_hex(digest, hex);
    LatcdCurrentManifest manifest = {0};
    pthread_mutex_lock(&cache_lock);
    int valid = !current_manifest_read(config, hex, &manifest);
    char *module = valid ? g_build_filename(config->cache_dir,
                                             manifest.module, NULL) : NULL;
    char *native = valid ? g_build_filename(config->cache_dir,
                                             manifest.native, NULL) : NULL;
    LatAotModuleInfoV2 info;
    valid = valid && cached_module_inspect(module, digest, &info) &&
            !access(native, R_OK);
    pthread_mutex_unlock(&cache_lock);
    g_free(native);
    g_free(module);
    current_manifest_clear(&manifest);
    return valid;
}

static int cache_contains_tbset(const LatcdConfig *config,
                                const uint8_t digest[32],
                                const char *tbset)
{
    LatTbKeySet requested = {0};
    LatTbKeySet published = {0};
    LatTbKeySet missing = {0};
    char error[128] = {0};
    char hex[65];
    digest_hex(digest, hex);
    LatcdCurrentManifest manifest = {0};
    int manifest_valid = !current_manifest_read(config, hex, &manifest) &&
                         *manifest.tbset;
    char *path = manifest_valid ?
        g_build_filename(config->cache_dir, manifest.tbset, NULL) : NULL;
    int valid = !lat_tb_key_set_read_file(tbset, digest, &requested,
                                           error, sizeof(error)) &&
                path && !lat_tb_key_set_read_file(path, digest, &published,
                                           error, sizeof(error)) &&
                !lat_tb_key_set_difference(&requested, &published, &missing,
                                            error, sizeof(error)) &&
                !missing.count;
    pthread_mutex_lock(&cache_lock);
    char *module_path = manifest_valid ? g_build_filename(
        config->cache_dir, manifest.module, NULL) : NULL;
    char *native_path = manifest_valid ? g_build_filename(
        config->cache_dir, manifest.native, NULL) : NULL;
    LatAotModuleInfoV2 info;
    valid = valid && cached_module_inspect(module_path, digest, &info) &&
            !access(native_path, R_OK);
    pthread_mutex_unlock(&cache_lock);
    g_free(native_path);
    g_free(module_path);
    lat_tb_key_set_destroy(&requested);
    lat_tb_key_set_destroy(&published);
    lat_tb_key_set_destroy(&missing);
    current_manifest_clear(&manifest);
    g_free(path);
    return valid;
}

static void write_stats_locked(LatcdService *service)
{
    if (!service->config->stats_path) {
        return;
    }
    char *temporary = g_strdup_printf("%s.tmp.%ld",
        service->config->stats_path, (long)getpid());
    char contents[1024];
    int length = snprintf(contents, sizeof(contents),
        "{\"requests\":%" PRIu64 ",\"queued\":%" PRIu64
        ",\"deduplicated\":%" PRIu64 ",\"cache_hits\":%" PRIu64
        ",\"compiled\":%" PRIu64 ",\"failed\":%" PRIu64
        ",\"negative_hits\":%" PRIu64 ",\"queue_full\":%" PRIu64
        ",\"queue_depth\":%u,\"queue_bytes\":%" PRIu64
        ",\"active_jobs\":%u,\"running_sources\":%u,\"workers\":%u"
        ",\"max_cache_bytes\":%" PRIu64
        ",\"cache_owner_pid\":%ld}\n",
        service->requests, service->queued, service->deduplicated,
        service->cache_hits, service->compiled, service->failed,
        service->negative_hits, service->queue_full, service->queue->len,
        service->queued_bytes,
        g_hash_table_size(service->active),
        g_hash_table_size(service->running_sources), service->config->workers,
        service->config->max_cache_bytes, (long)getpid());
    int fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd >= 0) {
        ssize_t written = write(fd, contents, length);
        if (written == length) {
            close(fd);
            if (!rename(temporary, service->config->stats_path)) {
                g_free(temporary);
                return;
            }
        } else {
            close(fd);
        }
    }
    unlink(temporary);
    g_free(temporary);
}

static LatcdJob *queue_take_next_locked(LatcdService *service,
                                        int64_t *next_ready_at_us)
{
    *next_ready_at_us = 0;
    if (!service->queue->len) {
        return NULL;
    }
    int64_t now = g_get_monotonic_time();
    guint selected = 0;
    LatcdJob *best = NULL;
    uint64_t best_work = 0;
    for (guint i = 0; i < service->queue->len; i++) {
        LatcdJob *candidate = g_ptr_array_index(service->queue, i);
        if (g_hash_table_contains(service->running_sources,
                                  candidate->source_key)) {
            continue;
        }
        if (!service->stopping && candidate->ready_at_us > now) {
            if (!*next_ready_at_us ||
                candidate->ready_at_us < *next_ready_at_us) {
                *next_ready_at_us = candidate->ready_at_us;
            }
            continue;
        }
        struct stat tbset_status;
        uint64_t candidate_work = candidate->source_size;
        if (service->config->flush_only && candidate->tbset &&
            !stat(candidate->tbset, &tbset_status) &&
            tbset_status.st_size > 0) {
            candidate_work = tbset_status.st_size;
        }
        bool preferred = !best;
        if (best && service->config->flush_only) {
            preferred = candidate_work > best_work ||
                (candidate_work == best_work &&
                 (candidate->priority > best->priority ||
                  (candidate->priority == best->priority &&
                   candidate->sequence < best->sequence)));
        } else if (best) {
            preferred = candidate->priority > best->priority ||
                (candidate->priority == best->priority &&
                 candidate->sequence < best->sequence);
        }
        if (preferred) {
            selected = i;
            best = candidate;
            best_work = candidate_work;
        }
    }
    if (!best) {
        return NULL;
    }
    g_ptr_array_remove_index(service->queue, selected);
    service->queued_bytes -= best->source_size;
    g_hash_table_add(service->running_sources,
                     g_strdup(best->source_key));
    best->running = true;
    return best;
}

static void negative_record_locked(LatcdService *service, const char *key,
                                   int status)
{
    LatcdNegativeEntry *entry = g_hash_table_lookup(service->negative, key);
    if (!entry) {
        if (g_hash_table_size(service->negative) >=
            service->config->max_negative) {
            GHashTableIter iterator;
            gpointer old_key;
            g_hash_table_iter_init(&iterator, service->negative);
            if (g_hash_table_iter_next(&iterator, &old_key, NULL)) {
                g_hash_table_iter_remove(&iterator);
            }
        }
        entry = g_new0(LatcdNegativeEntry, 1);
        g_hash_table_insert(service->negative, g_strdup(key), entry);
    }
    entry->failures++;
    entry->status = status;
    uint32_t shift = entry->failures > 6 ? 6 : entry->failures - 1;
    entry->retry_at_us = g_get_monotonic_time() +
        (int64_t)service->config->negative_ms * 1000 * (UINT64_C(1) << shift);
}

static void *compiler_worker(void *opaque)
{
    LatcdWorker *worker = opaque;
    LatcdService *service = worker->service;
    for (;;) {
        pthread_mutex_lock(&service->lock);
        LatcdJob *job = NULL;
        while (!job) {
            if (service->stopping && !service->queue->len) {
                pthread_mutex_unlock(&service->lock);
                return NULL;
            }
            int64_t next_ready_at_us = 0;
            job = queue_take_next_locked(service, &next_ready_at_us);
            if (!job) {
                if (next_ready_at_us) {
                    int64_t wait_us = next_ready_at_us -
                                      g_get_monotonic_time();
                    if (wait_us < 0) wait_us = 0;
                    struct timespec deadline;
                    clock_gettime(CLOCK_REALTIME, &deadline);
                    deadline.tv_sec += wait_us / 1000000;
                    deadline.tv_nsec += (wait_us % 1000000) * 1000;
                    if (deadline.tv_nsec >= 1000000000) {
                        deadline.tv_sec++;
                        deadline.tv_nsec -= 1000000000;
                    }
                    pthread_cond_timedwait(&service->ready, &service->lock,
                                           &deadline);
                } else {
                    pthread_cond_wait(&service->ready, &service->lock);
                }
            }
        }
        write_stats_locked(service);
        job->compile_sequence = job->accepted_sequence;
        pthread_mutex_unlock(&service->lock);
        LatcdResponseV2 response = {
            .magic = LATCD_RESPONSE_MAGIC,
            .version = LATCD_PROTOCOL_VERSION,
            .size = sizeof(response),
        };
        char error[sizeof(response.message)] = {0};
        char *canonical = NULL;
        char *stable_tbset = NULL;
        char *compile_delta = NULL;
        bool delta_empty = false;
        int result = job->tbset && !job->tbset_canonical &&
            !job->static_tbset && merge_tbset(
            service->config, job->digest, job->tbset, &canonical, NULL,
            error, sizeof(error));
        if (!result && job->static_tbset) {
            result = prepare_static_tbset(service->config, job->digest,
                                           job->tbset, &stable_tbset,
                                           error, sizeof(error));
        }
        if (!result && job->tbset_canonical) {
            int tbset_fd = open(job->tbset,
                                  O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
            char *temporary_dir = g_build_filename(
                service->config->cache_dir, ".tmp", NULL);
            if (tbset_fd < 0) {
                result = fail(error, sizeof(error),
                              "cannot open canonical TB set: %s",
                              strerror(errno));
            } else {
                result = snapshot_tbset(
                    tbset_fd, job->snapshot, temporary_dir, job->digest,
                    &stable_tbset, error, sizeof(error));
            }
            if (tbset_fd >= 0) close(tbset_fd);
            g_free(temporary_dir);
            if (!result) {
                char *complete = NULL;
                result = prepare_static_tbset(service->config, job->digest,
                                               stable_tbset, &complete,
                                               error, sizeof(error));
                if (!result) {
                    unlink(stable_tbset);
                    g_free(stable_tbset);
                    stable_tbset = complete;
                }
            }
        }
        const char *compile_tbset = job->tbset_canonical || job->static_tbset ?
                                    stable_tbset :
                                    (canonical ? canonical : job->tbset);
        if (!result && compile_tbset && !job->static_tbset) {
            result = prepare_compile_delta(service->config, job->digest,
                                           compile_tbset, &compile_delta,
                                           &delta_empty,
                                           error, sizeof(error));
                /*
                 * A published TB set only describes key coverage.  It must
                 * not suppress recompilation when its current module was
                 * removed, corrupted, or built by another code generator.
                 */
                if (!result && delta_empty) {
                    pthread_mutex_lock(&cache_lock);
                    bool manifest_valid = persisted_manifest_valid(
                        service->config, job->source_key, job->digest);
                    pthread_mutex_unlock(&cache_lock);
                    if (!manifest_valid) {
                        delta_empty = false;
                    }
                }
        }
        if (!result && !delta_empty) {
            result = publish_snapshot(service->config, worker->index,
                                      job->snapshot,
                                      compile_delta ? compile_delta :
                                      compile_tbset,
                                      compile_tbset, job->static_tbset,
                                      job->digest,
                                      job->compile_sequence, &response);
        } else if (!result) {
            int publish_result = publish_covered_tbset(
                service->config, compile_tbset, job->digest,
                job->compile_sequence, &response);
            if (publish_result > 0) {
                result = publish_snapshot(
                    service->config, worker->index, job->snapshot,
                    compile_tbset, compile_tbset, job->static_tbset,
                    job->digest,
                    job->compile_sequence, &response);
            } else {
                result = publish_result;
            }
        }
        if (result && response.status == LATCD_STATUS_OK) {
            response.status = LATCD_STATUS_BAD_REQUEST;
            g_strlcpy(response.message, error, sizeof(response.message));
        }
        g_free(canonical);
        if (compile_delta) unlink(compile_delta);
        g_free(compile_delta);
        if (stable_tbset) unlink(stable_tbset);
        g_free(stable_tbset);

        pthread_mutex_lock(&service->lock);
        g_hash_table_remove(service->running_sources, job->source_key);
        LatcdSourceState *source_state = source_state_locked(
            service, job->source_key);
        if (job->static_tbset) {
            LatcdStaticBarrier *barrier = g_hash_table_lookup(
                service->static_barriers, job->key);
            if (!result) {
                g_hash_table_remove(service->static_barriers, job->key);
            } else if (barrier) {
                barrier->failed = true;
            }
        }
        if (!result) {
            service->compiled++;
            uint64_t published_sequence = job->dirty ?
                job->compile_sequence : job->accepted_sequence;
            if (published_sequence > source_state->published_sequence) {
                source_state->published_sequence = published_sequence;
            }
            g_hash_table_remove(service->negative, job->key);
        } else {
            service->failed++;
            /* This failed job is removed, including requests accepted while
             * it was running. None can wait for a nonexistent retry job. */
            uint64_t failed_sequence = job->accepted_sequence;
            if (failed_sequence > source_state->failed_sequence) {
                source_state->failed_sequence = failed_sequence;
            }
            negative_record_locked(service, job->key, response.status);
            fprintf(stderr, "latcd: job %s failed status=%d: %s\n",
                    job->key, response.status,
                    response.message[0] ? response.message : "unknown error");
        }
        if (!result && job->tbset_canonical && job->dirty) {
            job->running = false;
            job->dirty = false;
            job->sequence = service->next_sequence++;
            g_ptr_array_add(service->queue, job);
            service->queued_bytes += job->source_size;
            service->queued++;
            write_stats_locked(service);
            pthread_cond_broadcast(&service->ready);
            pthread_mutex_unlock(&service->lock);
            continue;
        }
        g_hash_table_remove(service->active, job->key);
        write_stats_locked(service);
        pthread_cond_broadcast(&service->ready);
        pthread_mutex_unlock(&service->lock);
        job_free(job);
    }
    return NULL;
}

static void response_init(LatcdResponseV2 *response, uint64_t request_id)
{
    memset(response, 0, sizeof(*response));
    response->magic = LATCD_RESPONSE_MAGIC;
    response->version = LATCD_PROTOCOL_VERSION;
    response->size = sizeof(*response);
    response->request_id = request_id;
}

static LatcdSourceState *source_state_locked(LatcdService *service,
                                             const char *source_key)
{
    LatcdSourceState *state = g_hash_table_lookup(service->source_states,
                                                   source_key);
    if (!state) {
        state = g_new0(LatcdSourceState, 1);
        g_hash_table_insert(service->source_states, g_strdup(source_key),
                            state);
    }
    return state;
}

static int hex_digest(const char text[64], uint8_t digest[32])
{
    for (size_t i = 0; i < 32; i++) {
        int high = g_ascii_xdigit_value(text[i * 2]);
        int low = g_ascii_xdigit_value(text[i * 2 + 1]);
        if (high < 0 || low < 0) return -1;
        digest[i] = (uint8_t)((high << 4) | low);
    }
    return 0;
}

static int persisted_manifest_valid(const LatcdConfig *config,
                                    const char source_key[65],
                                    const uint8_t digest[32])
{
    LatcdCurrentManifest manifest = {0};
    int valid = !current_manifest_read(config, source_key, &manifest);
    char *module = valid ? g_build_filename(config->cache_dir,
                                             manifest.module, NULL) : NULL;
    char *native = valid ? g_build_filename(config->cache_dir,
                                             manifest.native, NULL) : NULL;
    char *tbset = valid && *manifest.tbset ?
        g_build_filename(config->cache_dir, manifest.tbset, NULL) : NULL;
    LatAotModuleInfoV2 info;
    valid = valid && cached_module_inspect(module, digest, &info) &&
            !access(native, R_OK) && (!*manifest.tbset || !access(tbset, R_OK));
    g_free(tbset);
    g_free(native);
    g_free(module);
    current_manifest_clear(&manifest);
    return valid;
}

static void load_persisted_source_states(LatcdService *service)
{
    char *directory = g_build_filename(service->config->cache_dir,
                                        ".tbsets", NULL);
    GDir *entries = g_dir_open(directory, 0, NULL);
    if (!entries) {
        g_free(directory);
        return;
    }
    const char *name;
    while ((name = g_dir_read_name(entries))) {
        if (strlen(name) != 70 || strcmp(name + 64, ".tbset")) continue;
        char source_key[65];
        memcpy(source_key, name, 64);
        source_key[64] = '\0';
        uint8_t digest[32];
        if (hex_digest(source_key, digest)) continue;
        char *known_path = g_build_filename(directory, name, NULL);
        LatcdCurrentManifest manifest = {0};
        int manifest_valid = !current_manifest_read(
            service->config, source_key, &manifest) && *manifest.tbset;
        char *published_path = manifest_valid ? g_build_filename(
            service->config->cache_dir, manifest.tbset, NULL) : NULL;
        LatTbKeySet known = {0};
        LatTbKeySet published = {0};
        LatTbKeySet missing = {0};
        char error[128] = {0};
        int known_valid = !lat_tb_key_set_read_file(
            known_path, digest, &known, error, sizeof(error));
        int fully_published = known_valid && published_path &&
            !lat_tb_key_set_read_file(published_path, digest, &published,
                                      error, sizeof(error)) &&
            !lat_tb_key_set_difference(&known, &published, &missing,
                                       error, sizeof(error)) &&
            !missing.count && persisted_manifest_valid(
                service->config, source_key, digest);
        if (known_valid) {
            LatcdSourceState *state = source_state_locked(service, source_key);
            state->accepted_sequence = known.sequence;
            if (fully_published) state->published_sequence = known.sequence;
            else state->failed_sequence = known.sequence;
            if (known.sequence > service->next_sequence) {
                service->next_sequence = known.sequence;
            }
        }
        lat_tb_key_set_destroy(&known);
        lat_tb_key_set_destroy(&published);
        lat_tb_key_set_destroy(&missing);
        current_manifest_clear(&manifest);
        g_free(published_path);
        g_free(known_path);
    }
    g_dir_close(entries);
    g_free(directory);
}

static int service_queue_request(LatcdService *service, int source_fd,
                                 int tbset_fd,
                                 const LatcdRequestV2 *request,
                                 LatcdResponseV2 *response)
{
    char error[sizeof(response->message)] = {0};
    char *temporary_dir = g_build_filename(service->config->cache_dir,
                                            ".tmp", NULL);
    char *snapshot = NULL;
    char *tbset = NULL;
    response_init(response, request->request_id);
    struct stat identity;
    if (snapshot_source(source_fd, temporary_dir,
                        service->config->max_input, &snapshot,
                        response->source_sha256, &identity,
                        error, sizeof(error))) {
        response->status = LATCD_STATUS_BAD_SOURCE;
        g_strlcpy(response->message, error, sizeof(response->message));
        g_free(temporary_dir);
        return -1;
    }
    publish_source_identity(service->config, &identity,
                            response->source_sha256);
    if (tbset_fd >= 0 && snapshot_tbset(
            tbset_fd, snapshot, temporary_dir, response->source_sha256,
            &tbset, error, sizeof(error))) {
        response->status = LATCD_STATUS_BAD_REQUEST;
        g_strlcpy(response->message, error, sizeof(response->message));
        unlink(snapshot);
        g_free(snapshot);
        g_free(temporary_dir);
        return -1;
    }
    bool tbset_canonical = false;
    bool tbset_changed = false;
    bool static_tbset = request->operation == LATCD_OP_SUBMIT_STATIC_KEYS;
    if (tbset && !static_tbset) {
        char *canonical = NULL;
        if (merge_tbset(service->config, response->source_sha256, tbset,
                        &canonical, &tbset_changed,
                        error, sizeof(error))) {
            response->status = LATCD_STATUS_BAD_REQUEST;
            g_strlcpy(response->message, error, sizeof(response->message));
            unlink(snapshot);
            unlink(tbset);
            g_free(snapshot);
            g_free(tbset);
            g_free(temporary_dir);
            return -1;
        }
        unlink(tbset);
        g_free(tbset);
        tbset = canonical;
        tbset_canonical = true;
    }
    g_free(temporary_dir);
    char key[130];
    digest_hex(response->source_sha256, key);
    char source_key[65];
    memcpy(source_key, key, 65);
    if (static_tbset) {
        gchar *contents = NULL;
        gsize size = 0;
        if (!g_file_get_contents(tbset, &contents, &size, NULL)) {
            response->status = LATCD_STATUS_IO_ERROR;
            g_strlcpy(response->message, "cannot hash static TB set",
                      sizeof(response->message));
            unlink(snapshot);
            unlink(tbset);
            g_free(snapshot);
            g_free(tbset);
            return -1;
        }
        gchar *hash = g_compute_checksum_for_data(G_CHECKSUM_SHA256,
                                                   (guchar *)contents, size);
        snprintf(key + 64, sizeof(key) - 64, "-%s", hash);
        g_free(hash);
        g_free(contents);
    } else if (tbset) {
        snprintf(key + 64, sizeof(key) - 64, "-tbset");
    }
    struct stat snapshot_status;
    if (stat(snapshot, &snapshot_status) || snapshot_status.st_size < 0) {
        response->status = LATCD_STATUS_IO_ERROR;
        g_strlcpy(response->message, "cannot inspect source snapshot",
                  sizeof(response->message));
        unlink(snapshot);
        if (tbset && !tbset_canonical) unlink(tbset);
        g_free(snapshot);
        g_free(tbset);
        return -1;
    }
    uint64_t source_size = snapshot_status.st_size;

    pthread_mutex_lock(&service->lock);
    service->requests++;
    uint64_t accepted_sequence = ++service->next_sequence;
    LatcdSourceState *source_state = source_state_locked(service, source_key);
    uint64_t previous_accepted = source_state->accepted_sequence;
    source_state->accepted_sequence = accepted_sequence;
    response->accepted_sequence = accepted_sequence;
    response->published_sequence = source_state->published_sequence;
    if (!tbset && cache_contains(service->config,
                                   response->source_sha256)) {
        service->cache_hits++;
        source_state->published_sequence = accepted_sequence;
        response->published_sequence = accepted_sequence;
        response->status = LATCD_STATUS_OK;
        snprintf(response->message, sizeof(response->message),
                 "cache hit: %s", key);
        write_stats_locked(service);
        pthread_mutex_unlock(&service->lock);
        unlink(snapshot);
        if (tbset && !tbset_canonical) unlink(tbset);
        g_free(snapshot);
        g_free(tbset);
        return 0;
    }
    if (tbset_canonical &&
        cache_contains_tbset(service->config, response->source_sha256,
                             tbset)) {
        service->cache_hits++;
        source_state->published_sequence = accepted_sequence;
        response->published_sequence = accepted_sequence;
        response->status = LATCD_STATUS_OK;
        snprintf(response->message, sizeof(response->message),
                 "cache hit: %s", key);
        write_stats_locked(service);
        pthread_mutex_unlock(&service->lock);
        unlink(snapshot);
        g_free(snapshot);
        g_free(tbset);
        return 0;
    }
    if (g_hash_table_contains(service->active, key)) {
        LatcdJob *active = g_hash_table_lookup(service->active, key);
        if (tbset_canonical && tbset_changed && active) {
            int64_t now = g_get_monotonic_time();
            if (service->config->flush_only) {
                active->ready_at_us = INT64_MAX;
            } else if (!active->running) {
                int64_t quiet_ready = now + LATCD_BATCH_QUIET_US;
                int64_t maximum_ready = active->batch_started_at_us +
                                        LATCD_BATCH_MAX_US;
                active->ready_at_us = quiet_ready < maximum_ready ?
                                      quiet_ready : maximum_ready;
            } else {
                if (!active->dirty) {
                    active->batch_started_at_us = now;
                }
                active->dirty = true;
                int64_t quiet_ready = now + LATCD_BATCH_QUIET_US;
                int64_t maximum_ready = active->batch_started_at_us +
                                        LATCD_BATCH_MAX_US;
                active->ready_at_us = quiet_ready < maximum_ready ?
                                      quiet_ready : maximum_ready;
            }
        }
        if (active && accepted_sequence > active->accepted_sequence) {
            active->accepted_sequence = accepted_sequence;
        }
        if (static_tbset) {
            LatcdStaticBarrier *barrier = g_hash_table_lookup(
                service->static_barriers, key);
            if (barrier) barrier->last_sequence = accepted_sequence;
        }
        service->deduplicated++;
        response->status = LATCD_STATUS_OK;
        snprintf(response->message, sizeof(response->message),
                 "deduplicated: %s", key);
        write_stats_locked(service);
        pthread_mutex_unlock(&service->lock);
        unlink(snapshot);
        if (tbset && !tbset_canonical) unlink(tbset);
        g_free(snapshot);
        g_free(tbset);
        return 0;
    }
    LatcdNegativeEntry *negative = g_hash_table_lookup(service->negative, key);
    if (negative && negative->retry_at_us > g_get_monotonic_time()) {
        source_state->accepted_sequence = previous_accepted;
        response->accepted_sequence = previous_accepted;
        service->negative_hits++;
        response->status = LATCD_STATUS_NEGATIVE_CACHE;
        snprintf(response->message, sizeof(response->message),
                 "negative cache: retry in %" PRId64 " ms",
                 (negative->retry_at_us - g_get_monotonic_time()) / 1000);
        write_stats_locked(service);
        pthread_mutex_unlock(&service->lock);
        unlink(snapshot);
        if (tbset && !tbset_canonical) unlink(tbset);
        g_free(snapshot);
        g_free(tbset);
        return -1;
    }
    if (service->queue->len >= service->config->max_jobs ||
        source_size > service->config->max_queue_bytes -
                      service->queued_bytes) {
        source_state->accepted_sequence = previous_accepted;
        response->accepted_sequence = previous_accepted;
        service->queue_full++;
        response->status = LATCD_STATUS_QUEUE_FULL;
        g_strlcpy(response->message, "compiler queue is full",
                  sizeof(response->message));
        write_stats_locked(service);
        pthread_mutex_unlock(&service->lock);
        unlink(snapshot);
        if (tbset && !tbset_canonical) unlink(tbset);
        g_free(snapshot);
        g_free(tbset);
        return -1;
    }
    LatcdJob *job = g_new0(LatcdJob, 1);
    job->snapshot = snapshot;
    job->tbset = tbset;
    job->tbset_canonical = tbset_canonical;
    job->static_tbset = static_tbset;
    g_strlcpy(job->key, key, sizeof(job->key));
    memcpy(job->source_key, key, 64);
    job->source_key[64] = '\0';
    memcpy(job->digest, response->source_sha256, sizeof(job->digest));
    job->priority = request->priority;
    job->sequence = accepted_sequence;
    job->accepted_sequence = accepted_sequence;
    job->source_size = source_size;
    job->batch_started_at_us = g_get_monotonic_time();
    job->ready_at_us = service->config->flush_only ? INT64_MAX :
                       job->batch_started_at_us + LATCD_BATCH_QUIET_US;
    if (static_tbset) {
        LatcdStaticBarrier *barrier = g_hash_table_lookup(
            service->static_barriers, key);
        if (!barrier) {
            barrier = g_new0(LatcdStaticBarrier, 1);
            g_strlcpy(barrier->source_key, source_key,
                      sizeof(barrier->source_key));
            barrier->first_sequence = accepted_sequence;
            g_hash_table_insert(service->static_barriers, g_strdup(key),
                                barrier);
        }
        barrier->last_sequence = accepted_sequence;
        barrier->failed = false;
    }
    g_ptr_array_add(service->queue, job);
    service->queued_bytes += source_size;
    g_hash_table_insert(service->active, g_strdup(key), job);
    service->queued++;
    response->status = LATCD_STATUS_OK;
    snprintf(response->message, sizeof(response->message), "queued: %s", key);
    write_stats_locked(service);
    pthread_cond_signal(&service->ready);
    pthread_mutex_unlock(&service->lock);
    return 0;
}

static int source_flush_state_locked(LatcdService *service,
                                     const char *source_key,
                                     LatcdSourceState *state,
                                     uint64_t target)
{
    GHashTableIter iterator;
    gpointer value;
    bool pending = false;
    g_hash_table_iter_init(&iterator, service->static_barriers);
    while (g_hash_table_iter_next(&iterator, NULL, &value)) {
        LatcdStaticBarrier *barrier = value;
        if (!strcmp(barrier->source_key, source_key) &&
            barrier->first_sequence <= target) {
            if (barrier->failed) return -1;
            pending = true;
        }
    }
    if (pending) return 0;
    if (!state || state->published_sequence >= target) return 1;
    if (state->failed_sequence >= target) return -1;
    return 0;
}

static int all_flush_state_locked(LatcdService *service,
                                  uint64_t *accepted,
                                  uint64_t *published)
{
    int result = 1;
    *accepted = 0;
    *published = 0;
    GHashTableIter iterator;
    gpointer key, value;
    g_hash_table_iter_init(&iterator, service->source_states);
    while (g_hash_table_iter_next(&iterator, &key, &value)) {
        LatcdSourceState *state = value;
        if (state->accepted_sequence > *accepted) {
            *accepted = state->accepted_sequence;
        }
        if (state->published_sequence > *published) {
            *published = state->published_sequence;
        }
        int current = source_flush_state_locked(service, key, state,
                                                state->accepted_sequence);
        if (current < 0) return -1;
        if (!current) result = 0;
    }
    return result;
}

static int service_flush_request(LatcdService *service, int source_fd,
                                 const LatcdRequestV2 *request,
                                 LatcdResponseV2 *response)
{
    response_init(response, request->request_id);
    char source_key[65] = {0};
    if (request->operation == LATCD_OP_FLUSH_SOURCE) {
        char error[sizeof(response->message)] = {0};
        if (hash_source(source_fd, service->config->max_input,
                        response->source_sha256, error, sizeof(error))) {
            response->status = LATCD_STATUS_BAD_SOURCE;
            g_strlcpy(response->message, error, sizeof(response->message));
            return -1;
        }
        digest_hex(response->source_sha256, source_key);
    }

    pthread_mutex_lock(&service->lock);
    GHashTableIter active_iterator;
    gpointer active_value;
    g_hash_table_iter_init(&active_iterator, service->active);
    while (g_hash_table_iter_next(&active_iterator, NULL, &active_value)) {
        LatcdJob *job = active_value;
        if (request->operation == LATCD_OP_FLUSH_ALL ||
            !strcmp(job->source_key, source_key)) {
            job->ready_at_us = 0;
        }
    }
    pthread_cond_broadcast(&service->ready);
    int state_result = 0;
    if (request->operation == LATCD_OP_FLUSH_SOURCE) {
        LatcdSourceState *state = g_hash_table_lookup(service->source_states,
                                                       source_key);
        uint64_t target = state ? state->accepted_sequence : 0;
        while (!(state_result = source_flush_state_locked(
                     service, source_key, state, target))) {
            pthread_cond_wait(&service->ready, &service->lock);
        }
        response->accepted_sequence = target;
        response->published_sequence = state ? state->published_sequence : 0;
    } else {
        while (!(state_result = all_flush_state_locked(
                     service, &response->accepted_sequence,
                     &response->published_sequence))) {
            pthread_cond_wait(&service->ready, &service->lock);
        }
    }
    if (state_result < 0) {
        response->status = LATCD_STATUS_COMPILE_FAILED;
        g_strlcpy(response->message,
                  "an accepted key set failed to compile",
                  sizeof(response->message));
    } else {
        response->status = LATCD_STATUS_OK;
        g_strlcpy(response->message, "all accepted key sets are published",
                  sizeof(response->message));
    }
    pthread_mutex_unlock(&service->lock);
    return state_result < 0 ? -1 : 0;
}

static int make_server(const char *path, char *error, size_t error_size)
{
    char *parent = g_path_get_dirname(path);
    struct stat parent_status;
    if (lstat(parent, &parent_status) || !S_ISDIR(parent_status.st_mode) ||
        parent_status.st_uid != geteuid() || (parent_status.st_mode & 0077)) {
        g_free(parent);
        return fail(error, error_size,
                    "socket directory must be private and user-owned");
    }
    g_free(parent);
    struct stat status;
    if (!lstat(path, &status)) {
        if (!S_ISSOCK(status.st_mode) || status.st_uid != geteuid() ||
            unlink(path)) {
            return fail(error, error_size, "refusing to replace socket path");
        }
    } else if (errno != ENOENT) {
        return fail(error, error_size, "cannot inspect socket path: %s",
                    strerror(errno));
    }
    int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return fail(error, error_size, "cannot create socket: %s",
                    strerror(errno));
    }
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    int directory_fd;
    if (latcd_socket_address(path, &address, &directory_fd, error, error_size)) {
        close(fd);
        return -1;
    }
    mode_t old_mask = umask(0077);
    int result = bind(fd, (const void *)&address, sizeof(address));
    int bind_error = errno;
    if (directory_fd >= 0) close(directory_fd);
    errno = bind_error;
    umask(old_mask);
    if (result || listen(fd, 64)) {
        fail(error, error_size, "cannot listen on %s: %s", path,
             strerror(errno));
        close(fd);
        unlink(path);
        return -1;
    }
    return fd;
}

static int peer_is_current_user(int fd);

static char *precompile_guest_path(const LatcdConfig *config, int source_fd,
                                   char *error, size_t error_size)
{
    if (!config->x86_rootfs || !*config->x86_rootfs) {
        fail(error, error_size,
             "precompile requires latcd --x86-rootfs");
        return NULL;
    }
    char *rootfs = realpath(config->x86_rootfs, NULL);
    char descriptor[64];
    snprintf(descriptor, sizeof(descriptor), "/proc/self/fd/%d", source_fd);
    char *source = realpath(descriptor, NULL);
    if (!rootfs || !source) {
        fail(error, error_size, "cannot resolve precompile source: %s",
             strerror(errno));
        g_free(rootfs);
        g_free(source);
        return NULL;
    }
    size_t root_length = strlen(rootfs);
    if (strncmp(source, rootfs, root_length) ||
        (source[root_length] && source[root_length] != '/')) {
        fail(error, error_size,
             "precompile source is outside configured x86 rootfs");
        g_free(rootfs);
        g_free(source);
        return NULL;
    }
    char *guest = g_strdup(source[root_length] ? source + root_length : "/");
    g_free(rootfs);
    g_free(source);
    return guest;
}

static void precompile_request_worker(gpointer data, gpointer user_data)
{
    (void)user_data;
    LatcdPrecompileRequest *work = data;
    LatcdResponseV2 response;
    response_init(&response, work->request_id);
    char error[sizeof(response.message)] = {0};
    char *guest = precompile_guest_path(work->config, work->source_fd,
                                        error, sizeof(error));
    if (guest && hash_source(work->source_fd, work->config->max_input,
                             response.source_sha256,
                             error, sizeof(error))) {
        g_free(guest);
        guest = NULL;
    }
    if (guest) {
        char *executable = g_file_read_link("/proc/self/exe", NULL);
        char jobs[16];
        snprintf(jobs, sizeof(jobs), "%u",
                 MIN(work->config->workers, LATCD_AUTO_WORKERS_MAX));
        char *arguments[] = {
            executable, (char *)"--precompile",
            (char *)"--socket", (char *)work->config->socket_path,
            (char *)"--x86-rootfs", (char *)work->config->x86_rootfs,
            (char *)"--latc", (char *)work->config->compiler,
            (char *)"--jobs", jobs, guest, NULL,
        };
        gint wait_status = 0;
        gchar *standard_error = NULL;
        GError *gerror = NULL;
        bool spawned = executable && g_spawn_sync(
            NULL, arguments, NULL, G_SPAWN_STDOUT_TO_DEV_NULL,
            NULL, NULL, NULL, &standard_error, &wait_status, &gerror);
        if (spawned && LATCD_SPAWN_CHECK_EXIT_STATUS(wait_status, &gerror)) {
            response.status = LATCD_STATUS_OK;
            g_strlcpy(response.message, "precompile published",
                      sizeof(response.message));
        } else {
            response.status = LATCD_STATUS_COMPILE_FAILED;
            g_strlcpy(response.message,
                      gerror ? gerror->message :
                      (standard_error && *standard_error ? standard_error :
                       "precompile failed"),
                      sizeof(response.message));
        }
        g_clear_error(&gerror);
        g_free(standard_error);
        g_free(executable);
    } else {
        response.status = LATCD_STATUS_BAD_SOURCE;
        g_strlcpy(response.message, error[0] ? error : "bad source",
                  sizeof(response.message));
    }
    latcd_send_response(work->client_fd, &response, NULL, 0);
    close(work->source_fd);
    close(work->client_fd);
    g_free(guest);
    g_free(work);
    atomic_fetch_sub(&active_precompiles, 1);
}

static int run_once(const LatcdConfig *config)
{
    char error[256] = {0};
    int server = make_server(config->socket_path, error, sizeof(error));
    if (server < 0) {
        fprintf(stderr, "latcd: %s\n", error);
        return 1;
    }
    int client;
    do {
        client = accept4(server, NULL, NULL, SOCK_CLOEXEC);
    } while (client < 0 && errno == EINTR);
    LatcdRequestV2 request = {0};
    int source = -1;
    int tbset = -1;
    LatcdResponseV2 response = {
        .magic = LATCD_RESPONSE_MAGIC,
        .version = LATCD_PROTOCOL_VERSION,
        .size = sizeof(response),
    };
    if (client < 0 || !peer_is_current_user(client) ||
        latcd_receive_request(client, &request, &source, &tbset,
                              error, sizeof(error))) {
        response.status = LATCD_STATUS_BAD_REQUEST;
        g_strlcpy(response.message, error[0] ? error : "accept failed",
                  sizeof(response.message));
    } else if (request.operation != LATCD_OP_SUBMIT_KEYS &&
               request.operation != LATCD_OP_SUBMIT_STATIC_KEYS) {
        response.request_id = request.request_id;
        response.status = LATCD_STATUS_BAD_REQUEST;
        g_strlcpy(response.message, "flush requires --serve mode",
                  sizeof(response.message));
    } else {
        response.request_id = request.request_id;
        process_request(config, source, tbset,
                        request.operation == LATCD_OP_SUBMIT_STATIC_KEYS,
                        &response);
    }
    if (client >= 0) {
        latcd_send_response(client, &response, NULL, 0);
        close(client);
    }
    if (source >= 0) {
        close(source);
    }
    if (tbset >= 0) {
        close(tbset);
    }
    close(server);
    unlink(config->socket_path);
    return response.status == LATCD_STATUS_OK ? 0 : 1;
}

static int peer_is_current_user(int fd)
{
    struct ucred credentials;
    socklen_t size = sizeof(credentials);
    return !getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &size) &&
           size == sizeof(credentials) && credentials.uid == geteuid();
}

static int request_is_ready(int fd)
{
    struct pollfd request = { .fd = fd, .events = POLLIN };
    int result;
    do {
        result = poll(&request, 1, 1000);
    } while (result < 0 && errno == EINTR && !stop_requested);
    return result > 0 && (request.revents & POLLIN);
}

static int run_service(const LatcdConfig *config)
{
    char error[256] = {0};
    char *temporary_dir = g_build_filename(config->cache_dir, ".tmp", NULL);
    if (ensure_private_directory(config->cache_dir, error, sizeof(error)) ||
        ensure_private_directory(temporary_dir, error, sizeof(error))) {
        fprintf(stderr, "latcd: %s\n", error);
        g_free(temporary_dir);
        return 1;
    }
    g_free(temporary_dir);
    int server = make_server(config->socket_path, error, sizeof(error));
    if (server < 0) {
        fprintf(stderr, "latcd: %s\n", error);
        return 1;
    }
    LatcdService service = {
        .config = config,
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .ready = PTHREAD_COND_INITIALIZER,
        .queue = g_ptr_array_new(),
        .active = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL),
        .running_sources = g_hash_table_new_full(
            g_str_hash, g_str_equal, g_free, NULL),
        .negative = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                          g_free),
        .source_states = g_hash_table_new_full(g_str_hash, g_str_equal,
                                               g_free, g_free),
        .static_barriers = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                g_free, g_free),
        .workers = g_new0(pthread_t, config->workers),
        .stopping = 0,
        .next_sequence = 0,
        .requests = 0,
        .queued = 0,
        .deduplicated = 0,
        .cache_hits = 0,
        .compiled = 0,
        .failed = 0,
        .negative_hits = 0,
        .queue_full = 0,
        .queued_bytes = 0,
    };
    load_persisted_source_states(&service);
    LatcdWorker *worker_args = g_new0(LatcdWorker, config->workers);
    uint32_t started_workers = 0;
    for (; started_workers < config->workers; started_workers++) {
        worker_args[started_workers] = (LatcdWorker) {
            .service = &service,
            .index = started_workers,
        };
        if (pthread_create(&service.workers[started_workers], NULL,
                           compiler_worker, &worker_args[started_workers])) {
            break;
        }
    }
    if (started_workers != config->workers) {
        fprintf(stderr, "latcd: cannot start compiler worker\n");
        pthread_mutex_lock(&service.lock);
        service.stopping = 1;
        pthread_cond_broadcast(&service.ready);
        pthread_mutex_unlock(&service.lock);
        for (uint32_t i = 0; i < started_workers; i++) {
            pthread_join(service.workers[i], NULL);
        }
        close(server);
        unlink(config->socket_path);
        g_free(worker_args);
        g_free(service.workers);
        g_ptr_array_free(service.queue, TRUE);
        g_hash_table_destroy(service.active);
        g_hash_table_destroy(service.running_sources);
        g_hash_table_destroy(service.negative);
        g_hash_table_destroy(service.source_states);
        g_hash_table_destroy(service.static_barriers);
        return 1;
    }
    pthread_mutex_lock(&service.lock);
    write_stats_locked(&service);
    pthread_mutex_unlock(&service.lock);
    struct sigaction action = { .sa_handler = signal_stop };
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    stop_requested = 0;
    GError *pool_error = NULL;
    GThreadPool *precompile_pool = g_thread_pool_new(
        precompile_request_worker, NULL, 1, FALSE, &pool_error);
    if (!precompile_pool) {
        fprintf(stderr, "latcd: cannot start precompile worker: %s\n",
                pool_error ? pool_error->message : "unknown error");
        g_clear_error(&pool_error);
        stop_requested = 1;
    }
    int64_t last_activity = g_get_monotonic_time();
    while (!stop_requested) {
        struct pollfd poll_fd = { .fd = server, .events = POLLIN };
        int available = poll(&poll_fd, 1, 250);
        if (available < 0 && errno == EINTR) {
            continue;
        }
        if (available <= 0 || !(poll_fd.revents & POLLIN)) {
            pthread_mutex_lock(&service.lock);
            bool busy = service.queue->len ||
                g_hash_table_size(service.running_sources);
            pthread_mutex_unlock(&service.lock);
            busy |= atomic_load(&active_precompiles) ||
                (precompile_pool && g_thread_pool_unprocessed(precompile_pool));
            if (busy) last_activity = g_get_monotonic_time();
            if (config->idle_seconds &&
                g_get_monotonic_time() - last_activity >=
                (int64_t)config->idle_seconds * G_USEC_PER_SEC) {
                break;
            }
            continue;
        }
        int client = accept4(server, NULL, NULL, SOCK_CLOEXEC);
        if (client < 0) {
            continue;
        }
        last_activity = g_get_monotonic_time();
        LatcdRequestV2 request = {0};
        LatcdResponseV2 response;
        response_init(&response, 0);
        int source = -1;
        int tbset = -1;
        bool handed_off = false;
        if (!peer_is_current_user(client) || !request_is_ready(client)) {
            response.status = LATCD_STATUS_BAD_REQUEST;
            g_strlcpy(response.message, "request user does not own latcd",
                      sizeof(response.message));
        } else if (latcd_receive_request(client, &request, &source, &tbset,
                                         error, sizeof(error))) {
            response.status = LATCD_STATUS_BAD_REQUEST;
            g_strlcpy(response.message, error, sizeof(response.message));
        } else if (request.operation == LATCD_OP_HELLO) {
            response.request_id = request.request_id;
            g_strlcpy(response.message, LATC_BUILD_ID, sizeof(response.message));
            char *canonical = realpath(config->cache_dir, NULL);
            if (canonical) {
                GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
                g_checksum_update(checksum, (const guchar *)canonical, strlen(canonical));
                gsize size = sizeof(response.source_sha256);
                g_checksum_get_digest(checksum, response.source_sha256, &size);
                g_checksum_free(checksum);
                free(canonical);
            } else {
                response.status = LATCD_STATUS_IO_ERROR;
            }
        } else if (request.operation == LATCD_OP_SUBMIT_KEYS ||
                   request.operation == LATCD_OP_SUBMIT_STATIC_KEYS) {
            service_queue_request(&service, source, tbset, &request,
                                  &response);
        } else if (request.operation == LATCD_OP_PRECOMPILE_SOURCE) {
            LatcdPrecompileRequest *work = g_new0(
                LatcdPrecompileRequest, 1);
            if (work) {
                *work = (LatcdPrecompileRequest) {
                    .config = config,
                    .client_fd = client,
                    .source_fd = source,
                    .request_id = request.request_id,
                };
                atomic_fetch_add(&active_precompiles, 1);
                if (g_thread_pool_push(precompile_pool, work, NULL)) {
                    handed_off = true;
                } else {
                    atomic_fetch_sub(&active_precompiles, 1);
                    g_free(work);
                    response.status = LATCD_STATUS_QUEUE_FULL;
                    g_strlcpy(response.message,
                              "cannot queue precompile request",
                              sizeof(response.message));
                }
            } else {
                response.status = LATCD_STATUS_QUEUE_FULL;
                g_strlcpy(response.message, "out of memory",
                          sizeof(response.message));
            }
        } else {
            service_flush_request(&service, source, &request, &response);
        }
        if (handed_off) continue;
        latcd_send_response(client, &response, NULL, 0);
        if (source >= 0) {
            close(source);
        }
        if (tbset >= 0) {
            close(tbset);
        }
        close(client);
    }
    close(server);
    unlink(config->socket_path);
    if (precompile_pool) {
        g_thread_pool_free(precompile_pool, FALSE, TRUE);
    }
    pthread_mutex_lock(&service.lock);
    service.stopping = 1;
    for (guint i = 0; i < service.queue->len; i++) {
        LatcdJob *job = g_ptr_array_index(service.queue, i);
        g_hash_table_remove(service.active, job->key);
        job_free(job);
    }
    g_ptr_array_set_size(service.queue, 0);
    service.queued_bytes = 0;
    pthread_cond_broadcast(&service.ready);
    pthread_mutex_unlock(&service.lock);
    for (uint32_t i = 0; i < config->workers; i++) {
        if (compiler_process_groups[i] > 0) {
            kill(-compiler_process_groups[i], SIGTERM);
        }
    }
    for (uint32_t i = 0; i < config->workers; i++) {
        pthread_join(service.workers[i], NULL);
    }
    pthread_mutex_lock(&service.lock);
    write_stats_locked(&service);
    pthread_mutex_unlock(&service.lock);
    for (guint i = 0; i < service.queue->len; i++) {
        job_free(g_ptr_array_index(service.queue, i));
    }
    g_ptr_array_free(service.queue, TRUE);
    g_hash_table_destroy(service.active);
    g_hash_table_destroy(service.running_sources);
    g_hash_table_destroy(service.negative);
    g_hash_table_destroy(service.source_states);
    g_hash_table_destroy(service.static_barriers);
    g_free(worker_args);
    g_free(service.workers);
    pthread_cond_destroy(&service.ready);
    pthread_mutex_destroy(&service.lock);
    return 0;
}

static int run_client_request(const char *socket_path, uint32_t operation,
                              const char *source_path,
                              const char *tbset_path, uint32_t priority)
{
    int source = source_path ? open(source_path, O_RDONLY | O_CLOEXEC) : -1;
    int tbset = tbset_path ? open(tbset_path, O_RDONLY | O_CLOEXEC) : -1;
    int client = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    int expected_fds = operation == LATCD_OP_SUBMIT_KEYS ? 2 :
                       operation == LATCD_OP_FLUSH_SOURCE ? 1 : 0;
    if ((expected_fds >= 1 && source < 0) ||
        (expected_fds == 2 && tbset < 0) || client < 0) {
        fprintf(stderr, "latcd: cannot open request input: %s\n",
                strerror(errno));
        if (source >= 0) close(source);
        if (tbset >= 0) close(tbset);
        if (client >= 0) close(client);
        return 1;
    }
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    int directory_fd;
    char address_error[256];
    if (latcd_socket_address(socket_path, &address, &directory_fd,
                             address_error, sizeof(address_error))) {
        fprintf(stderr, "latcd: %s\n", address_error);
        if (source >= 0) close(source);
        if (tbset >= 0) close(tbset);
        close(client);
        return 1;
    }
    int connected = connect(client, (const void *)&address, sizeof(address));
    int connect_error = errno;
    if (directory_fd >= 0) close(directory_fd);
    errno = connect_error;
    if (connected) {
        fprintf(stderr, "latcd: cannot connect: %s\n", strerror(errno));
        if (source >= 0) close(source);
        if (tbset >= 0) close(tbset);
        close(client);
        return 1;
    }
    LatcdRequestV2 request = {
        .magic = LATCD_REQUEST_MAGIC,
        .version = LATCD_PROTOCOL_VERSION,
        .size = sizeof(request),
        .priority = priority,
        .operation = operation,
        .request_id = ((uint64_t)getpid() << 32) ^ g_get_monotonic_time(),
    };
    char error[256] = {0};
    LatcdResponseV2 response;
    int result = latcd_send_request(client, source, tbset, &request, error,
                                    sizeof(error)) ||
                 latcd_receive_response(client, &response, error,
                                        sizeof(error));
    if (result) {
        fprintf(stderr, "latcd: %s\n", error);
    } else {
        char hex[65];
        digest_hex(response.source_sha256, hex);
        printf("status=%d\nrequest_id=%" PRIu64
               "\naccepted_sequence=%" PRIu64
               "\npublished_sequence=%" PRIu64
               "\nsource_sha256=%s\n%s\n",
               response.status, response.request_id,
               response.accepted_sequence, response.published_sequence,
               hex, response.message);
        result = response.status != LATCD_STATUS_OK;
    }
    if (source >= 0) close(source);
    if (tbset >= 0) close(tbset);
    close(client);
    return result;
}

static void usage(const char *name)
{
    fprintf(stderr,
            "usage:\n"
            "  %s --once --socket PATH --cache-dir DIR --latc PATH"
            " --runner PATH --runtime-dir DIR [--max-input BYTES]\n"
            "  %s --serve --socket PATH --cache-dir DIR --latc PATH"
            " --runner PATH --runtime-dir DIR [--stats PATH]"
            " [--x86-rootfs DIR]"
            " [--max-jobs N] [--negative-ms N] [--max-negative N]"
            " [--max-queue-bytes BYTES] [--max-cache-bytes BYTES]"
            " [--workers N] [--flush-only] [--idle-seconds N] [--daemonize]"
            " [--cpu-seconds N] [--address-space BYTES]"
            " [--file-size BYTES] [--open-files N]\n"
            "  %s --submit --socket PATH --tbset FILE [--priority N] X86_ELF\n"
            "  %s --flush-source --socket PATH X86_ELF\n"
            "  %s --flush-all --socket PATH\n"
            "  %s --precompile --socket PATH --x86-rootfs DIR"
            " [--latc PATH] [--jobs N] GUEST_ELF\n"
            "  %s --build-id\n",
            name, name, name, name, name, name, name);
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--build-id")) {
        puts(LATC_BUILD_ID);
        return 0;
    }
    int once = 0, serve = 0, submit = 0, flush_source = 0, flush_all = 0;
    int precompile = 0, daemonize = 0;
    uint32_t precompile_jobs = 0;
    const char *source = NULL;
    const char *tbset = NULL;
    uint32_t priority = LATCD_PRIORITY_LIBRARY;
    LatcdConfig config = {
        .max_input = LATCD_DEFAULT_MAX_INPUT,
        .address_space_limit = LATCD_DEFAULT_ADDRESS_SPACE,
        .file_size_limit = LATCD_DEFAULT_FILE_SIZE,
        .max_queue_bytes = LATCD_DEFAULT_MAX_QUEUE_BYTES,
        .max_cache_bytes = LATCD_DEFAULT_MAX_CACHE_BYTES,
        .max_jobs = LATCD_DEFAULT_MAX_JOBS,
        .workers = 0,
        .max_negative = LATCD_DEFAULT_MAX_NEGATIVE,
        .negative_ms = LATCD_DEFAULT_NEGATIVE_MS,
        .cpu_seconds = LATCD_DEFAULT_CPU_SECONDS,
        .open_files = LATCD_DEFAULT_OPEN_FILES,
    };
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--once")) once = 1;
        else if (!strcmp(argv[i], "--daemonize")) daemonize = 1;
        else if (!strcmp(argv[i], "--serve")) serve = 1;
        else if (!strcmp(argv[i], "--submit")) submit = 1;
        else if (!strcmp(argv[i], "--flush-source")) flush_source = 1;
        else if (!strcmp(argv[i], "--flush-all")) flush_all = 1;
        else if (!strcmp(argv[i], "--precompile")) precompile = 1;
        else if (!strcmp(argv[i], "--flush-only")) config.flush_only = true;
        else if (!strcmp(argv[i], "--idle-seconds") && i + 1 < argc) {
            char *end = NULL;
            uint64_t seconds = g_ascii_strtoull(argv[++i], &end, 10);
            if (!end || *end || seconds > UINT32_MAX) return 2;
            config.idle_seconds = seconds;
        }
        else if (!strcmp(argv[i], "--socket") && i + 1 < argc)
            config.socket_path = argv[++i];
        else if (!strcmp(argv[i], "--cache-dir") && i + 1 < argc)
            config.cache_dir = argv[++i];
        else if (!strcmp(argv[i], "--latc") && i + 1 < argc)
            config.compiler = argv[++i];
        else if (!strcmp(argv[i], "--runner") && i + 1 < argc)
            config.runner = argv[++i];
        else if (!strcmp(argv[i], "--runtime-dir") && i + 1 < argc)
            config.runtime_dir = argv[++i];
        else if (!strcmp(argv[i], "--x86-rootfs") && i + 1 < argc)
            config.x86_rootfs = argv[++i];
        else if (!strcmp(argv[i], "--stats") && i + 1 < argc)
            config.stats_path = argv[++i];
        else if (!strcmp(argv[i], "--max-input") && i + 1 < argc)
            config.max_input = g_ascii_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--max-jobs") && i + 1 < argc)
            config.max_jobs = g_ascii_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--max-queue-bytes") && i + 1 < argc)
            config.max_queue_bytes = g_ascii_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--max-cache-bytes") && i + 1 < argc)
            config.max_cache_bytes = g_ascii_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--workers") && i + 1 < argc)
            config.workers = g_ascii_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--max-negative") && i + 1 < argc)
            config.max_negative = g_ascii_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--negative-ms") && i + 1 < argc)
            config.negative_ms = g_ascii_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--cpu-seconds") && i + 1 < argc)
            config.cpu_seconds = g_ascii_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--address-space") && i + 1 < argc)
            config.address_space_limit = g_ascii_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--file-size") && i + 1 < argc)
            config.file_size_limit = g_ascii_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--open-files") && i + 1 < argc)
            config.open_files = g_ascii_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--priority") && i + 1 < argc)
            priority = g_ascii_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--jobs") && i + 1 < argc)
            precompile_jobs = g_ascii_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--tbset") && i + 1 < argc)
            tbset = argv[++i];
        else if ((submit || flush_source || precompile) && !source)
            source = argv[i];
        else { usage(argv[0]); return 2; }
    }
    if (once + serve + submit + flush_source + flush_all + precompile != 1 ||
        !config.socket_path) {
        usage(argv[0]);
        return 2;
    }
    if (submit) {
        if (!source || !tbset) { usage(argv[0]); return 2; }
        return run_client_request(config.socket_path, LATCD_OP_SUBMIT_KEYS,
                                  source, tbset, priority);
    }
    if (flush_source) {
        if (!source || tbset) { usage(argv[0]); return 2; }
        return run_client_request(config.socket_path, LATCD_OP_FLUSH_SOURCE,
                                  source, NULL, priority);
    }
    if (flush_all) {
        if (source || tbset) { usage(argv[0]); return 2; }
        return run_client_request(config.socket_path, LATCD_OP_FLUSH_ALL,
                                  NULL, NULL, priority);
    }
    if (precompile) {
        if (!source || !config.x86_rootfs || tbset ||
            precompile_jobs > LATCD_MAX_WORKERS) {
            usage(argv[0]);
            return 2;
        }
        char *compiler = NULL;
        if (config.compiler) {
            compiler = g_canonicalize_filename(config.compiler, NULL);
        } else {
            char *executable = g_file_read_link("/proc/self/exe", NULL);
            if (!executable) {
                fprintf(stderr, "latcd: cannot resolve /proc/self/exe\n");
                return 1;
            }
            char *directory = g_path_get_dirname(executable);
            compiler = g_build_filename(directory, "latc", NULL);
            g_free(directory);
            g_free(executable);
        }
        if (!precompile_jobs) precompile_jobs = default_worker_count();
        int result = latcd_run_precompile(config.socket_path,
                                          config.x86_rootfs, compiler,
                                          source, precompile_jobs);
        g_free(compiler);
        return result;
    }
    if (!config.workers) {
        config.workers = default_worker_count();
    }
    if (!config.cache_dir || !config.compiler || !config.runner ||
        !config.runtime_dir || !config.max_input || !config.max_jobs ||
        !config.max_queue_bytes || !config.max_cache_bytes ||
        !config.workers || config.workers > LATCD_MAX_WORKERS ||
        !config.max_negative || !config.negative_ms || !config.cpu_seconds ||
        !config.address_space_limit || !config.file_size_limit ||
        !config.open_files) {
        usage(argv[0]);
        return 2;
    }
    char error[512] = {0};
    if (daemonize && !serve) {
        fprintf(stderr, "latcd: --daemonize requires --serve\n");
        return 2;
    }
    if (validate_toolchain(&config, error, sizeof(error))) {
        fprintf(stderr, "latcd: %s\n", error);
        return 1;
    }
    if (daemonize) {
        pid_t child = fork();
        if (child < 0) return 1;
        if (child > 0) return 0;
        if (setsid() < 0) _exit(1);
        child = fork();
        if (child < 0) _exit(1);
        if (child > 0) _exit(0);
        if (chdir("/")) _exit(1);
        long maximum = sysconf(_SC_OPEN_MAX);
        for (int fd = 3; fd < maximum; fd++) close(fd);
    }
    int cache_owner = acquire_cache_owner(&config, error, sizeof(error));
    if (cache_owner < 0) {
        fprintf(stderr, "latcd: %s\n", error);
        return 1;
    }
    int result = serve ? run_service(&config) : run_once(&config);
    close(cache_owner);
    return result;
}
