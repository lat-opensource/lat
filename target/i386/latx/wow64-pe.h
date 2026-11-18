/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef LATX_WOW64_PE_H
#define LATX_WOW64_PE_H

#include "latx-wow64.h"

typedef struct LatWow64PeFaultContext {
    const uint64_t *gpr;
    const uint64_t (*vector)[4];
    uint32_t fcsr, eflags, ftop;
} LatWow64PeFaultContext;

bool lat_wow64_pe_init(void);
void lat_wow64_pe_fault(LatWow64PeFaultContext *context, uintptr_t pc,
                        uintptr_t address, unsigned int access,
                        uint32_t float_status);
void lat_wow64_pe_memory_event(const LatWow64MemoryEvent *event);
void lat_wow64_pe_process_term(bool post, bool success);
void lat_wow64_pe_thread_exit(void);
bool lat_wow64_pe_pause_thread(void *handle, void **paused, void **control);
void lat_wow64_pe_finish_thread(void *paused, bool terminated);
bool lat_wow64_pe_execute(LatWow64Context *context, LatWow64Exit *exit_info);
void lat_wow64_pe_end_execute(void);
void lat_wow64_pe_fetch_code(void *env, uint32_t pc, uint8_t code[15]);

#endif
