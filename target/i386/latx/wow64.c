/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <signal.h>

#include <glib.h>

#include "wow64-internal.h"

typedef struct LatWow64Mapping {
    uint64_t start;
    uint64_t end;
    uint32_t protection;
} LatWow64Mapping;

struct LatWow64Process {
    GMutex lock;
    GCond idle_cond;
    GArray *mappings;
    LatWow64BackendOps ops;
    void *backend_opaque;
    uint64_t generation;
    uint32_t max_enter_depth;
    uint32_t thread_count;
    uint32_t active_entries;
    uint64_t next_thread_serial;
    bool memory_barrier;
};

struct LatWow64Thread {
    GMutex lock;
    LatWow64Process *process;
    LatWow64Context context;
    LatWow64Context *active_context;
    GThread *owner;
    uint64_t serial;
    uint32_t enter_depth;
    uint64_t next_sequence;
    uint64_t context_generation;
    uint64_t pending_sequence;
    uint64_t pending_guest_pc;
    uint32_t pending_reason;
    uint32_t pending_reply_mask;
    bool reply_pending;
    bool suspended;
    bool stopped;
    bool destroying;
};

static GPrivate lat_wow64_active_process = G_PRIVATE_INIT(NULL);

static uint64_t *lat_wow64_context_generation(LatWow64Context *context)
{
    return &context->generation;
}

static void lat_wow64_context_set_pc(LatWow64Context *context, uint64_t pc)
{
    context->eip = pc;
}

static bool lat_wow64_struct_valid(uint32_t size, uint32_t expected_size,
                                uint32_t abi_version)
{
    return size >= expected_size && abi_version == LAT_WOW64_ABI_VERSION;
}

static bool lat_wow64_range_valid(uint64_t address, uint64_t length,
                               uint64_t *end_out)
{
    uint64_t end;

    if (!length || __builtin_add_overflow(address, length, &end)) {
        return false;
    }
    *end_out = end;
    return true;
}

static bool lat_wow64_ranges_overlap(uint64_t start_a, uint64_t end_a,
                                  uint64_t start_b, uint64_t end_b)
{
    return start_a < end_b && start_b < end_a;
}

static uint32_t lat_wow64_exit_reply_mask(uint32_t reason)
{
    switch (reason) {
    case LAT_WOW64_EXIT_CALLBACK:
    case LAT_WOW64_EXIT_SYSCALL:
        return LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_CONTINUE) |
               LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_REDIRECT) |
               LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_STOP);
    case LAT_WOW64_EXIT_EXCEPTION:
    case LAT_WOW64_EXIT_BREAKPOINT:
        return LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_CONTINUE) |
               LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_NOT_HANDLED) |
               LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_RETRY) |
               LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_REDIRECT) |
               LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_STOP);
    case LAT_WOW64_EXIT_SINGLE_STEP:
        return LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_CONTINUE) |
               LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_NOT_HANDLED) |
               LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_REDIRECT) |
               LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_STOP);
    case LAT_WOW64_EXIT_STOPPED:
        return LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_CONTINUE) |
               LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_STOP);
    case LAT_WOW64_EXIT_RETURN:
    case LAT_WOW64_EXIT_SYSCALL_UNROUTED:
    case LAT_WOW64_EXIT_STALE_MAPPING:
        return 0;
    default:
        return UINT32_MAX;
    }
}

static uint32_t lat_wow64_thread_state_locked(const LatWow64Thread *thread)
{
    if (thread->destroying) {
        return LAT_WOW64_THREAD_DESTROYING;
    }
    if (thread->enter_depth) {
        return LAT_WOW64_THREAD_RUNNING;
    }
    if (thread->reply_pending) {
        return LAT_WOW64_THREAD_WAITING_REPLY;
    }
    if (thread->suspended) {
        return LAT_WOW64_THREAD_SUSPENDED;
    }
    if (thread->stopped) {
        return LAT_WOW64_THREAD_STOPPED;
    }
    return LAT_WOW64_THREAD_READY;
}

