#include "elf-fixture.h"
#include "elf-validate.h"

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum { MUTATION_COUNT = 100000 };

static uint64_t random_state = UINT64_C(0x4c4154414f543256);

static uint64_t next_random(void)
{
    uint64_t value = random_state;
    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    random_state = value;
    return value;
}

static int validate(const void *data, size_t size,
                    const LatAotExpectedV2 *expected)
{
    char error[256];
    LatAotNoteV2 note;
    int result = lat_aot_v2_elf_validate_memory(data, size, expected, &note,
                                                error, sizeof(error));
    if (result != 0 && result != -1) {
        fprintf(stderr, "validator returned unexpected result %d\n", result);
        return -2;
    }
    return result;
}

static int write_seed(const char *path,
                      const unsigned char data[LAT_AOT_TEST_FILE_SIZE])
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        fprintf(stderr, "cannot create seed %s: %s\n", path, strerror(errno));
        return -1;
    }
    size_t written = 0;
    while (written < LAT_AOT_TEST_FILE_SIZE) {
        ssize_t count = write(fd, data + written,
                              LAT_AOT_TEST_FILE_SIZE - written);
        if (count <= 0) {
            fprintf(stderr, "cannot write seed %s: %s\n", path,
                    strerror(errno));
            close(fd);
            return -1;
        }
        written += count;
    }
    return close(fd);
}

int main(int argc, char **argv)
{
    _Alignas(Elf64_Ehdr) unsigned char valid[LAT_AOT_TEST_FILE_SIZE];
    _Alignas(Elf64_Ehdr) unsigned char mutated[LAT_AOT_TEST_FILE_SIZE + 8];
    LatAotExpectedV2 expected;
    unsigned accepted = 0;

    lat_aot_test_build_elf(valid);
    lat_aot_test_expected(&expected);
    if (argc == 3 && !strcmp(argv[1], "--write-seed")) {
        return write_seed(argv[2], valid) != 0;
    }
    if (argc != 1) {
        fprintf(stderr, "usage: %s [--write-seed PATH]\n", argv[0]);
        return 2;
    }
    if (validate(valid, sizeof(valid), &expected)) {
        fputs("valid in-memory fixture was rejected\n", stderr);
        return 1;
    }
    for (size_t size = 0; size < sizeof(valid); size++) {
        if (!validate(valid, size, &expected)) {
            fprintf(stderr, "truncated fixture accepted at size %zu\n", size);
            return 1;
        }
    }
    memcpy(mutated + 1, valid, sizeof(valid));
    if (!validate(mutated + 1, sizeof(valid), &expected)) {
        fputs("unaligned fixture was accepted\n", stderr);
        return 1;
    }

    static const uint64_t boundaries[] = {
        0, 1, 3, 4, 7, 8, UINT32_MAX, UINT64_MAX - 3, UINT64_MAX,
    };
    for (unsigned iteration = 0; iteration < MUTATION_COUNT; iteration++) {
        memcpy(mutated, valid, sizeof(valid));
        unsigned changes = 1 + next_random() % 8;
        for (unsigned change = 0; change < changes; change++) {
            size_t offset = next_random() % sizeof(valid);
            unsigned kind = next_random() % 3;
            if (kind == 0) {
                mutated[offset] ^= (unsigned char)(1u << (next_random() & 7));
            } else if (kind == 1) {
                mutated[offset] = (unsigned char)next_random();
            } else {
                uint64_t value = boundaries[next_random() %
                    (sizeof(boundaries) / sizeof(boundaries[0]))];
                size_t width = sizeof(value);
                if (width > sizeof(valid) - offset) {
                    width = sizeof(valid) - offset;
                }
                memcpy(mutated + offset, &value, width);
            }
        }
        size_t size = next_random() % 5 == 0 ?
                      next_random() % (sizeof(valid) + 1) : sizeof(valid);
        int result = validate(mutated, size, &expected);
        if (result == -2) {
            return 1;
        }
        accepted += result == 0;
    }
    printf("test-aot-v2-elf-mutate: PASS mutations=%u accepted=%u seed=%#llx\n",
           MUTATION_COUNT, accepted,
           (unsigned long long)UINT64_C(0x4c4154414f543256));
    return 0;
}
