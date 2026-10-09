/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "wrappedlibs.h"

#include "debug.h"
#include "wrapper.h"
#include "bridge.h"
#include "library_private.h"
#include "box64context.h"
#include "librarian.h"
#include "callback.h"
#include "library.h"
#include "kzt-groups.h"
#include "kzt-runtime.h"

const char* libeglName = "libEGL.so.1";
#define LIBNAME libegl

/*
 * Chromium's ANGLE passthrough decoder needs these EGL extensions.  They are
 * display extensions, so ask the display and not only the client query; some
 * implementations also list them among the client extensions, so accept
 * either list.
 */
static bool host_egl_supports_angle_passthrough(void)
{
    static const char *const required_extensions[] = {
        "EGL_CHROMIUM_create_context_bind_generates_resource",
        "EGL_ANGLE_create_context_webgl_compatibility",
        "EGL_ANGLE_robust_resource_initialization",
        "EGL_ANGLE_display_texture_share_group",
        "EGL_ANGLE_create_context_client_arrays",
    };
    static int cached = -1;
    void *egl;
    void *(*get_display)(void *);
    unsigned int (*initialize)(void *, int *, int *);
    const char *(*query_string)(void *, int);
    const char *client_extensions = NULL;
    const char *display_extensions = NULL;

    if (cached >= 0)
        return cached;

    egl = dlopen(libeglName, RTLD_LAZY | RTLD_LOCAL);
    if (!egl) {
        cached = 0;
        return false;
    }

    get_display = dlsym(egl, "eglGetDisplay");
    initialize = dlsym(egl, "eglInitialize");
    query_string = dlsym(egl, "eglQueryString");
    if (query_string) {
        client_extensions = query_string(NULL, 0x3055);
        if (get_display && initialize) {
            void *display = get_display(NULL);
            int major = 0;
            int minor = 0;

            if (display && initialize(display, &major, &minor))
                display_extensions = query_string(display, 0x3055);
        }
    }

    cached = display_extensions != NULL || client_extensions != NULL;
    for (size_t i = 0; cached && i < G_N_ELEMENTS(required_extensions); ++i) {
        cached = (display_extensions &&
                  strstr(display_extensions, required_extensions[i]) != NULL) ||
                 (client_extensions &&
                  strstr(client_extensions, required_extensions[i]) != NULL);
    }
    dlclose(egl);
    return cached;
}

/*
 * The guest EGL is used when the host cannot serve ANGLE; the "egl" KZT group
 * is the explicit switch for stacks that want the host library anyway.
 */
#define PRE_INIT                                                        \
    do {                                                                \
        if (latx_kzt_runtime_enabled() &&                               \
            !kzt_group_was_named(KZT_GROUP_EGL) &&                      \
            !host_egl_supports_angle_passthrough()) {                   \
            kzt_groups_log_wrapper_rejection(                           \
                libeglName,                                             \
                "host EGL lacks required ANGLE extensions");            \
            return -1;                                                  \
        }                                                               \
    } while (0);

#include "generated/wrappedlibegltypes.h"
#include "wrappercallback.h"

static char* make_proc_name(const char* prefix, const char* name, const char* suffix)
{
    const size_t prefix_len = strlen(prefix);
    const size_t name_len = strlen(name);
    const size_t suffix_len = strlen(suffix);
    if(name_len > SIZE_MAX - prefix_len - suffix_len - 1)
        return NULL;
    char* result = (char*)malloc(prefix_len + name_len + suffix_len + 1);
    if(!result)
        return NULL;
    memcpy(result, prefix, prefix_len);
    memcpy(result + prefix_len, name, name_len);
    memcpy(result + prefix_len + name_len, suffix, suffix_len + 1);
    return result;
}

static khint_t find_egl_wrapper(kh_symbolmap_t* wrappers, const char* rname)
{
    khint_t k = kh_get(symbolmap, wrappers, rname);
    static const char* const suffixes[] = {"ARB", "EXT"};
    for(size_t i = 0; k == kh_end(wrappers) && i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        if(strstr(rname, suffixes[i]) != NULL)
            continue;
        char* alternate = make_proc_name("", rname, suffixes[i]);
        if(!alternate)
            return kh_end(wrappers);
        k = kh_get(symbolmap, wrappers, alternate);
        free(alternate);
    }
    return k;
}

EXPORT void* my_eglGetProcAddress(void* name);

EXPORT void* my_eglGetProcAddress(void* name)
{
    khint_t k;
    const char* rname = (const char*)name;
    if(relocation_log) printf_log(LOG_INFO, "Calling eglGetProcAddress(\"%s\") => ", rname);
    if(!my_context->glwrappers)
        fillGLProcWrapper();
    // check if glxprocaddress is filled, and search for lib and fill it if needed
    // get proc adress using actual glXGetProcAddress
    k = kh_get(symbolmap, my_context->glmymap, rname);
    int is_my = (k==kh_end(my_context->glmymap))?0:1;
    void* native_symbol = my->eglGetProcAddress((void*)rname);
    void* symbol = native_symbol;
    if(is_my) {
        // try again, by using custom "my_" now...
        if(!native_symbol)
            symbol = NULL;
        else {
            char* alternate = make_proc_name("my_", rname, "");
            symbol = alternate ? dlsym(my_context->box64lib, alternate) : NULL;
            free(alternate);
        }
    }
    if(!symbol) {
        if(relocation_log<LOG_DEBUG) printf_log(LOG_NONE, "%p\n", NULL);
        return NULL;    // easy
    }
    // check if alread bridged
    uintptr_t ret = CheckBridged(my_context->system, symbol);
    if(ret) {
        if(relocation_log<LOG_DEBUG) printf_log(LOG_NONE, "%p\n", (void*)ret);
        return (void*)ret; // already bridged
    }
    // get wrapper
    k = find_egl_wrapper(my_context->glwrappers, rname);
    if(k==kh_end(my_context->glwrappers)) {
        return NULL;
    }
    const char* constname = kh_key(my_context->glwrappers, k);
    AddOffsetSymbol(my_context->maplib, symbol, rname);
    ret = AddBridge(my_context->system, kh_value(my_context->glwrappers, k), symbol, 0, constname);
    if(relocation_log<LOG_DEBUG) printf_log(LOG_NONE, "%p\n", (void*)ret);
    return (void*)ret;

}


#define CUSTOM_INIT                 \
    getMy(lib);                     \
    setNeededLibs(lib, 1, "libGL.so.1");\
    if (!box64->glxprocaddress)     \
        box64->glxprocaddress = (procaddess_t)my->eglGetProcAddress;

#define CUSTOM_FINI \
    freeMy();


#include "wrappedlib_init.h"
