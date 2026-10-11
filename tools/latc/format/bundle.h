#ifndef LATC_BUNDLE_H
#define LATC_BUNDLE_H

#include "cfg_program.h"

#include <stddef.h>
#include <stdint.h>

#define LATC_EXECUTION_MODEL "lat-aot-bundle"

typedef struct LatcBundleInfo {
    uint64_t runner_size;
    uint64_t guest_offset;
    uint64_t guest_size;
    uint64_t cfg_offset;
    uint64_t cfg_size;
    uint64_t aot_offset;
    uint64_t aot_size;
    uint64_t function_count;
    uint64_t tb_count;
    uint64_t edge_count;
    uint64_t selected_tb_count;
    char guest_sha256[65];
    char aot_sha256[65];
    char aot_name[161];
} LatcBundleInfo;

int latc_bundle_write(const char *runner_path, const char *guest_path,
                      const char *output_path, const char *aot_path,
                      const CfgProgram *program, const uint8_t guest_digest[32],
                      char *error, size_t error_size);
int latc_bundle_inspect(const char *path, LatcBundleInfo *info,
                        int verify_guest, char *error, size_t error_size);

#endif
