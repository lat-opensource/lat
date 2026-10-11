#include "qemu/osdep.h"

#include "latc-bundle-loader.h"
#include "latc-bundle-format.h"
#include "lat-aot-v2.h"

#include "accel/tcg/internal.h"
#include "exec/exec-all.h"
#include "aot.h"
#include "jrra.h"
#include "tcg/tcg.h"

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

static int bundle_self_fd = -1;
static LatcDiskFooter bundle_footer;
static bool pretranslation_complete;
static uint64_t stat_cfg_tbs, stat_selected, stat_pretranslated, stat_failed;
static uint64_t stat_continuation_tbs, stat_edge_target_tbs;
static uint64_t stat_same_extent, stat_shorter_extent, stat_longer_extent;
static uint64_t stat_runtime_tb_gen_calls, stat_runtime_first_pc;
static uint64_t stat_runtime_tb_gen_attempts;
static uint64_t stat_runtime_program_tb_gen_calls;
static uint64_t stat_runtime_system_tb_gen_calls;
static uint64_t stat_runtime_program_tb_gen_attempts;
static uint64_t stat_runtime_system_tb_gen_attempts;
static uint64_t stat_runtime_file_tb_gen_calls;
static uint64_t stat_runtime_nonfile_tb_gen_calls;
static uint64_t stat_runtime_file_tb_gen_attempts;
static uint64_t stat_runtime_nonfile_tb_gen_attempts;
static uint32_t stat_runtime_first_cflags;
static bool strict_aot;
static bool strict_program_aot;
static bool strict_file_aot;
static char *stats_output_pattern;

bool latc_aot_v2_is_file_pc(target_ulong guest_pc);
static bool stat_pretranslation_disabled;
static bool stat_aot_cache_hit;
static uint64_t stat_bundle_verify_ns, stat_guest_extract_ns;
static uint64_t stat_aot_prepare_ns;
static LatcDiskExecRange *bundle_exec_ranges;
static uint64_t bundle_exec_range_count;

static char *expand_pid_path(const char *pattern)
{
    GString *path = g_string_new(NULL);
    char pid[32];
    snprintf(pid, sizeof(pid), "%ld", (long)getpid());
    for (const char *p = pattern; *p; p++) {
        if (p[0] == '%' && p[1] == 'p') {
            g_string_append(path, pid);
            p++;
        } else {
            g_string_append_c(path, *p);
        }
    }
    return g_string_free(path, false);
}

static void write_stats(void)
{
    const char *stats_pattern = stats_output_pattern;
    if (!stats_pattern || !*stats_pattern) return;
    char *stats_path = expand_pid_path(stats_pattern);
    FILE *stats = fopen(stats_path, "w");
    if (!stats) {
        g_free(stats_path);
        return;
    }
    fprintf(stats, "{\"pid\":%ld,\"cfg_tbs\":%llu,\"selected_tbs\":%llu,"
            "\"pretranslated\":%llu,\"continuation_tbs\":%llu,"
            "\"edge_target_tbs\":%llu,"
            "\"failed\":%llu,"
            "\"same_extent\":%llu,\"shorter_than_cfg\":%llu,"
            "\"longer_than_cfg\":%llu,\"runtime_tb_gen_calls\":%llu,"
            "\"runtime_tb_gen_attempts\":%llu,"
            "\"runtime_program_tb_gen_calls\":%llu,"
            "\"runtime_system_tb_gen_calls\":%llu,"
            "\"runtime_program_tb_gen_attempts\":%llu,"
            "\"runtime_system_tb_gen_attempts\":%llu,"
            "\"runtime_file_tb_gen_calls\":%llu,"
            "\"runtime_nonfile_tb_gen_calls\":%llu,"
            "\"runtime_file_tb_gen_attempts\":%llu,"
            "\"runtime_nonfile_tb_gen_attempts\":%llu,"
            "\"runtime_first_pc\":%llu,\"runtime_first_cflags\":%u,"
            "\"pretranslation_disabled\":%s,\"aot_cache_hit\":%s,"
            "\"bundle_verify_ns\":%llu,\"guest_extract_ns\":%llu,"
            "\"aot_prepare_ns\":%llu}\n",
            (long)getpid(), (unsigned long long)stat_cfg_tbs,
            (unsigned long long)stat_selected,
            (unsigned long long)stat_pretranslated,
            (unsigned long long)stat_continuation_tbs,
            (unsigned long long)stat_edge_target_tbs,
            (unsigned long long)stat_failed,
            (unsigned long long)stat_same_extent,
            (unsigned long long)stat_shorter_extent,
            (unsigned long long)stat_longer_extent,
            (unsigned long long)stat_runtime_tb_gen_calls,
            (unsigned long long)stat_runtime_tb_gen_attempts,
            (unsigned long long)stat_runtime_program_tb_gen_calls,
            (unsigned long long)stat_runtime_system_tb_gen_calls,
            (unsigned long long)stat_runtime_program_tb_gen_attempts,
            (unsigned long long)stat_runtime_system_tb_gen_attempts,
            (unsigned long long)stat_runtime_file_tb_gen_calls,
            (unsigned long long)stat_runtime_nonfile_tb_gen_calls,
            (unsigned long long)stat_runtime_file_tb_gen_attempts,
            (unsigned long long)stat_runtime_nonfile_tb_gen_attempts,
            (unsigned long long)stat_runtime_first_pc,
            stat_runtime_first_cflags,
            stat_pretranslation_disabled ? "true" : "false",
            stat_aot_cache_hit ? "true" : "false",
            (unsigned long long)stat_bundle_verify_ns,
            (unsigned long long)stat_guest_extract_ns,
            (unsigned long long)stat_aot_prepare_ns);
    fclose(stats);
    g_free(stats_path);
}

