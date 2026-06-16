/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/mman.h>

#include <wrappedlibs.h>
#include "bridge.h"
#include "bridge_private.h"
#include "khash.h"
#include "debug.h"
#include "box64context.h"
#include "elfloader.h"

KHASH_MAP_INIT_INT64(bridgemap, uintptr_t)

//onebridge is 32 bytes
#define NBRICK  4096/sizeof(onebridge_t)
typedef struct brick_s brick_t;
typedef struct brick_s {
    onebridge_t *b;
    int         sz;
    brick_t     *next;
} brick_t;

typedef struct bridge_s {
    brick_t         *head;
    brick_t         *last;      // to speed up
    kh_bridgemap_t  *bridgemap;
} bridge_t;

//from wrapped/wrappedlibc.c
//void* my_mmap(x64emu_t* emu, void* addr, unsigned long length, int prot, int flags, int fd, int64_t offset);
//int my_munmap(x64emu_t* emu, void* addr, unsigned long length);

brick_t* NewBrick(void)
{
    brick_t* ret = (brick_t*)box_calloc(1, sizeof(brick_t));
    // ptr needed to be fixed
    void* ptr = mmap64(NULL, NBRICK * sizeof(onebridge_t), PROT_READ | PROT_WRITE, MAP_PRIVATE | 0x40 | MAP_ANONYMOUS, -1, 0); // 0x40 is MAP_32BIT
    if(ptr == MAP_FAILED) {
        printf("Warning, cannot allocate 0x%lx aligned bytes for bridge, will probably crash later\n", NBRICK*sizeof(onebridge_t));
    }
    ret->b = ptr;
    return ret;
}

bridge_t *NewBridge(void)
{
    bridge_t *b = (bridge_t*)box_calloc(1, sizeof(bridge_t));
    b->head = NewBrick();
    b->last = b->head;
    b->bridgemap = kh_init(bridgemap);

    return b;
}
void FreeBridge(bridge_t** bridge)
{
    if(!bridge || !*bridge)
        return;
    brick_t *b = (*bridge)->head;
    while(b) {
        brick_t *n = b->next;
        munmap(b->b, NBRICK*sizeof(onebridge_t));
        box_free(b);
        b = n;
    }
    kh_destroy(bridgemap, (*bridge)->bridgemap);
    box_free(*bridge);
    *bridge = NULL;
}

uintptr_t AddBridge(bridge_t* bridge, wrapper_t w, void* fnc, int N, const char* name)
{
    brick_t *b = NULL;
    int sz = -1;
        b = bridge->last;
        if(b->sz == NBRICK) {
            b->next = NewBrick();
            b = b->next;
            bridge->last = b;
        }
   	sz = b->sz;
    b->sz++;
    b->b[sz].CC = 0xCC;
    b->b[sz].S = 'S'; b->b[sz].C='C';
    b->b[sz].w = w;
    b->b[sz].f = (uintptr_t)fnc;
    b->b[sz].C3 = N?0xC2:0xC3;
    b->b[sz].N = N;
    // add bridge to map, for fast recovery
    int ret;
    khint_t k = kh_put(bridgemap, bridge->bridgemap, (uintptr_t)fnc, &ret);
    kh_value(bridge->bridgemap, k) = (uintptr_t)&b->b[sz].CC;

    return (uintptr_t)&b->b[sz].CC;
}

uintptr_t CheckBridged(bridge_t* bridge, void* fnc)
{
    // check if function alread have a bridge (the function wrapper will not be tested)
    khint_t k = kh_get(bridgemap, bridge->bridgemap, (uintptr_t)fnc);
    if(k==kh_end(bridge->bridgemap))
        return 0;
    return kh_value(bridge->bridgemap, k);
}

uintptr_t AddCheckBridge(bridge_t* bridge, wrapper_t w, void* fnc, int N, const char* name)
{
    if(!fnc && w)
        return 0;
    uintptr_t ret = CheckBridged(bridge, fnc);
    if(!ret)
        ret = AddBridge(bridge, w, fnc, N, name);
    return ret;
}

uintptr_t AddAutomaticBridge(bridge_t* bridge, wrapper_t w, void* fnc, int N)
{
    if(!fnc)
        return 0;
    uintptr_t ret = CheckBridged(bridge, fnc);
    if(!ret)
        ret = AddBridge(bridge, w, fnc, N, NULL);
    if(!hasAlternate(fnc)) {
        printf_log(LOG_DEBUG, "Adding AutomaticBridge for %p to %p\n", fnc, (void*)ret);
        addAlternate(fnc, (void*)ret);
    }
    return ret;
}

void* GetNativeFnc(uintptr_t fnc)
{
    if(!fnc) return NULL;
    // check if function exist in some loaded lib
    if(!FindElfAddress(my_context, fnc)) {
        Dl_info info;
        if(dladdr((void*)fnc, &info))
            return (void*)fnc;
    }
    // check if it's an indirect jump
    #define PK(a)       *(uint8_t*)(fnc+a)
    #define PK32(a)     *(uint32_t*)(fnc+a)
    if(PK(0)==0xff && PK(1)==0x25) {    // "absolute" jump, maybe the GOT (it's a RIP+relative in fact)
        uintptr_t a1 = fnc+6+(PK32(2)); // need to add a check to see if the address is from the GOT !
        a1 = *(uintptr_t*)a1;
        if(a1 && a1>0x10000) {
            a1 = (uintptr_t)GetNativeFnc(a1);
            if(a1)
                return (void*)a1;
        }
    }
    #undef PK
    #undef PK32
    // check if bridge exist
    onebridge_t *b = (onebridge_t*)fnc;
    if(b->CC != 0xCC || b->S!='S' || b->C!='C' || (b->C3!=0xC3 && b->C3!=0xC2))
        return NULL;    // not a bridge?!
    return (void*)b->f;
}

void* GetNativeFncOrFnc(uintptr_t fnc)
{
    onebridge_t *b = (onebridge_t*)fnc;
    if(b->CC != 0xCC || b->S!='S' || b->C!='C' || (b->C3!=0xC3 && b->C3!=0xC2))
        return (void*)fnc;    // not a bridge?!
    return (void*)b->f;
}


// Alternate address handling
KHASH_MAP_INIT_INT64(alternate, void*)
static kh_alternate_t *my_alternates = NULL;

int hasAlternate(void* addr) {
    if(!my_alternates)
        return 0;
    khint_t k = kh_get(alternate, my_alternates, (uintptr_t)addr);
    if(k==kh_end(my_alternates))
        return 0;
    return 1;
}

void* getAlternate(void* addr) {
    if(!my_alternates)
        return addr;
    khint_t k = kh_get(alternate, my_alternates, (uintptr_t)addr);
    if(k!=kh_end(my_alternates))
        return kh_value(my_alternates, k);
    return addr;
}
void addAlternate(void* addr, void* alt) {
    if(!my_alternates) {
        my_alternates = kh_init(alternate);
    }
    int ret;
    khint_t k = kh_put(alternate, my_alternates, (uintptr_t)addr, &ret);
    if(!ret)    // already there
        return;
    kh_value(my_alternates, k) = alt;
}

void cleanAlternate(void) {
    if(my_alternates) {
        kh_destroy(alternate, my_alternates);
        my_alternates = NULL;
    }
}

void init_bridge_helper(void)
{
}

void fini_bridge_helper(void)
{
    cleanAlternate();
}
