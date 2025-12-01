#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "elf.h"
#include <link.h>

#include "wrappedlibs.h"

#include "debug.h"
#include "wrapper.h"
#include "bridge.h"
#include "library_private.h"
#include "library.h"
#include "librarian.h"
#include "box64context.h"
#include "elfloader.h"
#include "elfloader_private.h"
#include "callback.h"
#include "myalign.h"
#include "fileutils.h"

#define FORWORDBACK 0
dlprivate_t *NewDLPrivate(void) {
    dlprivate_t* dl =  (dlprivate_t*)box_calloc(1, sizeof(dlprivate_t));
    return dl;
}
void FreeDLPrivate(dlprivate_t **lib) {
    box_free((*lib)->last_error);
    box_free(*lib);
}

void* my_dlopen(void *filename, int flag) EXPORT;
void* my_dlmopen(void* mlid, void *filename, int flag) EXPORT;
char* my_dlerror(void) EXPORT;
void* my_dlsym(void *handle, void *symbol) EXPORT;
int my_dlclose(void *handle) EXPORT;
int my_dladdr(void *addr, void *info) EXPORT;
int my_dladdr1(void *addr, void *info, void** extra_info, int flags) EXPORT;
void* my_dlvsym(void *handle, void *symbol, const char *vername) EXPORT;
int my_dlinfo(void* handle, int request, void* info) EXPORT;

#define LIBNAME libdl
const char* libdlName = "libdl.so.2";

#define CLEARERR    if(dl->last_error) box_free(dl->last_error); dl->last_error = NULL;
//#define R_RSP cpu->regs[R_ESP]
static void Push64(CPUX86State *cpu, uint64_t v)
{
    cpu->regs[R_ESP] -= 8;
    *((uint64_t*)cpu->regs[R_ESP]) = v;
}

