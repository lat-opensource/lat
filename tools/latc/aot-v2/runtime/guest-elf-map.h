#ifndef LAT_AOT_V2_GUEST_ELF_MAP_H
#define LAT_AOT_V2_GUEST_ELF_MAP_H

#include <stddef.h>
#include <stdint.h>

#define LAT_GUEST_ELF_MAX_EXEC_RANGES 16

typedef struct LatGuestElfRangeV2 {
    uint64_t begin;
    uint64_t end;
} LatGuestElfRangeV2;

typedef struct LatGuestElfInfoV2 {
    uint64_t device;
    uint64_t inode;
    uint8_t source_sha256[32];
    uint16_t elf_type;
    uint16_t exec_range_count;
    uint64_t preferred_base;
    uint64_t load_bias;
    uint64_t guest_begin;
    uint64_t guest_end;
    LatGuestElfRangeV2 exec_ranges[LAT_GUEST_ELF_MAX_EXEC_RANGES];
} LatGuestElfInfoV2;

typedef struct LatGuestElfTrackerV2 LatGuestElfTrackerV2;

int lat_guest_elf_inspect_mapping_v2(int fd, uint64_t guest_start,
                                     uint64_t mapping_size,
                                     uint64_t file_offset,
                                     uint64_t page_size,
                                     LatGuestElfInfoV2 *info,
                                     char *error, size_t error_size);

LatGuestElfTrackerV2 *lat_guest_elf_tracker_new_v2(void);
void lat_guest_elf_set_cache_directory_v2(const char *directory);
void lat_guest_elf_tracker_free_v2(LatGuestElfTrackerV2 *tracker);
int lat_guest_elf_tracker_note_v2(LatGuestElfTrackerV2 *tracker, int fd,
                                  uint64_t guest_start,
                                  uint64_t mapping_size,
                                  uint64_t file_offset,
                                  uint64_t page_size,
                                  const LatGuestElfInfoV2 **info,
                                  int *added, char *error,
                                  size_t error_size);
size_t lat_guest_elf_tracker_count_v2(const LatGuestElfTrackerV2 *tracker);
const LatGuestElfInfoV2 *lat_guest_elf_tracker_get_v2(
    const LatGuestElfTrackerV2 *tracker, size_t index);
size_t lat_guest_elf_tracker_remove_range_v2(LatGuestElfTrackerV2 *tracker,
                                              uint64_t guest_start,
                                              uint64_t mapping_size);

#endif
