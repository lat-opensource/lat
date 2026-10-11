#ifndef LATC_NATIVE_EXPORT_H
#define LATC_NATIVE_EXPORT_H

#include "aot.h"

#include <stdint.h>

int latc_native_export(const char *path, const char *guest_path,
        const aot_header *header, const aot_segment *segments,
        const aot_tb *tbs, uintptr_t tb_table_end,
        const void *code, uint64_t code_size, uint64_t aot_code_offset);

#endif
