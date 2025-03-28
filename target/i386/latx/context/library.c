#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <link.h>
#include <stdarg.h>

#include "debug.h"
#include "library.h"
#include "elfloader.h"
#include "bridge.h"
#include "library_private.h"
#include "khash.h"
#include "box64context.h"
#include "fileutils.h"
#include "librarian.h"
#include "librarian_private.h"
#include "pathcoll.h"

#define GO(P, N) int wrapped##N##_init(library_t* lib, box64context_t *box64); \
                 void wrapped##N##_fini(library_t* lib); \
                 int wrapped##N##_get(library_t* lib, const char* name, khint_t pre_k, uintptr_t *offs, uintptr_t *sz, int version, const char* vername, int local); \
                 int wrapped##N##_getnoweak(library_t* lib, const char* name, khint_t pre_k, uintptr_t *offs, uintptr_t *sz, int version, const char* vername, int local);
#define GOALIAS(P, N)
#include "library_list.h"
#undef GO
#undef GOALIAS

#define GO(P, N) {P, wrapped##N##_init, wrapped##N##_fini, wrapped##N##_get, wrapped##N##_getnoweak},
#define GOALIAS(P, N) {P, wrapped##N##_init, wrapped##N##_fini, wrapped##N##_get, wrapped##N##_getnoweak},
wrappedlib_t wrappedlibs[] = {
#include "library_list.h"
};
#undef GO
#undef GOALIAS

typedef struct bridged_s {
    char*       name;
    uintptr_t   start;
    uintptr_t   end;
} bridged_t;

KHASH_MAP_INIT_STR(bridgemap, bridged_t)
KHASH_MAP_IMPL_STR(symbol1map, symbol1_t)
KHASH_MAP_IMPL_STR(symbolmap, wrapper_t)
KHASH_MAP_IMPL_STR(symbol2map, symbol2_t)
KHASH_MAP_IMPL_STR(datamap, uint64_t)

char* Path2Name(const char* path)
{
    char* name = (char*)box_calloc(1, MAX_PATH);
    char* p = strrchr(path, '/');
    strcpy(name, (p)?(p+1):path);
    // name should be libXXX.so.A(.BB.CCCC)
    // so keep only 2 dot after ".so" (if any)
    p = strstr(name, ".so");
    if(p) {
        p=strchr(p+3, '.');   // this one is ok
        //if(p) p=strchr(p+1, '.');// this one is ok too
        if(p) p=strchr(p+1, '.');// this one is not
        if(p) *p = '\0';
    }
    return name;
}
int NbDot(const char* name)
{
    char *p = strstr(name, ".so");
    if(!p)
        return -1;
    int ret = 0;
    while(p) {
        p = strchr(p+1, '.');
        if(p) ++ret;
    }
    return ret;
}

void NativeLib_CommonInit(library_t *lib) {
    lib->priv.w.bridge = NewBridge();
    
    lib->symbolmap = kh_init(symbolmap);
    lib->wsymbolmap = kh_init(symbolmap);
    lib->mysymbolmap = kh_init(symbolmap);
    lib->wmysymbolmap = kh_init(symbolmap);
    lib->stsymbolmap = kh_init(symbolmap);
    lib->symbol2map = kh_init(symbol2map);
    lib->datamap = kh_init(datamap);
    lib->wdatamap = kh_init(datamap);
    lib->mydatamap = kh_init(datamap);
}

void EmuLib_Fini(library_t* lib)
{
    kh_destroy(mapsymbols, lib->priv.n.mapsymbols);
    kh_destroy(mapsymbols, lib->priv.n.weaksymbols);
    kh_destroy(mapsymbols, lib->priv.n.localsymbols);
}
void NativeLib_FinishFini(library_t* lib)
{
    if(lib->priv.w.lib)
        dlclose(lib->priv.w.lib);
    lib->priv.w.lib = NULL;
    if(lib->priv.w.altprefix)
        box_free(lib->priv.w.altprefix);
    if(lib->priv.w.neededlibs) {
        for(int i=0; i<lib->priv.w.needed; ++i)
            box_free(lib->priv.w.neededlibs[i]);
        box_free(lib->priv.w.neededlibs);
    }
    FreeBridge(&lib->priv.w.bridge);
}

