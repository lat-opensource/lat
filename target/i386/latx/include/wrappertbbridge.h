#ifndef WRAPPERTBBRIDGE_H
#define WRAPPERTBBRIDGE_H
#include "qemu/osdep.h"
#include "exec/cpu-defs.h"
#include "error.h"
#include "latx-types.h"

struct kzt_tbbridge {
    target_ulong pc;
    ADDR func;
    void* wrapper;
};

void* kzt_tbbridge_init(void);
struct kzt_tbbridge* kzt_tbbridge_lookup(target_ulong pc);
bool kzt_tbbridge_contains(target_ulong pc);
int kzt_tbbridge_insert(target_ulong pc, ADDR func, void * wrapper);
#endif
