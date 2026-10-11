#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "guest-elf-map.h"

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct LatGuestElfTrackerV2 {
    GPtrArray *entries;
};

static char *configured_cache;
static bool cache_configured;

void lat_guest_elf_set_cache_directory_v2(const char *directory)
{
    g_free(configured_cache);
    configured_cache = g_strdup(directory);
    cache_configured = true;
}

static int invalid(char *error, size_t error_size, const char *format, ...)
{
    if (error && error_size) {
        va_list args;
        va_start(args, format);
        vsnprintf(error, error_size, format, args);
        va_end(args);
    }
    errno = ENOEXEC;
    return -1;
}

static uint64_t align_down(uint64_t value, uint64_t alignment)
{
    return value & ~(alignment - 1);
}

static int align_up(uint64_t value, uint64_t alignment, uint64_t *result)
{
    if (value > UINT64_MAX - (alignment - 1)) {
        errno = EOVERFLOW;
        return -1;
    }
    *result = (value + alignment - 1) & ~(alignment - 1);
    return 0;
}

static int same_identity(const struct stat *left, const struct stat *right)
{
    return left->st_dev == right->st_dev && left->st_ino == right->st_ino &&
        left->st_size == right->st_size &&
        left->st_mtim.tv_sec == right->st_mtim.tv_sec &&
        left->st_mtim.tv_nsec == right->st_mtim.tv_nsec &&
        left->st_ctim.tv_sec == right->st_ctim.tv_sec &&
        left->st_ctim.tv_nsec == right->st_ctim.tv_nsec;
}

static char *digest_cache_path(const struct stat *status)
{
    const char *cache = cache_configured ? configured_cache :
                       getenv("LATX_AOT_V2_CACHE_DIR");
    if (!cache || !*cache) {
        return NULL;
    }
    struct stat root;
    if (lstat(cache, &root) || !S_ISDIR(root.st_mode) ||
        root.st_uid != geteuid() || (root.st_mode & 0077)) {
        return NULL;
    }
    char *directory = g_build_filename(cache, ".identities", NULL);
    struct stat identity_dir;
    if (lstat(directory, &identity_dir) || !S_ISDIR(identity_dir.st_mode) ||
        identity_dir.st_uid != geteuid() || (identity_dir.st_mode & 0077)) {
        g_free(directory);
        return NULL;
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
    char *path = g_build_filename(directory, name, NULL);
    g_free(name);
    g_free(directory);
    return path;
}

static int decode_digest(const char text[64], uint8_t digest[32])
{
    for (size_t i = 0; i < 32; i++) {
        int high = g_ascii_xdigit_value(text[i * 2]);
        int low = g_ascii_xdigit_value(text[i * 2 + 1]);
        if (high < 0 || low < 0) {
            return -1;
        }
        digest[i] = (uint8_t)((high << 4) | low);
    }
    return 0;
}

static int read_cached_digest(const char *path, uint8_t digest[32])
{
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        return errno == ENOENT ? 1 : -1;
    }
    struct stat status;
    char text[65];
    ssize_t count = read(fd, text, sizeof(text));
    int valid = !fstat(fd, &status) &&
        S_ISREG(status.st_mode) && status.st_uid == geteuid() &&
        status.st_nlink == 1 && !(status.st_mode & 0077) &&
        status.st_size == 65 && count == 65 &&
        text[64] == '\n' && !decode_digest(text, digest);
    close(fd);
    if (!valid) errno = EINVAL;
    return valid ? 0 : -1;
}

static int digest_fd_uncached(int fd, uint8_t digest[32])
{
    unsigned char buffer[64 * 1024];
    GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
    if (!checksum) {
        errno = ENOMEM;
        return -1;
    }
    uint64_t offset = 0;
    for (;;) {
        ssize_t count = pread(fd, buffer, sizeof(buffer), (off_t)offset);
        if (count < 0) {
            g_checksum_free(checksum);
            return -1;
        }
        if (!count) {
            break;
        }
        g_checksum_update(checksum, buffer, (gsize)count);
        offset += (uint64_t)count;
    }
    gsize size = 32;
    g_checksum_get_digest(checksum, digest, &size);
    g_checksum_free(checksum);
    return size == 32 ? 0 : -1;
}

static int digest_fd(int fd, const struct stat *initial, uint8_t digest[32])
{
    for (int attempt = 0; attempt < 2; attempt++) {
        struct stat before = *initial;
        if (attempt && fstat(fd, &before)) {
            return -1;
        }
        char *path = digest_cache_path(&before);
        int cache_result = path ? read_cached_digest(path, digest) : 1;
        int cached = cache_result == 0;
        if (!cached && digest_fd_uncached(fd, digest)) {
            g_free(path);
            return -1;
        }
        struct stat after;
        if (fstat(fd, &after)) {
            g_free(path);
            return -1;
        }
        if (same_identity(&before, &after)) {
            g_free(path);
            return 0;
        }
        g_free(path);
    }
    errno = EAGAIN;
    return -1;
}

