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
#include "kzt-guest-tls.h"
#include "myalign.h"
#include "fileutils.h"
#include "qemu.h"
#include "x86dlfun.h"

#ifndef CONFIG_LOONGARCH_NEW_WORLD
#define LIBNAME libdl
const char *libdlName = "libdl.so.2";
#endif

#define FORWORDBACK 0

#ifdef CONFIG_LATX_KZT
static GRecMutex kzt_guest_loader_operation_lock;
static GMutex kzt_dl_metadata_lock;
static gsize kzt_guest_loader_operation_lock_initialized;
static GThread *kzt_guest_loader_operation_owner;
static gint kzt_guest_loader_operation_depth;

static int kzt_guest_loader_operation_enter(void)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return 0;
    }
    CPUX86State *cpu = lsenv && lsenv->cpu_state
        ? (CPUX86State *)lsenv->cpu_state : NULL;
    int execution_paused =
        kzt_guest_tls_execution_pause(cpu);

    if (g_once_init_enter(
            &kzt_guest_loader_operation_lock_initialized)) {
        g_rec_mutex_init(&kzt_guest_loader_operation_lock);
        g_once_init_leave(
            &kzt_guest_loader_operation_lock_initialized, 1);
    }
    g_rec_mutex_lock(&kzt_guest_loader_operation_lock);
    g_assert(!g_atomic_int_get(&kzt_guest_loader_operation_depth) ||
             g_atomic_pointer_get(
                 &kzt_guest_loader_operation_owner) == g_thread_self());
    g_atomic_pointer_set(
        &kzt_guest_loader_operation_owner, g_thread_self());
    g_atomic_int_inc(&kzt_guest_loader_operation_depth);
    kzt_guest_tls_execution_resume(cpu, execution_paused);
    return 1;
}

static void kzt_guest_loader_operation_leave(int *locked)
{
    if (locked && *locked) {
        g_assert(g_atomic_int_get(
                     &kzt_guest_loader_operation_depth) > 0 &&
                 g_atomic_pointer_get(
                     &kzt_guest_loader_operation_owner) ==
                     g_thread_self());
        if (g_atomic_int_dec_and_test(
                &kzt_guest_loader_operation_depth)) {
            g_atomic_pointer_set(
                &kzt_guest_loader_operation_owner, NULL);
        }
        g_rec_mutex_unlock(&kzt_guest_loader_operation_lock);
    }
}

void kzt_guest_loader_after_fork_child(void)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return;
    }
    GThread *current = g_thread_self();
    int depth = g_atomic_pointer_get(
                    &kzt_guest_loader_operation_owner) == current
        ? g_atomic_int_get(&kzt_guest_loader_operation_depth) : 0;

    memset(&kzt_guest_loader_operation_lock, 0,
           sizeof(kzt_guest_loader_operation_lock));
    g_rec_mutex_init(&kzt_guest_loader_operation_lock);
    memset(&kzt_dl_metadata_lock, 0,
           sizeof(kzt_dl_metadata_lock));
    g_mutex_init(&kzt_dl_metadata_lock);
    kzt_guest_loader_operation_lock_initialized = 1;
    g_atomic_pointer_set(&kzt_guest_loader_operation_owner,
                         depth ? current : NULL);
    g_atomic_int_set(&kzt_guest_loader_operation_depth, 0);
    for (int index = 0; index < depth; ++index) {
        g_rec_mutex_lock(&kzt_guest_loader_operation_lock);
        g_atomic_int_inc(&kzt_guest_loader_operation_depth);
    }
}

#define KZT_GUEST_LOADER_OPERATION_GUARD()                            \
    int kzt_guest_loader_operation_guard                             \
        __attribute__((cleanup(kzt_guest_loader_operation_leave),    \
                       unused)) =                                    \
            kzt_guest_loader_operation_enter()

static void kzt_dl_metadata_lock_acquire(void)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return;
    }
    g_mutex_lock(&kzt_dl_metadata_lock);
}

static void kzt_dl_metadata_lock_release(void)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return;
    }
    g_mutex_unlock(&kzt_dl_metadata_lock);
}
#else
#define KZT_GUEST_LOADER_OPERATION_GUARD() do { } while (0)
static void kzt_dl_metadata_lock_acquire(void)
{
}

static void kzt_dl_metadata_lock_release(void)
{
}
#endif

