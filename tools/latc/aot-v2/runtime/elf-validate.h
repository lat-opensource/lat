#ifndef LAT_AOT_V2_ELF_VALIDATE_H
#define LAT_AOT_V2_ELF_VALIDATE_H

#include "lat-aot-v2.h"

#include <stddef.h>

int lat_aot_v2_elf_validate_memory(const void *data, size_t size,
                                   const LatAotExpectedV2 *expected,
                                   LatAotNoteV2 *note, char *error,
                                   size_t error_size);

int lat_aot_v2_elf_validate_fd(int fd, const LatAotExpectedV2 *expected,
                               LatAotNoteV2 *note, char *error,
                               size_t error_size);

#endif