int init_x86dlfun(void);
int init_x86dlfun(void)
{
    elfheader_t* h = loadElfFromFile("libdl.so.2");
    lsassert(h);
    const char* syms[] = {"dlopen", "dlsym", "dlclose", "dladdr", "dladdr1", "dlinfo"};
    void *rsyms[6] = {0};
    int rrsyms = 0;
    ResetSpecialCaseElf(h, syms, 6, rsyms, &rrsyms);
    if (rrsyms != 6) {
        h = loadElfFromFile("libc.so.6");
        ResetSpecialCaseElf(h, syms, 6, rsyms, &rrsyms);
    }
    lsassert(rrsyms == 6);
    my_context->dlprivate->x86dlopen = rsyms[0];
    my_context->dlprivate->x86dlsym = rsyms[1];
    my_context->dlprivate->x86dlclose = rsyms[2];
    my_context->dlprivate->x86dladdr = rsyms[3];
    my_context->dlprivate->x86dladdr1 = rsyms[4];
    my_context->dlprivate->x86dlinfo = rsyms[5];
    return 0;
}
static int callx86dlopen(void *filename, int flag, elfheader_t * h, int is_local) {
    struct link_map* ret = (struct link_map*)(uintptr_t)RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlopen, 2, filename, flag);
    if (ret) {
        printf_dlsym(LOG_DEBUG, "latx RunFunctionWithState dlopen %s addr %p\n", (char *)filename, (void *)ret->l_addr);
        h->lib->x86linkmap = ret;
    } else {
        //open error
        return -1;
    }
    h->delta = ret->l_addr;
    linkmap_t* lm = getLinkMapLib(h->lib);
    if (lm) {
        lm->l_addr = ret->l_addr;
    }
    h->latx_hasfix = 1;
    lib_t *maplib = (is_local)?h->lib->maplib:my_context->maplib;
    if(AddSymbolsLibrary(maplib, h->lib)) {   // also add needed libs
        printf_dlsym(LOG_INFO, "Failure to Add lib => fail\n");
        lsassert(0);
    }
    return 0;
}
static void LatxResetElf(elfheader_t * h)
{
    h->latx_hasfix = 0;
    h->had_RelocateElfPlt = 0;
    h->had_RelocateElf = 0;
    h->latx_type = 0;
    h->latx_hasfix = 0;
}
void* my_dlopen(void *filename, int flag){
    // TODO, handling special values for filename, like RTLD_SELF?
    // TODO, handling flags?
    library_t *lib = NULL;
    dlprivate_t *dl = my_context->dlprivate;
    size_t dlopened = 0;
    int is_local = (flag&0x100)?0:1;  // if not global, then local, and that means symbols are not put in the global "pot" for other libs
    CLEARERR
    if (!dl->x86dlopen) {
        init_x86dlfun();
        lsassert(dl->x86dlopen);
    }
    if(filename) {
        char* rfilename = (char*)alloca(MAX_PATH);
        strcpy(rfilename, (char*)filename);
        printf_dlsym(LOG_DEBUG, "Call to dlopen(\"%s\"/%p, %X)\n", rfilename, filename, flag);
        while(strstr(rfilename, "${ORIGIN}")) {
            char* origin = box_strdup(my_context->fullpath);
            char* p = strrchr(origin, '/');
            if(p) *p = '\0';    // remove file name to have only full path, without last '/'
            char* tmp = (char*)box_calloc(1, strlen(rfilename)-strlen("${ORIGIN}")+strlen(origin)+1);
            p = strstr(rfilename, "${ORIGIN}");
            memcpy(tmp, rfilename, p-rfilename);
            strcat(tmp, origin);
            strcat(tmp, p+strlen("${ORIGIN}"));
            strcpy(rfilename, tmp);
            box_free(tmp);
            box_free(origin);
        }
        while(strstr(rfilename, "${PLATFORM}")) {
            char* platform = box_strdup("x86_64");
            char* p = strrchr(platform, '/');
            if(p) *p = '\0';    // remove file name to have only full path, without last '/'
            char* tmp = (char*)box_calloc(1, strlen(rfilename)-strlen("${PLATFORM}")+strlen(platform)+1);
            p = strstr(rfilename, "${PLATFORM}");
            memcpy(tmp, rfilename, p-rfilename);
            strcat(tmp, platform);
            strcat(tmp, p+strlen("${PLATFORM}"));
            strcpy(rfilename, tmp);
            box_free(tmp);
            box_free(platform);
        }
        if (rfilename[0] == '/' && !FileExist(rfilename, IS_FILE)) {
            char filetmp[PATH_MAX] = {0};
            snprintf(filetmp , PATH_MAX, "%s%s", interp_prefix, rfilename);
            strcpy(rfilename, filetmp);
            printf_dlsym(LOG_DEBUG, "dlopen filename change to \"%s\"\n", rfilename);
        }
        // check if alread dlopenned...
        for (size_t i=0; i<dl->lib_sz; ++i) {
            if(IsSameLib(dl->libs[i], rfilename)) {
                if(dl->count[i]==0 && dl->dlopened[i]) {   // need to lauch init again!
                    int idx = GetElfIndex(dl->libs[i]);
                    if(idx!=-1) {
                        printf_dlsym(LOG_DEBUG, "dlopen: Recycling, calling Init for %p (%s)\n", (void*)(i+1), rfilename);
                        //TODO
                        if (IsEmuLib(dl->libs[i])) {
                            elfheader_t * h = my_context->elfs[idx];
                            lsassert(h);
                            LatxResetElf(h);
                            callx86dlopen(rfilename, flag, h, is_local);
                        }
                        ReloadLibrary(dl->libs[i]);    // reset memory image, redo reloc, run inits
                    }
                }
                if(!(flag&0x4))
                    dl->count[i] = dl->count[i]+1;
                printf_dlsym(LOG_DEBUG, "dlopen: Recycling %s/%p count=%ld (dlopened=%ld, elf_index=%d)\n", rfilename, (void*)(i+1), dl->count[i], dl->dlopened[i], GetElfIndex(dl->libs[i]));
                return (void*)(i+1);
            }
        }
        if(strstr(rfilename, "libGL.so")){
            strcpy(rfilename, "libGL.so.1");
        }
        dlopened = (GetLibInternal(rfilename)==NULL);
        // Then open the lib
        const char* libs[] = {rfilename};
        my_context->deferedInit = 1;
        int bindnow = (flag&0x2)?1:0;
        if (!FindLibIsWrapped(basename(rfilename))) {
#if FORWORDBACK
            lsassert(dl->x86dlopen);
            __MY_CPU;
            Push64(cpu, (uint64_t)dl->x86dlopen);
            printf_dlsym(LOG_DEBUG, "warning call x86dlopen filename is %s %x\n", (char *)filename, flag);
            return NULL;
#else
            uint64_t ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlopen, 2, filename, flag);
            printf_dlsym(LOG_DEBUG, "warning call call x86dlopen filename %s %x ret=0x%lx\n",  (char *)filename, flag, ret);
            //lsassert(0);
            if (ret) {
                return (void *)ret;
            }
            if(!dl->last_error)
                dl->last_error = box_malloc(129);
            snprintf(dl->last_error, 129, "filename \"%s\" flag=%x\n", (char *)filename, flag);
            printf_dlsym(LOG_NEVER, "%p return %p\n", dl->last_error, (void*)NULL);
            return NULL;
#endif
        }
        if(AddNeededLib(NULL, NULL, NULL, is_local, bindnow, libs, 1, my_context)) {
            printf_dlsym(strchr(rfilename,'/')?LOG_DEBUG:LOG_INFO, "Warning: Cannot dlopen(\"%s\"/%p, %X)\n", rfilename, filename, flag);
            if(!dl->last_error)
                dl->last_error = box_malloc(129);
            snprintf(dl->last_error, 129, "Cannot dlopen(\"%s\"/%p, %X)\n", rfilename, filename, flag);
            return NULL;
        }
        lib = GetLibInternal(rfilename);
        if (!lib) return NULL;
        lib->x86dlopenflag = flag;
        if (lib && lib->type == LIB_EMULATED) {
            // if dlopened = 0 ---> lib added but not loaded
            int libidx = GetElfIndex(lib);
            lsassert(libidx >= 0);
            elfheader_t * h = my_context->elfs[libidx];
            lsassert(h);
            if (!h->latx_hasfix || !lib->x86linkmap) {//lib->x86linkmap is null ---- this lib has been needed by other elf and opened 
                callx86dlopen(rfilename, flag, h, is_local);
            }
        }
        //TODO:RunDeferedElfInit;
    } else {
        // check if already dlopenned...
        for (size_t i=0; i<dl->lib_sz; ++i) {
            if(!dl->libs[i]) {
                dl->count[i] = dl->count[i]+1;
                return (void*)(i+1);
            }
        }
        printf_dlsym(LOG_DEBUG, "Call to dlopen(NULL, %X) forword call x86dlopen \n", flag);
        lsassert(dl->x86dlopen);
        __MY_CPU;
        Push64(cpu, (uint64_t)dl->x86dlopen);
        return NULL;
    }
    //get the lib and add it to the collection

    if(dl->lib_sz == dl->lib_cap) {
        dl->lib_cap += 4;
        dl->libs = (library_t**)box_realloc(dl->libs, sizeof(library_t*)*dl->lib_cap);
        dl->count = (size_t*)box_realloc(dl->count, sizeof(size_t)*dl->lib_cap);
        dl->dlopened = (size_t*)box_realloc(dl->dlopened, sizeof(size_t)*dl->lib_cap);
        // memset count...
        memset(dl->count+dl->lib_sz, 0, (dl->lib_cap-dl->lib_sz)*sizeof(size_t));
    }
    intptr_t idx = dl->lib_sz++;
    dl->libs[idx] = lib;
    dl->count[idx] = dl->count[idx]+1;
    dl->dlopened[idx] = dlopened;
    printf_dlsym(LOG_DEBUG, "dlopen: New handle %p (%s), dlopened=%ld\n", (void*)(idx+1), (char*)filename, dlopened);
    if (lib && lib->type == LIB_EMULATED) {
        return lib->x86linkmap;
    }
    return (void*)(idx+1);
}

