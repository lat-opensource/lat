#ifndef __CALLBACK_H__
#define __CALLBACK_H__

#include <stdint.h>

uint64_t RunFunctionWithState(uintptr_t fnc, int nargs, ...);
uint64_t RunFunctionWithStateInternal(uintptr_t fnc, int nargs, ...);
uint64_t RunFunctionWithStateInternalNoRefresh(uintptr_t fnc, int nargs,
                                               ...);
uint64_t RunFunctionFmt(uintptr_t fnc, const char *fmt, ...);
float RunFunctionFmtFloat(uintptr_t fnc, const char *fmt, ...);
#define RunFunction RunFunctionWithState

int latx_run_guest_callback(uintptr_t entry, const long *gpr_args,
                            int gpr_count, const long *xmm_args,
                            int xmm_count, const long *stack_args,
                            int stack_count, long *rax, long *rdx,
                            long *xmm0, long *xmm1,
                            unsigned __int128 *st0);

int latx_run_guest_callback_with_libc(
    uintptr_t entry, const long *gpr_args, int gpr_count,
    const long *xmm_args, int xmm_count, const long *stack_args,
    int stack_count, long *rax, long *rdx, long *xmm0, long *xmm1,
    unsigned __int128 *st0);

#ifdef CONFIG_LIBLAT
#include "latx/liblat.h"
typedef uint64_t (*latx_native_callback_stub_t)(
    const char *signature, uintptr_t entry, long *gpr_args,
    long *xmm_args, char *stack_args, long *rax, long *rdx,
    long *xmm0, double *st0, uintptr_t variadic_adapter);

void call_loongarch64_fun(const char *signature, uintptr_t entry,
                          latx_native_callback_stub_t callback_stub,
                          uintptr_t variadic_adapter,
                          const char *variadic_signature);
uintptr_t get_next_pc(void);
#endif

#endif //__CALLBACK_H__
