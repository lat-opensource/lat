#ifndef LATC_AOT_V2_MODULE_INSPECT_H
#define LATC_AOT_V2_MODULE_INSPECT_H

#include "lat-aot-v2.h"
#include "lat-tb-key-set.h"

#include <stddef.h>
#include <stdint.h>

typedef struct LatAotModuleInfoV2 {
    LatAotNoteV2 note;
    uint64_t text_size;
    uint64_t tb_count;
    uint64_t pc_map_count;
    uint64_t guest_slot_records;
} LatAotModuleInfoV2;

int lat_aot_v2_module_inspect_file(const char *path,
                                   LatAotModuleInfoV2 *info,
                                   char *error, size_t error_size);

int lat_aot_v2_module_validate_tbset_file(const char *path,
                                          const char *tbset_path,
                                          char *error,
                                          size_t error_size);
int lat_aot_v2_module_inspect_and_validate_tbset_file(
    const char *path, const char *tbset_path, LatAotModuleInfoV2 *info,
    char *error, size_t error_size);
int lat_aot_v2_module_missing_tbset_file(
    const char *path, const char *tbset_path, LatAotModuleInfoV2 *info,
    LatTbKeySet *missing, char *error, size_t error_size);

#endif