void* my_dlmopen(void* lmid, void *filename, int flag)
{
    if(lmid) {
        printf_dlsym(LOG_INFO, "Warning, dlmopen(%p, %p(\"%s\"), 0x%x) called with lmid not LMID_ID_BASE (unsupported)\n", lmid, filename, filename?(char*)filename:"self", flag);
    }
    // lmid is ignored for now...
    return my_dlopen(filename, flag);
}

KHASH_SET_INIT_INT(libs);

static int recursive_dlsym_lib(kh_libs_t* collection, library_t* lib, const char* rsymbol, uintptr_t *start, uintptr_t *end, int version, const char* vername)
{
    if(!lib)
        return 0;
    khint_t k = kh_get(libs, collection, (uintptr_t)lib);
    if(k != kh_end(collection))
        return 0;
    int ret;
    kh_put(libs, collection, (uintptr_t)lib, &ret);
    // look in the library itself
    khint_t pre_k = kh_str_hash_func(rsymbol);
    if(lib->get(lib, rsymbol, pre_k, start, end, version, vername, 1))
        return 1;
    // look in other libs
    int n = GetNeededLibN(lib);
    for (int i=0; i<n; ++i) {
        library_t *l = GetNeededLib(lib, i);
        if(recursive_dlsym_lib(collection, l, rsymbol, start, end, version, vername))
            return 1;
    }

    return 0;
}