void latc_bundle_flush_stats(void)
{
    write_stats();
}

static bool program_address(uint64_t guest_pc)
{
    for (uint64_t i = 0; i < bundle_exec_range_count; i++) {
        LatcDiskExecRange *range = &bundle_exec_ranges[i];
        if (guest_pc >= range->start && guest_pc - range->start < range->size)
            return true;
    }
    return false;
}

static bool runtime_stats_enabled(void)
{
    const char *path = stats_output_pattern;
    return (bundle_self_fd >= 0 && pretranslation_complete) ||
           (bundle_self_fd < 0 && path && *path);
}

static uint32_t tbset_cflags(uint32_t base, uint32_t semantic_flags)
{
    uint32_t result = base & ~CF_PARALLEL;
    if (semantic_flags & LAT_AOT_TB_PARALLEL) result |= CF_PARALLEL;
    return result;
}

typedef struct LatcPendingTb {
    target_ulong pc;
    uint32_t cflags;
} LatcPendingTb;

static guint pending_tb_hash(gconstpointer pointer)
{
    const LatcPendingTb *tb = pointer;
    return (guint)(tb->pc ^ (tb->pc >> 32) ^
                   ((tb->cflags & CF_PARALLEL) ? 0x9e3779b9U : 0));
}

static gboolean pending_tb_equal(gconstpointer left, gconstpointer right)
{
    const LatcPendingTb *a = left;
    const LatcPendingTb *b = right;
    return a->pc == b->pc &&
           (a->cflags & CF_PARALLEL) == (b->cflags & CF_PARALLEL);
}

static void mark_pretranslate_scheduled(GHashTable *scheduled,
                                        target_ulong pc, uint32_t cflags)
{
    LatcPendingTb candidate = { .pc = pc, .cflags = cflags };
    if (g_hash_table_contains(scheduled, &candidate)) {
        return;
    }
    LatcPendingTb *key = g_new(LatcPendingTb, 1);
    *key = candidate;
    g_hash_table_add(scheduled, key);
}

static void queue_pretranslate_target(GArray *pending, GHashTable *scheduled,
                                      target_ulong pc, uint32_t cflags)
{
    LatcPendingTb item = { .pc = pc, .cflags = cflags };
    if (!pc || !program_address(pc) ||
        g_hash_table_contains(scheduled, &item)) {
        return;
    }
    mark_pretranslate_scheduled(scheduled, pc, cflags);
    g_array_append_val(pending, item);
}

static void queue_pretranslate_successors(GArray *pending,
                                          GHashTable *scheduled,
                                          TranslationBlock *tb)
{
    if (tb->s_data) {
        switch (tb->s_data->last_ir1_type) {
        case IR1_TYPE_BRANCH:
            queue_pretranslate_target(pending, scheduled,
                                      tb->s_data->next_pc, tb->cflags);
            /* fall through */
        case IR1_TYPE_CALL:
            if (tb->s_data->last_ir1_type == IR1_TYPE_CALL) {
                queue_pretranslate_target(pending, scheduled,
                                          tb->s_data->next_pc, tb->cflags);
            }
            /* fall through */
        case IR1_TYPE_JUMP:
            queue_pretranslate_target(pending, scheduled,
                                      tb->s_data->target_pc, tb->cflags);
            break;
        case IR1_TYPE_CALLIN:
        case IR1_TYPE_JUMPIN:
        case IR1_TYPE_SYSCALL:
            queue_pretranslate_target(pending, scheduled,
                                      tb->s_data->next_pc, tb->cflags);
            break;
        default:
            break;
        }
    }
}