int WrappedLib_defget(library_t* lib, const char* name,khint_t pre_k, uintptr_t *offs, uintptr_t *sz, int version, const char* vername, int local) {
    uintptr_t addr = 0;
    uintptr_t size = 0;
    if (!getSymbolInMaps(lib, name, pre_k, 0, &addr, &size, version, vername, local)) {
        return 0;
    }
    if(!addr && !size)
        return 0;
    *offs = addr;
    *sz = size;
    return 1;
}
int EmuLib_Get(library_t* lib, const char* name, uintptr_t *offs, uintptr_t *sz, int version, const char* vername, int local)
{
    // symbols...
    uintptr_t start, end;
    if(GetSymbolStartEnd(lib->priv.n.mapsymbols, name, &start, &end, version, vername, local))
    {
        *offs = start;
        *sz = end-start;
        return 1;
    }
    // weak symbols...
    if(GetSymbolStartEnd(lib->priv.n.weaksymbols, name, &start, &end, version, vername, local))
    {
        *offs = start;
        *sz = end-start;
        return 1;
    }
    return 0;
}
int WrappedLib_defgetnoweak(library_t* lib, const char* name, khint_t pre_k, uintptr_t *offs, uintptr_t *sz, int version, const char* vername, int local) {
    uintptr_t addr = 0;
    uintptr_t size = 0;
    if (!getSymbolInMaps(lib, name, pre_k, 1, &addr, &size, version, vername, local)) {
        return 0;
    }
    if(!addr && !size)
        return 0;
    *offs = addr;
    *sz = size;
    return 1;
}
int EmuLib_GetNoWeak(library_t* lib, const char* name, uintptr_t *offs, uintptr_t *sz, int version, const char* vername, int local)
{
    uintptr_t start, end;
    if(GetSymbolStartEnd(lib->priv.n.mapsymbols, name, &start, &end, version, vername, local))
    {
        *offs = start;
        *sz = end-start;
        return 1;
    }
    return 0;
}
int EmuLib_GetLocal(library_t* lib, const char* name, uintptr_t *offs, uintptr_t *sz, int version, const char* vername, int local)
{
    uintptr_t start, end;
    if(GetSymbolStartEnd(lib->priv.n.localsymbols, name, &start, &end, version, vername, local))
    {
        *offs = start;
        *sz = end-start;
        return 1;
    }
    return 0;
}

int NativeLib_GetLocal(library_t* lib, const char* name, khint_t pre_k, uintptr_t *offs, uintptr_t *sz, int version, const char* vername, int local)
{
    (void)lib; (void)name; (void)offs; (void)sz; (void)version; (void)vername; (void)local;
    return 0;
}
int FindLibIsWrapped(char * name)
{
    int nb = sizeof(wrappedlibs) / sizeof(wrappedlib_t);
    for (int i=0; i<nb; ++i) {
        if(strcmp(name, wrappedlibs[i].name)==0) {
            return 1;
        }
    }
    return 0;
}
static void initNativeLib(library_t *lib, box64context_t* context) {
    int nb = sizeof(wrappedlibs) / sizeof(wrappedlib_t);
    for (int i=0; i<nb; ++i) {
        if(strcmp(lib->name, wrappedlibs[i].name)==0) {
            if(wrappedlibs[i].init(lib, context)) {
                // error!
                const char* error_str = dlerror();
                if(error_str)   // don't print the message if there is no error string from last error
                    printf_log(LOG_INFO, "Error initializing native %s (last dlerror is %s)\n", lib->name, error_str);
                return; // non blocker...
            }
            printf_log(LOG_INFO, "Using native(wrapped) %s\n", lib->name);
            lib->priv.w.box64lib = context->box64lib;
            lib->context = context;
            lib->fini = wrappedlibs[i].fini;
            lib->get = wrappedlibs[i].get;
            lib->getnoweak = wrappedlibs[i].getnoweak;
            lib->getlocal = NativeLib_GetLocal;
            lib->type = LIB_WRAPPED;
            // Call librarian to load all dependant elf
            if(AddNeededLib(context->maplib, &lib->needed, lib, 0, 0, (const char**)lib->priv.w.neededlibs, lib->priv.w.needed, context, 0 /* not init_main_elf */)) {
                printf_log(LOG_INFO, "Error: loading a needed libs in elf %s\n", lib->name);
                return;
            }

            linkmap_t *lm = addLinkMapLib(lib);
            if(!lm) {
                // Crashed already
                printf_log(LOG_INFO, "Failure to add lib %s linkmap\n", lib->name);
                break;
            }
            struct link_map real_lm;
            if(dlinfo(lib->priv.w.lib, RTLD_DI_LINKMAP, &real_lm)) {
                printf_log(LOG_INFO, "Failed to dlinfo lib %s\n", lib->name);
            }
            lm->l_addr = real_lm.l_addr;
            lm->l_name = real_lm.l_name;
            lm->l_ld = real_lm.l_ld;
            break;
        }
    }
}