static bool lat_wow64_range_fully_mapped(LatWow64Process *process,
                                      uint64_t start, uint64_t end)
{
    uint64_t cursor = start;

    while (cursor < end) {
        uint64_t next = cursor;
        unsigned int i;

        for (i = 0; i < process->mappings->len; ++i) {
            LatWow64Mapping *mapping = &g_array_index(process->mappings,
                                                   LatWow64Mapping, i);

            if (mapping->start <= cursor && mapping->end > next) {
                next = mapping->end;
            }
        }
        if (next == cursor) {
            return false;
        }
        cursor = MIN(next, end);
    }
    return true;
}

static void lat_wow64_append_mapping(GArray *mappings, uint64_t start,
                                  uint64_t end, uint32_t protection)
{
    LatWow64Mapping mapping = {
        .start = start,
        .end = end,
        .protection = protection,
    };

    if (start != end) {
        g_array_append_val(mappings, mapping);
    }
}

static void lat_wow64_update_mapping_range(LatWow64Process *process,
                                        uint64_t start, uint64_t end,
                                        uint32_t protection, bool unmap)
{
    GArray *updated = g_array_sized_new(false, false,
                                        sizeof(LatWow64Mapping),
                                        process->mappings->len + 2);
    unsigned int i;

    for (i = 0; i < process->mappings->len; ++i) {
        LatWow64Mapping current = g_array_index(process->mappings,
                                             LatWow64Mapping, i);
        uint64_t overlap_start, overlap_end;

        if (!lat_wow64_ranges_overlap(start, end, current.start, current.end)) {
            g_array_append_val(updated, current);
            continue;
        }
        overlap_start = MAX(start, current.start);
        overlap_end = MIN(end, current.end);
        lat_wow64_append_mapping(updated, current.start, overlap_start,
                              current.protection);
        if (!unmap) {
            lat_wow64_append_mapping(updated, overlap_start, overlap_end,
                                  protection);
        }
        lat_wow64_append_mapping(updated, overlap_end, current.end,
                              current.protection);
    }
    g_array_free(process->mappings, true);
    process->mappings = updated;
}

static bool lat_wow64_range_has_executable_mapping(LatWow64Process *process,
                                                uint64_t start, uint64_t end)
{
    unsigned int i;

    for (i = 0; i < process->mappings->len; ++i) {
        LatWow64Mapping *mapping = &g_array_index(process->mappings,
                                               LatWow64Mapping, i);

        if ((mapping->protection & LAT_WOW64_PROT_EXEC) &&
            lat_wow64_ranges_overlap(start, end, mapping->start, mapping->end)) {
            return true;
        }
    }
    return false;
}

LatWow64Status lat_wow64_process_create(const LatWow64BackendOps *ops,
                                  void *backend_opaque,
                                  const LatWow64ProcessConfig *config,
                                  LatWow64Process **process_out)
{
    LatWow64Process *process;

    if (!ops || !config || !process_out) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }
    if (ops->size < sizeof(*ops) ||
        ops->abi_version != LAT_WOW64_ABI_VERSION ||
        !lat_wow64_struct_valid(config->size, sizeof(*config),
                             config->abi_version)) {
        return LAT_WOW64_STATUS_VERSION_MISMATCH;
    }
    if (config->max_enter_depth > LAT_WOW64_MAX_ENTER_DEPTH ||
        config->flags) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }
    if (!ops->execute_guest) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }

    process = g_try_new0(LatWow64Process, 1);
    if (!process) {
        return LAT_WOW64_STATUS_NO_MEMORY;
    }
    process->mappings = g_array_sized_new(false, false,
                                          sizeof(LatWow64Mapping), 8);
    if (!process->mappings) {
        g_free(process);
        return LAT_WOW64_STATUS_NO_MEMORY;
    }
    g_mutex_init(&process->lock);
    g_cond_init(&process->idle_cond);
    memcpy(&process->ops, ops, MIN((size_t)ops->size,
                                   sizeof(process->ops)));
    process->backend_opaque = backend_opaque;
    process->generation = 1;
    process->max_enter_depth = config->max_enter_depth ?: 8;
    *process_out = process;
    return LAT_WOW64_STATUS_OK;
}

LatWow64Status lat_wow64_process_destroy(LatWow64Process *process)
{
    if (!process) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }
    g_mutex_lock(&process->lock);
    if (process->thread_count || process->active_entries ||
        process->memory_barrier) {
        g_mutex_unlock(&process->lock);
        return LAT_WOW64_STATUS_BUSY;
    }
    g_mutex_unlock(&process->lock);
    g_array_free(process->mappings, true);
    g_cond_clear(&process->idle_cond);
    g_mutex_clear(&process->lock);
    g_free(process);
    return LAT_WOW64_STATUS_OK;
}

