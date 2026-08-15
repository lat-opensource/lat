/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_WRAPPEDFONT_VAARGS_H
#define LATX_WRAPPEDFONT_VAARGS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "myalign.h"

typedef struct LatxX64VaReader {
    CPUX86State *cpu;
    uint32_t gp_offset;
    uint32_t fp_offset;
    uint8_t *overflow_arg_area;
    uint8_t *reg_save_area;
    bool live_registers;
} LatxX64VaReader;

static inline void latx_x64_va_reader_live(LatxX64VaReader *reader,
                                            unsigned fixed_gp)
{
    reader->cpu = (CPUX86State *)lsenv->cpu_state;
    reader->gp_offset = fixed_gp * 8;
    reader->fp_offset = X64_VA_MAX_REG;
    reader->overflow_arg_area =
        (uint8_t *)(reader->cpu->regs[R_ESP] + 8);
    reader->reg_save_area = NULL;
    reader->live_registers = true;
}

static inline void latx_x64_va_reader_list(LatxX64VaReader *reader,
                                            x64_va_list_t list)
{
    reader->cpu = (CPUX86State *)lsenv->cpu_state;
    reader->gp_offset = list->gp_offset;
    reader->fp_offset = list->fp_offset;
    reader->overflow_arg_area = list->overflow_arg_area;
    reader->reg_save_area = list->reg_save_area;
    reader->live_registers = false;
}

static inline uint64_t latx_x64_va_gp(LatxX64VaReader *reader)
{
    static const int registers[] = {
        R_EDI, R_ESI, R_EDX, R_ECX, R_R8, R_R9,
    };
    uint64_t value;

    if (reader->gp_offset < X64_VA_MAX_REG) {
        unsigned index = reader->gp_offset / 8;

        if (reader->live_registers) {
            value = reader->cpu->regs[registers[index]];
        } else {
            memcpy(&value, reader->reg_save_area + reader->gp_offset,
                   sizeof(value));
        }
        reader->gp_offset += 8;
        return value;
    }
    memcpy(&value, reader->overflow_arg_area, sizeof(value));
    reader->overflow_arg_area += 8;
    return value;
}

static inline double latx_x64_va_double(LatxX64VaReader *reader)
{
    double value;

    if (reader->fp_offset < X64_VA_MAX_XMM) {
        unsigned index = (reader->fp_offset - X64_VA_MAX_REG) / 16;

        if (reader->live_registers) {
            value = *(double *)&reader->cpu->xmm_regs[index].ZMM_Q(0);
        } else {
            memcpy(&value, reader->reg_save_area + reader->fp_offset,
                   sizeof(value));
        }
        reader->fp_offset += 16;
        return value;
    }
    memcpy(&value, reader->overflow_arg_area, sizeof(value));
    reader->overflow_arg_area += 8;
    return value;
}

void *latx_fontconfig_pattern_build(LatxX64VaReader *reader,
                                    void *pattern);
void *latx_fontconfig_object_set_build(LatxX64VaReader *reader,
                                       const char *first);

#endif /* LATX_WRAPPEDFONT_VAARGS_H */