static int my_dlsym_lib(library_t* lib, const char* rsymbol, uintptr_t *start, uintptr_t *end, int version, const char* vername)
{
    kh_libs_t *collection = kh_init(libs);
    int ret = recursive_dlsym_lib(collection, lib, rsymbol, start, end, version, vername);
    kh_destroy(libs, collection);

    return ret;
}

void* my_dlsym(void *handle, void *symbol){
    dlprivate_t *dl = my_context->dlprivate;
    uintptr_t start = 0, end = 0;
    char* rsymbol = (char*)symbol;
    CLEARERR
    if (!dl->x86dlsym) {
        init_x86dlfun();
        lsassert(dl->x86dlsym);
    }
    printf_dlsym(LOG_DEBUG, "Call to dlsym(%p, \"%s\")%s\n", handle, rsymbol, dlsym_error?"":"\n");
   //lsassert(!strstr(rsymbol, "XcursorGetDefaultSize"));
    if(handle==NULL) {
        // special case, look globably
#ifdef LATX_RELOCATION_SAVE_SYMBOLS
        if(GetGlobalSymbolStartEnd(my_context->maplib, rsymbol, &start, &end, NULL, -1, NULL)) {
            printf_dlsym(LOG_NEVER, "%p\n", (void*)start);
            return (void*)start;
        }
#endif
#if 0
        lsassert(dl->x86dlsym);
        __MY_CPU;
        Push64(cpu, (uint64_t)dl->x86dlsym);
        printf_dlsym(LOG_DEBUG, "warning call x86dlsym filename is NULL\n");
        return NULL;
#else
        uint64_t ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlsym, 2, handle, symbol);
        printf_dlsym(LOG_DEBUG, "warning call x86dlsym filename is NULL ret=0x%lx\n", ret);
        if (ret) {
            return (void *)ret;
        } else {
            if(GetGlobalSymbolStartEnd(my_context->maplib, rsymbol, &start, &end, NULL, -1, NULL)) {
                printf_dlsym(LOG_NEVER, "%p\n", (void*)start);
                return (void*)start;
            }
            printf_dlsym(LOG_NEVER, "debug my %d\n", __LINE__);
        }
        if(!dl->last_error)
            dl->last_error = box_malloc(129);
        snprintf(dl->last_error, 129, "Symbol \"%s\" not found in %p)\n", rsymbol, handle);
        printf_dlsym(LOG_NEVER, "%p return %p\n", dl->last_error, (void*)NULL);
        return NULL;
#endif
    }
    if(handle==(void*)~0LL) {
        // special case (RTLD_NEXT) -- call x86dlsym
        lsassert(dl->x86dlsym);
        __MY_CPU;
        Push64(cpu, (uint64_t)dl->x86dlsym);
        printf_dlsym(LOG_DEBUG, "warning call x86dlsym filename is RTLD_NEXT\n");
        return NULL;
    }
    size_t nlib = (size_t)handle;
    if(nlib > dl->lib_sz) {
        for (int i = 0; i < dl->lib_sz; i++) {
            if (dl->libs[i] && dl->libs[i]->active && dl->libs[i]->type == LIB_EMULATED && ((size_t)dl->libs[i]->x86linkmap) == nlib) {
                nlib = i + 1;
                break;
            } 
        }
    }
    --nlib;
    // size_t is unsigned
    if(nlib>=dl->lib_sz) {
#ifdef LATX_RELOCATION_SAVE_SYMBOLS
        if(GetGlobalSymbolStartEnd(my_context->maplib, rsymbol, &start, &end, NULL, -1, NULL)) {
            printf_dlsym(LOG_NEVER, "%p\n", (void*)start);
            return (void*)start;
        }
#endif
        const char* lmfile = ((struct link_map *)handle)->l_name;
        if (strlen(lmfile)) {
            const char* libs[] = {basename(lmfile)};
            //try to wrapper.
            int iswrapped = 0.;
            if (FindLibIsWrapped((char *)libs[0])) {
                //if file is wrapped.
                iswrapped = 1;
                printf_dlsym(LOG_DEBUG, "find lib \"%s\" shuold be wrapped. init it.\n", libs[0]);
                if(AddNeededLib(NULL, NULL, NULL, 0, 1, libs, 1, my_context)) {
                    printf_dlsym(LOG_DEBUG, "Warning: Cannot AddNeededLib(\"%s\")\n", libs[0]);
                }
                printf_dlsym(LOG_DEBUG, "info: success AddNeededLib(\"%s\")\n", libs[0]);
                if(GetGlobalSymbolStartEnd(my_context->maplib, rsymbol, &start, &end, NULL, -1, NULL)) {
                    printf_dlsym(LOG_NEVER, "%p\n", (void*)start);
                    return (void*)start;
                }
            }
            if (iswrapped) {
                //Perhaps exe want to test func for earch libs, return nil.
                printf_dlsym(LOG_NEVER, "%p\n", (void*)NULL);
                return NULL;
            }
        }
#if !defined(LATX_RELOCATION_SAVE_SYMBOLS)
        else {//dlopen(NULL) --- dlopen self maplink filename is "NULL".
                if(GetGlobalSymbolStartEnd(my_context->maplib, rsymbol, &start, &end, NULL, -1, NULL)) {
                    printf_dlsym(LOG_NEVER, "%p\n", (void*)start);
                    return (void*)start;
                }
        }
#endif
        __MY_CPU;
#if FORWORDBACK
        lsassert(dl->x86dlsym);
        Push64(cpu, (uint64_t)dl->x86dlsym);
        printf_dlsym(LOG_DEBUG, "warning call x86dlsym filename is %s 0x%lx %s\n", strlen(lmfile)?lmfile:"NULL", cpu->regs[R_EDI], (char*)symbol);
        return NULL;
#else
        uint64_t ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlsym, 2, cpu->regs[R_EDI], symbol);
        printf_dlsym(LOG_DEBUG, "warning call call x86dlsym filename is %s handle 0x%lx ret=0x%lx\n", strlen(lmfile)?lmfile:"NULL", cpu->regs[R_EDI], ret);
        if (ret) {
            return (void *)ret;
        }
        if(!dl->last_error)
            dl->last_error = box_malloc(129);
        snprintf(dl->last_error, 129, "Symbol \"%s\" not found in %p)\n", rsymbol, handle);
        printf_dlsym(LOG_NEVER, "%p return %p\n", dl->last_error, (void*)NULL);
        return NULL;