void latc_bundle_note_tb_attempt(uint64_t guest_pc, uint32_t cflags)
{
    bool program_pc = program_address(guest_pc);
    if (!runtime_stats_enabled()) return;
    if (!stat_runtime_tb_gen_attempts) {
        stat_runtime_first_pc = guest_pc;
        stat_runtime_first_cflags = cflags;
    }
    stat_runtime_tb_gen_attempts++;
    if (program_pc) stat_runtime_program_tb_gen_attempts++;
    else stat_runtime_system_tb_gen_attempts++;
    if (latc_aot_v2_is_file_pc(guest_pc)) stat_runtime_file_tb_gen_attempts++;
    else stat_runtime_nonfile_tb_gen_attempts++;
    const char *profile_path = getenv("LATC_PROFILE_OUT");
    if (profile_path && *profile_path) {
        FILE *profile = fopen(profile_path, "a");
        if (profile) {
            fprintf(profile, "0x%llx 1\n", (unsigned long long)guest_pc);
            fclose(profile);
        }
    }
    /* Keep the evidence valid even when the guest terminates via _exit. */
    write_stats();
}

void latc_bundle_note_tb_generated(uint64_t guest_pc, uint32_t cflags)
{
    bool program_pc = program_address(guest_pc);
    bool file_pc = latc_aot_v2_is_file_pc(guest_pc);
    if (strict_aot || (program_pc && strict_program_aot) ||
        (file_pc && strict_file_aot)) {
        fprintf(stderr,
                "latc: strict AOT rejected generated JIT TB at 0x%llx "
                "cflags=0x%x file=%d program=%d\n",
                (unsigned long long)guest_pc, cflags, file_pc, program_pc);
        _exit(125);
    }
    if (!runtime_stats_enabled()) return;
    stat_runtime_tb_gen_calls++;
    if (program_pc) stat_runtime_program_tb_gen_calls++;
    else stat_runtime_system_tb_gen_calls++;
    if (file_pc) stat_runtime_file_tb_gen_calls++;
    else stat_runtime_nonfile_tb_gen_calls++;
    write_stats();
}

static int copy_range(int src, uint64_t offset, uint64_t size, int dst)
{
    unsigned char buffer[64 * 1024];
    while (size) {
        size_t want = size < sizeof(buffer) ? (size_t)size : sizeof(buffer);
        ssize_t got = pread(src, buffer, want, (off_t)offset);
        if (got <= 0) return -1;
        size_t done = 0;
        while (done < (size_t)got) {
            ssize_t put = write(dst, buffer + done, (size_t)got - done);
            if (put <= 0) return -1;
            done += (size_t)put;
        }
        offset += (uint64_t)got;
        size -= (uint64_t)got;
    }
    return lseek(dst, 0, SEEK_SET) < 0 ? -1 : 0;
}

static int verify_range(int fd, uint64_t offset, uint64_t size,
                        const char expected[64])
{
    unsigned char buffer[64 * 1024];
    GChecksum *sum = g_checksum_new(G_CHECKSUM_SHA256);
    while (size) {
        size_t want = size < sizeof(buffer) ? (size_t)size : sizeof(buffer);
        ssize_t got = pread(fd, buffer, want, (off_t)offset);
        if (got != (ssize_t)want) {
            g_checksum_free(sum);
            return -1;
        }
        g_checksum_update(sum, buffer, want);
        offset += want;
        size -= want;
    }
    int match = memcmp(g_checksum_get_string(sum), expected, 64) == 0;
    g_checksum_free(sum);
    return match ? 0 : -1;
}