LatWow64Status lat_wow64_thread_create(LatWow64Process *process,
                                      const LatWow64Context *initial_context,
                                      LatWow64Thread **thread_out)
{
    LatWow64Thread *thread;
    LatWow64Status status;

    if (!process || !initial_context || !thread_out) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }
    if (!lat_wow64_struct_valid(initial_context->size, sizeof(*initial_context),
                             initial_context->abi_version)) {
        return LAT_WOW64_STATUS_VERSION_MISMATCH;
    }
    if (initial_context->generation || (initial_context->dr7 & 0xff)) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }
    *thread_out = NULL;
    thread = g_try_new0(LatWow64Thread, 1);
    if (!thread) {
        return LAT_WOW64_STATUS_NO_MEMORY;
    }
    g_mutex_init(&thread->lock);
    thread->process = process;
    thread->context = *initial_context;
    thread->context_generation = 1;
    thread->context.generation = thread->context_generation;

    g_mutex_lock(&process->lock);
    thread->serial = ++process->next_thread_serial;
    ++process->thread_count;
    g_mutex_unlock(&process->lock);
    if (process->ops.thread_create &&
        (status = process->ops.thread_create(process->backend_opaque, thread)) !=
            LAT_WOW64_STATUS_OK) {
        g_mutex_lock(&process->lock);
        --process->thread_count;
        g_mutex_unlock(&process->lock);
        g_mutex_clear(&thread->lock);
        g_free(thread);
        return status;
    }
    *thread_out = thread;
    return LAT_WOW64_STATUS_OK;
}

LatWow64Status lat_wow64_thread_destroy(LatWow64Thread *thread)
{
    LatWow64Process *process;

    if (!thread) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }
    g_mutex_lock(&thread->lock);
    if (thread->destroying) {
        g_mutex_unlock(&thread->lock);
        return LAT_WOW64_STATUS_INVALID_STATE;
    }
    if (thread->enter_depth || thread->reply_pending) {
        g_mutex_unlock(&thread->lock);
        return LAT_WOW64_STATUS_BUSY;
    }
    thread->destroying = true;
    process = thread->process;
    g_mutex_unlock(&thread->lock);

    if (process->ops.thread_destroy) {
        process->ops.thread_destroy(process->backend_opaque, thread);
    }

    g_mutex_lock(&process->lock);
    --process->thread_count;
    g_mutex_unlock(&process->lock);
    g_mutex_clear(&thread->lock);
    g_free(thread);
    return LAT_WOW64_STATUS_OK;
}

uint64_t lat_wow64_thread_serial(const LatWow64Thread *thread)
{
    return thread ? thread->serial : 0;
}

LatWow64Status lat_wow64_thread_get_info(LatWow64Thread *thread,
                                   LatWow64ThreadInfo *info_out)
{
    if (!thread || !info_out) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }
    if (!lat_wow64_struct_valid(info_out->size, sizeof(*info_out),
                             info_out->abi_version)) {
        return LAT_WOW64_STATUS_VERSION_MISMATCH;
    }

    g_mutex_lock(&thread->lock);
    memset(info_out, 0, sizeof(*info_out));
    info_out->size = sizeof(*info_out);
    info_out->abi_version = LAT_WOW64_ABI_VERSION;
    info_out->state = lat_wow64_thread_state_locked(thread);
    info_out->enter_depth = thread->enter_depth;
    info_out->context_generation = thread->context_generation;
    info_out->pending_sequence = thread->pending_sequence;
    g_mutex_unlock(&thread->lock);
    return LAT_WOW64_STATUS_OK;
}

LatWow64Status lat_wow64_thread_suspend(LatWow64Thread *thread)
{
    if (!thread) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }

    g_mutex_lock(&thread->lock);
    if (thread->enter_depth) {
        g_mutex_unlock(&thread->lock);
        return LAT_WOW64_STATUS_BUSY;
    }
    if (thread->destroying || thread->reply_pending || thread->suspended ||
        thread->stopped) {
        g_mutex_unlock(&thread->lock);
        return LAT_WOW64_STATUS_INVALID_STATE;
    }
    thread->suspended = true;
    g_mutex_unlock(&thread->lock);
    return LAT_WOW64_STATUS_OK;
}