static const char* essential_libs[] = {
    "libc.so.6", "libpthread.so.0", "librt.so.1", "libGL.so.1", "libGL.so", "libX11.so.6", 
    "libasound.so.2", "libdl.so.2", "libm.so.6",
    "libXxf86vm.so.1", "libXinerama.so.1", "libXrandr.so.2", "libXext.so.6", "libXfixes.so.3", "libXcursor.so.1",
    "libXrender.so.1", "libXft.so.2", "libXi.so.6", "libXss.so.1", "libXpm.so.4", "libXau.so.6", "libXdmcp.so.6",
    "libX11-xcb.so.1", "libxcb.so.1", "libxcb-xfixes.so.0", "libxcb-shape.so.0", "libxcb-shm.so.0", "libxcb-randr.so.0",
    "libxcb-image.so.0", "libxcb-keysyms.so.1", "libxcb-xtest.so.0", "libxcb-glx.so.0", "libxcb-dri2.so.0", "libxcb-dri3.so.0",
    "libXtst.so.6", "libXt.so.6", "libXcomposite.so.1", "libXdamage.so.1", "libXmu.so.6", "libxkbcommon.so.0", 
    "libxkbcommon-x11.so.0", "libpulse-simple.so.0", "libpulse.so.0", "libvulkan.so.1", "libvulkan.so",
    "ld-linux-x86-64.so.2", "crashhandler.so", "libtcmalloc_minimal.so.0", "libtcmalloc_minimal.so.4", "libGLEW.so.2.1",
};
static int isEssentialLib(const char* name) {
    for (int i=0; i<sizeof(essential_libs)/sizeof(essential_libs[0]); ++i)
        if(!strcmp(name, essential_libs[i]))
            return 1;
    return 0;
}

static GList* loaded_libs = NULL;

void FreeLoadedLibs(void)
{
    if (loaded_libs) {
        g_list_free(loaded_libs);
        loaded_libs = NULL;
    }
}

static gint compare_string(gconstpointer a, gconstpointer b)
{
    return g_strcmp0(a, b);
}

GList* FindLoadedLibs(const char* path)
{
    return g_list_find_custom(loaded_libs, path, compare_string);
}

void AppendLoadedLibs(const char* path)
{
    loaded_libs = g_list_append(loaded_libs, (void*)path);
}

library_t *NewLibrary(const char* path, box64context_t* context)
{
    if (FindLoadedLibs(path) != NULL) {
        return NULL;
    }
    AppendLoadedLibs(path);
    printf_log(LOG_INFO, "Trying to load \"%s\"\n", path);
    library_t *lib = (library_t*)box_calloc(1, sizeof(library_t));
    lib->path = realpath(path, NULL);
    if(!lib->path)
        lib->path = box_strdup(path);
    if(libGL && !strcmp(path, libGL))
        lib->name = box_strdup("libGL.so.1");
    else
        lib->name = Path2Name(path);
    lib->nbdot = NbDot(lib->name);
    lib->context = context;
    lib->type = LIB_UNNKNOW;
    printf_log(LOG_INFO, "Simplified name is \"%s\"\n", lib->name);
    //lsassert(!strstr(lib->name, "libXinerama.so.1"));
    if(box64_nopulse) {
        if(strstr(lib->name, "libpulse.so")==lib->name || strstr(lib->name, "libpulse-simple.so")==lib->name) {
            box_free(lib->name);
            box_free(lib->path);
            box_free(lib);
            return NULL;
        }
    }
    if(box64_novulkan) {
        if(strstr(lib->name, "libvulkan.so")==lib->name) {
            box_free(lib->name);
            box_free(lib->path);
            box_free(lib);
            return NULL;
        }
    }
    int notwrapped = FindInCollection(lib->name, &context->box64_emulated_libs);
    int essential = isEssentialLib(lib->name);
    if(!notwrapped && box64_prefer_emulated && !essential)
        notwrapped = 1;
    int precise = (!box64_prefer_wrapped && !essential && path && strchr(path, '/'))?1:0;
    if(!notwrapped && precise && strstr(path, "libtcmalloc_minimal.so"))
        precise = 0;    // allow native version for tcmalloc_minimum
    // check if name is libSDL_sound-1.0.so.1 but with SDL2 loaded, then try emulated first...
    if(!notwrapped && !strcmp(lib->name, "libSDL_sound-1.0.so.1") && my_context->sdl2lib)
        notwrapped = 1;
    // And now, actually loading a library
    // look for native(wrapped) libs first
    if(!notwrapped && !precise)
        initNativeLib(lib, context);
    // then look for a native one
#if 0
    if(lib->type==LIB_UNNKNOW) {
        initEmulatedLib(path, lib, context, NULL);
    }
#endif
    // still not loaded but notwrapped indicated: use wrapped...
    if(lib->type==LIB_UNNKNOW && notwrapped && !precise)
        initNativeLib(lib, context);
    // nothing loaded, so error...
    if(lib->type==LIB_UNNKNOW)
    {
        box_free(lib->name);
        box_free(lib->path);
        box_free(lib);
        return NULL;
    }

    lib->bridgemap = kh_init(bridgemap);

    return lib;
}
int IsEmuLib(library_t* lib)
{
    return lib && lib->type == LIB_EMULATED;
}

