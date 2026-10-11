#include "elf-validate.h"

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char error[64];
    LatAotNoteV2 note;

    lat_aot_v2_elf_validate_memory(data, size, NULL, &note,
                                   error, sizeof(error));
    return 0;
}
