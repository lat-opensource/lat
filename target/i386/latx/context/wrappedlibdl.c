/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#include "config-host.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
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
#include "x86dlfun.h"

#ifndef CONFIG_LOONGARCH_NEW_WORLD
#define LIBNAME libdl
const char *libdlName = "libdl.so.2";
#endif

#define FORWORDBACK 0
dlprivate_t *NewDLPrivate(void) {
    dlprivate_t* dl =  (dlprivate_t*)box_calloc(1, sizeof(dlprivate_t));
    return dl;
}
void FreeDLPrivate(dlprivate_t **lib) {
    box_free(*lib);
}

static __thread char dl_error_buffer[512];
static __thread int dl_error_pending;

static void clear_dl_error(dlprivate_t *dl)
{
    if (dl && dl->x86dlerror)
        (void)RunFunctionWithState((uintptr_t)dl->x86dlerror, 0);
    dl_error_pending = 0;
}

static void set_dl_error(dlprivate_t *dl, const char *message)
{
    (void)dl;
    snprintf(dl_error_buffer, sizeof(dl_error_buffer), "%s", message);
    dl_error_pending = 1;
}

static void set_dl_errorf(dlprivate_t *dl, const char *format, ...)
{
    char message[512];
    va_list args;

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    set_dl_error(dl, message);
}

#define CLEARERR clear_dl_error(dl);

static int replace_path_token(char **path, const char *token,
                              const char *replacement)
{
    const size_t token_len = strlen(token);
    const size_t replacement_len = strlen(replacement);
    size_t search_from = 0;
    char *match;

    while ((match = strstr(*path + search_from, token))) {
        const size_t prefix_len = (size_t)(match - *path);
        const size_t suffix_len = strlen(match + token_len);
        size_t expanded_len;
        char *expanded;

        if (suffix_len == SIZE_MAX ||
            prefix_len > SIZE_MAX - replacement_len ||
            prefix_len + replacement_len > SIZE_MAX - suffix_len - 1)
            return -1;
        expanded_len = prefix_len + replacement_len + suffix_len + 1;
        expanded = box_malloc(expanded_len);
        if (!expanded)
            return -1;
        memcpy(expanded, *path, prefix_len);
        memcpy(expanded + prefix_len, replacement, replacement_len);
        memcpy(expanded + prefix_len + replacement_len,
               match + token_len, suffix_len + 1);
        box_free(*path);
        *path = expanded;
        search_from = prefix_len + replacement_len;
    }
    return 0;
}

static char *expand_dlopen_path(const char *filename)
{
    char *path = box_strdup(filename);
    char *origin;
    char *slash;

    if (!path)
        return NULL;
    origin = box_strdup(my_context->fullpath ? my_context->fullpath : "");
    if (!origin) {
        box_free(path);
        return NULL;
    }
    slash = strrchr(origin, '/');

    if (slash)
        *slash = '\0';
    else
        origin[0] = '\0';
    if (replace_path_token(&path, "${ORIGIN}", origin) ||
        replace_path_token(&path, "${PLATFORM}", "x86_64")) {
        box_free(path);
        path = NULL;
    }
    box_free(origin);
    return path;
}
//#define R_RSP cpu->regs[R_ESP]
static void Push64(CPUX86State *cpu, uint64_t v)
{
    cpu->regs[R_ESP] -= 8;
    *((uint64_t*)cpu->regs[R_ESP]) = v;
}

#ifdef CONFIG_LOONGARCH_NEW_WORLD
void kzt_wine_init_x86(void);
#endif

