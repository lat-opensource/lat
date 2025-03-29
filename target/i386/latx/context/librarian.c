#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "debug.h"
#include "librarian.h"
#include "librarian_private.h"
#include "library.h"
#include "library_private.h"
#include "box64context.h"
#include "elfloader.h"
#include "bridge.h"
#include "lsenv.h"

KHASH_MAP_IMPL_INT(mapoffsets, cstr_t);

lib_t *NewLibrarian(box64context_t* context, int ownlibs)
{
    lib_t *maplib = (lib_t*)box_calloc(1, sizeof(lib_t));
    
    maplib->mapsymbols = NewMapSymbols();
    maplib->weaksymbols = NewMapSymbols();
    maplib->localsymbols = NewMapSymbols();
    maplib->mapoffsets = kh_init(mapoffsets);
    maplib->globaldata = NewMapSymbols();
    maplib->bridge = NewBridge();

    maplib->context = context;

    maplib->ownlibs = ownlibs;

    return maplib;
}

static void freeLibraryRecurse(lib_t *maplib, int idx, char *freed, library_t* owner) {
    if (freed[idx]) return; // Already freed
    
    library_t *lib = maplib->libraries[idx];
    if(lib==owner) return; // don't free owner of maplib
    freed[idx] = 1; // Avoid infinite loops
    printf_log(LOG_DEBUG, "Free %s\n", lib->name);
    for (int i = lib->depended.size - 1; i >= 0; --i) {
        int j;
        for (j = 0; j < maplib->libsz; ++j) {
            if (lib->depended.libs[i] == maplib->libraries[j]) break;
        }
        if (j == maplib->libsz) {
            // dependant lib already freed
            continue;
        }
        if (freed[j] == 1) {
            printf_log(LOG_DEBUG, "Cyclic dependancy detected (cycle is between %s and %s)\n", lib->name, lib->depended.libs[i]->name);
            continue;
        }
        freeLibraryRecurse(maplib, j, freed, owner);
        if (freed[idx] != 1) {
            printf_log(LOG_DEBUG, "Library cleared (cyclic dependancy break)\n");
            return;
        }
    }
    library_t *ptr = maplib->libraries[idx];
    if(maplib->ownlibs)
        Free1Library(&ptr);
    freed[idx] = 2;
}

void FreeLibrarian(lib_t **maplib)
{
    if(!maplib || !*maplib)
        return;
    
    library_t* owner = (*maplib)->owner;
    (*maplib)->owner = NULL;    // to avoid recursive free
    if((*maplib)->ownlibs && (*maplib)->libsz) {
        printf_log(LOG_DEBUG, "Closing %d libs from maplib %p\n", (*maplib)->libsz, *maplib);
        char *freed = (char*)box_calloc((*maplib)->libsz, sizeof(char));
        if (!freed) {
            printf_log(LOG_INFO, "Failed to malloc freed table, using old algorithm (a crash is likely)\n");
            for (int i=(*maplib)->libsz-1; i>=0; --i) 
                if((*maplib)->libraries[i]!=owner) {
                    printf_log(LOG_DEBUG, "Unloading %s\n", (*maplib)->libraries[i]->name);
                    Free1Library(&(*maplib)->libraries[i]);
                }
        } else {
            for (int i=(*maplib)->libsz-1; i>=0; --i) {
                freeLibraryRecurse(*maplib, i, freed, owner);
            }
            memset((*maplib)->libraries, 0, (*maplib)->libsz*sizeof(library_t*)); // NULL = 0 anyway
            (*maplib)->libsz = 0;
            box_free(freed);
        }
    }
    box_free((*maplib)->libraries);
    (*maplib)->libraries = NULL;

    FreeMapSymbols(&(*maplib)->mapsymbols);
    FreeMapSymbols(&(*maplib)->weaksymbols);
    FreeMapSymbols(&(*maplib)->localsymbols);
    if((*maplib)->mapoffsets) {
        kh_destroy(mapoffsets, (*maplib)->mapoffsets);
    }
    FreeMapSymbols(&(*maplib)->globaldata);
    (*maplib)->libsz = (*maplib)->libcap = 0;

    if((*maplib)->bridge)
        FreeBridge(&(*maplib)->bridge);

    box_free(*maplib);
    *maplib = NULL;
}

