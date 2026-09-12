/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef LATX_LIBLAT_H
#define LATX_LIBLAT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* V1 is a single, process-lifetime runtime; see docs/devel/liblat.md. */
typedef const char *(*LatHostSymbolQuery)(uintptr_t entry, void *callback_stub,
                                        void *variadic_adapter,
                                        char *variadic_signature);
typedef void (*LatHostSymbolOffsetQuery)(const char *name, uintptr_t *offset);
typedef uint64_t (*LatHostCallback)(const char *signature, uintptr_t entry,
                                  long *gpr, long *xmm, char *stack,
                                  long *rax, long *rdx, long *xmm0,
                                  double *st0, uintptr_t variadic_adapter);

/* argv is {program_name, bootstrap_elf_path}. Negative return means failure.
 * This shared-runtime profile requires host_dispatch_signal == false.
 */
int lat_init(bool host_dispatch_signal, int argc, char **argv,
             LatHostSymbolQuery query, LatHostSymbolOffsetQuery offsets);
/* Call only after quiescing callers; drains registered Guest exit callbacks. */
void lat_end(void);

/* Legacy pointer-shaped loader ABI, retained for existing NBL clients. */
void *lat_dlopen(void *filename, int flags);
void *lat_dlsym(void *handle, void *symbol);
int lat_dlinfo(void *handle, int request, void *info);
int lat_dlclose(void *handle);
char *lat_dlerror(void);
void lat_dlrun_method(uintptr_t entry, const long *gpr_args, int gpr_count,
                     const long *xmm_args, int xmm_count,
                     const long *stack_args, int stack_count,
                     long *rax, long *rdx, long *xmm0, long *xmm1,
                     unsigned __int128 *st0);
bool is_lat_symbol(const void *address);
bool is_lat_method(const void *address);
bool is_lat_signal(const void *pc);

#ifdef __cplusplus
}
#endif
#endif