#endif
    }
    if(dl->count[nlib]==0) {
        if(!dl->last_error)
            dl->last_error = box_malloc(129);
        snprintf(dl->last_error, 129, "Bad handle %p (already closed))\n", handle);
        printf_dlsym(LOG_NEVER, "%p return %p\n", dl->last_error, (void*)NULL);
        return NULL;
    }
    if(dl->libs[nlib]) {
        if(my_dlsym_lib(dl->libs[nlib], rsymbol, &start, &end, -1, NULL)==0) {
            // not found
            __MY_CPU;
            #if 1
            if(!dl->libs[nlib]->x86linkmap) {
                //redlopen
                uint64_t ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlopen, 2, dl->libs[nlib]->name, dl->libs[nlib]->x86dlopenflag);
                if (!ret) {//user sometime test for finding a func.
                    printf_dlsym(LOG_NEVER, "redlopen %p return %p\n", rsymbol, (void*)NULL);
                    return NULL;
                }
                lsassert(ret);
                dl->libs[nlib]->x86linkmap = (void *)ret;
                ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlsym, 2, dl->libs[nlib]->x86linkmap , cpu->regs[R_ESI]);
                printf_dlsym(LOG_DEBUG, "call x86dlsym filename %s is wrapped but not find symbol, dlsym(%p, %s) ret=0x%lx\n",
                dl->libs[nlib]->name, dl->libs[nlib]->x86linkmap, (char *)cpu->regs[R_ESI], ret);
                return (void *)ret;
            }
            #endif
            lsassert(dl->x86dlsym);
            if (dl->libs[nlib]->x86linkmap != handle) {
                cpu->regs[R_EDI] = (uintptr_t)dl->libs[nlib]->x86linkmap;
            }
