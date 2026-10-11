#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "guest-elf-map.h"

#include <elf.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int executable_mapping(int fd, uint64_t bias, uint64_t *start,
                              uint64_t *size, uint64_t *offset)
{
    Elf64_Ehdr elf;
    if (pread(fd, &elf, sizeof(elf), 0) != sizeof(elf)) {
        return -1;
    }
    for (uint16_t i = 0; i < elf.e_phnum; i++) {
        Elf64_Phdr phdr;
        if (pread(fd, &phdr, sizeof(phdr),
                  (off_t)(elf.e_phoff + i * sizeof(phdr))) != sizeof(phdr)) {
            return -1;
        }
        if (phdr.p_type == PT_LOAD && (phdr.p_flags & PF_X) &&
            phdr.p_filesz) {
            *offset = phdr.p_offset & ~UINT64_C(0xfff);
            *start = bias + (phdr.p_vaddr & ~UINT64_C(0xfff));
            *size = ((phdr.p_offset + phdr.p_filesz + 0xfff) &
                     ~UINT64_C(0xfff)) - *offset;
            return 0;
        }
    }
    return -1;
}

static int inspect_once(int fd, uint8_t digest[32])
{
    LatGuestElfTrackerV2 *tracker = lat_guest_elf_tracker_new_v2();
    uint64_t start, size, offset;
    const LatGuestElfInfoV2 *info;
    int added;
    char error[256] = {0};
    int result = !tracker || executable_mapping(
        fd, UINT64_C(0x555500000000), &start, &size, &offset) ||
        lat_guest_elf_tracker_note_v2(tracker, fd, start, size, offset, 4096,
                                      &info, &added, error, sizeof(error)) ||
        !added;
    if (!result) {
        memcpy(digest, info->source_sha256, 32);
    }
    lat_guest_elf_tracker_free_v2(tracker);
    return result;
}

static int copy_file(const char *source, char path[PATH_MAX])
{
    int input = open(source, O_RDONLY | O_CLOEXEC);
    snprintf(path, PATH_MAX, "%s/latc-guest-elf-XXXXXX",
             getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
    int output = mkstemp(path);
    char buffer[65536];
    ssize_t count = 0;
    while (input >= 0 && output >= 0 &&
           (count = read(input, buffer, sizeof(buffer))) > 0) {
        if (write(output, buffer, count) != count) {
            count = -1;
            break;
        }
    }
    if (input >= 0) close(input);
    if (count < 0 || input < 0 || output < 0 || fsync(output)) {
        if (output >= 0) close(output);
        unlink(path);
        return -1;
    }
    return output;
}

static int count_identities(const char *cache)
{
    char path[PATH_MAX + 32];
    snprintf(path, sizeof(path), "%s/.identities", cache);
    DIR *directory = opendir(path);
    int count = 0;
    if (!directory) return -1;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] != '.') count++;
    }
    closedir(directory);
    return count;
}

static int publish_identity(const char *cache, int source_fd,
                            const uint8_t digest[32])
{
    char directory[PATH_MAX + 32];
    snprintf(directory, sizeof(directory), "%s/.identities", cache);
    if (mkdir(directory, 0700) && errno != EEXIST) return -1;
    struct stat status;
    if (fstat(source_fd, &status)) return -1;
    char path[PATH_MAX + 512];
    snprintf(path, sizeof(path),
        "%s/%llx-%llx-%llx-%llx-%lx-%llx-%lx.sha256", directory,
        (unsigned long long)status.st_dev,
        (unsigned long long)status.st_ino,
        (unsigned long long)status.st_size,
        (unsigned long long)status.st_mtim.tv_sec,
        (unsigned long)status.st_mtim.tv_nsec,
        (unsigned long long)status.st_ctim.tv_sec,
        (unsigned long)status.st_ctim.tv_nsec);
    static const char digits[] = "0123456789abcdef";
    char text[65];
    for (size_t i = 0; i < 32; i++) {
        text[i * 2] = digits[digest[i] >> 4];
        text[i * 2 + 1] = digits[digest[i] & 15];
    }
    text[64] = '\n';
    int output = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    int result = output < 0 || write(output, text, sizeof(text)) != sizeof(text);
    if (output >= 0) close(output);
    return result ? -1 : 0;
}

