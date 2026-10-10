/* Helpers for Steam Runtime paths. */

#ifndef QEMU_STEAM_H
#define QEMU_STEAM_H

#include <stdbool.h>

#include "qemu/envlist.h"

/*
 * Discover a Runtime candidate. Generic Runtime handling is enabled only
 * when expected_files matches the candidate's canonical files path.
 */
bool latx_steam_runtime_configure(const envlist_t *envlist,
                                  const char *expected_files);
/* Candidate accessors used by the Steam launch helper. */
const char *latx_steam_runtime_files(void);
/* Generic callers only observe this once the Runtime is active. */
const char *latx_steam_runtime_library_path(void);
char *latx_steam_runtime_make_library_path(const char *suffix);
bool latx_steam_runtime_is_library_path(const char *path);
bool latx_steam_runtime_is_i386_library_path(const char *path);
bool latx_steam_runtime_is_wrapper(const char *program);
char *latx_steam_runtime_resolve_path(const char *name);
void latx_steam_context_configure(const envlist_t *envlist, bool in_runtime);
bool latx_steam_context_active(void);

#endif /* QEMU_STEAM_H */
