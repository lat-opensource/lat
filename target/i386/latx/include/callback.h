#ifndef __CALLBACK_H__
#define __CALLBACK_H__

#include <stdint.h>

uint64_t RunFunctionWithState(uintptr_t fnc, int nargs, ...);
uint64_t RunFunctionFmt(uintptr_t fnc, const char *fmt, ...);
float RunFunctionFmtFloat(uintptr_t fnc, const char *fmt, ...);
uint64_t RunFunctionWithStateInternal(uintptr_t fnc, int nargs, ...);
uint64_t RunFunctionWithStateInternalNoRefresh(uintptr_t fnc, int nargs, ...);
#define RunFunction RunFunctionWithState

#endif //__CALLBACK_H__