box64context_t* GetLibrarianContext(lib_t* maplib)
{
    return maplib->context;
}

kh_mapsymbols_t* GetMapSymbol(lib_t* maplib)
{
    return maplib->mapsymbols;
}

kh_mapsymbols_t* GetWeakSymbol(lib_t* maplib)
{
    return maplib->weaksymbols;
}

kh_mapsymbols_t* GetLocalSymbol(lib_t* maplib)
{
    return maplib->localsymbols;
}

kh_mapsymbols_t* GetGlobalData(lib_t* maplib)
{
    return maplib->globaldata;
}

library_t* getLib(lib_t* maplib, const char* path)
{
    for(int i=0; i<maplib->libsz; ++i) {
        library_t *lib = maplib->libraries[i];
        if(IsSameLib(lib, path)) {
            return lib;
        }
    }
    return NULL;
}

static int libraryInMapLib(lib_t* maplib, library_t* lib)
{
    if(!maplib)
        return 0;
    for(int i=0; i<maplib->libsz; ++i)
        if(maplib->libraries[i]==lib)
            return 1;
    return 0;
}

void MapLibAddLib(lib_t* maplib, library_t* lib)
{
    if(libraryInMapLib(maplib, lib))
        return;
    if (maplib->libsz == maplib->libcap) {
        maplib->libcap += 8;
        maplib->libraries = (library_t**)box_realloc(maplib->libraries, maplib->libcap*sizeof(library_t*));
    }
    maplib->libraries[maplib->libsz] = lib;
    ++maplib->libsz;
}

void MapLibAddMapLib(lib_t* dest, lib_t* src)
{
    if(!src)
        return;
    for(int i=0; i<src->libsz; ++i) {
        library_t* lib = src->libraries[i];
        if(!lib || libraryInMapLib(dest, lib)) continue;
        MapLibAddLib(dest, lib);
        if(lib->maplib && src!=lib->maplib) {   //TODO: find why is src!=lib->maplib needed
            MapLibAddMapLib(dest, lib->maplib);
            box_free(lib->maplib);
            lib->maplib = NULL;
        }
    }
}

void MapLibRemoveLib(lib_t* maplib, library_t* lib)
{
    if(!maplib || !lib)
        return;
    int idx = 0;
    while(idx<maplib->libsz && maplib->libraries[idx]!=lib) ++idx;
    if(idx==maplib->libsz)  //not found
        return;
    --maplib->libsz;
    if(idx!=(maplib->libsz))
        memmove(&maplib->libraries[idx], &maplib->libraries[idx+1], sizeof(library_t*)*(maplib->libsz-idx));
}

static void MapLibRemoveMapLib(lib_t* dest, lib_t* src)
{
    if(!src)
        return;
    //library_t *owner = src->owner;
    for(int i=0; i<src->libsz; ++i) {
        library_t* lib = src->libraries[i];
        if(!lib || !libraryInMapLib(dest, lib)) continue;
        MapLibRemoveLib(dest, lib);
        if(lib->maplib && src!=lib->maplib && dest!=lib->maplib) {
            MapLibRemoveMapLib(dest, lib->maplib);
            if(lib->maplib && src==my_context->local_maplib)
                lib->maplib = NULL;
        }
    }
}