#ifdef CONFIG_LATX_KZT
uintptr_t kzt_guest_loader_hold_open(uintptr_t guest_name)
{
    KZT_GUEST_LOADER_OPERATION_GUARD();
    dlprivate_t *dl = my_context ? my_context->dlprivate : NULL;

    if (!guest_name || !dl || !dl->x86dlopen) {
        return 0;
    }
    return RunFunctionWithStateInternalNoRefresh(
        (uintptr_t)dl->x86dlopen, 2,
        guest_name, RTLD_LAZY | RTLD_NOLOAD);
}

void kzt_guest_loader_hold_close(uintptr_t handle)
{
    KZT_GUEST_LOADER_OPERATION_GUARD();
    dlprivate_t *dl = my_context ? my_context->dlprivate : NULL;

    if (handle && dl && dl->x86dlclose) {
        (void)RunFunctionWithStateInternalNoRefresh(
            (uintptr_t)dl->x86dlclose, 1, handle);
    }
}
#endif

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
        (void)RunFunctionWithStateInternal((uintptr_t)dl->x86dlerror, 0);
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

static void *finish_dlopen_with_guest_tls(dlprivate_t *dl, void *result)
{
#ifdef CONFIG_LATX_KZT
    if (!latx_kzt_guest_tls_enabled()) {
        return result;
    }
    if (result) {
        uintptr_t link_map_addr = (uintptr_t)result;
        __MY_CPU;

        if (link_map_addr <= dl->lib_sz && link_map_addr != 0) {
            library_t *lib = dl->libs[link_map_addr - 1];

            link_map_addr = lib ? (uintptr_t)lib->x86linkmap : 0;
        }
        if (link_map_addr &&
            kzt_register_guest_tls_link_map(link_map_addr) != 0) {
            fprintf(stderr,
                    "KZT cannot register Guest TLS after a successful "
                    "dlopen; refusing to continue\n");
            _exit(EXIT_FAILURE);
        }

        if (kzt_guest_tls_refresh(cpu) != 0) {
            fprintf(stderr,
                    "KZT cannot refresh Guest TLS after a successful "
                    "dlopen; refusing to run further callbacks\n");
            _exit(EXIT_FAILURE);
        }
    }
#else
    (void)dl;
#endif
    return result;
}

#ifdef CONFIG_LATX_KZT
static int call_guest_dlclose_with_guest_tls(void *handle)
{
    __MY_CPU;
    int result;

    if (kzt_guest_tls_refresh(cpu) != 0) {
        fprintf(stderr,
                "KZT cannot refresh Guest TLS before dlclose; "
                "refusing to continue\n");
        _exit(EXIT_FAILURE);
    }
    kzt_guest_tls_loader_event_begin();
    kzt_unregister_guest_tls_link_map((uintptr_t)handle);
    result = (int)RunFunctionWithStateInternalNoRefresh(
        (uintptr_t)my_context->dlprivate->x86dlclose,
        1, handle);
    if (result == 0) {
        if (kzt_guest_tls_refresh(cpu) != 0) {
            fprintf(stderr,
                    "KZT cannot refresh Guest TLS after dlclose; "
                    "refusing to continue\n");
            _exit(EXIT_FAILURE);
        }
    } else if (kzt_register_guest_tls_link_map(
                   (uintptr_t)handle) != 0) {
        fprintf(stderr,
                "KZT cannot restore Guest TLS loader state after a "
                "failed dlclose; refusing to continue\n");
        _exit(EXIT_FAILURE);
    }
    return result;
}
#endif

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
    if (init_x86dlfun_from("libc.so.6", "libdl.so.2") != 0) {
        return -1;
    }
    kzt_wine_init_x86();
    return 0;
#else
    return init_x86dlfun_from("libdl.so.2", "libc.so.6");
#endif
}

static void *redlopen_guest_library(dlprivate_t *dl, library_t *lib)
{
    KZT_GUEST_LOADER_OPERATION_GUARD();
    __MY_CPU;
    void *guest_handle;

    if (lib->x86linkmap) {
        return lib->x86linkmap;
    }

    if (kzt_guest_tls_refresh(cpu) != 0) {
        return NULL;
    }
    kzt_guest_tls_loader_event_begin();
    guest_handle = (void *)(uintptr_t)RunFunctionWithStateInternalNoRefresh(
        (uintptr_t)my_context->dlprivate->x86dlopen, 2,
        lib->name, lib->x86dlopenflag);
    if (!guest_handle) {
        return NULL;
    }

    kzt_dl_metadata_lock_acquire();
    lib->x86linkmap = guest_handle;
    kzt_dl_metadata_lock_release();
    return finish_dlopen_with_guest_tls(dl, guest_handle);
}

