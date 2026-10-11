#ifndef LAT_AOT_V2_TEST_ELF_FIXTURE_H
#define LAT_AOT_V2_TEST_ELF_FIXTURE_H

#include "lat-aot-v2.h"

#include <stddef.h>

enum {
    LAT_AOT_TEST_FILE_SIZE = 2048,
    LAT_AOT_TEST_NOTE_OFFSET = 0x200,
    LAT_AOT_TEST_DYNAMIC_OFFSET = 0x300,
    LAT_AOT_TEST_STRING_OFFSET = 0x380,
    LAT_AOT_TEST_SYMBOL_OFFSET = 0x400,
    LAT_AOT_TEST_TEXT_OFFSET = 0x480,
    LAT_AOT_TEST_SECTION_OFFSET = 0x500,
    LAT_AOT_TEST_RELOCATION_OFFSET = 0x700,
};

void lat_aot_test_build_elf(unsigned char file[LAT_AOT_TEST_FILE_SIZE]);
void lat_aot_test_expected(LatAotExpectedV2 *expected);

#endif