int AddNeededLib_add(lib_t* maplib, needed_libs_t* neededlibs, library_t* deplib, int local, const char* path, box64context_t* box64)
{
    printf_log(LOG_INFO, "Trying to add \"%s\" to maplib%s\n", path, local?" (local)":"");
    // first check if lib is already loaded
    library_t *lib = getLib(my_context->maplib, path);
    if(lib) {
        add_neededlib(neededlibs, lib);
        if (lib && deplib) add_dependedlib(&lib->depended, deplib);
        printf_log(LOG_INFO, "Already present in maplib => success\n");
        return 0;
    }
    // check also in the local loaded lib
    lib = getLib(my_context->local_maplib, path);
    if(lib) {
        printf_log(LOG_INFO, "Already present in local_maplib => success\n");
        if(local) {
            // add lib to maplib...
            if(maplib) {
                if(lib->maplib) {
                    MapLibAddMapLib(maplib, lib->maplib);
                }
                if(!libraryInMapLib(maplib, lib))
                    MapLibAddLib(maplib, lib);
                if(maplib->ownlibs)
                    MapLibRemoveMapLib(my_context->local_maplib, maplib);
            }
        } else {
            // promote lib from local to global...
            // for add the depending local libs...
            if(lib->maplib) {
                MapLibAddMapLib(my_context->maplib, lib->maplib);
                box_free(lib->maplib);
                lib->maplib = NULL;
            }
            if(!libraryInMapLib(my_context->maplib, lib))
                MapLibAddLib(my_context->maplib, lib);
            MapLibRemoveMapLib(my_context->local_maplib, my_context->maplib);
        }
        add_neededlib(neededlibs, lib);
        if (lib && deplib) add_dependedlib(&lib->depended, deplib);
        return 0;
    }
    // load a new one
      //      lsassert(!strstr(path,"libGLEW.so.2.1"));
    lib = NewLibrary(path, box64);
    if(!lib) {
        printf_log(LOG_INFO, "Faillure to create lib => fail\n");
        return 1;   //Error
    }

    add_neededlib(neededlibs, lib);
    if (lib && deplib) add_dependedlib(&lib->depended, deplib);

    // add lib now
    if(local) {
        MapLibAddLib(my_context->local_maplib, lib);
        if(maplib)
            MapLibAddLib(maplib, lib);
        lib->maplib = NewLibrarian(box64, 0);
        MapLibAddLib(lib->maplib, lib);
    } else {
        MapLibAddLib(my_context->maplib, lib);
    }

    if(!maplib)
        maplib = (local)?lib->maplib:my_context->maplib;

    if(AddSymbolsLibrary(maplib, lib)) {   // also add needed libs
        printf_log(LOG_INFO, "Failure to Add lib => fail\n");
        return 1;
    }
    if (lib->type == LIB_EMULATED) {
        // Need to add library to the linkmap (put here so the link is ordered)
        linkmap_t *lm = addLinkMapLib(lib);
        if(!lm) {
            // Crashed already
            printf_log(LOG_INFO, "Failure to add lib linkmap\n");
            return 1;
        }
        lm->l_addr = (Elf64_Addr)GetElfDelta(my_context->elfs[lib->priv.n.elf_index]);
        lm->l_name = lib->name;
        lm->l_ld = GetDynamicSection(my_context->elfs[lib->priv.n.elf_index]);
    }
    return 0;
}

int AddNeededLib(lib_t* maplib, needed_libs_t* neededlibs, library_t* deplib, int local, int bindnow, const char** paths, int npath, box64context_t* box64, int init_main_elf)
{
    if(!neededlibs) {
        neededlibs = box_calloc(1, sizeof(needed_libs_t));
    };
    // Add libs and symbol
    for(int i=0; i<npath; ++i) {
        if(AddNeededLib_add(maplib, neededlibs, deplib, local, paths[i], box64)) {
            printf_log(LOG_INFO, "Error loading needed lib %s\n", paths[i]);
            //return 1;
        }
    }
    return 0;
}

library_t* GetLibMapLib(lib_t* maplib, const char* name)
{
    printf_log(LOG_DEBUG, "Trying to Get \"%s\" to maplib\n", name);
    return getLib(maplib, name);
}

library_t* GetLibInternal(const char* name)
{
    printf_log(LOG_DEBUG, "Trying to Get \"%s\" to maplib\n", name);
    library_t* lib = getLib(my_context->maplib, name);
    if(!lib) lib = getLib(my_context->local_maplib, name);
    return lib;
}