static int callx86dlopen(void *filename, int flag, elfheader_t * h, int is_local) {
    __MY_CPU;

    if (kzt_guest_tls_refresh(cpu) != 0) {
        return -1;
    }
    kzt_guest_tls_loader_event_begin();
    struct link_map *ret =
        (struct link_map *)(uintptr_t)RunFunctionWithStateInternalNoRefresh(
            (uintptr_t)my_context->dlprivate->x86dlopen, 2,
            filename, flag);
    if (ret) {
        printf_dlsym(LOG_DEBUG, "latx RunFunctionWithState dlopen %s addr %p\n", (char *)filename, (void *)ret->l_addr);
        kzt_dl_metadata_lock_acquire();
        h->lib->x86linkmap = ret;
        kzt_dl_metadata_lock_release();
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
    KZT_GUEST_LOADER_OPERATION_GUARD();
    // TODO, handling special values for filename, like RTLD_SELF?
    // TODO, handling flags?
    library_t *lib = NULL;
    dlprivate_t *dl = my_context->dlprivate;
    size_t dlopened = 0;
    int is_local = (flag&0x100)?0:1;  // if not global, then local, and that means symbols are not put in the global "pot" for other libs
    CLEARERR
    if (!dl->x86dlopen) {
        if (init_x86dlfun() != 0 || !dl->x86dlopen) {
            set_dl_error(dl, "Cannot resolve guest dlfcn entry points");
            return NULL;
        }
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
                    if (latx_kzt_guest_tls_enabled() && (flag & RTLD_NOLOAD)) {
                        box_free(rfilename);
                        set_dl_error(
                            dl, "RTLD_NOLOAD object is not loaded");
                        return NULL;
                    }
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
                kzt_dl_metadata_lock_acquire();
                if (latx_kzt_guest_tls_enabled() || !(flag & RTLD_NOLOAD)) {
                    dl->count[i] += 1;
                }
                kzt_dl_metadata_lock_release();
                printf_dlsym(LOG_DEBUG, "dlopen: Recycling %s/%p count=%ld (dlopened=%ld, elf_index=%d)\n", rfilename, (void*)(i+1), dl->count[i], dl->dlopened[i], GetElfIndex(dl->libs[i]));
                box_free(rfilename);
                return finish_dlopen_with_guest_tls(
                    dl, (void *)(i + 1));
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
            __MY_CPU;
            if (kzt_guest_tls_refresh(cpu) != 0) {
                box_free(rfilename);
                return NULL;
            }
            kzt_guest_tls_loader_event_begin();
            uint64_t ret = RunFunctionWithStateInternalNoRefresh(
                (uintptr_t)my_context->dlprivate->x86dlopen, 2,
                filename, flag);
            printf_dlsym(LOG_DEBUG, "warning call call x86dlopen filename %s %x ret=0x%lx\n",  (char *)filename, flag, ret);
            //lsassert(0);
            if (ret) {
                box_free(rfilename);
                return finish_dlopen_with_guest_tls(
                    dl, (void *)(uintptr_t)ret);
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
                kzt_dl_metadata_lock_acquire();
                dl->count[i] = dl->count[i]+1;
                kzt_dl_metadata_lock_release();
                return finish_dlopen_with_guest_tls(
                    dl, (void *)(i + 1));
            }
        }
        printf_dlsym(LOG_DEBUG,
                     "Call to dlopen(NULL, %X), forward to x86 dlopen\n",
                     flag);
        lsassert(dl->x86dlopen);
        __MY_CPU;
        Push64(cpu, (uint64_t)dl->x86dlopen);
        return NULL;
    }
    //get the lib and add it to the collection

    kzt_dl_metadata_lock_acquire();
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
    kzt_dl_metadata_lock_release();
    printf_dlsym(LOG_DEBUG, "dlopen: New handle %p (%s), dlopened=%ld\n", (void*)(idx+1), (char*)filename, dlopened);
    if (lib && lib->type == LIB_EMULATED) {
        return finish_dlopen_with_guest_tls(dl, lib->x86linkmap);
    }
    return finish_dlopen_with_guest_tls(dl, (void *)(idx + 1));
}