#if FORWORDBACK
            Push64(cpu, (uint64_t)dl->x86dlsym);
            printf_dlsym(LOG_DEBUG, "warning call x86dlsym filename is %s %lx\n", dl->libs[nlib]->x86linkmap->l_name, cpu->regs[R_EDI]);
            return NULL;
#else
            uint64_t ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlsym, 2, cpu->regs[R_EDI], cpu->regs[R_ESI]);
            printf_dlsym(LOG_DEBUG, "call x86dlsym filename is %s %s ret=0x%lx\n", dl->libs[nlib]->x86linkmap->l_name, (char *)cpu->regs[R_ESI], ret);
            if (ret) {
                return (void *)ret;
            }
            if(!dl->last_error)
                dl->last_error = box_malloc(129);
            snprintf(dl->last_error, 129, "Symbol \"%s\" not found in %p)\n", rsymbol, handle);
            printf_dlsym(LOG_NEVER, "%p return %p\n", dl->last_error, (void*)NULL);
            return NULL;
#endif
        }
    } else {
        // still usefull?
        //  => look globably
#ifdef LATX_RELOCATION_SAVE_SYMBOLS
        if(GetGlobalSymbolStartEnd(my_context->maplib, rsymbol, &start, &end, NULL, -1, NULL)) {
            printf_dlsym(LOG_NEVER, "%p\n", (void*)start);
            return (void*)start;
        }
#endif
        if(!dl->last_error)
            dl->last_error = box_malloc(129);
        snprintf(dl->last_error, 129, "Symbol \"%s\" not found in %p)\n", rsymbol, handle);
        printf_dlsym(LOG_NEVER, "%p\n", NULL);
        lsassertm(0,"%s",dl->last_error);
        return NULL;
    }
    printf_dlsym(LOG_NEVER, "%p\n", (void*)start);
    return (void*)start;
}

int my_dlclose(void *handle)
{
    printf_dlsym(LOG_DEBUG, "Call to dlclose(%p)\n", handle);
    dlprivate_t *dl = my_context->dlprivate;
    CLEARERR
    if (!dl->x86dlclose) {
        init_x86dlfun();
        lsassert(dl->x86dlclose);
    }
    size_t nlib = (size_t)handle;
    if(nlib > dl->lib_sz) {
        for (int i = 0; i < dl->lib_sz; i++) {
            if (dl->libs[i] && dl->libs[i]->active && dl->libs[i]->type == LIB_EMULATED && ((size_t)dl->libs[i]->x86linkmap) == nlib) {
                nlib = i + 1;
                break;
            } 
        }
    }
    --nlib;
    // size_t is unsigned
    if(nlib>=dl->lib_sz) {
        int ret = -1;
        if (dl->x86dlclose) {
            __MY_CPU;
            Push64(cpu, (uint64_t)dl->x86dlclose);
            return 0;
        }
        if(!dl->last_error)
            dl->last_error = box_malloc(129);
        snprintf(dl->last_error, 129, "Bad handle %p, ret = %d)\n", handle, ret);
        printf_dlsym(LOG_DEBUG, "dlclose: %s\n", dl->last_error);
        lsassertm(0,"%s",dl->last_error);
        return -1;
    }
    if(dl->count[nlib]==0) {
        if(!dl->last_error)
            dl->last_error = box_malloc(129);
        snprintf(dl->last_error, 129, "Bad handle %p (already closed))\n", handle);
        printf_dlsym(LOG_DEBUG, "dlclose: %s\n", dl->last_error);
        return -1;
    }
    dl->count[nlib] = dl->count[nlib]-1;
    if(dl->count[nlib]==0 && dl->dlopened[nlib]) {   // need to call Fini...
        int idx = GetElfIndex(dl->libs[nlib]);
        if(idx!=-1) {
            printf_dlsym(LOG_DEBUG, "dlclose: Call to Fini for %p\n", handle);
            InactiveLibrary(dl->libs[nlib]);
            if (dl->x86dlclose) {
                __MY_CPU;
                if (dl->libs[nlib]->x86linkmap != handle) {
                    cpu->regs[R_EDI] = (uintptr_t)dl->libs[nlib]->x86linkmap;
                }
                Push64(cpu, (uint64_t)dl->x86dlclose);
                return 0;
            }
        }
    }
    return 0;
}

char* my_dlerror(void)
{
    dlprivate_t *dl = my_context->dlprivate;
    return dl->last_error;
}

