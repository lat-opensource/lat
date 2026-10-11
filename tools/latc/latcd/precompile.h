#ifndef LATCD_PRECOMPILE_H
#define LATCD_PRECOMPILE_H

#include <glib.h>
#include <stddef.h>
#include <stdint.h>

typedef struct LatcdPrecompileDependency {
    char *guest_path;
    char *host_path;
    char source_sha256[65];
    char *tbset_path;
    uint64_t static_tb_keys;
    int analysis_status;
    char *analysis_error;
} LatcdPrecompileDependency;

int latcd_precompile_collect(const char *rootfs, const char *guest_path,
                             GPtrArray **dependencies,
                             char *error, size_t error_size);
void latcd_precompile_dependency_free(gpointer pointer);

int latcd_run_precompile(const char *socket_path, const char *rootfs,
                         const char *compiler, const char *guest_path,
                         uint32_t jobs);

#endif