EXPORT void* my_dlmopen(void* lmid, void *filename, int flag)
{
    KZT_GUEST_LOADER_OPERATION_GUARD();
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

typedef struct dl_handle_snapshot {
    size_t index;
    size_t count;
    library_t *library;
} dl_handle_snapshot_t;

static int snapshot_dl_handle(dlprivate_t *dl, void *handle,
                              dl_handle_snapshot_t *snapshot)
{
    int found;

    kzt_dl_metadata_lock_acquire();
    found = find_dl_library_index(dl, handle, &snapshot->index);
    if (found) {
        snapshot->count = dl->count[snapshot->index];
        snapshot->library = dl->libs[snapshot->index];
    }
    kzt_dl_metadata_lock_release();
    return found;
}

EXPORT void* my_dlsym(void *handle, void *symbol){
    dlprivate_t *dl = my_context->dlprivate;
    dl_handle_snapshot_t handle_snapshot = { 0 };
    int known_handle = 0;
    uintptr_t start = 0, end = 0;
    char* rsymbol = (char*)symbol;
    CLEARERR
    if (!dl->x86dlsym) {
        if (init_x86dlfun() != 0 || !dl->x86dlsym) {
            set_dl_error(dl, "Cannot resolve guest dlfcn entry points");
            return NULL;
        }
    }
    printf_dlsym(LOG_DEBUG, "Call to dlsym(%p, \"%s\")%s\n", handle, rsymbol, dlsym_error?"":"\n");
    if (handle && handle != (void*)~0LL) {
        known_handle = snapshot_dl_handle(
            dl, handle, &handle_snapshot);
        if (!known_handle) {
            uint64_t ret = RunFunctionWithStateInternal(
                (uintptr_t)dl->x86dlsym, 2, handle, symbol);
            if (!ret) {
                ret = kzt_resolve_guest_link_map_symbol(
                    (uintptr_t)handle, rsymbol);
            }
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
        uint64_t ret = RunFunctionWithStateInternal(
            (uintptr_t)my_context->dlprivate->x86dlsym, 2,
            handle, symbol);
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
    if (!known_handle) {
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
        uint64_t ret = RunFunctionWithStateInternal(
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
    if (handle_snapshot.count == 0) {
        set_dl_errorf(dl, "Bad handle %p (already closed))\n", handle);
        return NULL;
    }
    if (handle_snapshot.library) {
        if (my_dlsym_lib(handle_snapshot.library, rsymbol,
                         &start, &end, -1, NULL) == 0) {
            // not found
            __MY_CPU;
            #if 1
            if (!handle_snapshot.library->x86linkmap) {
                //redlopen
                void *guest_handle = redlopen_guest_library(
                    dl, handle_snapshot.library);
                /* The caller may probe for an optional symbol. */
                if (!guest_handle) {
                    printf_dlsym(LOG_NEVER, "redlopen %p return %p\n", rsymbol, (void*)NULL);
                    return NULL;
                }
                uintptr_t ret = RunFunctionWithStateInternal(
                    (uintptr_t)my_context->dlprivate->x86dlsym, 2,
                    guest_handle, symbol);
                printf_dlsym(LOG_DEBUG, "call x86dlsym filename %s is wrapped but not find symbol, dlsym(%p, %s) ret=0x%lx\n",
                handle_snapshot.library->name,
                handle_snapshot.library->x86linkmap,
                (char *)symbol, ret);
                return (void *)ret;
            }
            #endif
            lsassert(dl->x86dlsym);
            if (handle_snapshot.library->x86linkmap != handle) {
                cpu->regs[R_EDI] =
                    (uintptr_t)handle_snapshot.library->x86linkmap;
            }
#if FORWORDBACK
            Push64(cpu, (uint64_t)dl->x86dlsym);
            printf_dlsym(
                LOG_DEBUG,
                "warning call x86dlsym filename is %s %lx\n",
                handle_snapshot.library->x86linkmap->l_name,
                cpu->regs[R_EDI]);
            return NULL;
#else
            uint64_t ret = RunFunctionWithStateInternal(
                (uintptr_t)my_context->dlprivate->x86dlsym, 2,
                handle_snapshot.library->x86linkmap, symbol);
            printf_dlsym(
                LOG_DEBUG,
                "call x86dlsym filename is %s %s ret=0x%lx\n",
                handle_snapshot.library->x86linkmap->l_name,
                (char *)symbol, ret);
            if (ret) {
                return (void *)ret;
            }
            ret = latx_kzt_guest_tls_enabled()
                ? kzt_resolve_guest_link_map_symbol(
                    (uintptr_t)handle_snapshot.library->x86linkmap,
                    rsymbol) : 0;
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
    KZT_GUEST_LOADER_OPERATION_GUARD();
    printf_dlsym(LOG_DEBUG, "Call to dlclose(%p)\n", handle);
    dlprivate_t *dl = my_context->dlprivate;
    CLEARERR
    kzt_guest_tls_loader_event_begin();
    if (!dl->x86dlclose) {
        if (init_x86dlfun() != 0 || !dl->x86dlclose) {
            set_dl_error(dl, "Cannot resolve guest dlfcn entry points");
            return -1;
        }
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
#ifdef CONFIG_LATX_KZT
            if (latx_kzt_guest_tls_enabled()) {
                return call_guest_dlclose_with_guest_tls(handle);
            }
#endif
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
#ifdef CONFIG_LATX_KZT
    if (latx_kzt_guest_tls_enabled() &&
        dl->count[nlib] == 1 && dl->dlopened[nlib] &&
        dl->libs[nlib] && dl->x86dlclose) {
        int idx = GetElfIndex(dl->libs[nlib]);

        if (idx != -1) {
            void *guest_handle = dl->libs[nlib]->x86linkmap
                ? (void *)dl->libs[nlib]->x86linkmap : handle;
            int close_result =
                call_guest_dlclose_with_guest_tls(guest_handle);

            if (close_result != 0) {
                return close_result;
            }
            kzt_dl_metadata_lock_acquire();
            dl->count[nlib] = 0;
            printf_dlsym(
                LOG_DEBUG, "dlclose: Call to Fini for %p\n", handle);
            InactiveLibrary(dl->libs[nlib]);
            kzt_dl_metadata_lock_release();
            return 0;
        }
    }
#endif
    kzt_dl_metadata_lock_acquire();
    dl->count[nlib] -= 1;
    if(dl->count[nlib]==0 && dl->dlopened[nlib]) {   // need to call Fini...
        int idx = GetElfIndex(dl->libs[nlib]);
        if(idx!=-1) {
            printf_dlsym(LOG_DEBUG, "dlclose: Call to Fini for %p\n", handle);
            InactiveLibrary(dl->libs[nlib]);
            if (dl->x86dlclose) {
                __MY_CPU;
                if (dl->libs[nlib]->x86linkmap != handle) {
                    cpu->regs[R_EDI] =
                        (uintptr_t)dl->libs[nlib]->x86linkmap;
                }
                Push64(cpu, (uint64_t)dl->x86dlclose);
                kzt_dl_metadata_lock_release();
                return 0;
            }
        }
    }
    kzt_dl_metadata_lock_release();
    if (dl->libs[nlib]) {
        kzt_unregister_guest_tls_link_map(
            (uintptr_t)dl->libs[nlib]->x86linkmap);
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
            (void)RunFunctionWithStateInternal((uintptr_t)dl->x86dlerror, 0);
        dl_error_pending = 0;
        return dl_error_buffer;
    }
    if (!dl->x86dlerror)
        return NULL;
    return (char *)(uintptr_t)RunFunctionWithStateInternal(
        (uintptr_t)dl->x86dlerror, 0);
}

EXPORT int my_dladdr1(void *addr, void *i, void** extra_info, int flags)
{
    //int dladdr(void *addr, Dl_info *info);
    dlprivate_t *dl = my_context->dlprivate;
    CLEARERR
    if (!dl->x86dladdr1) {
        if (init_x86dlfun() != 0 || !dl->x86dladdr1) {
            set_dl_error(dl, "Cannot resolve guest dlfcn entry points");
            return 0;
        }
    }
    Dl_info *info = (Dl_info*)i;
    printf_dlsym(LOG_DEBUG, "Warning: partially unimplement call to dladdr/dladdr1(%p, %p, %p, %d)\n", addr, info, extra_info, flags);
     __MY_CPU;
    uint64_t ret = 0;
    if (extra_info == NULL && flags == 0) {
        ret = RunFunctionWithStateInternal(
            (uintptr_t)my_context->dlprivate->x86dladdr, 2,
            cpu->regs[R_EDI], cpu->regs[R_ESI]);
    } else {
        ret = RunFunctionWithStateInternal(
            (uintptr_t)my_context->dlprivate->x86dladdr1, 4,
            cpu->regs[R_EDI], cpu->regs[R_ESI],
            cpu->regs[R_EDX], cpu->regs[R_ECX]);
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
        if (init_x86dlfun() != 0 || !dl->x86dladdr) {
            set_dl_error(dl, "Cannot resolve guest dlfcn entry points");
            return 0;
        }
    }
#ifdef CONFIG_LATX_DEBUG
    Dl_info *info = (Dl_info*)i;
#endif
    printf_dlsym(LOG_DEBUG, "Warning: partially unimplement call to dladdr(%p, %p)\n", addr, info);
     __MY_CPU;
    uint64_t ret = RunFunctionWithStateInternal(
        (uintptr_t)my_context->dlprivate->x86dladdr, 2,
        cpu->regs[R_EDI], cpu->regs[R_ESI]);
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
    dl_handle_snapshot_t handle_snapshot = { 0 };
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
        return (void *)(uintptr_t)RunFunctionWithStateInternal(
            (uintptr_t)dl->x86dlvsym, 3, guest_handle, symbol, vername);
    if (snapshot_dl_handle(dl, handle, &handle_snapshot)) {
        if (!handle_snapshot.count) {
            set_dl_errorf(dl, "Bad handle %p (already closed)\n", handle);
            return NULL;
        }
        if (!handle_snapshot.library) {
            return (void *)(uintptr_t)RunFunctionWithStateInternal(
                (uintptr_t)dl->x86dlvsym, 3, NULL, symbol, vername);
        }
        guest_handle = handle_snapshot.library->x86linkmap;
        if (!guest_handle) {
            guest_handle = redlopen_guest_library(
                dl, handle_snapshot.library);
            if (!guest_handle) {
                set_dl_errorf(dl, "Missing guest link_map for handle %p\n",
                              handle);
                return NULL;
            }
        }
    }
    uintptr_t ret = RunFunctionWithStateInternal(
        (uintptr_t)dl->x86dlvsym, 3, guest_handle, symbol, vername);
    return (void*)ret;
}

EXPORT int my_dlinfo(void* handle, int request, void* info)
{
    printf_dlsym(LOG_DEBUG, "Call to dlinfo(%p, %d, %p)\n", handle, request, info);
    dlprivate_t *dl = my_context->dlprivate;
    CLEARERR
    if (!dl->x86dlinfo) {
        if (init_x86dlfun() != 0 || !dl->x86dlinfo) {
            set_dl_error(dl, "Cannot resolve guest dlfcn entry points");
            return -1;
        }
    }
    dl_handle_snapshot_t handle_snapshot = { 0 };
    void *guest_handle = handle;
    if (snapshot_dl_handle(dl, handle, &handle_snapshot)) {
        if (!handle_snapshot.count) {
            set_dl_errorf(dl, "Bad handle %p (already closed)\n", handle);
            return -1;
        }
        if (!handle_snapshot.library) {
            guest_handle = NULL;
        } else {
            guest_handle = handle_snapshot.library->x86linkmap;
            if (!guest_handle) {
                guest_handle = redlopen_guest_library(
                    dl, handle_snapshot.library);
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
    uint64_t ret = RunFunctionWithStateInternal(
        (uintptr_t)my_context->dlprivate->x86dlinfo, 3,
        guest_handle, request, info);
    return ret;
}

#ifdef CONFIG_LIBLAT
static __thread char latx_loader_attach_error[] =
    "KZT cannot attach the current Host thread to the Guest loader";

static int latx_attach_loader_thread(void)
{
    if (lsenv && lsenv->cpu_state) {
        return 0;
    }
    return latx_attach_current_host_thread();
}


EXPORT void *lat_dlopen(void *filename, int flags)
{
    if (latx_attach_loader_thread() != 0) {
        return NULL;
    }
    return my_dlopen(filename, flags);
}

EXPORT void *lat_dlsym(void *handle, void *symbol)
{
    if (latx_attach_loader_thread() != 0) {
        return NULL;
    }
    return my_dlsym(handle, symbol);
}

EXPORT int lat_dlinfo(void *handle, int request, void *info)
{
    if (latx_attach_loader_thread() != 0) {
        return -1;
    }
    return my_dlinfo(handle, request, info);
}

EXPORT int lat_dlclose(void *handle)
{
    if (latx_attach_loader_thread() != 0) {
        return -1;
    }
    __MY_CPU;
    cpu->regs[R_EDI] = (uintptr_t)handle;
    return my_dlclose(handle);
}

EXPORT char *lat_dlerror(void)
{
    if (latx_attach_loader_thread() != 0) {
        return latx_loader_attach_error;
    }
    return my_dlerror();
}
#endif

#ifndef CONFIG_LOONGARCH_NEW_WORLD
#include "wrappedlib_init.h"
#endif