int my_dladdr1(void *addr, void *i, void** extra_info, int flags)
{
    //int dladdr(void *addr, Dl_info *info);
    dlprivate_t *dl = my_context->dlprivate;
    CLEARERR
    if (!dl->x86dladdr1) {
        init_x86dlfun();
        lsassert(dl->x86dladdr1);
    }
    Dl_info *info = (Dl_info*)i;
    printf_dlsym(LOG_DEBUG, "Warning: partially unimplement call to dladdr/dladdr1(%p, %p, %p, %d)\n", addr, info, extra_info, flags);
     __MY_CPU;
    uint64_t ret = 0;
    if (extra_info == NULL && flags == 0) {
        ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dladdr, 2, cpu->regs[R_EDI], cpu->regs[R_ESI]);
    } else {
        ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dladdr1, 4, cpu->regs[R_EDI], cpu->regs[R_ESI], cpu->regs[R_EDX], cpu->regs[R_ECX]);
    }
    printf_dlsym(LOG_DEBUG, "     call to x86dladdr1 return saddr=%p, fname=\"%s\", sname=\"%s\" ret=%ld\n", info->dli_saddr, info->dli_sname?info->dli_sname:"", info->dli_fname?info->dli_fname:"", ret);
    if (ret == 1) {
        return ret;
    }
    //emu->quit = 1;
    library_t* lib = NULL;
    info->dli_saddr = NULL;
    info->dli_fname = NULL;
    info->dli_sname = FindSymbolName(my_context->maplib, addr, &info->dli_saddr, NULL, &info->dli_fname, &info->dli_fbase, &lib);
    printf_dlsym(LOG_DEBUG, "     dladdr return saddr=%p, fname=\"%s\", sname=\"%s\"\n", info->dli_saddr, info->dli_sname?info->dli_sname:"", info->dli_fname?info->dli_fname:"");
    if(flags==RTLD_DL_SYMENT) {
        printf_dlsym(LOG_INFO, "Warning, unimplement call to dladdr1 with RTLD_DL_SYMENT flags\n");
    } else if (flags==RTLD_DL_LINKMAP) {
        printf_dlsym(LOG_INFO, "Warning, partially unimplemented call to dladdr1 with RTLD_DL_LINKMAP flags\n");
        *(linkmap_t**)extra_info = getLinkMapLib(lib);
    }
    return (info->dli_sname)?1:0;   // success is non-null here...
}
int my_dladdr(void *addr, void *i)
{
    dlprivate_t *dl = my_context->dlprivate;
    CLEARERR
    if (!dl->x86dladdr) {
        init_x86dlfun();
        lsassert(dl->x86dladdr);
    }
#ifdef CONFIG_LATX_DEBUG
    Dl_info *info = (Dl_info*)i;
#endif
    printf_dlsym(LOG_DEBUG, "Warning: partially unimplement call to dladdr(%p, %p)\n", addr, info);
     __MY_CPU;
    uint64_t ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dladdr, 2, cpu->regs[R_EDI], cpu->regs[R_ESI]);
    printf_dlsym(LOG_DEBUG, "     call to x86dladdr return saddr=%p, fname=\"%s\", sname=\"%s\" ret=%ld\n", info->dli_saddr, info->dli_sname?info->dli_sname:"", info->dli_fname?info->dli_fname:"", ret);
    if (ret == 1) {
        return ret;
    }
    return my_dladdr1(addr, i, NULL, 0);
}
void* my_dlvsym(void *handle, void *symbol, const char *vername)
{
    printf_dlsym(LOG_DEBUG, "Call to dlvsym(%p, \"%s\", %s)", handle, (char *)symbol, vername?vername:"(nil)");
    return my_dlsym(handle, symbol);
}

int my_dlinfo(void* handle, int request, void* info)
{
    printf_dlsym(LOG_DEBUG, "Call to dlinfo(%p, %d, %p)\n", handle, request, info);
    dlprivate_t *dl = my_context->dlprivate;
    CLEARERR
    if (!dl->x86dlinfo) {
        init_x86dlfun();
        lsassert(dl->x86dlinfo);
    }
    __MY_CPU;
    uint64_t ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlinfo, 3, cpu->regs[R_EDI], cpu->regs[R_ESI], cpu->regs[R_EDX]);
    return ret;
}

#include "wrappedlib_init.h"

