#ifndef __BRIDGE_H_
#define __BRIDGE_H_
#include <stdint.h>
#include "lsenv.h"

typedef struct bridge_s bridge_t;
typedef struct box64context_s box64context_t;
typedef void (*wrapper_t)( uintptr_t fnc);
typedef struct brick_s brick_t;
brick_t* NewBrick(void);
bridge_t *NewBridge(void);
void FreeBridge(bridge_t** bridge);

uintptr_t AddBridge(bridge_t* bridge, wrapper_t w, void* fnc, int N, const char* name);
uintptr_t CheckBridged(bridge_t* bridge, void* fnc);
uintptr_t AddCheckBridge(bridge_t* bridge, wrapper_t w, void* fnc, int N, const char* name);
uintptr_t AddAutomaticBridge(bridge_t* bridge, wrapper_t w, void* fnc, int N);
bool kzt_is_registered_onebridge(uintptr_t addr);
bool kzt_registered_onebridge_snapshot(uintptr_t addr, wrapper_t *wrapper,
                                       uintptr_t *function);
void* GetNativeFnc(uintptr_t fnc);
void* GetNativeFncOrFnc(uintptr_t fnc);

int hasAlternate(void* addr);
void* getAlternate(void* addr);
void addAlternate(void* addr, void* alt);
void cleanAlternate(void);

void init_bridge_helper(void);
void fini_bridge_helper(void);

#endif //__BRIDGE_H_
