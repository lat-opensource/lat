#ifndef __CALLBACK_H__
#define __CALLBACK_H__

#include <stdint.h>

uint64_t RunFunctionWithState(uintptr_t fnc, int nargs, ...);
uint64_t RunFunctionFmt(uintptr_t fnc, const char *fmt, ...);
#define RunFunction RunFunctionWithState

#endif //__CALLBACK_H__