int AddSymbolsLibrary(lib_t *maplib, library_t* lib)
{

    lib->active = 1;
    if(lib->type==LIB_EMULATED) {
        elfheader_t *elf_header = lib->context->elfs[lib->priv.n.elf_index];
        // add symbols
        AddSymbols(maplib, lib->priv.n.mapsymbols, lib->priv.n.weaksymbols, lib->priv.n.localsymbols, elf_header);
    }
    return 0;
}

int ReloadLibrary(library_t* lib)
{
    lib->active = 1;
    if(lib->type == LIB_EMULATED) {
        elfheader_t *elf_header = lib->context->elfs[lib->priv.n.elf_index];
        // reload image in memory and re-run the mapping
        char libname[MAX_PATH];
        strcpy(libname, lib->path);
        int found = FileExist(libname, IS_FILE);
        if(!found && !strchr(lib->path, '/'))
            for(int i=0; i<lib->context->box64_ld_lib.size; ++i)
            {
                strcpy(libname, lib->context->box64_ld_lib.paths[i]);
                strcat(libname, lib->path);
                if(FileExist(libname, IS_FILE))
                    break;
            }
        if(!FileExist(libname, IS_FILE)) {
            printf_log(LOG_NONE, "Error: open file to re-load elf %s\n", libname);
            return 1;   // failed to reload...
        }
        FILE *f = fopen(libname, "rb");
        if(!f) {
            printf_log(LOG_NONE, "Error: cannot open file to re-load elf %s (errno=%d/%s)\n", libname, errno, strerror(errno));
            return 1;   // failed to reload...
        }
        // can close the file now
        fclose(f);
        // should bindnow be store in a per/library basis?
        if(RelocateElf(lib->context->maplib, lib->maplib, 0, elf_header)) {
            printf_log(LOG_NONE, "Error: relocating symbols in elf %s\n", lib->name);
            return 1;
        }
        RelocateElfPlt(lib->context->maplib, lib->maplib, 0, elf_header);
    }
    return 0;
}
void InactiveLibrary(library_t* lib)
{
    lib->active = 0;
}

void Free1Library(library_t **lib)
{
    if(!(*lib)) return;

    //if((*lib)->type==1) {
    //    elfheader_t *elf_header = (*lib)->context->elfs[(*lib)->priv.n.elf_index];
    //    RunElfFini(elf_header, env);
    //}

    if((*lib)->maplib)
        FreeLibrarian(&(*lib)->maplib);

    if((*lib)->type!=-1 && (*lib)->fini) {
        (*lib)->fini(*lib);
    }
    box_free((*lib)->name);
    box_free((*lib)->path);
    box_free((*lib)->altmy);

    if((*lib)->bridgemap) {
        bridged_t *br;
        kh_foreach_value_ref((*lib)->bridgemap, br,
            box_free(br->name);
        );
        kh_destroy(bridgemap, (*lib)->bridgemap);
    }
    if((*lib)->symbolmap)
        kh_destroy(symbolmap, (*lib)->symbolmap);
    if((*lib)->wsymbolmap)
        kh_destroy(symbolmap, (*lib)->wsymbolmap);
    if((*lib)->datamap)
        kh_destroy(datamap, (*lib)->datamap);
    if((*lib)->wdatamap)
        kh_destroy(datamap, (*lib)->wdatamap);
    if((*lib)->mydatamap)
        kh_destroy(datamap, (*lib)->mydatamap);
    if((*lib)->mysymbolmap)
        kh_destroy(symbolmap, (*lib)->mysymbolmap);
    if((*lib)->wmysymbolmap)
        kh_destroy(symbolmap, (*lib)->wmysymbolmap);
    if((*lib)->stsymbolmap)
        kh_destroy(symbolmap, (*lib)->stsymbolmap);
    if((*lib)->symbol2map)
        kh_destroy(symbol2map, (*lib)->symbol2map);
    free_neededlib(&(*lib)->needed);
    free_neededlib(&(*lib)->depended);

    box_free(*lib);
    *lib = NULL;
}