static int init_x86dlfun(void)
{
#ifdef CONFIG_LOONGARCH_NEW_WORLD
    init_x86dlfun_from("libc.so.6", "libdl.so.2");
    kzt_wine_init_x86();
    return 0;
#else
    return init_x86dlfun_from("libdl.so.2", "libc.so.6");
#endif
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
EXPORT void* my_dlopen(void *filename, int flag){
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
        char* rfilename = expand_dlopen_path((char*)filename);
        if (!rfilename) {
            set_dl_error(dl, "Cannot expand dlopen path");
            return NULL;
        }
        printf_dlsym(LOG_DEBUG, "Call to dlopen(\"%s\"/%p, %X)\n", rfilename, filename, flag);
        if (rfilename[0] == '/' && !FileExist(rfilename, IS_FILE)) {
            const size_t interp_len = strlen(interp_prefix);
            const size_t filename_len = strlen(rfilename);
            if (filename_len == SIZE_MAX ||
                interp_len > SIZE_MAX - filename_len - 1) {
                box_free(rfilename);
                set_dl_error(dl, "Cannot prefix dlopen path");
                return NULL;
            }
            char *prefixed = box_malloc(interp_len + filename_len + 1);
            if (!prefixed) {
                box_free(rfilename);
                set_dl_error(dl, "Cannot prefix dlopen path");
                return NULL;
            }
            strcpy(prefixed, interp_prefix);
            strcat(prefixed, rfilename);
            box_free(rfilename);
            rfilename = prefixed;
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
                box_free(rfilename);
                return (void*)(i+1);
            }
        }
        if(strstr(rfilename, "libGL.so")){
            box_free(rfilename);
            rfilename = box_strdup("libGL.so.1");
            if (!rfilename) {
                set_dl_error(dl, "Cannot rewrite dlopen path");
                return NULL;
            }
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
                box_free(rfilename);
                return (void *)ret;
            }
            set_dl_errorf(dl, "filename \"%s\" flag=%x\n",
                          (char *)filename, flag);
            box_free(rfilename);
            return NULL;
#endif
        }
        if(AddNeededLib(NULL, NULL, NULL, is_local, bindnow, libs, 1, my_context)) {
            printf_dlsym(strchr(rfilename,'/')?LOG_DEBUG:LOG_INFO, "Warning: Cannot dlopen(\"%s\"/%p, %X)\n", rfilename, filename, flag);
            set_dl_errorf(dl, "Cannot dlopen(\"%s\"/%p, %X)\n",
                          rfilename, filename, flag);
            box_free(rfilename);
            return NULL;
        }
        lib = GetLibInternal(rfilename);
        if (!lib) {
            box_free(rfilename);
            return NULL;
        }
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
        box_free(rfilename);
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

