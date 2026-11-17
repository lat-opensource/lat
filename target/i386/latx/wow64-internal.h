/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef LATX_WOW64_INTERNAL_H
#define LATX_WOW64_INTERNAL_H
#include "latx-wow64.h"

typedef struct LatWow64BackendOps {
    uint32_t size;
    uint32_t abi_version;
    LatWow64Status (*execute_guest)(void *, LatWow64Thread *, LatWow64Context *, LatWow64Exit *);
    void (*invalidate_code)(void *, uint64_t, uint64_t, uint64_t);
    void (*memory_event)(void *, const LatWow64MemoryEvent *, uint64_t, int);
    void (*thread_destroy)(void *, LatWow64Thread *);
    LatWow64Status (*thread_create)(void *, LatWow64Thread *);
} LatWow64BackendOps;

LatWow64Status lat_wow64_process_create(const LatWow64BackendOps *, void *, const LatWow64ProcessConfig *, LatWow64Process **);
LatWow64Status lat_wow64_process_destroy(LatWow64Process *);
LatWow64Status lat_wow64_thread_create(LatWow64Process *, const LatWow64Context *, LatWow64Thread **);
LatWow64Status lat_wow64_thread_destroy(LatWow64Thread *);
uint64_t lat_wow64_thread_serial(const LatWow64Thread *);
LatWow64Status lat_wow64_get_context(LatWow64Thread *, LatWow64Context *);
LatWow64Status lat_wow64_set_context(LatWow64Thread *, const LatWow64Context *);
LatWow64Status lat_wow64_thread_get_info(LatWow64Thread *, LatWow64ThreadInfo *);
LatWow64Status lat_wow64_thread_suspend(LatWow64Thread *);
LatWow64Status lat_wow64_thread_resume(LatWow64Thread *);
LatWow64Status lat_wow64_enter(LatWow64Thread *, uint64_t, LatWow64Exit *);
LatWow64Status lat_wow64_reply(LatWow64Thread *, const LatWow64Reply *);
LatWow64Status lat_wow64_memory_event(LatWow64Process *, const LatWow64MemoryEvent *);
uint64_t lat_wow64_mapping_generation(LatWow64Process *);
#endif