LatWow64Status lat_wow64_thread_resume(LatWow64Thread *thread)
{
    if (!thread) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }

    g_mutex_lock(&thread->lock);
    if (thread->enter_depth) {
        g_mutex_unlock(&thread->lock);
        return LAT_WOW64_STATUS_BUSY;
    }
    if (thread->destroying || !thread->suspended || thread->stopped) {
        g_mutex_unlock(&thread->lock);
        return LAT_WOW64_STATUS_INVALID_STATE;
    }
    thread->suspended = false;
    g_mutex_unlock(&thread->lock);
    return LAT_WOW64_STATUS_OK;
}

LatWow64Status lat_wow64_get_context(LatWow64Thread *thread,
                                    LatWow64Context *context_out)
{
    if (!thread || !context_out) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }
    g_mutex_lock(&thread->lock);
    if (thread->destroying) {
        g_mutex_unlock(&thread->lock);
        return LAT_WOW64_STATUS_INVALID_STATE;
    }
    if (thread->enter_depth) {
        g_mutex_unlock(&thread->lock);
        return LAT_WOW64_STATUS_BUSY;
    }
    *context_out = thread->context;
    g_mutex_unlock(&thread->lock);
    return LAT_WOW64_STATUS_OK;
}

LatWow64Status lat_wow64_set_context(LatWow64Thread *thread,
                                    const LatWow64Context *context)
{
    if (!thread || !context) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }
    if (!lat_wow64_struct_valid(context->size, sizeof(*context),
                             context->abi_version)) {
        return LAT_WOW64_STATUS_VERSION_MISMATCH;
    }
    if (context->dr7 & 0xff) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }
    g_mutex_lock(&thread->lock);
    if (thread->destroying) {
        g_mutex_unlock(&thread->lock);
        return LAT_WOW64_STATUS_INVALID_STATE;
    }
    if (thread->enter_depth) {
        g_mutex_unlock(&thread->lock);
        return LAT_WOW64_STATUS_BUSY;
    }
    if (context->generation != thread->context_generation) {
        g_mutex_unlock(&thread->lock);
        return LAT_WOW64_STATUS_INVALID_STATE;
    }
    thread->context = *context;
    thread->context_generation++;
    thread->context.generation = thread->context_generation;
    g_mutex_unlock(&thread->lock);
    return LAT_WOW64_STATUS_OK;
}