EXPORTDYN
uintptr_t FindGlobalSymbol(lib_t *maplib, const char* name, int version, const char* vername)
{
    uintptr_t start = 0, end = 0;
    if(GetGlobalSymbolStartEnd(maplib, name, &start, &end, (void*)1, version, vername))
        return start;
    return 0;
}

static int isLocal(elfheader_t* self, library_t* l)
{
    if(GetElfIndex(l)==-1)
        return 1;
    if(my_context->elfs[GetElfIndex(l)]==self)
        return 1;
    return 0;
}

extern elfheader_t* elf_header;
int GetNoSelfSymbolStartEnd(lib_t *maplib, const char* name, uintptr_t* start, uintptr_t* end, elfheader_t* self, int version, const char* vername)
{
    assert(self);   // need self for this one
    //search for the self, to start "next"
    int go = -1;
    for(int i=0; i<maplib->libsz && (go==-1); ++i) {
        if(GetElfIndex(maplib->libraries[i])!=-1 && (maplib->context->elfs[GetElfIndex(maplib->libraries[i])]==self))
            go = i+1;
    }
    if(go<0)
        go = 0; // not found...
    for(int i=go; i<maplib->libsz; ++i) {
        if(GetElfIndex(maplib->libraries[i])==-1 || (maplib->context->elfs[GetElfIndex(maplib->libraries[i])]!=self))
            if(GetLibSymbolStartEnd(maplib->libraries[i], name, 0, start, end, version, vername, 0))
                if(*start)
                    return 1;
    }
    // if self defined, give it another chance with self...
    if(self) {
        if(elf_header==self) {
            if(GetSymbolStartEnd(maplib->mapsymbols, name, start, end, version, vername, 1))
                if(*start)
                    return 1;
            if(GetSymbolStartEnd(maplib->weaksymbols, name, start, end, version, vername, 1))
                if(*start)
                    return 1;
        }
        for(int i=0; i<go; ++i) {
            if(GetElfIndex(maplib->libraries[i])==-1 || (maplib->context->elfs[GetElfIndex(maplib->libraries[i])]==self))
                if(GetLibSymbolStartEnd(maplib->libraries[i], name, 0, start, end, version, vername, 1))
                    if(*start)
                        return 1;
        }
    }
    // nope, not found
    return 0;
}
static int GetGlobalSymbolStartEnd_internal(lib_t *maplib, const char* name, uintptr_t* start, uintptr_t* end, elfheader_t* self, int version, const char* vername)
{
    khint_t pre_k = kh_str_hash_func(name);

// this condition set noweak = 1, but we don't add weak symbole now
//    for(int i=0; i<maplib->libsz; ++i) {
//        if(GetLibNoWeakSymbolStartEnd(maplib->libraries[i], name, pre_k, start, end, version, vername, isLocal(self, maplib->libraries[i])))
//            if(*start)
//                return 1;
//    }

    //noweak=0
    for(int i=0; i<maplib->libsz; ++i) {
        if(GetLibSymbolStartEnd(maplib->libraries[i], name, pre_k, start, end, version, vername, isLocal(self, maplib->libraries[i])))    // only weak symbol haven't been found yet
            if(*start)
                return 1;
    }
    // nope, not found
    return 0;
}