EXPORT void* my_dlmopen(void* lmid, void *filename, int flag)
{
    dlprivate_t *dl = my_context->dlprivate;

    if ((Lmid_t)lmid != LM_ID_BASE) {
        char error[160];
        snprintf(error, sizeof(error),
                 "dlmopen namespace %p is unsupported", lmid);
        set_dl_error(dl, error);
        printf_dlsym(LOG_INFO,
                     "Warning, dlmopen(%p, %p(\"%s\"), 0x%x) rejected: unsupported namespace\n",
                     lmid, filename, filename ? (char*)filename : "self", flag);
        return NULL;
    }
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

static int find_dl_library_index(dlprivate_t *dl, void *handle, size_t *index)
{
    const size_t raw_handle = (size_t)handle;

    if (raw_handle > 0 && raw_handle <= dl->lib_sz) {
        *index = raw_handle - 1;
        return 1;
    }
    for (size_t i = 0; i < dl->lib_sz; ++i) {
        if (dl->libs[i] && dl->libs[i]->x86linkmap == handle) {
            *index = i;
            return 1;
        }
    }
    return 0;
}

EXPORT void* my_dlsym(void *handle, void *symbol){
    dlprivate_t *dl = my_context->dlprivate;
    uintptr_t start = 0, end = 0;
    char* rsymbol = (char*)symbol;
    CLEARERR
    if (!dl->x86dlsym) {
        init_x86dlfun();
        lsassert(dl->x86dlsym);
    }
    printf_dlsym(LOG_DEBUG, "Call to dlsym(%p, \"%s\")%s\n", handle, rsymbol, dlsym_error?"":"\n");
    if (handle && handle != (void*)~0LL) {
        size_t known_index;
        if (!find_dl_library_index(dl, handle, &known_index)) {
            uint64_t ret = RunFunctionWithState(
                (uintptr_t)dl->x86dlsym, 2, handle, symbol);
            if (!ret)
                set_dl_errorf(dl, "Symbol \"%s\" not found in %p\n",
                              rsymbol, handle);
            return (void*)ret;
        }
    }
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
        set_dl_errorf(dl, "Symbol \"%s\" not found in %p)\n", rsymbol,
                      handle);
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
#if FORWORDBACK
        __MY_CPU;
        lsassert(dl->x86dlsym);
        Push64(cpu, (uint64_t)dl->x86dlsym);
        printf_dlsym(LOG_DEBUG, "warning call x86dlsym filename is %s 0x%lx %s\n", strlen(lmfile)?lmfile:"NULL", cpu->regs[R_EDI], (char*)symbol);
        return NULL;
#else
        uint64_t ret = RunFunctionWithState(
            (uintptr_t)my_context->dlprivate->x86dlsym, 2, handle,
            symbol);
        printf_dlsym(LOG_DEBUG, "warning call call x86dlsym filename is %s handle %p ret=0x%lx\n", strlen(lmfile)?lmfile:"NULL", handle, ret);
        if (ret) {
            return (void *)ret;
        }
        set_dl_errorf(dl, "Symbol \"%s\" not found in %p)\n", rsymbol,
                      handle);
        return NULL;
#endif
    }
    if(dl->count[nlib]==0) {
        set_dl_errorf(dl, "Bad handle %p (already closed))\n", handle);
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
                ret = RunFunctionWithState(
                    (uintptr_t)my_context->dlprivate->x86dlsym, 2,
                    dl->libs[nlib]->x86linkmap, symbol);
                printf_dlsym(LOG_DEBUG, "call x86dlsym filename %s is wrapped but not find symbol, dlsym(%p, %s) ret=0x%lx\n",
                dl->libs[nlib]->name, dl->libs[nlib]->x86linkmap, (char *)symbol, ret);
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
            uint64_t ret = RunFunctionWithState(
                (uintptr_t)my_context->dlprivate->x86dlsym, 2,
                dl->libs[nlib]->x86linkmap, symbol);
            printf_dlsym(LOG_DEBUG, "call x86dlsym filename is %s %s ret=0x%lx\n", dl->libs[nlib]->x86linkmap->l_name, (char *)symbol, ret);
            if (ret) {
                return (void *)ret;
            }
            set_dl_errorf(dl, "Symbol \"%s\" not found in %p)\n", rsymbol,
                          handle);
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
        set_dl_errorf(dl, "Symbol \"%s\" not found in %p)\n", rsymbol,
                      handle);
        printf_dlsym(LOG_NEVER, "%p\n", NULL);
        return NULL;
    }
    printf_dlsym(LOG_NEVER, "%p\n", (void*)start);
    return (void*)start;
}