int lat_guest_elf_inspect_mapping_v2(int fd, uint64_t guest_start,
                                     uint64_t mapping_size,
                                     uint64_t file_offset,
                                     uint64_t page_size,
                                     LatGuestElfInfoV2 *info,
                                     char *error, size_t error_size)
{
    Elf64_Ehdr elf;
    struct stat status;
    if (fd < 0 || !mapping_size || !page_size ||
        (page_size & (page_size - 1)) || !info ||
        fstat(fd, &status) ||
        pread(fd, &elf, sizeof(elf), 0) != sizeof(elf)) {
        return invalid(error, error_size, "cannot inspect mapped ELF");
    }
    if (memcmp(elf.e_ident, ELFMAG, SELFMAG) ||
        elf.e_ident[EI_CLASS] != ELFCLASS64 ||
        elf.e_ident[EI_DATA] != ELFDATA2LSB ||
        elf.e_machine != EM_X86_64 ||
        (elf.e_type != ET_EXEC && elf.e_type != ET_DYN) ||
        elf.e_phentsize != sizeof(Elf64_Phdr) || !elf.e_phnum ||
        status.st_size < 0 || elf.e_phoff > (uint64_t)status.st_size ||
        (uint64_t)elf.e_phnum * sizeof(Elf64_Phdr) >
            (uint64_t)status.st_size - elf.e_phoff) {
        return invalid(error, error_size, "mapping is not a supported x86-64 ELF");
    }
    size_t phdr_size = (size_t)elf.e_phnum * sizeof(Elf64_Phdr);
    Elf64_Phdr *phdrs = malloc(phdr_size);
    if (!phdrs) {
        return -1;
    }
    if (pread(fd, phdrs, phdr_size, (off_t)elf.e_phoff) !=
        (ssize_t)phdr_size) {
        free(phdrs);
        return invalid(error, error_size, "cannot read mapped ELF headers");
    }

    uint64_t mapping_end;
    if (file_offset > UINT64_MAX - mapping_size) {
        free(phdrs);
        return invalid(error, error_size, "mapping file range overflows");
    }
    mapping_end = file_offset + mapping_size;
    int found_bias = 0;
    uint64_t load_bias = 0;
    for (uint16_t i = 0; i < elf.e_phnum; i++) {
        const Elf64_Phdr *phdr = &phdrs[i];
        if (phdr->p_type != PT_LOAD || !phdr->p_filesz ||
            phdr->p_offset < file_offset ||
            phdr->p_offset >= mapping_end) {
            continue;
        }
        uint64_t delta = phdr->p_offset - file_offset;
        if (guest_start > UINT64_MAX - delta ||
            guest_start + delta < phdr->p_vaddr) {
            free(phdrs);
            return invalid(error, error_size, "mapped ELF load bias overflows");
        }
        uint64_t candidate = guest_start + delta - phdr->p_vaddr;
        if (found_bias && candidate != load_bias) {
            free(phdrs);
            return invalid(error, error_size, "mapped ELF has inconsistent load bias");
        }
        load_bias = candidate;
        found_bias = 1;
    }
    if (!found_bias) {
        free(phdrs);
        return invalid(error, error_size,
                       "mapping does not contain a PT_LOAD");
    }

    LatGuestElfInfoV2 output = {
        .device = (uint64_t)status.st_dev,
        .inode = (uint64_t)status.st_ino,
        .elf_type = elf.e_type,
        .load_bias = load_bias,
        .preferred_base = UINT64_MAX,
        .guest_begin = UINT64_MAX,
    };
    for (uint16_t i = 0; i < elf.e_phnum; i++) {
        const Elf64_Phdr *phdr = &phdrs[i];
        if (phdr->p_type != PT_LOAD || !phdr->p_memsz ||
            phdr->p_vaddr > UINT64_MAX - phdr->p_memsz) {
            continue;
        }
        uint64_t preferred_begin = align_down(phdr->p_vaddr, page_size);
        uint64_t preferred_end;
        if (align_up(phdr->p_vaddr + phdr->p_memsz, page_size,
                     &preferred_end) ||
            preferred_begin > UINT64_MAX - load_bias ||
            preferred_end > UINT64_MAX - load_bias) {
            free(phdrs);
            return invalid(error, error_size, "mapped ELF guest range overflows");
        }
        uint64_t begin = load_bias + preferred_begin;
        uint64_t end = load_bias + preferred_end;
        output.preferred_base = MIN(output.preferred_base, preferred_begin);
        output.guest_begin = MIN(output.guest_begin, begin);
        output.guest_end = MAX(output.guest_end, end);
        if (phdr->p_flags & PF_X) {
            if (output.exec_range_count == LAT_GUEST_ELF_MAX_EXEC_RANGES) {
                free(phdrs);
                return invalid(error, error_size,
                               "mapped ELF has too many executable ranges");
            }
            output.exec_ranges[output.exec_range_count++] =
                (LatGuestElfRangeV2){ .begin = begin, .end = end };
        }
    }
    free(phdrs);
    if (output.preferred_base == UINT64_MAX ||
        output.guest_begin == UINT64_MAX ||
        output.guest_end <= output.guest_begin || !output.exec_range_count ||
        digest_fd(fd, &status, output.source_sha256)) {
        return invalid(error, error_size, "cannot identify mapped ELF");
    }
    *info = output;
    return 0;
}