static uint64_t monotonic_ns(void)
{
    struct timespec ts;
    return clock_gettime(CLOCK_MONOTONIC, &ts) ? 0 :
        (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static bool cached_aot_matches(const char *path, const char *marker_path,
                               const LatcDiskFooter *footer)
{
    struct stat aot_st, marker_st;
    if (lstat(path, &aot_st) || lstat(marker_path, &marker_st) ||
        !S_ISREG(aot_st.st_mode) || !S_ISREG(marker_st.st_mode) ||
        aot_st.st_uid != geteuid() || marker_st.st_uid != geteuid() ||
        (aot_st.st_mode & 0222) || aot_st.st_size != (off_t)footer->aot_size)
        return false;
    FILE *marker = fopen(marker_path, "r");
    if (!marker) return false;
    char digest[65] = {0};
    unsigned long long dev, ino, size, sec, nsec;
    int fields = fscanf(marker, "%64s %llu %llu %llu %llu %llu",
                        digest, &dev, &ino, &size, &sec, &nsec);
    fclose(marker);
    return fields == 6 && !memcmp(digest, footer->aot_sha256, 64) &&
        dev == (unsigned long long)aot_st.st_dev &&
        ino == (unsigned long long)aot_st.st_ino &&
        size == (unsigned long long)aot_st.st_size &&
        sec == (unsigned long long)aot_st.st_mtim.tv_sec &&
        nsec == (unsigned long long)aot_st.st_mtim.tv_nsec;
}

static int install_cached_aot(int self, const LatcDiskFooter *footer,
                              const char *path, const char *marker_path)
{
    char *tmp = g_strdup_printf("%s.tmp.%ld", path, (long)getpid());
    char *marker_tmp = g_strdup_printf("%s.tmp.%ld", marker_path,
                                       (long)getpid());
    int fd = -1, marker_fd = -1, rc = -1;
    unlink(tmp);
    unlink(marker_tmp);
    fd = open(tmp, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
    if (fd < 0 || copy_range(self, footer->aot_offset,
                             footer->aot_size, fd) || fsync(fd) ||
        fchmod(fd, 0400) || close(fd)) {
        if (fd >= 0) close(fd);
        fd = -1;
        goto out;
    }
    fd = -1;
    if (rename(tmp, path)) goto out;
    struct stat st;
    if (stat(path, &st)) goto out;
    marker_fd = open(marker_tmp, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC,
                     0600);
    if (marker_fd < 0 ||
        dprintf(marker_fd, "%.64s %llu %llu %llu %llu %llu\n",
                footer->aot_sha256,
                (unsigned long long)st.st_dev,
                (unsigned long long)st.st_ino,
                (unsigned long long)st.st_size,
                (unsigned long long)st.st_mtim.tv_sec,
                (unsigned long long)st.st_mtim.tv_nsec) < 0 ||
        fsync(marker_fd) || fchmod(marker_fd, 0400) || close(marker_fd)) {
        if (marker_fd >= 0) close(marker_fd);
        marker_fd = -1;
        goto out;
    }
    marker_fd = -1;
    if (rename(marker_tmp, marker_path)) goto out;
    rc = 0;
out:
    if (fd >= 0) close(fd);
    if (marker_fd >= 0) close(marker_fd);
    if (rc) {
        unlink(tmp);
        unlink(marker_tmp);
    }
    g_free(tmp);
    g_free(marker_tmp);
    return rc;
}

int latc_bundle_inject_argv(int *argc, char ***argv)
{
    strict_aot = getenv("LATC_STRICT_AOT") != NULL;
    strict_program_aot = getenv("LATC_STRICT_PROGRAM_AOT") != NULL;
    strict_file_aot = getenv("LATC_STRICT_FILE_AOT") != NULL;
    const char *stats = getenv("LATC_STATS_OUT");
    stats_output_pattern = stats && *stats ? g_strdup(stats) : NULL;
    unsetenv("LATC_STATS_OUT");

    int self = open("/proc/self/exe", O_RDONLY | O_CLOEXEC);
    if (self < 0) return 0;
    struct stat st;
    LatcDiskFooter footer;
    int rc = 0;
    if (fstat(self, &st) || st.st_size < (off_t)sizeof(footer) ||
        pread(self, &footer, sizeof(footer), st.st_size - (off_t)sizeof(footer)) !=
            (ssize_t)sizeof(footer)) goto out;
    if (memcmp(footer.magic, LATC_BUNDLE_MAGIC, 8) != 0) goto out;
    if (footer.version != LATC_BUNDLE_VERSION ||
        footer.footer_size != sizeof(footer) ||
        footer.guest_offset != footer.runner_size ||
        footer.cfg_offset != footer.guest_offset + footer.guest_size ||
        footer.aot_offset != footer.cfg_offset + footer.cfg_size ||
        footer.aot_offset + footer.aot_size + sizeof(footer) != (uint64_t)st.st_size ||
        (footer.aot_size && (!footer.aot_name[0] ||
                            footer.aot_name[sizeof(footer.aot_name) - 1] ||
                            strchr(footer.aot_name, '/')))) {
        errno = EINVAL; rc = -1; goto out;
    }
    uint64_t started = monotonic_ns();
    if (verify_range(self, footer.guest_offset, footer.guest_size,
                     footer.guest_sha256)) {
        errno = EINVAL; rc = -1; goto out;
    }
    stat_bundle_verify_ns = monotonic_ns() - started;
    char automatic_guest[PATH_MAX];
    const char *named_guest = getenv("LATC_NAMED_GUEST");
    if ((!named_guest || !*named_guest) && footer.aot_size) {
        snprintf(automatic_guest, sizeof(automatic_guest),
                 "/tmp/latc-%.16s-x86-guest", footer.guest_sha256);
        named_guest = automatic_guest;
    }
    started = monotonic_ns();
    int guest = named_guest && *named_guest ?
        open(named_guest, O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC, 0700) :
        (int)syscall(SYS_memfd_create, "latc-x86-guest", 0);
    if (guest < 0 || copy_range(self, footer.guest_offset,
                                footer.guest_size, guest)) {
        if (guest >= 0) close(guest);
        rc = -1; goto out;
    }
    if (named_guest && *named_guest) {
        const struct timespec times[2] = {
            { .tv_nsec = UTIME_OMIT },
            { .tv_sec = 1, .tv_nsec = 0 },
        };
        if (futimens(guest, times)) {
            close(guest); rc = -1; goto out;
        }
    }
    stat_guest_extract_ns = monotonic_ns() - started;
    bundle_footer = footer;
    bundle_self_fd = self;
    self = -1;
    if (footer.aot_size) {
        started = monotonic_ns();
        const char *home = getenv("HOME");
        char *cache_dir = home && *home ?
            g_build_filename(home, ".cache", "latx", NULL) : NULL;
        char *aot_path = cache_dir ?
            g_build_filename(cache_dir, footer.aot_name, NULL) : NULL;
        char *marker_path = aot_path ?
            g_strdup_printf("%s.latc-cache", aot_path) : NULL;
        if (!cache_dir || !aot_path || !marker_path ||
            g_mkdir_with_parents(cache_dir, 0700) ||
            strlen(aot_path) >= PATH_MAX || strlen(marker_path) >= PATH_MAX) {
            g_free(marker_path);
            g_free(cache_dir); g_free(aot_path); close(guest); rc = -1; goto out;
        }
        stat_aot_cache_hit = cached_aot_matches(aot_path, marker_path, &footer);
        if (!stat_aot_cache_hit &&
            (verify_range(bundle_self_fd, footer.aot_offset, footer.aot_size,
                          footer.aot_sha256) ||
             install_cached_aot(bundle_self_fd, &footer, aot_path,
                                marker_path))) {
            g_free(marker_path); g_free(cache_dir); g_free(aot_path);
            close(guest); rc = -1; goto out;
        }
        stat_aot_prepare_ns = monotonic_ns() - started;
        g_free(marker_path);
        g_free(cache_dir);
        g_free(aot_path);
        setenv("LATX_AOT", "1", 1);
        setenv("LATC_DISABLE_PRETRANSLATE", "1", 1);
    }
    char *guest_path = named_guest && *named_guest ? strdup(named_guest) : NULL;
    if (!guest_path && asprintf(&guest_path, "/proc/self/fd/%d", guest) < 0) {
        close(guest); rc = -1; goto out;
    }
    char **old = *argv;
    char **next = calloc((size_t)*argc + 3, sizeof(*next));
    if (!next) { free(guest_path); close(guest); rc = -1; goto out; }
    next[0] = old[0];
    next[1] = strdup("--");
    next[2] = guest_path;
    for (int i = 1; i < *argc; i++) next[i + 2] = old[i];
    if (!next[1]) {
        free(next); free(guest_path); close(guest); rc = -1; goto out;
    }
    *argc += 2;
    *argv = next;
    rc = 1;
out:
    if (self >= 0) close(self);
    return rc;
}

static int hex_nibble(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    return -1;
}

int latc_bundle_verified_guest_digest(uint8_t digest[32])
{
    if (!digest) {
        errno = EINVAL;
        return -1;
    }
    if (bundle_self_fd < 0) {
        return 0;
    }
    for (size_t i = 0; i < 32; i++) {
        int high = hex_nibble(bundle_footer.guest_sha256[i * 2]);
        int low = hex_nibble(bundle_footer.guest_sha256[i * 2 + 1]);
        if (high < 0 || low < 0) {
            errno = ENOEXEC;
            return -1;
        }
        digest[i] = (uint8_t)((high << 4) | low);
    }
    return 1;
}

int latc_bundle_verified_guest(uint8_t digest[32], uint64_t *guest_begin,
                               uint64_t *guest_end)
{
    if (!digest || !guest_begin || !guest_end) {
        errno = EINVAL;
        return -1;
    }
    if (bundle_self_fd < 0) {
        return 0;
    }
    Elf64_Ehdr header;
    if (bundle_footer.guest_size < sizeof(header) ||
        pread(bundle_self_fd, &header, sizeof(header),
              (off_t)bundle_footer.guest_offset) != sizeof(header) ||
        memcmp(header.e_ident, ELFMAG, SELFMAG) ||
        header.e_ident[EI_CLASS] != ELFCLASS64 ||
        header.e_machine != EM_X86_64 || header.e_type != ET_EXEC ||
        header.e_phentsize != sizeof(Elf64_Phdr) || !header.e_phnum ||
        header.e_phoff > bundle_footer.guest_size ||
        header.e_phnum > (bundle_footer.guest_size - header.e_phoff) /
                             sizeof(Elf64_Phdr)) {
        errno = ENOEXEC;
        return -1;
    }
    size_t phdr_size = header.e_phnum * sizeof(Elf64_Phdr);
    Elf64_Phdr *phdrs = g_malloc(phdr_size);
    if (pread(bundle_self_fd, phdrs, phdr_size,
              (off_t)(bundle_footer.guest_offset + header.e_phoff)) !=
        (ssize_t)phdr_size) {
        g_free(phdrs);
        errno = ENOEXEC;
        return -1;
    }
    uint64_t begin = UINT64_MAX;
    uint64_t end = 0;
    for (uint16_t i = 0; i < header.e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) {
            continue;
        }
        if (phdrs[i].p_vaddr > UINT64_MAX - phdrs[i].p_memsz) {
            g_free(phdrs);
            errno = ENOEXEC;
            return -1;
        }
        begin = MIN(begin, phdrs[i].p_vaddr);
        end = MAX(end, phdrs[i].p_vaddr + phdrs[i].p_memsz);
    }
    g_free(phdrs);
    if (begin == UINT64_MAX || end <= begin) {
        errno = ENOEXEC;
        return -1;
    }
    if (latc_bundle_verified_guest_digest(digest) != 1) return -1;
    *guest_begin = begin;
    *guest_end = end;
    return 1;
}

static int read_cfg(void *buffer, size_t size, uint64_t offset)
{
    return bundle_self_fd >= 0 &&
           pread(bundle_self_fd, buffer, size,
                 (off_t)(bundle_footer.cfg_offset + offset)) == (ssize_t)size ? 0 : -1;
}

void latc_bundle_pretranslate(struct CPUState *cpu, uint64_t guest_entry)
{
    if (bundle_self_fd < 0 || !cpu) return;
    Elf64_Ehdr elf;
    if (bundle_footer.guest_size < sizeof(elf) ||
        pread(bundle_self_fd, &elf, sizeof(elf),
              (off_t)bundle_footer.guest_offset) != sizeof(elf) ||
        (elf.e_type != ET_EXEC && elf.e_type != ET_DYN) ||
        (elf.e_type == ET_DYN && guest_entry < elf.e_entry)) {
        fprintf(stderr, "latc: invalid embedded ELF load bias\n");
        return;
    }
    uint64_t load_bias = elf.e_type == ET_DYN ? guest_entry - elf.e_entry : 0;
    LatcDiskCfgHeader header;
    if (read_cfg(&header, sizeof(header), 0) ||
        memcmp(header.magic, LATC_CFG_MAGIC, 8) ||
        header.version != LATC_CFG_VERSION || header.record_size != sizeof(header)) {
        fprintf(stderr, "latc: invalid embedded CFG\n");
        return;
    }
    uint64_t range_offset = sizeof(header) +
        header.function_count * sizeof(LatcDiskFunction);
    bundle_exec_range_count = header.exec_range_count;
    bundle_exec_ranges = calloc(header.exec_range_count,
                                sizeof(*bundle_exec_ranges));
    if (header.exec_range_count && (!bundle_exec_ranges ||
        read_cfg(bundle_exec_ranges,
                 header.exec_range_count * sizeof(*bundle_exec_ranges),
                 range_offset))) {
        free(bundle_exec_ranges);
        bundle_exec_ranges = NULL;
        bundle_exec_range_count = 0;
        fprintf(stderr, "latc: invalid embedded executable ranges\n");
        return;
    }
    for (uint64_t i = 0; i < bundle_exec_range_count; i++) {
        if (bundle_exec_ranges[i].start > UINT64_MAX - load_bias) {
            free(bundle_exec_ranges);
            bundle_exec_ranges = NULL;
            bundle_exec_range_count = 0;
            fprintf(stderr, "latc: embedded executable range overflows\n");
            return;
        }
        bundle_exec_ranges[i].start += load_bias;
    }
    if (getenv("LATC_DISABLE_PRETRANSLATE")) {
        stat_cfg_tbs = header.tb_count;
        stat_pretranslation_disabled = true;
        pretranslation_complete = true;
        write_stats();
        return;
    }
    uint64_t tb_offset = sizeof(header) +
        header.function_count * sizeof(LatcDiskFunction) +
        header.exec_range_count * sizeof(LatcDiskExecRange);
    CPUArchState *env = (CPUArchState *)cpu->env_ptr;
    target_ulong current_pc, cs_base;
    uint32_t flags;
    cpu_get_tb_cpu_state(env, &current_pc, &cs_base, &flags);
    (void)current_pc;
    uint32_t cflags = curr_cflags(cpu);
    uint64_t translated = 0, continuations = 0, edge_targets = 0;
    uint64_t failed = 0, selected = 0;
    uint64_t same_extent = 0, shorter_extent = 0, longer_extent = 0;
    GHashTable *translated_pcs = g_hash_table_new(g_direct_hash,
                                                  g_direct_equal);
    GHashTable *scheduled_pcs = g_hash_table_new_full(
        pending_tb_hash, pending_tb_equal, g_free, NULL);
    GArray *pending = g_array_new(FALSE, FALSE, sizeof(LatcPendingTb));
    uint64_t pretranslate_started = monotonic_ns();

    if (getenv("LATC_EMIT_AOT")) {
        for (uint64_t i = 0; i < header.tb_count; i++) {
            LatcDiskTb disk_tb;
            if (read_cfg(&disk_tb, sizeof(disk_tb),
                         tb_offset + i * sizeof(disk_tb))) {
                failed++;
                continue;
            }
            if (!disk_tb.selected) {
                continue;
            }
            selected++;
            uint64_t pc = disk_tb.start + load_bias;
            if (pc < disk_tb.start || !program_address(pc)) {
                failed++;
                continue;
            }
            uint32_t tb_cflags = tbset_cflags(cflags,
                                              disk_tb.semantic_flags);
            uint16_t bool_flags = IS_CODE64;
            if (disk_tb.semantic_flags & LATC_CFG_TB_BOUNDED) {
                bool_flags |= IS_AOT_BOUNDED;
            }
            aot_compile_key_add(pc, tb_cflags, bool_flags);
        }
        stat_cfg_tbs = header.tb_count;
        stat_selected = selected;
        stat_pretranslated = selected;
        stat_failed = failed;
        pretranslation_complete = true;
        write_stats();
        return;
    }

    /* The TB set is authoritative.  Do not compile the rest of the static CFG. */
    for (uint64_t i = 0; i < header.tb_count; i++) {
            LatcDiskTb disk_tb;
            if (read_cfg(&disk_tb, sizeof(disk_tb),
                         tb_offset + i * sizeof(disk_tb))) {
                failed++;
                continue;
            }
            if (!disk_tb.selected) continue;
            selected++;
            if (disk_tb.end < disk_tb.start ||
                disk_tb.end > UINT64_MAX - load_bias) {
                failed++;
                continue;
            }
            uint64_t translated_start = disk_tb.start + load_bias;
            uint64_t translated_end = disk_tb.end + load_bias;
            target_ulong pc = translated_start;
            uint32_t tb_cflags = tbset_cflags(cflags,
                                              disk_tb.semantic_flags);
            bool first = true;
            while (pc < translated_end) {
                mmap_lock();
                TranslationBlock *tb = tb_gen_code(cpu, pc, cs_base,
                                                   flags, tb_cflags);
                if (tb) {
                    mark_pretranslate_scheduled(scheduled_pcs, pc,
                                                tb_cflags);
                    queue_pretranslate_successors(pending, scheduled_pcs, tb);
                    jrra_pre_translate((void **)&tb, 1, cpu, flags,
                                       tb_cflags);
                }
                mmap_unlock();
                if (!tb) {
                    failed++;
                    break;
                }
                translated++;
                g_hash_table_add(translated_pcs,
                                 (gpointer)(uintptr_t)pc);
                uint64_t tb_end = (uint64_t)pc + tb->size;
                if (first) {
                    uint64_t cfg_size = translated_end - translated_start;
                    if (tb->size == cfg_size) {
                        same_extent++;
                    } else if (tb->size < cfg_size) {
                        shorter_extent++;
                        if (getenv("LATC_DEBUG_CFG")) {
                            fprintf(stderr, "latc: shorter TB pc=0x%llx cfg=%llu lat=%u\n",
                                    (unsigned long long)translated_start,
                                    (unsigned long long)cfg_size, tb->size);
                        }
                    } else {
                        /* CFG blocks stop at incoming targets; LAT TBs need not. */
                        longer_extent++;
                    }
                    first = false;
                }
                if (tb_end >= translated_end) {
                    break;
                }
                if (!tb->size) {
                    break;
                }
                pc = tb_end;
                continuations++;
            }
    }
    for (guint i = 0; i < pending->len; i++) {
        LatcPendingTb item = g_array_index(pending, LatcPendingTb, i);
        mmap_lock();
        TranslationBlock *tb = tb_gen_code(cpu, item.pc, cs_base,
                                           flags, item.cflags);
        if (tb) {
            queue_pretranslate_successors(pending, scheduled_pcs, tb);
            jrra_pre_translate((void **)&tb, 1, cpu, flags, item.cflags);
        }
        mmap_unlock();
        if (!tb) {
            failed++;
            continue;
        }
        translated++;
        edge_targets++;
        g_hash_table_add(translated_pcs, (gpointer)(uintptr_t)item.pc);
    }
    uint64_t edge_offset = tb_offset +
        header.tb_count * sizeof(LatcDiskTb);
    uint64_t previous_count;
    do {
        previous_count = g_hash_table_size(translated_pcs);
        for (uint64_t i = 0; i < header.edge_count; i++) {
        LatcDiskEdge edge;
        if (read_cfg(&edge, sizeof(edge),
                     edge_offset + i * sizeof(edge))) {
            failed++;
            continue;
        }
        if (!edge.from || !edge.to ||
            edge.from > UINT64_MAX - load_bias ||
            edge.to > UINT64_MAX - load_bias) {
            continue;
        }
        uint64_t source_pc = edge.from + load_bias;
        uint64_t edge_pc = edge.to + load_bias;
        if (!g_hash_table_contains(translated_pcs,
                                  (gpointer)(uintptr_t)source_pc) ||
            !program_address(edge_pc) ||
            g_hash_table_contains(translated_pcs,
                                  (gpointer)(uintptr_t)edge_pc)) {
            continue;
        }
        target_ulong pc = edge_pc;
        mmap_lock();
        TranslationBlock *tb = tb_gen_code(cpu, pc, cs_base,
                                           flags, cflags);
        if (tb) {
            jrra_pre_translate((void **)&tb, 1, cpu, flags, cflags);
        }
        mmap_unlock();
        if (!tb) {
            failed++;
            continue;
        }
        translated++;
        edge_targets++;
        g_hash_table_add(translated_pcs, (gpointer)(uintptr_t)pc);
        }
    } while (g_hash_table_size(translated_pcs) != previous_count);
    if (getenv("LATC_COMPILE_TIMING")) {
        fprintf(stderr, "latc: compile timing tb_translate_ms=%llu\n",
                (unsigned long long)
                ((monotonic_ns() - pretranslate_started) / 1000000));
    }
    g_hash_table_destroy(translated_pcs);
    g_hash_table_destroy(scheduled_pcs);
    g_array_free(pending, TRUE);
    stat_cfg_tbs = header.tb_count;
    stat_selected = selected;
    stat_pretranslated = translated;
    stat_continuation_tbs = continuations;
    stat_edge_target_tbs = edge_targets;
    stat_failed = failed;
    stat_same_extent = same_extent;
    stat_shorter_extent = shorter_extent;
    stat_longer_extent = longer_extent;
    pretranslation_complete = true;
    write_stats();
}