EXPORT int my_dlclose(void *handle)
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
        set_dl_errorf(dl, "Bad handle %p, ret = %d)\n", handle, ret);
        return -1;
    }
    if(dl->count[nlib]==0) {
        set_dl_errorf(dl, "Bad handle %p (already closed))\n", handle);
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

EXPORT char* my_dlerror(void)
{
    dlprivate_t *dl = my_context->dlprivate;

    if (!dl->x86dlerror)
        init_x86dlfun();
    if (dl_error_pending) {
        if (dl->x86dlerror)
            (void)RunFunctionWithState((uintptr_t)dl->x86dlerror, 0);
        dl_error_pending = 0;
        return dl_error_buffer;
    }
    if (!dl->x86dlerror)
        return NULL;
    return (char*)(uintptr_t)RunFunctionWithState(
        (uintptr_t)dl->x86dlerror, 0);
}

EXPORT int my_dladdr1(void *addr, void *i, void** extra_info, int flags)
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
EXPORT int my_dladdr(void *addr, void *i)
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
EXPORT void* my_dlvsym(void *handle, void *symbol, const char *vername)
{
    printf_dlsym(LOG_DEBUG, "Call to dlvsym(%p, \"%s\", %s)", handle, (char *)symbol, vername?vername:"(nil)");
    dlprivate_t *dl = my_context->dlprivate;
    size_t nlib;
    void *guest_handle = handle;

    clear_dl_error(dl);
    if (!dl->x86dlvsym)
        init_x86dlfun();
    if (!dl->x86dlvsym) {
        set_dl_error(dl, "dlvsym is unavailable in the guest loader");
        return NULL;
    }
    if (handle == (void*)~0LL) {
        __MY_CPU;
        Push64(cpu, (uint64_t)dl->x86dlvsym);
        return NULL;
    }
    if (!handle)
        return (void*)(uintptr_t)RunFunctionWithState(
            (uintptr_t)dl->x86dlvsym, 3, guest_handle, symbol, vername);
    if (find_dl_library_index(dl, handle, &nlib)) {
        if (!dl->count[nlib]) {
            set_dl_errorf(dl, "Bad handle %p (already closed)\n", handle);
            return NULL;
        }
        if (!dl->libs[nlib]) {
            return (void*)(uintptr_t)RunFunctionWithState(
                (uintptr_t)dl->x86dlvsym, 3, NULL, symbol, vername);
        }
        guest_handle = dl->libs[nlib]->x86linkmap;
        if (!guest_handle) {
            guest_handle = (void*)(uintptr_t)RunFunctionWithState(
                (uintptr_t)dl->x86dlopen, 2, dl->libs[nlib]->name,
                dl->libs[nlib]->x86dlopenflag);
            dl->libs[nlib]->x86linkmap = guest_handle;
            if (!guest_handle) {
                set_dl_errorf(dl, "Missing guest link_map for handle %p\n",
                              handle);
                return NULL;
            }
        }
    }
    uintptr_t ret = RunFunctionWithState(
        (uintptr_t)dl->x86dlvsym, 3, guest_handle, symbol, vername);
    return (void*)ret;
}

EXPORT int my_dlinfo(void* handle, int request, void* info)
{
    printf_dlsym(LOG_DEBUG, "Call to dlinfo(%p, %d, %p)\n", handle, request, info);
    dlprivate_t *dl = my_context->dlprivate;
    CLEARERR
    if (!dl->x86dlinfo) {
        init_x86dlfun();
        lsassert(dl->x86dlinfo);
    }
    size_t nlib;
    void *guest_handle = handle;
    if (find_dl_library_index(dl, handle, &nlib)) {
        if (!dl->count[nlib]) {
            set_dl_errorf(dl, "Bad handle %p (already closed)\n", handle);
            return -1;
        }
        if (!dl->libs[nlib]) {
            guest_handle = NULL;
        } else {
            guest_handle = dl->libs[nlib]->x86linkmap;
            if (!guest_handle) {
                guest_handle = (void*)(uintptr_t)RunFunctionWithState(
                    (uintptr_t)dl->x86dlopen, 2, dl->libs[nlib]->name,
                    dl->libs[nlib]->x86dlopenflag);
                dl->libs[nlib]->x86linkmap = guest_handle;
                if (!guest_handle) {
                    set_dl_errorf(dl,
                                  "Cannot open guest library for handle %p\n",
                                  handle);
                    return -1;
                }
            }
            if (request == RTLD_DI_LINKMAP) {
                if (!info) {
                    set_dl_errorf(dl,
                                  "Invalid dlinfo result for handle %p\n",
                                  handle);
                    return -1;
                }
                *(struct link_map**)info = guest_handle;
                return 0;
            }
        }
    }
    uint64_t ret = RunFunctionWithState(
        (uintptr_t)my_context->dlprivate->x86dlinfo, 3,
        guest_handle, request, info);
    return ret;
}

#ifndef CONFIG_LOONGARCH_NEW_WORLD
#include "wrappedlib_init.h"
#endif