static LatWow64Status lat_wow64_enter_internal(LatWow64Thread *thread,
                                         uint64_t guest_pc,
                                         uint64_t expected_generation,
                                         LatWow64Exit *exit_info)
{
    LatWow64Context context;
    LatWow64Process *process;
    GThread *self;
    LatWow64Context *parent_context;
    LatWow64Status status;
    uint64_t generation;
    sigset_t suspend_set, previous_set;
    bool outer_entry;
    bool stale_after_execution = false;

    if (!thread || !exit_info ||
        (guest_pc != LAT_WOW64_ENTER_CURRENT_PC && guest_pc > UINT32_MAX)) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }
    if (!lat_wow64_struct_valid(exit_info->size, sizeof(*exit_info),
                             exit_info->abi_version)) {
        return LAT_WOW64_STATUS_VERSION_MISMATCH;
    }
    process = thread->process;
    self = g_thread_self();

    /* Wine uses SIGUSR1 to suspend threads; keep it out of locked updates. */
    sigemptyset(&suspend_set);
    sigaddset(&suspend_set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &suspend_set, &previous_set);
    g_mutex_lock(&thread->lock);
    if (thread->destroying) {
        g_mutex_unlock(&thread->lock);
        pthread_sigmask(SIG_SETMASK, &previous_set, NULL);
        return LAT_WOW64_STATUS_INVALID_STATE;
    }
    if (thread->enter_depth && thread->owner != self) {
        g_mutex_unlock(&thread->lock);
        pthread_sigmask(SIG_SETMASK, &previous_set, NULL);
        return LAT_WOW64_STATUS_BUSY;
    }
    if (thread->enter_depth >= process->max_enter_depth) {
        g_mutex_unlock(&thread->lock);
        pthread_sigmask(SIG_SETMASK, &previous_set, NULL);
        return LAT_WOW64_STATUS_BUSY;
    }
    if (!thread->enter_depth &&
        (thread->reply_pending || thread->suspended || thread->stopped)) {
        g_mutex_unlock(&thread->lock);
        pthread_sigmask(SIG_SETMASK, &previous_set, NULL);
        return LAT_WOW64_STATUS_INVALID_STATE;
    }
    outer_entry = thread->enter_depth == 0;
    thread->owner = self;
    ++thread->enter_depth;
    context = thread->active_context ? *thread->active_context :
                                       thread->context;
    if (guest_pc != LAT_WOW64_ENTER_CURRENT_PC) {
        lat_wow64_context_set_pc(&context, guest_pc);
    }
    parent_context = thread->active_context;
    thread->active_context = &context;
    g_mutex_lock(&process->lock);
    if (process->memory_barrier ||
        (expected_generation && expected_generation != process->generation)) {
        generation = process->generation;
        g_mutex_unlock(&process->lock);
        thread->active_context = parent_context;
        --thread->enter_depth;
        if (!thread->enter_depth) {
            thread->owner = NULL;
        }
        g_mutex_unlock(&thread->lock);
        pthread_sigmask(SIG_SETMASK, &previous_set, NULL);
        memset(exit_info, 0, sizeof(*exit_info));
        exit_info->size = sizeof(*exit_info);
        exit_info->abi_version = LAT_WOW64_ABI_VERSION;
        exit_info->reason = LAT_WOW64_EXIT_STALE_MAPPING;
        exit_info->mapping_generation = generation;
        return LAT_WOW64_STATUS_STALE_MAPPING;
    }
    generation = process->generation;
    if (outer_entry) {
        ++process->active_entries;
        g_private_set(&lat_wow64_active_process, process);
    }
    g_mutex_unlock(&process->lock);
    g_mutex_unlock(&thread->lock);
    pthread_sigmask(SIG_SETMASK, &previous_set, NULL);

    memset(exit_info, 0, sizeof(*exit_info));
    exit_info->size = sizeof(*exit_info);
    exit_info->abi_version = LAT_WOW64_ABI_VERSION;
    exit_info->mapping_generation = generation;
    status = process->ops.execute_guest(process->backend_opaque, thread,
                                         &context, exit_info);

    pthread_sigmask(SIG_BLOCK, &suspend_set, &previous_set);
    g_mutex_lock(&process->lock);
    /* A completed syscall or fault still belongs to the caller, even when
     * another thread invalidated code while the backend was returning. */
    if (generation != process->generation &&
        status == LAT_WOW64_STATUS_OK &&
        exit_info->reason == LAT_WOW64_EXIT_STOPPED) {
        stale_after_execution = true;
        status = LAT_WOW64_STATUS_STALE_MAPPING;
        exit_info->reason = LAT_WOW64_EXIT_STALE_MAPPING;
        exit_info->mapping_generation = process->generation;
    }
    if (!outer_entry) {
        g_mutex_unlock(&process->lock);
    }
    if (status == LAT_WOW64_STATUS_OK) {
        uint32_t reply_mask = lat_wow64_exit_reply_mask(exit_info->reason);
        uint32_t detail_flags = exit_info->flags &
            (LAT_WOW64_EXIT_FLAG_TRAP_NUMBER_VALID |
             LAT_WOW64_EXIT_FLAG_ERROR_CODE_VALID |
             LAT_WOW64_EXIT_FLAG_FAULT_ADDRESS_VALID |
             LAT_WOW64_EXIT_FLAG_SERVICE_NUMBER_VALID);

        if (reply_mask == UINT32_MAX || exit_info->reason == LAT_WOW64_EXIT_NONE ||
            (parent_context && reply_mask) || exit_info->flags != detail_flags) {
            status = LAT_WOW64_STATUS_PROTOCOL_ERROR;
            exit_info->reason = LAT_WOW64_EXIT_NONE;
            exit_info->flags = 0;
            exit_info->reply_mask = 0;
        } else {
            exit_info->flags = LAT_WOW64_EXIT_FLAG_SAFE_POINT | detail_flags;
            exit_info->sequence = ++thread->next_sequence;
            exit_info->reply_mask = reply_mask;
            if (reply_mask) {
                exit_info->flags |= LAT_WOW64_EXIT_FLAG_REPLY_REQUIRED;
            }
        }
    }

    g_mutex_lock(&thread->lock);
    thread->active_context = parent_context;
    if (!parent_context &&
        (status != LAT_WOW64_STATUS_STALE_MAPPING || stale_after_execution)) {
        *lat_wow64_context_generation(&context) =
            ++thread->context_generation;
        thread->context = context;
    }
    --thread->enter_depth;
    if (!thread->enter_depth) {
        thread->owner = NULL;
        if (status == LAT_WOW64_STATUS_OK && exit_info->reply_mask) {
            thread->reply_pending = true;
            thread->pending_sequence = exit_info->sequence;
            thread->pending_guest_pc = exit_info->guest_pc;
            thread->pending_reason = exit_info->reason;
            thread->pending_reply_mask = exit_info->reply_mask;
        }
    }
    exit_info->context_generation = thread->context_generation;
    if (outer_entry) {
        --process->active_entries;
        g_private_set(&lat_wow64_active_process, NULL);
        if (!process->active_entries) {
            g_cond_broadcast(&process->idle_cond);
        }
    }
    g_mutex_unlock(&thread->lock);
    if (outer_entry) {
        g_mutex_unlock(&process->lock);
    }
    pthread_sigmask(SIG_SETMASK, &previous_set, NULL);
    return status;
}