static int corrupt_identity(const char *cache)
{
    char directory[PATH_MAX + 32];
    snprintf(directory, sizeof(directory), "%s/.identities", cache);
    DIR *identities = opendir(directory);
    struct dirent *entry;
    int result = -1;
    while (identities && (entry = readdir(identities))) {
        if (entry->d_name[0] == '.') continue;
        char path[PATH_MAX + 512];
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        int fd = open(path, O_WRONLY | O_TRUNC | O_CLOEXEC);
        if (fd >= 0) {
            result = write(fd, "bad\n", 4) == 4 ? 0 : -1;
            close(fd);
        }
        break;
    }
    if (identities) closedir(identities);
    return result;
}

static int test_identity_cache(const char *source)
{
    char cache[PATH_MAX];
    char copy[PATH_MAX];
    snprintf(cache, sizeof(cache), "%s/latc-identity-cache-XXXXXX",
             getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
    if (!mkdtemp(cache) || setenv("LATX_AOT_V2_CACHE_DIR", cache, 1)) {
        return -1;
    }
    int fd = copy_file(source, copy);
    char relocated[PATH_MAX] = {0};
    int relocated_fd = -1;
    uint8_t before[32], cached[32], changed[32], relocated_digest[32];
    if (fd < 0 || inspect_once(fd, before) || count_identities(cache) != -1 ||
        publish_identity(cache, fd, before) || count_identities(cache) != 1 ||
        inspect_once(fd, cached) || memcmp(before, cached, 32) ||
        count_identities(cache) != 1 || corrupt_identity(cache) ||
        inspect_once(fd, cached) || memcmp(before, cached, 32)) {
        if (fd >= 0) close(fd);
        unlink(copy);
        return -1;
    }
    /* A copied executable has the same contents but a new identity cache key. */
    relocated_fd = copy_file(source, relocated);
    struct stat original_status, relocated_status;
    if (relocated_fd < 0 || fstat(fd, &original_status) ||
        fstat(relocated_fd, &relocated_status) ||
        relocated_status.st_ino == original_status.st_ino ||
        unsetenv("LATX_AOT_V2_LATCD_SOCKET") ||
        inspect_once(relocated_fd, relocated_digest) ||
        memcmp(before, relocated_digest, 32)) {
        if (relocated_fd >= 0) close(relocated_fd);
        if (fd >= 0) close(fd);
        unlink(relocated);
        unlink(copy);
        return -1;
    }
    close(relocated_fd);
    relocated_fd = -1;
    unlink(relocated);
    struct stat status;
    unsigned char byte;
    if (fstat(fd, &status) || status.st_size < 1 ||
        pread(fd, &byte, 1, status.st_size - 1) != 1) {
        close(fd);
        unlink(copy);
        return -1;
    }
    struct timespec times[2] = { status.st_atim, status.st_mtim };
    /* Give coarse filesystem timestamps a distinct mutation time. */
    struct timespec delay = { .tv_sec = 1 };
    while (nanosleep(&delay, &delay) && errno == EINTR) {
    }
    byte ^= 1;
    if (pwrite(fd, &byte, 1, status.st_size - 1) != 1 || fsync(fd) ||
        futimens(fd, times) || inspect_once(fd, changed) ||
        setenv("LATX_AOT_V2_LATCD_SOCKET", "unused-test-socket", 1) ||
        inspect_once(fd, changed) || unsetenv("LATX_AOT_V2_LATCD_SOCKET") ||
        !memcmp(before, changed, 32) || count_identities(cache) != 1 ||
        publish_identity(cache, fd, changed) || count_identities(cache) != 2) {
        close(fd);
        unlink(copy);
        return -1;
    }
    for (int child = 0; child < 8; child++) {
        pid_t pid = fork();
        if (!pid) {
            uint8_t concurrent[32];
            _exit(inspect_once(fd, concurrent) ||
                  memcmp(changed, concurrent, 32));
        }
        if (pid < 0) {
            close(fd);
            unlink(copy);
            return -1;
        }
    }
    int result = 0;
    for (int child = 0; child < 8; child++) {
        int wait_status;
        if (wait(&wait_status) < 0 || !WIFEXITED(wait_status) ||
            WEXITSTATUS(wait_status)) {
            result = -1;
        }
    }
    close(fd);
    unlink(copy);
    char directory[PATH_MAX + 32];
    snprintf(directory, sizeof(directory), "%s/.identities", cache);
    DIR *identities = opendir(directory);
    struct dirent *entry;
    while (identities && (entry = readdir(identities))) {
        if (entry->d_name[0] == '.') continue;
        char path[PATH_MAX + 512];
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        unlink(path);
    }
    if (identities) closedir(identities);
    rmdir(directory);
    rmdir(cache);
    unsetenv("LATX_AOT_V2_CACHE_DIR");
    return result;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: %s MAIN INTERPRETER DSO\n", argv[0]);
        return 2;
    }
    if (test_identity_cache(argv[1])) {
        fprintf(stderr, "AOT v2 identity cache validation failed\n");
        return 1;
    }
    LatGuestElfTrackerV2 *tracker = lat_guest_elf_tracker_new_v2();
    if (!tracker) {
        return 1;
    }
    static const uint64_t biases[] = {
        UINT64_C(0x555500000000),
        UINT64_C(0x7f1000000000),
        UINT64_C(0x7f2000000000),
    };
    char error[256] = {0};
    for (int i = 0; i < 3; i++) {
        int fd = open(argv[i + 1], O_RDONLY);
        uint64_t start, size, offset;
        const LatGuestElfInfoV2 *info;
        int added;
        if (fd < 0 || executable_mapping(fd, biases[i], &start, &size,
                                         &offset) ||
            lat_guest_elf_tracker_note_v2(tracker, fd, start, size, offset,
                                          4096, &info, &added,
                                          error, sizeof(error)) || !added ||
            info->load_bias != biases[i] ||
            info->preferred_base > UINT64_MAX - info->load_bias ||
            info->guest_begin != info->load_bias + info->preferred_base ||
            info->guest_end <= info->guest_begin ||
            !info->exec_range_count) {
            fprintf(stderr, "cannot track ELF %s: %s\n", argv[i + 1], error);
            if (fd >= 0) close(fd);
            lat_guest_elf_tracker_free_v2(tracker);
            return 1;
        }
        if (lat_guest_elf_tracker_note_v2(tracker, fd, start, size, offset,
                                          4096, &info, &added,
                                          error, sizeof(error)) || added) {
            fprintf(stderr, "duplicate ELF mapping was registered twice\n");
            close(fd);
            lat_guest_elf_tracker_free_v2(tracker);
            return 1;
        }
        close(fd);
    }
    if (lat_guest_elf_tracker_count_v2(tracker) != 3) {
        fprintf(stderr, "wrong tracked ELF count\n");
        lat_guest_elf_tracker_free_v2(tracker);
        return 1;
    }
    const LatGuestElfInfoV2 *removed =
        lat_guest_elf_tracker_get_v2(tracker, 1);
    uint64_t removed_begin = removed->guest_begin;
    uint64_t removed_size = removed->guest_end - removed->guest_begin;
    uint64_t nonexec = removed->guest_begin;
    for (uint16_t i = 0; i < removed->exec_range_count; i++) {
        if (nonexec >= removed->exec_ranges[i].begin &&
            nonexec < removed->exec_ranges[i].end) {
            nonexec = removed->exec_ranges[i].end;
        }
    }
    if (nonexec < removed->guest_end &&
        lat_guest_elf_tracker_remove_range_v2(tracker, nonexec, 1) != 0) {
        fprintf(stderr, "non-executable range removed tracked ELF\n");
        lat_guest_elf_tracker_free_v2(tracker);
        return 1;
    }
    if (lat_guest_elf_tracker_remove_range_v2(
            tracker, removed_begin, removed_size) != 1 ||
        lat_guest_elf_tracker_count_v2(tracker) != 2 ||
        lat_guest_elf_tracker_remove_range_v2(
            tracker, removed_begin, removed_size) != 0) {
        fprintf(stderr, "tracked ELF range removal failed\n");
        lat_guest_elf_tracker_free_v2(tracker);
        return 1;
    }
    lat_guest_elf_tracker_free_v2(tracker);
    puts("test-aot-v2-guest-elf-map: PASS modules=3 duplicates=0 removed=1 identities=2 concurrent=8");
    return 0;
}
