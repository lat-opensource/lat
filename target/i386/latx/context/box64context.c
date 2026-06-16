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

#include "box64context.h"
#include "debug.h"
#include "elfloader.h"
#include "bridge.h"
#include "librarian.h"
#include "library.h"
#include "wrapper.h"
#include <pthread.h>

box64context_t *NewBox64Context(int argc)
{
    // init and put default values
    box64context_t *context = (box64context_t*)box_calloc(1, sizeof(box64context_t));

    context->deferedInit = 1;
    context->sel_serial = 1;

    context->maplib = NewLibrarian(context, 1);
    context->local_maplib = NewLibrarian(context, 1);
    context->versym = NewDictionnary();
    context->system = NewBridge();
    context->dlprivate = NewDLPrivate();
    context->box64lib = dlopen(NULL, RTLD_NOW|RTLD_GLOBAL);
    context->argc = argc;
    context->argv = (char**)box_calloc(context->argc+1, sizeof(char*));
    pthread_mutex_init(&context->mutex_lock, NULL);

    return context;
}

EXPORTDYN
void FreeBox64Context(box64context_t** context)
{
    if(!context)
        return;
    
    if(--(*context)->forked >= 0)
        return;

    box64context_t* ctx = *context;   // local copy to do the cleanning

    if(ctx->local_maplib)
        FreeLibrarian(&ctx->local_maplib);
    if(ctx->maplib)
        FreeLibrarian(&ctx->maplib);
    FreeDictionnary(&ctx->versym);

    for(int i=0; i<ctx->elfsize; ++i) {
        FreeElfHeader(&ctx->elfs[i]);
    }
    box_free(ctx->elfs);

    FreeCollection(&ctx->box64_path);
    FreeCollection(&ctx->box64_ld_lib);
    FreeCollection(&ctx->box64_emulated_libs);

    if(ctx->deferedInitList)
        box_free(ctx->deferedInitList);

    /*box_free(ctx->argv);*/
    
    /*for (int i=0; i<ctx->envc; ++i)
        box_free(ctx->envv[i]);
    box_free(ctx->envv);*/

    if(ctx->atfork_sz) {
        box_free(ctx->atforks);
        ctx->atforks = NULL;
        ctx->atfork_sz = ctx->atfork_cap = 0;
    }

    for(int i=0; i<MAX_SIGNAL; ++i)
        if(ctx->signals[i]!=0 && ctx->signals[i]!=1) {
            signal(i, SIG_DFL);
        }

    *context = NULL;                // bye bye my_context

    box_free(ctx->fullpath);
    box_free(ctx->box64path);

    FreeBridge(&ctx->system);

    if(ctx->stack_clone)
        box_free(ctx->stack_clone);

    free_neededlib(&ctx->neededlibs);

    box_free(ctx);
}
int AddMallocMap(box64context_t* ctx, struct malloc_map* map) {
    int idx = ctx->mallocmapsize;
    if(idx==ctx->mallocmapcap) {
        // resize...
        ctx->mallocmapcap += 16;
        ctx->mallocmaps = (struct malloc_map**)box_realloc(ctx->mallocmaps, sizeof(struct malloc_map *) * ctx->mallocmapcap);
    }
    ctx->mallocmaps[idx] = map;
    ctx->mallocmapsize++;
    printf_log(LOG_INFO, "Adding \"%p\" as #%d in mallocmap collection\n", ctx->mallocmaps[idx], idx);
    return idx;
}
struct malloc_map * SearchMallocMap(box64context_t* ctx, char *elfname)
{
    for (int i =0; i < ctx->mallocmapsize; i++) {
        if (!strcmp(basename(ElfName(ctx->mallocmaps[i]->h)), elfname)) {
            return ctx->mallocmaps[i];
        }
    }
    return NULL;
}

#if defined(CONFIG_LATX_KZT) && defined(CONFIG_LATX_DEBUG)
int AddKztDebugInfo(box64context_t* ctx, struct latx_kzt_debug* debuginfo)
{
    int idx = ctx->latx_kzt_debugsize;
    if(idx==ctx->latx_kzt_debugcap) {
        // resize...
        ctx->latx_kzt_debugcap += 16;
        ctx->latx_kzt_debugs = (struct latx_kzt_debug**)box_realloc(ctx->latx_kzt_debugs, sizeof(struct latx_kzt_debug *) * ctx->latx_kzt_debugcap);
    }
    ctx->latx_kzt_debugs[idx] = debuginfo;
    ctx->latx_kzt_debugsize++;
    printf_log(LOG_NONE, "Adding \"%p\" as #%d in latx_kzt_debug collection\n", ctx->latx_kzt_debugs[idx], idx);
    return idx;
}
#endif

int AddElfHeader(box64context_t* ctx, elfheader_t* head) {
    int idx = ctx->elfsize;
    if(idx==ctx->elfcap) {
        // resize...
        ctx->elfcap += 16;
        ctx->elfs = (elfheader_t**)box_realloc(ctx->elfs, sizeof(elfheader_t*) * ctx->elfcap);
    }
    ctx->elfs[idx] = head;
    ctx->elfsize++;
    printf_log(LOG_INFO, "Adding \"%s\" as #%d in elf collection\n", ElfName(head), idx);
    return idx;
}


void add_neededlib(needed_libs_t* needed, library_t* lib)
{
    if(!needed)
        return;
    for(int i=0; i<needed->size; ++i)
        if(needed->libs[i] == lib)
            return;
    if(needed->size == needed->cap) {
        needed->cap += 8;
        needed->libs = (library_t**)box_realloc(needed->libs, needed->cap*sizeof(library_t*));
    }
    needed->libs[needed->size++] = lib;
}

void free_neededlib(needed_libs_t* needed)
{
    if(!needed)
        return;
    needed->cap = 0;
    needed->size = 0;
    if(needed->libs)
        box_free(needed->libs);
    needed->libs = NULL;
}

void add_dependedlib(needed_libs_t* depended, library_t* lib)
{
    if(!depended)
        return;
    for(int i=0; i<depended->size; ++i)
        if(depended->libs[i] == lib)
            return;
    if(depended->size == depended->cap) {
        depended->cap += 8;
        depended->libs = (library_t**)box_realloc(depended->libs, depended->cap*sizeof(library_t*));
    }
    depended->libs[depended->size++] = lib;
}

void free_dependedlib(needed_libs_t* depended)
{
    if(!depended)
        return;
    depended->cap = 0;
    depended->size = 0;
    if(depended->libs)
        box_free(depended->libs);
    depended->libs = NULL;
}