char* GetNameLib(library_t *lib)
{
    return lib->name;
}
int IsSameLib(library_t* lib, const char* path)
{
    int ret = 0;
    if(!lib) 
        return 0;
    char* name = Path2Name(path);
    if(!strchr(path, '/') || lib->type==LIB_WRAPPED || !lib->path) {
        if(strcmp(name, lib->name)==0)
            ret=1;
    } else {
        char rpath[PATH_MAX];
        char* ptrRet = NULL;
        ptrRet = realpath(path, rpath);
        if(ptrRet != NULL)
            if(!strcmp(rpath, lib->path))
                ret=1;
    }
    if(!ret) {
        int n = NbDot(name);
        if(n>=0 && n<lib->nbdot)
            if(strncmp(name, lib->name, strlen(name))==0)
                ret=1;
    }

    box_free(name);
    return ret;
}
int GetLibSymbolStartEnd(library_t* lib, const char* name, khint_t pre_k, uintptr_t* start, uintptr_t* end, int version, const char* vername, int local)
{
    if(!name[0] || !lib->active)
        return 0;
    if(!pre_k) { pre_k = kh_str_hash_func(name); }
    khint_t k;
    // check first if already in the map
    k = pre_kh_get(bridgemap, lib->bridgemap, name, pre_k);
    if(k!=kh_end(lib->bridgemap)) {
        *start = kh_value(lib->bridgemap, k).start;
        *end = kh_value(lib->bridgemap, k).end;
        return 1;
    }
    // get a new symbol
    if(lib->get(lib, name, pre_k, start, end, version, vername, local)) {
        *end += *start;     // lib->get(...) gives size, not end
        char* symbol = box_strdup(name);
        int ret;
        k = kh_put(bridgemap, lib->bridgemap, symbol, &ret);
        kh_value(lib->bridgemap, k).name = symbol;
        kh_value(lib->bridgemap, k).start = *start;
        kh_value(lib->bridgemap, k).end = *end;
        return 1;
    }
    // nope
    return 0;
}
int GetLibNoWeakSymbolStartEnd(library_t* lib, const char* name, khint_t pre_k, uintptr_t* start, uintptr_t* end, int version, const char* vername, int local)
{
    if(!name[0] || !lib->active)
        return 0;
    khint_t k;
    // get a new symbol
    if(!pre_k) { pre_k = kh_str_hash_func(name); }
    if(lib->getnoweak(lib, name, pre_k, start, end, version, vername, local)) {
        *end += *start;     // lib->get(...) gives size, not end
        // check if already in the map
        k = pre_kh_get(bridgemap, lib->bridgemap, name, pre_k);
        if(k!=kh_end(lib->bridgemap)) {
            *start = kh_value(lib->bridgemap, k).start;
            *end = kh_value(lib->bridgemap, k).end;
            return 1;
        }
        char* symbol = box_strdup(name);
        int ret;
        k = kh_put(bridgemap, lib->bridgemap, symbol, &ret);
        kh_value(lib->bridgemap, k).name = symbol;
        kh_value(lib->bridgemap, k).start = *start;
        kh_value(lib->bridgemap, k).end = *end;
        return 1;
    }
    // nope
    return 0;
}
int GetLibLocalSymbolStartEnd(library_t* lib, const char* name, khint_t pre_k ,uintptr_t* start, uintptr_t* end, int version, const char* vername, int local)
{
    if(!name[0] || !lib->active)
        return 0;
    khint_t k;
    if(!pre_k) { pre_k = kh_str_hash_func(name); }
    // check first if already in the map
    k = pre_kh_get(bridgemap, lib->bridgemap, name, pre_k);
    if(k!=kh_end(lib->bridgemap)) {
        *start = kh_value(lib->bridgemap, k).start;
        *end = kh_value(lib->bridgemap, k).end;
        return 1;
    }
    // get a new symbol
    if(lib->getlocal(lib, name, pre_k, start, end, version, vername, local)) {
        *end += *start;     // lib->get(...) gives size, not end
        char* symbol = box_strdup(name);
        int ret;
        k = kh_put(bridgemap, lib->bridgemap, symbol, &ret);
        kh_value(lib->bridgemap, k).name = symbol;
        kh_value(lib->bridgemap, k).start = *start;
        kh_value(lib->bridgemap, k).end = *end;
        return 1;
    }
    // nope
    return 0;
}
int GetElfIndex(library_t* lib)
{
    if(!lib || lib->type!=LIB_EMULATED)
        return -1;
    return lib->priv.n.elf_index;
}

