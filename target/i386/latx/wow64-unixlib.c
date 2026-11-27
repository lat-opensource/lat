/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

#include "ntstatus.h"
#include "windef.h"
#include "wine/unixlib.h"
#include "wow64-unix-bridge.h"

static const LatWow64Api *api;
static LatWow64Process *process;
static pthread_key_t thread_key;
static void (*provider_process_term)(uint32_t post);

static NTSTATUS provider_status(LatWow64Status status)
{
    switch (status) {
    case LAT_WOW64_STATUS_OK:
        return STATUS_SUCCESS;
    case LAT_WOW64_STATUS_NO_MEMORY:
        return STATUS_NO_MEMORY;
    case LAT_WOW64_STATUS_INVALID_ARGUMENT:
        return STATUS_INVALID_PARAMETER;
    case LAT_WOW64_STATUS_NOT_FOUND:
        return STATUS_NOT_FOUND;
    case LAT_WOW64_STATUS_VERSION_MISMATCH:
        return STATUS_REVISION_MISMATCH;
    default:
        return STATUS_UNSUCCESSFUL;
    }
}

static void destroy_thread(void *thread)
{
    api->thread_destroy(thread);
}

static NTSTATUS initialize(void *opaque)
{
    const LatWow64ProcessConfig *config = opaque;
    const LatWow64Api *(*get_api)(uint32_t);
    const char *name = getenv("LATX_WOW64_PROVIDER");
    void *module;
    LatWow64Status status;

    if (!name || !*name) {
        name = "liblatx-wow64.so";
    }
    module = dlopen(name, RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
    if (!module) {
        return STATUS_DLL_NOT_FOUND;
    }
    get_api = dlsym(module, "lat_wow64_get_api");
    api = get_api ? get_api(LAT_WOW64_ABI_VERSION) : NULL;
    if (!api || api->size < sizeof(*api) ||
        api->abi_version != LAT_WOW64_ABI_VERSION) {
        dlclose(module);
        return STATUS_REVISION_MISMATCH;
    }
    if (pthread_key_create(&thread_key, destroy_thread)) {
        dlclose(module);
        return STATUS_NO_MEMORY;
    }
    status = api->process_create(config, &process);
    if (status) {
        pthread_key_delete(thread_key);
        dlclose(module);
    } else {
        provider_process_term = dlsym(module, "lat_wow64_process_term");
    }
    return provider_status(status);
}

static LatWow64Status reply_to_exit(LatWow64Thread *thread,
                                   const LatWow64Exit *exit)
{
    const LatWow64Reply reply = {
        .size = sizeof(reply),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .sequence = exit->sequence,
        .action = exit->reply_mask & LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_CONTINUE)
                ? LAT_WOW64_REPLY_CONTINUE : LAT_WOW64_REPLY_NOT_HANDLED,
    };
    return api->reply(thread, &reply);
}

static NTSTATUS run(void *opaque)
{
    struct wowlatx_run *args = opaque;
    LatWow64Thread *thread = pthread_getspecific(thread_key);
    LatWow64Context prior;
    LatWow64Status status;

    if (thread) {
        status = api->get_context(thread, &prior);
        if (!status) {
            args->context.generation = prior.generation;
            status = api->set_context(thread, &args->context);
        }
    } else {
        status = api->thread_create(process, &args->context, &thread);
        if (!status && pthread_setspecific(thread_key, thread)) {
            api->thread_destroy(thread);
            status = LAT_WOW64_STATUS_NO_MEMORY;
        }
    }
    if (status) {
        return provider_status(status);
    }

    for (;;) {
        args->exit.size = sizeof(args->exit);
        args->exit.abi_version = LAT_WOW64_ABI_VERSION;
        status = api->enter(thread, LAT_WOW64_ENTER_CURRENT_PC, &args->exit);
        if (status == LAT_WOW64_STATUS_STALE_MAPPING &&
            !(args->exit.flags & LAT_WOW64_EXIT_FLAG_REPLY_REQUIRED)) {
            continue;
        }
        if (status) {
            return provider_status(status);
        }
        if (args->exit.flags & LAT_WOW64_EXIT_FLAG_REPLY_REQUIRED) {
            status = reply_to_exit(thread, &args->exit);
            if (status) {
                return provider_status(status);
            }
        }
        if (args->exit.reason != LAT_WOW64_EXIT_STOPPED) {
            break;
        }
    }
    return provider_status(api->get_context(thread, &args->context));
}

static NTSTATUS thread_exit(void *opaque)
{
    LatWow64Thread *thread = pthread_getspecific(thread_key);
    LatWow64Status status;

    if (!thread) {
        return STATUS_SUCCESS;
    }
    status = api->thread_destroy(thread);
    if (!status) {
        pthread_setspecific(thread_key, NULL);
    }
    return provider_status(status);
}

static NTSTATUS memory_event(void *opaque)
{
    LatWow64MemoryEvent event = *(const LatWow64MemoryEvent *)opaque;
    LatWow64Status status = api->memory_event(process, &event);

    if (status == LAT_WOW64_STATUS_OVERLAP && event.type == LAT_WOW64_MEMORY_MAP) {
        event.type = LAT_WOW64_MEMORY_PROTECT;
        status = api->memory_event(process, &event);
    } else if (status == LAT_WOW64_STATUS_NOT_FOUND &&
               event.type == LAT_WOW64_MEMORY_PROTECT) {
        event.type = LAT_WOW64_MEMORY_MAP;
        status = api->memory_event(process, &event);
    }
    return provider_status(status);
}

static NTSTATUS process_term(void *opaque)
{
    if (provider_process_term) {
        provider_process_term(*(const BOOL *)opaque);
    }
    return STATUS_SUCCESS;
}

const unixlib_entry_t __wine_unix_call_funcs[] = {
    [WOWLATX_INIT] = initialize,
    [WOWLATX_RUN] = run,
    [WOWLATX_THREAD_EXIT] = thread_exit,
    [WOWLATX_MEMORY] = memory_event,
    [WOWLATX_PROCESS_TERM] = process_term,
};