LatWow64Status lat_wow64_enter(LatWow64Thread *thread, uint64_t guest_pc,
                         LatWow64Exit *exit_info)
{
    return lat_wow64_enter_internal(thread, guest_pc, 0, exit_info);
}

LatWow64Status lat_wow64_reply(LatWow64Thread *thread, const LatWow64Reply *reply)
{
    if (!thread || !reply) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }
    if (!lat_wow64_struct_valid(reply->size, sizeof(*reply),
                             reply->abi_version)) {
        return LAT_WOW64_STATUS_VERSION_MISMATCH;
    }
    if (reply->flags || reply->action == LAT_WOW64_REPLY_NONE ||
        reply->action > LAT_WOW64_REPLY_REDIRECT ||
        (reply->action == LAT_WOW64_REPLY_REDIRECT ?
         reply->detail > UINT32_MAX : reply->detail != 0)) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }

    g_mutex_lock(&thread->lock);
    if (thread->destroying) {
        g_mutex_unlock(&thread->lock);
        return LAT_WOW64_STATUS_INVALID_STATE;
    }
    if (thread->enter_depth) {
        g_mutex_unlock(&thread->lock);
        return LAT_WOW64_STATUS_BUSY;
    }
    if (!thread->reply_pending || thread->stopped ||
        reply->sequence != thread->pending_sequence) {
        g_mutex_unlock(&thread->lock);
        return LAT_WOW64_STATUS_INVALID_STATE;
    }
    if (!(thread->pending_reply_mask & LAT_WOW64_REPLY_MASK(reply->action))) {
        g_mutex_unlock(&thread->lock);
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }

    if (reply->action == LAT_WOW64_REPLY_RETRY) {
        lat_wow64_context_set_pc(&thread->context,
                              thread->pending_guest_pc);
        *lat_wow64_context_generation(&thread->context) =
            ++thread->context_generation;
    } else if (reply->action == LAT_WOW64_REPLY_REDIRECT) {
        lat_wow64_context_set_pc(&thread->context, reply->detail);
        *lat_wow64_context_generation(&thread->context) =
            ++thread->context_generation;
    }
    thread->reply_pending = false;
    thread->pending_sequence = 0;
    thread->pending_guest_pc = 0;
    thread->pending_reason = LAT_WOW64_EXIT_NONE;
    thread->pending_reply_mask = 0;
    if (reply->action == LAT_WOW64_REPLY_STOP) {
        thread->stopped = true;
    }
    g_mutex_unlock(&thread->lock);
    return LAT_WOW64_STATUS_OK;
}