#define OPT_KHASH

static int getSymbolInDataMaps(library_t*lib, const char* name, khint_t pre_k, int noweak, uintptr_t *addr, uintptr_t *size)
{
    void* symbol;
#ifdef OPT_KHASH
    khint_t k = pre_kh_get(datamap, lib->datamap, name, pre_k);
#else
    khint_t k = kh_get(datamap, lib->datamap, name);
#endif
    if (k!=kh_end(lib->datamap)) {
        symbol = dlsym(lib->priv.w.lib, kh_key(lib->datamap, k));
        if(symbol) {
            // found!
            *addr = (uintptr_t)symbol;
            *size = kh_value(lib->datamap, k);
            return 1;
        }
    }
    if(!noweak) {
	#ifdef OPT_KHASH
        k = pre_kh_get(datamap, lib->wdatamap, name, pre_k);
	#else
	k = kh_get(datamap, lib->wdatamap, name);
	#endif
        if (k!=kh_end(lib->wdatamap)) {
            symbol = dlsym(lib->priv.w.lib, kh_key(lib->wdatamap, k));
            if(symbol) {
                // found!
                *addr = (uintptr_t)symbol;
                *size = kh_value(lib->wdatamap, k);
                return 1;
            }
        }
    }
    // check in mydatamap
#ifdef OPT_KHASH
    k = pre_kh_get(datamap, lib->mydatamap, name, pre_k);
#else
    k = kh_get(datamap, lib->mydatamap, name);
#endif
    if (k!=kh_end(lib->mydatamap)) {
        char buff[200];
        if(lib->altmy)
            strcpy(buff, lib->altmy);
        else
            strcpy(buff, "my_");
        strcat(buff, name);
        symbol = dlsym(lib->priv.w.box64lib, buff);
        if(!symbol)
            printf_log(LOG_NONE, "\033[31mWarning, data %s not found\033[m\n", buff);
        if(symbol) {
            // found!
            *addr = (uintptr_t)symbol;
            *size = kh_value(lib->mydatamap, k);
            return 1;
        }
    }
    return 0;
}
static int getSymbolInSymbolMaps(library_t*lib, const char* name, khint_t pre_k, int noweak, uintptr_t *addr, uintptr_t *size)
{
    void* symbol;
    // check in mysymbolmap
#ifdef OPT_KHASH
    khint_t k = pre_kh_get(symbolmap, lib->mysymbolmap, name, pre_k);
#else
    khint_t k = kh_get(symbolmap, lib->mysymbolmap, name);
#endif
    if (k!=kh_end(lib->mysymbolmap)) {
        char buff[200];
        if(lib->altmy)
            strcpy(buff, lib->altmy);
        else
            strcpy(buff, "my_");
        strcat(buff, name);
        symbol = dlsym(lib->priv.w.box64lib, buff);
        if(!symbol) {
            printf_log(LOG_NONE, "\033[31mWarning, function %s not found\033[m\n", buff);
            return 0;
        } else 
            AddOffsetSymbol(lib->context->maplib, symbol, name);
        *addr = AddBridge(lib->priv.w.bridge, kh_value(lib->mysymbolmap, k), symbol, 0, name);
        *size = sizeof(void*);
        return 1;
    }
    // check in stsymbolmap (return struct...)
#ifdef OPT_KHASH
    k = pre_kh_get(symbolmap, lib->stsymbolmap, name, pre_k);
#else
    k = kh_get(symbolmap, lib->stsymbolmap, name);
#endif
    if (k!=kh_end(lib->stsymbolmap)) {
        char buff[200];
        if(lib->altmy)
            strcpy(buff, lib->altmy);
        else
            strcpy(buff, "my_");
        strcat(buff, name);
        symbol = dlsym(lib->priv.w.box64lib, buff);
        if(!symbol) {
            printf_log(LOG_NONE, "\033[31mWarning, function %s not found\033[m\n", buff);
        } else 
            AddOffsetSymbol(lib->context->maplib, symbol, name);
        *addr = AddBridge(lib->priv.w.bridge, kh_value(lib->stsymbolmap, k), symbol, sizeof(void*), name);
        *size = sizeof(void*);
        return 1;
    }
    // check in symbolmap
#ifdef OPT_KHASH
    k = pre_kh_get(symbolmap, lib->symbolmap, name, pre_k);
#else
    k = kh_get(symbolmap, lib->symbolmap, name);
#endif
    if (k!=kh_end(lib->symbolmap)) {
        symbol = dlsym(lib->priv.w.lib, name);
        if(!symbol && lib->priv.w.altprefix) {
            char newname[200];
            strcpy(newname, lib->priv.w.altprefix);
            strcat(newname, name);
            symbol = dlsym(lib->priv.w.lib, newname);
        }
        if(!symbol)
            symbol = GetNativeSymbolUnversionned(lib->priv.w.lib, name);
        if(!symbol && lib->priv.w.altprefix) {
            char newname[200];
            strcpy(newname, lib->priv.w.altprefix);
            strcat(newname, name);
            symbol = GetNativeSymbolUnversionned(lib->priv.w.lib, newname);
        }
        if(!symbol) {
            printf_log(LOG_NONE, "\033[31mWarning, function %s not found in lib %s\033[m\n", name, lib->name);
            return 0;
        } else 
            AddOffsetSymbol(lib->context->maplib, symbol, name);
        *addr = AddBridge(lib->priv.w.bridge, kh_value(lib->symbolmap, k), symbol, 0, name);
        *size = sizeof(void*);
        return 1;
    }
    if(!noweak) {
        // check in wmysymbolmap
	#ifdef OPT_KHASH
	khint_t k = pre_kh_get(symbolmap, lib->wmysymbolmap, name, pre_k);
	#else
	khint_t k = kh_get(symbolmap, lib->wmysymbolmap, name);
	#endif
	if (k!=kh_end(lib->wmysymbolmap)) {
            char buff[200];
            if(lib->altmy)
                strcpy(buff, lib->altmy);
            else
                strcpy(buff, "my_");
            strcat(buff, name);
            symbol = dlsym(lib->priv.w.box64lib, buff);
            if(!symbol) {
                printf_log(LOG_NONE, "\033[31mWarning, function %s not found\033[m\n", buff);
            } else 
                AddOffsetSymbol(lib->context->maplib, symbol, name);
            *addr = AddBridge(lib->priv.w.bridge, kh_value(lib->wmysymbolmap, k), symbol, 0, name);
            *size = sizeof(void*);
            return 1;
        }
	#ifdef OPT_KHASH
	k = pre_kh_get(symbolmap, lib->wsymbolmap, name, pre_k);
	#else
	k = kh_get(symbolmap, lib->wsymbolmap, name);
	#endif
	if (k!=kh_end(lib->wsymbolmap)) {
            symbol = dlsym(lib->priv.w.lib, name);
            if(!symbol && lib->priv.w.altprefix) {
                char newname[200];
                strcpy(newname, lib->priv.w.altprefix);
                strcat(newname, name);
                symbol = dlsym(lib->priv.w.lib, newname);
            }
            if(!symbol)
                symbol = GetNativeSymbolUnversionned(lib->priv.w.lib, name);
            if(!symbol && lib->priv.w.altprefix) {
                char newname[200];
                strcpy(newname, lib->priv.w.altprefix);
                strcat(newname, name);
                symbol = GetNativeSymbolUnversionned(lib->priv.w.lib, newname);
            }
            if(!symbol) {
                printf_log(LOG_NONE, "\033[31mWarning, function %s not found in lib %s\033[m\n", name, lib->name);
                return 0;
            } else 
                AddOffsetSymbol(lib->context->maplib, symbol, name);
            *addr = AddBridge(lib->priv.w.bridge, kh_value(lib->wsymbolmap, k), symbol, 0, name);
            *size = sizeof(void*);
            return 1;
        }
    }
    // check in symbol2map
#ifdef OPT_KHASH
    k = pre_kh_get(symbol2map, lib->symbol2map, name, pre_k);
#else
    k = kh_get(symbol2map, lib->symbol2map, name);
#endif
    if (k!=kh_end(lib->symbol2map)) 
        if(!noweak || !kh_value(lib->symbol2map, k).weak)
        {
            symbol = dlsym(lib->priv.w.lib, kh_value(lib->symbol2map, k).name);
            if(!symbol)
                symbol = dlsym(RTLD_DEFAULT, kh_value(lib->symbol2map, k).name);    // search globaly maybe
            if(!symbol)
                symbol = GetNativeSymbolUnversionned(lib->priv.w.lib, kh_value(lib->symbol2map, k).name);
            if(!symbol) {
                printf_log(LOG_NONE, "\033[31mWarning, function %s not found in lib %s\033[m\n", kh_value(lib->symbol2map, k).name, lib->name);
                return 0;
            } else 
                AddOffsetSymbol(lib->context->maplib, symbol, name);
            *addr = AddBridge(lib->priv.w.bridge, kh_value(lib->symbol2map, k).w, symbol, 0, name);
            *size = sizeof(void*);
            return 1;
        }

    return 0;
}