int GetGlobalSymbolStartEnd(lib_t *maplib, const char* name, uintptr_t* start, uintptr_t* end, elfheader_t* self, int version, const char* vername)
{
    if(!maplib)
        return 0;
    if(GetGlobalSymbolStartEnd_internal(maplib, name, start, end, self, version, vername)) {
        if(start && end && *end==*start) {  // object is of 0 sized, try to see an "_END" object of null size
            uintptr_t start2, end2;
            char* buff = (char*)box_malloc(strlen(name) + strlen("_END") + 1);
            strcpy(buff, name);
            strcat(buff, "_END");
            if(GetGlobalSymbolStartEnd_internal(maplib, buff, &start2, &end2, self, version, vername)) {
                if(end2>*end && start2==end2)
                    *end = end2;
            }
            box_free(buff);
        }
        return 1;
    }
    // some special case symbol, defined inside box64 itself
    if(!strcmp(name, "gdk_display")) {
        //*start = (uintptr_t)my_GetGTKDisplay();
        //*end = *start+sizeof(void*);
        //printf_log(LOG_INFO, "Using global gdk_display for gdk-x11 (%p:%p)\n", start, *(void**)start);
        return 0;
    }
    if(!strcmp(name, "g_threads_got_initialized")) {
        //*start = (uintptr_t)my_GetGthreadsGotInitialized();
        //*end = *start+sizeof(int);
        //printf_log(LOG_INFO, "Using global g_threads_got_initialized for gthread2 (%p:%p)\n", start, *(void**)start);
        return 0;
    }
    // not found...
    return 0;
}

elfheader_t* GetGlobalSymbolElf(lib_t *maplib, const char* name, int version, const char* vername)
{
    uintptr_t start = 0;
    uintptr_t end = 0;
    if(GetSymbolStartEnd(maplib->mapsymbols, name, &start, &end, version, vername, 1))
        return elf_header;
    if(GetSymbolStartEnd(maplib->weaksymbols, name, &start, &end, version, vername, 1))
        return elf_header;
    for(int i=0; i<maplib->libsz; ++i) {
        if(GetLibSymbolStartEnd(maplib->libraries[i], name, 0, &start, &end, version, vername, 1)) {
            int idx = GetElfIndex(maplib->libraries[i]);
            if(idx==-1) {
                printf_log(LOG_NONE, "Warning, getting Elf info for a native symbol \"%s\" from lib \"%s\"\n", name, GetNameLib(maplib->libraries[i]));
                return NULL;
            }
            return maplib->context->elfs[idx];
        }
    }
    // nope, not found
    return NULL;
}

int GetGlobalNoWeakSymbolStartEnd(lib_t *maplib, const char* name, uintptr_t* start, uintptr_t* end, int version, const char* vername)
{
    if(GetSymbolStartEnd(maplib->mapsymbols, name, start, end, version, vername, 1))
        if(*start || *end)
            return 1;
    for(int i=0; i<maplib->libsz; ++i)
        if(GetLibNoWeakSymbolStartEnd(maplib->libraries[i], name, 0,start, end, version, vername, 1))
            if(*start || *end)
                return 1;
    // nope, not found
    return 0;
}

int IsGlobalNoWeakSymbolInNative(lib_t *maplib, const char* name, int version, const char* vername)
{
    uintptr_t start, end;
    for(int i=0; i<maplib->libsz; ++i)
        if(GetElfIndex(maplib->libraries[i])==-1)
            if(GetLibNoWeakSymbolStartEnd(maplib->libraries[i], name, 0, &start, &end, version, vername, 1))
                if(start || end)
                    return 1;
    // nope, not found
    return 0;

}

int GetLocalSymbolStartEnd(lib_t *maplib, const char* name,khint_t pre_k, uintptr_t* start, uintptr_t* end, elfheader_t *self, int version, const char* vername)
{
    if(!pre_k) { pre_k = kh_str_hash_func(name); }
    if(elf_header ==self || !self) {
        if(GetSymbolStartEnd(maplib->localsymbols, name, start, end, version, vername, 1))
            if(*start || *end)
                return 1;
        if(self)
            return 0;
    }
    for(int i=0; i<maplib->libsz; ++i) {
        if(GetElfIndex(maplib->libraries[i])!=-1 && (!self || maplib->context->elfs[GetElfIndex(maplib->libraries[i])]==self)) {
            if(GetLibLocalSymbolStartEnd(maplib->libraries[i], name, pre_k, start, end, version, vername, 1))
                if(*start)
                    return 1;
            if(self)
                return 0;
        }
    }
    return 0;
}

