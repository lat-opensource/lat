/* Helpers for Steam pressure-vessel Runtime paths. */

#ifndef QEMU_PRESSURE_VESSEL_H
#define QEMU_PRESSURE_VESSEL_H

#include <stdbool.h>

#include "qemu/envlist.h"

void latx_pressure_vessel_runtime_configure(const envlist_t *envlist);
bool latx_pressure_vessel_runtime_is_active(void);
const char *latx_pressure_vessel_runtime_files(void);
const char *latx_pressure_vessel_runtime_library_path(void);
char *latx_pressure_vessel_runtime_make_library_path(const char *suffix);
bool latx_pressure_vessel_runtime_is_library_path(const char *path);
bool latx_pressure_vessel_runtime_is_i386_library_path(const char *path);
bool latx_pressure_vessel_runtime_is_wrapper(const char *program);
char *latx_pressure_vessel_runtime_resolve_path(const char *name);

#endif /* QEMU_PRESSURE_VESSEL_H */