LatWow64Status lat_wow64_memory_event(LatWow64Process *process,
                                const LatWow64MemoryEvent *event)
{
    LatWow64Mapping mapping;
    LatWow64Status status = LAT_WOW64_STATUS_OK;
    uint64_t end, generation = 0, current_generation;
    bool invalidate = false;
    unsigned int i;

    if (!process || !event) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }
    if (!lat_wow64_struct_valid(event->size, sizeof(*event),
                             event->abi_version)) {
        return LAT_WOW64_STATUS_VERSION_MISMATCH;
    }
    if (!lat_wow64_range_valid(event->address, event->length, &end)) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }
    if (((event->type == LAT_WOW64_MEMORY_MAP ||
          event->type == LAT_WOW64_MEMORY_PROTECT) &&
         event->protection & ~(LAT_WOW64_PROT_READ | LAT_WOW64_PROT_WRITE |
                               LAT_WOW64_PROT_EXEC))) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }

    g_mutex_lock(&process->lock);
    if (g_private_get(&lat_wow64_active_process) == process) {
        g_mutex_unlock(&process->lock);
        return LAT_WOW64_STATUS_BUSY;
    }
    while (process->memory_barrier) {
        g_cond_wait(&process->idle_cond, &process->lock);
    }
    switch (event->type) {
    case LAT_WOW64_MEMORY_MAP:
        for (i = 0; i < process->mappings->len; ++i) {
            LatWow64Mapping *current = &g_array_index(process->mappings,
                                                   LatWow64Mapping, i);
            if (lat_wow64_ranges_overlap(event->address, end, current->start,
                                      current->end)) {
                status = LAT_WOW64_STATUS_OVERLAP;
                break;
            }
        }
        if (status == LAT_WOW64_STATUS_OK) {
            mapping.start = event->address;
            mapping.end = end;
            mapping.protection = event->protection;
            g_array_append_val(process->mappings, mapping);
            invalidate = event->protection & LAT_WOW64_PROT_EXEC;
        }
        break;
    case LAT_WOW64_MEMORY_PROTECT:
        if (!lat_wow64_range_fully_mapped(process, event->address, end)) {
            status = LAT_WOW64_STATUS_NOT_FOUND;
        } else {
            invalidate = lat_wow64_range_has_executable_mapping(
                             process, event->address, end) ||
                         (event->protection & LAT_WOW64_PROT_EXEC);
            lat_wow64_update_mapping_range(process, event->address, end,
                                        event->protection, false);
        }
        break;
    case LAT_WOW64_MEMORY_UNMAP:
        if (!lat_wow64_range_fully_mapped(process, event->address, end)) {
            status = LAT_WOW64_STATUS_NOT_FOUND;
        } else {
            invalidate = lat_wow64_range_has_executable_mapping(
                process, event->address, end);
            lat_wow64_update_mapping_range(process, event->address, end, 0,
                                        true);
        }
        break;
    case LAT_WOW64_MEMORY_WRITE:
    case LAT_WOW64_MEMORY_FLUSH:
        if (!lat_wow64_range_fully_mapped(process, event->address, end)) {
            status = LAT_WOW64_STATUS_NOT_FOUND;
        } else {
            invalidate = lat_wow64_range_has_executable_mapping(
                process, event->address, end);
        }
        break;
    default:
        status = LAT_WOW64_STATUS_INVALID_ARGUMENT;
        break;
    }
    if (status == LAT_WOW64_STATUS_OK) {
        /* Serialize backend publication for data mappings as well as code. */
        process->memory_barrier = true;
        if (invalidate) {
            generation = ++process->generation;
        }
    }
    current_generation = process->generation;
    g_mutex_unlock(&process->lock);

    if (status == LAT_WOW64_STATUS_OK && process->ops.memory_event) {
        process->ops.memory_event(process->backend_opaque, event,
                                  current_generation, invalidate);
    }
    if (generation && process->ops.invalidate_code) {
        process->ops.invalidate_code(process->backend_opaque, event->address,
                                     event->length, generation);
    }
    if (status == LAT_WOW64_STATUS_OK) {
        g_mutex_lock(&process->lock);
        while (generation && process->active_entries) {
            g_cond_wait(&process->idle_cond, &process->lock);
        }
        process->memory_barrier = false;
        g_cond_broadcast(&process->idle_cond);
        g_mutex_unlock(&process->lock);
    }
    return status;
}

uint64_t lat_wow64_mapping_generation(LatWow64Process *process)
{
    uint64_t generation;

    if (!process) {
        return 0;
    }
    g_mutex_lock(&process->lock);
    generation = process->generation;
    g_mutex_unlock(&process->lock);
    return generation;
}