int getSymbolInMaps(library_t *lib, const char* name, khint_t pre_k, int noweak, uintptr_t *addr, uintptr_t *size, int version, const char* vername, int local)
{
    if(!lib->active)
        return 0;
    if(version==-2) // don't send global native symbol for a version==-2 search
        return 0;
    if(!pre_k) { pre_k = kh_str_hash_func(name); }
    //check in datamaps (but no version, it's not handled there)
    if(getSymbolInDataMaps(lib, name, pre_k, noweak, addr, size))
        return 1;
    
    //khint_t k_ver = kh_str_hash_func(VersionnedName(name, version, vername));cal_count+=1;
    //if(getSymbolInSymbolMaps(lib, VersionnedName(name, version, vername), k_ver, noweak, addr, size))
    //    return 1;

    if(getSymbolInSymbolMaps(lib, name, pre_k, noweak, addr, size))
        return 1;
 
    return 0;
}

int GetNeededLibN(library_t* lib) {
    return lib->needed.size;
}
library_t* GetNeededLib(library_t* lib, int idx)
{
    if(idx<0 || idx>=lib->needed.size)
        return NULL;
    return lib->needed.libs[idx];
}
needed_libs_t* GetNeededLibs(library_t* lib)
{
    return &lib->needed;
}

void* GetHandle(library_t* lib)
{
    if(!lib)
        return NULL;
    if(lib->type!=LIB_WRAPPED)
        return NULL;
    return lib->priv.w.lib;
}

