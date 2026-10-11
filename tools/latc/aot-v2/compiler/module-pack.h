#ifndef LAT_AOT_V2_MODULE_PACK_H
#define LAT_AOT_V2_MODULE_PACK_H

#include <stddef.h>
#include <stdint.h>

#include "cfg_program.h"

int lat_aot_v2_emit_module_sources(const char *native_image,
                                   const char *output_directory,
                                   char *error, size_t error_size);
int lat_aot_v2_emit_module_sources_with_cfg(
    const char *native_image, const char *output_directory,
    const CfgProgram *program, const uint8_t guest_sha256[32],
    char *error, size_t error_size);
void lat_aot_v2_codegen_digest(const char *build_id, uint8_t digest[32]);

#endif
