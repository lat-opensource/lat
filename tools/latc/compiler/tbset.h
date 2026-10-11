#ifndef LATC_TBSET_H
#define LATC_TBSET_H

#include "cfg_program.h"

#include <stddef.h>

int latc_tbset_apply(const char *path, const char *source_path,
                     CfgProgram *program,
                     bool ignore_outside_exec, size_t *matched,
                     size_t *unmatched, size_t *ignored, uint8_t digest[32],
                     char *error, size_t error_size);

int latc_tbset_write_static(const char *path, const char *source_path,
                            const CfgProgram *program,
                            size_t *written, char *error, size_t error_size);

#endif