lib_t* GetMaplib(library_t* lib)
{
    if(!lib)
        return NULL;
    return lib->maplib;
}

linkmap_t* getLinkMapLib(library_t* lib)
{
    linkmap_t* lm = my_context->linkmap;
    while(lm) {
        if(lm->l_lib == lib)
            return lm;
        lm = lm->l_next;
    }
    return NULL;
}

linkmap_t* getLinkMapElf(elfheader_t* h)
{
    linkmap_t* lm = my_context->linkmap;
    while(lm) {
        if(lm->l_lib && lm->l_lib->type==LIB_EMULATED && my_context->elfs[lm->l_lib->priv.n.elf_index] == h)
            return lm;
        lm = lm->l_next;
    }
    return NULL;
}

linkmap_t* addLinkMapLib(library_t* lib)
{
    if(!my_context->linkmap) {
        my_context->linkmap = (linkmap_t*)box_calloc(1, sizeof(linkmap_t));
        my_context->linkmap->l_lib = lib;
        return my_context->linkmap;
    }
    linkmap_t* lm = my_context->linkmap;
    while(lm->l_next)
        lm = lm->l_next;
    lm->l_next = (linkmap_t*)box_calloc(1, sizeof(linkmap_t));
    lm->l_next->l_lib = lib;
    lm->l_next->l_prev = lm;
    return lm->l_next;
}
void removeLinkMapLib(library_t* lib)
{
    linkmap_t* lm = getLinkMapLib(lib);
    if(!lm) return;
    if(lm->l_next)
        lm->l_next->l_prev = lm->l_prev;
    if(lm->l_prev)
        lm->l_prev->l_next = lm->l_next;
    box_free(lm);
}

void AddMainElfToLinkmap(elfheader_t* elf)
{
    linkmap_t* lm = addLinkMapLib(NULL);    // main elf will have a null lib link

    lm->l_addr = (Elf64_Addr)GetElfDelta(elf);
    lm->l_name = my_context->fullpath;
    lm->l_ld = GetDynamicSection(elf);
}

void setNeededLibs(library_t* lib, int n, ...)
{
    if(lib->type!=LIB_WRAPPED && lib->type!=LIB_UNNKNOW)
        return;
    lib->priv.w.needed = n;
    lib->priv.w.neededlibs = (char**)box_calloc(n, sizeof(char*));
    va_list va;
    va_start (va, n);
    for (int i=0; i<n; ++i) {
        lib->priv.w.neededlibs[i] = box_strdup(va_arg(va, char*));
    }
    va_end (va);
}
