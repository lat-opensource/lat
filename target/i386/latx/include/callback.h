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


#endif //__CALLBACK_H__