int GetSelfSymbolStartEnd(lib_t *maplib, const char* name, uintptr_t* start, uintptr_t* end, elfheader_t *self, int version, const char* vername)
{
    if(elf_header==self) {
        if(GetSymbolStartEnd(maplib->localsymbols, name, start, end, version, vername, 1))
            if(*start || *end)
                return 1;
    } else {
        for(int i=0; i<maplib->libsz; ++i) {
            if(GetElfIndex(maplib->libraries[i])!=-1 && (maplib->context->elfs[GetElfIndex(maplib->libraries[i])]==self))
                if(GetLibSymbolStartEnd(maplib->libraries[i], name, 0,start, end, version, vername, 1))
                    if(*start || *end)
                        return 1;
        }
    }
    return 0;
}

int GetNoWeakSymbolStartEnd(lib_t *maplib, const char* name, uintptr_t* start, uintptr_t* end, elfheader_t *self, int version, const char* vername)
{
    if(elf_header==self) {
        if(GetSymbolStartEnd(maplib->mapsymbols, name, start, end, version, vername, 1))
            if(*start || *end)
                return 1;
    } else {
        for(int i=0; i<maplib->libsz; ++i) {
            if(GetElfIndex(maplib->libraries[i])!=-1 && (maplib->context->elfs[GetElfIndex(maplib->libraries[i])]==self))
                if(GetLibNoWeakSymbolStartEnd(maplib->libraries[i], name, 0, start, end, version, vername, 1))
                    if(*start || *end)
                        return 1;
        }
    }
    return 0;
}
const char* FindSymbolName(lib_t *maplib, void* p, void** start, uint64_t* sz, const char** libname, void** base, library_t** lib)
{
    // first, search in self...
    const char* ret = NULL;
    uintptr_t offs = 0;
    uint64_t size = 0;
    elfheader_t* h = FindElfAddress(my_context, (uintptr_t)p);
    if(h) {
        ret = FindNearestSymbolName(h, p, &offs, &size);

        if(start)
            *start = (void*)offs;
        if(sz)
            *sz = size;
        if(libname)
            *libname = ElfName(h);
        if(base)
            *base = GetBaseAddress(h);
        if(lib) {
            if(h == elf_header)
                *lib = NULL;    // main elf
            else {
                for(int i=0; i<my_context->maplib->libsz; ++i) {
                    int idx = GetElfIndex(my_context->maplib->libraries[i]);
                    if((idx!=-1) && (my_context->elfs[idx]==h)) {
                        *lib = my_context->maplib->libraries[i];
                        return ret;
                    }
                }
                for(int i=0; i<my_context->local_maplib->libsz; ++i) {
                    int idx = GetElfIndex(my_context->local_maplib->libraries[i]);
                    if((idx!=-1) && (my_context->elfs[idx]==h)) {
                        *lib = my_context->local_maplib->libraries[i];
                        return ret;
                    }
                }
            }
        }
        return ret;
    }
    // TODO: find if cyclic references exists (should also fix MapLibAddMapLib)
    (void)maplib;
    /* for (int i = 0; i < maplib->libsz; ++i) {
        // if (maplib == maplib->libraries[i]->maplib) continue;
        const char *nameInLib = FindSymbolName(maplib->libraries[i]->maplib, p, start, sz, libname, base);
        if (nameInLib) return nameInLib;
    } */
    return NULL;
}

void AddOffsetSymbol(lib_t *maplib, void* offs, const char* name)
{
    int ret;
    khint_t k = kh_put(mapoffsets, maplib->mapoffsets, (uintptr_t)offs, &ret);
    kh_value(maplib->mapoffsets, k) = (cstr_t)name;
}

const char* GetNameOffset(lib_t *maplib, void* offs)
{
    if(!maplib || !maplib->mapoffsets)
        return NULL;
    khint_t k = kh_get(mapoffsets, maplib->mapoffsets, (uintptr_t)offs);
    if (k!=kh_end(maplib->mapoffsets))
        return kh_value(maplib->mapoffsets, k);
    return NULL;
}