LatGuestElfTrackerV2 *lat_guest_elf_tracker_new_v2(void)
{
    LatGuestElfTrackerV2 *tracker = calloc(1, sizeof(*tracker));
    if (tracker) {
        tracker->entries = g_ptr_array_new_with_free_func(free);
        if (!tracker->entries) {
            free(tracker);
            tracker = NULL;
        }
    }
    return tracker;
}

void lat_guest_elf_tracker_free_v2(LatGuestElfTrackerV2 *tracker)
{
    if (tracker) {
        g_ptr_array_free(tracker->entries, TRUE);
        free(tracker);
    }
}

int lat_guest_elf_tracker_note_v2(LatGuestElfTrackerV2 *tracker, int fd,
                                  uint64_t guest_start,
                                  uint64_t mapping_size,
                                  uint64_t file_offset,
                                  uint64_t page_size,
                                  const LatGuestElfInfoV2 **info,
                                  int *added, char *error,
                                  size_t error_size)
{
    if (!tracker || !info || !added) {
        errno = EINVAL;
        return -1;
    }
    struct stat status;
    if (fstat(fd, &status)) {
        return -1;
    }
    for (guint i = 0; i < tracker->entries->len; i++) {
        LatGuestElfInfoV2 *entry = g_ptr_array_index(tracker->entries, i);
        if (entry->device == (uint64_t)status.st_dev &&
            entry->inode == (uint64_t)status.st_ino &&
            guest_start >= entry->guest_begin && guest_start < entry->guest_end) {
            *info = entry;
            *added = 0;
            return 0;
        }
    }
    LatGuestElfInfoV2 inspected;
    if (lat_guest_elf_inspect_mapping_v2(fd, guest_start, mapping_size,
                                         file_offset, page_size, &inspected,
                                         error, error_size)) {
        return -1;
    }
    for (guint i = 0; i < tracker->entries->len; i++) {
        LatGuestElfInfoV2 *entry = g_ptr_array_index(tracker->entries, i);
        if (entry->device == inspected.device && entry->inode == inspected.inode &&
            entry->load_bias == inspected.load_bias) {
            *info = entry;
            *added = 0;
            return 0;
        }
    }
    LatGuestElfInfoV2 *entry = malloc(sizeof(*entry));
    if (!entry) {
        return -1;
    }
    *entry = inspected;
    g_ptr_array_add(tracker->entries, entry);
    *info = entry;
    *added = 1;
    return 0;
}

size_t lat_guest_elf_tracker_count_v2(const LatGuestElfTrackerV2 *tracker)
{
    return tracker ? tracker->entries->len : 0;
}

const LatGuestElfInfoV2 *lat_guest_elf_tracker_get_v2(
    const LatGuestElfTrackerV2 *tracker, size_t index)
{
    return tracker && index < tracker->entries->len ?
        g_ptr_array_index(tracker->entries, index) : NULL;
}

size_t lat_guest_elf_tracker_remove_range_v2(LatGuestElfTrackerV2 *tracker,
                                              uint64_t guest_start,
                                              uint64_t mapping_size)
{
    if (!tracker || !mapping_size ||
        guest_start > UINT64_MAX - mapping_size) {
        return 0;
    }
    uint64_t guest_end = guest_start + mapping_size;
    size_t removed = 0;
    for (guint i = tracker->entries->len; i > 0; i--) {
        LatGuestElfInfoV2 *entry = g_ptr_array_index(tracker->entries, i - 1);
        int overlaps_exec = 0;
        for (uint16_t range = 0; range < entry->exec_range_count; range++) {
            overlaps_exec |= guest_start < entry->exec_ranges[range].end &&
                             guest_end > entry->exec_ranges[range].begin;
        }
        if (overlaps_exec) {
            g_ptr_array_remove_index(tracker->entries, i - 1);
            removed++;
        }
    }
    return removed;
}
