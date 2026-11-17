/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdbool.h>

#include <glib.h>

#include "wow64-internal.h"


typedef struct TestBackend {
    GMutex lock;
    GCond cond;
    LatWow64Process *process;
    unsigned int invalidations;
    uint64_t last_generation;
    bool block_execute;
    bool execute_started;
    bool allow_execute_return;
    bool memory_event_finished;
    bool nested;
    bool nested_outer_exception;
    bool check_destroying;
    bool use_exit_reason;
    uint32_t exit_reason;
    uint32_t exit_flags;
    uint64_t last_guest_pc;
    unsigned int thread_creates;
    LatWow64Status thread_create_status;
} TestBackend;

G_STATIC_ASSERT(sizeof(LatWow64Context) == 616);
G_STATIC_ASSERT(offsetof(LatWow64Context, fxsave) == 92);
G_STATIC_ASSERT(offsetof(LatWow64Context, generation) == 608);
G_STATIC_ASSERT(sizeof(LatWow64Exit) == 88);
G_STATIC_ASSERT(offsetof(LatWow64Exit, sequence) == 40);
G_STATIC_ASSERT(offsetof(LatWow64Exit, reply_mask) == 48);
G_STATIC_ASSERT(offsetof(LatWow64Exit, trap_number) == 56);
G_STATIC_ASSERT(offsetof(LatWow64Exit, fault_address) == 64);
G_STATIC_ASSERT(offsetof(LatWow64Exit, service_number) == 72);
G_STATIC_ASSERT(offsetof(LatWow64Exit, context_generation) == 80);
G_STATIC_ASSERT(sizeof(LatWow64Reply) == 32);
G_STATIC_ASSERT(offsetof(LatWow64Reply, sequence) == 8);
G_STATIC_ASSERT(sizeof(LatWow64ThreadInfo) == 32);
G_STATIC_ASSERT(offsetof(LatWow64ThreadInfo, context_generation) == 16);

static LatWow64Context make_context(uint64_t rax)
{
    LatWow64Context context = {
        .size = sizeof(context),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .eflags = 2,
    };

    context.gpr[LAT_WOW64_EAX] = rax;
    context.fxsave[160] = (uint8_t)rax;
    context.fxsave[32] = (uint8_t)(rax + 0x40);
    return context;
}

static LatWow64Status execute_guest(void *opaque, LatWow64Thread *thread,
                                 LatWow64Context *context,
                                 LatWow64Exit *exit_info)
{
    TestBackend *backend = opaque;
    bool nested_completed = false;

    backend->last_guest_pc = context->eip;

    g_mutex_lock(&backend->lock);
    if (backend->block_execute) {
        backend->execute_started = true;
        g_cond_broadcast(&backend->cond);
        while (!backend->allow_execute_return) {
            g_cond_wait(&backend->cond, &backend->lock);
        }
    }
    g_mutex_unlock(&backend->lock);

    if (backend->nested) {
        LatWow64ThreadInfo info = {
            .size = sizeof(info),
            .abi_version = LAT_WOW64_ABI_VERSION,
        };

        g_assert_cmpint(lat_wow64_thread_get_info(thread, &info), ==,
                        LAT_WOW64_STATUS_OK);
        g_assert_cmpuint(info.state, ==, LAT_WOW64_THREAD_RUNNING);
        g_assert_cmpuint(info.enter_depth, ==,
                         context->gpr[LAT_WOW64_EAX] == 1 ? 1 : 2);
    }

    if (backend->nested && context->gpr[LAT_WOW64_EAX] == 1) {
        LatWow64Context nested = make_context(100);
        LatWow64Context saved;
        LatWow64Exit nested_exit = {
            .size = sizeof(nested_exit),
            .abi_version = LAT_WOW64_ABI_VERSION,
        };

        g_assert_cmpint(lat_wow64_set_context(thread, &nested), ==,
                        LAT_WOW64_STATUS_BUSY);
        g_assert_cmpint(lat_wow64_thread_suspend(thread), ==,
                        LAT_WOW64_STATUS_BUSY);
        g_assert_cmpint(lat_wow64_thread_destroy(thread), ==,
                        LAT_WOW64_STATUS_BUSY);
        context->gpr[LAT_WOW64_EAX] = 100;
        g_assert_cmpint(lat_wow64_enter(thread, 0x2000, &nested_exit), ==,
                        LAT_WOW64_STATUS_OK);
        g_assert_cmpint(nested_exit.reason, ==, LAT_WOW64_EXIT_RETURN);
        g_assert_cmpint(lat_wow64_get_context(thread, &saved), ==,
                        LAT_WOW64_STATUS_BUSY);
        context->gpr[LAT_WOW64_EAX] = 2;
        nested_completed = true;
    } else {
        ++context->gpr[LAT_WOW64_EAX];
    }

    exit_info->reason = nested_completed && backend->nested_outer_exception ?
                        LAT_WOW64_EXIT_EXCEPTION :
                        (backend->use_exit_reason ? backend->exit_reason :
                                                    LAT_WOW64_EXIT_RETURN);
    exit_info->flags = backend->exit_flags;
    exit_info->guest_pc = context->eip;
    return LAT_WOW64_STATUS_OK;
}

static void invalidate_code(void *opaque, uint64_t address, uint64_t length,
                            uint64_t generation)
{
    TestBackend *backend = opaque;

    g_assert_cmphex(address, >=, 0x1000);
    g_assert_cmpuint(length, >, 0);
    g_mutex_lock(&backend->lock);
    ++backend->invalidations;
    backend->last_generation = generation;
    if (backend->block_execute) {
        g_cond_broadcast(&backend->cond);
    }
    g_mutex_unlock(&backend->lock);
}

static void destroy_thread(void *opaque, LatWow64Thread *thread)
{
    TestBackend *backend = opaque;
    LatWow64ThreadInfo info = {
        .size = sizeof(info),
        .abi_version = LAT_WOW64_ABI_VERSION,
    };
    LatWow64Context context;

    if (!backend->check_destroying) {
        return;
    }
    g_assert_cmpint(lat_wow64_thread_get_info(thread, &info), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(info.state, ==, LAT_WOW64_THREAD_DESTROYING);
    g_assert_cmpint(lat_wow64_get_context(thread, &context), ==,
                    LAT_WOW64_STATUS_INVALID_STATE);
    g_assert_cmpint(lat_wow64_thread_suspend(thread), ==,
                    LAT_WOW64_STATUS_INVALID_STATE);
    g_assert_cmpint(lat_wow64_thread_resume(thread), ==,
                    LAT_WOW64_STATUS_INVALID_STATE);
    g_assert_cmpint(lat_wow64_thread_destroy(thread), ==,
                    LAT_WOW64_STATUS_INVALID_STATE);
}

static LatWow64Status create_thread(void *opaque, LatWow64Thread *thread)
{
    TestBackend *backend = opaque;

    g_assert_cmpuint(lat_wow64_thread_serial(thread), >, 0);
    ++backend->thread_creates;
    return backend->thread_create_status;
}

static LatWow64Process *create_process(TestBackend *backend)
{
    LatWow64BackendOps ops = {
        .size = sizeof(ops),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .execute_guest = execute_guest,
        .invalidate_code = invalidate_code,
        .thread_destroy = destroy_thread,
        .thread_create = create_thread,
    };
    LatWow64ProcessConfig config = {
        .size = sizeof(config),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .max_enter_depth = 4,
    };
    LatWow64Process *process = NULL;

    g_assert_cmpint(lat_wow64_process_create(&ops, backend, &config, &process),
                    ==, LAT_WOW64_STATUS_OK);
    backend->process = process;
    return process;
}

static void test_thread_create_backend_failure(void)
{
    TestBackend backend = {
        .thread_create_status = LAT_WOW64_STATUS_NO_MEMORY,
    };
    LatWow64Process *process;
    LatWow64Context context = make_context(1);
    LatWow64Thread *thread = (void *)0xdeadbeef;

    g_mutex_init(&backend.lock);
    process = create_process(&backend);
    g_assert_cmpint(lat_wow64_thread_create(process, &context, &thread), ==,
                    LAT_WOW64_STATUS_NO_MEMORY);
    g_assert_null(thread);
    g_assert_cmpuint(backend.thread_creates, ==, 1);
    g_assert_cmpint(lat_wow64_process_destroy(process), ==, LAT_WOW64_STATUS_OK);
    g_mutex_clear(&backend.lock);
}

static void test_abi_validation(void)
{
    TestBackend backend = { 0 };
    LatWow64BackendOps ops = {
        .size = sizeof(ops),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .execute_guest = execute_guest,
    };
    LatWow64ProcessConfig config = {
        .size = sizeof(config),
        .abi_version = LAT_WOW64_ABI_VERSION + 1,
    };
    LatWow64Process *process = NULL;
    LatWow64MemoryEvent map = {
        .size = sizeof(map),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .type = LAT_WOW64_MEMORY_MAP,
        .address = 0x1000,
        .length = 0x1000,
        .protection = LAT_WOW64_PROT_READ,
    };

    g_mutex_init(&backend.lock);
    g_assert_cmpint(lat_wow64_process_create(&ops, &backend, &config, &process),
                    ==, LAT_WOW64_STATUS_VERSION_MISMATCH);
    config.abi_version = LAT_WOW64_ABI_VERSION;
    config.flags = 1u << 31;
    g_assert_cmpint(lat_wow64_process_create(&ops, &backend, &config, &process),
                    ==, LAT_WOW64_STATUS_INVALID_ARGUMENT);
    config.flags = 0;
    g_assert_cmpint(lat_wow64_process_create(&ops, &backend, &config, &process),
                    ==, LAT_WOW64_STATUS_OK);
    backend.process = process;
    g_assert_cmpint(lat_wow64_memory_event(process, &map), ==,
                    LAT_WOW64_STATUS_OK);
    map.address = 0x1800;
    g_assert_cmpint(lat_wow64_memory_event(process, &map), ==,
                    LAT_WOW64_STATUS_OVERLAP);
    map.address = 0x3000;
    map.protection = 0x80;
    g_assert_cmpint(lat_wow64_memory_event(process, &map), ==,
                    LAT_WOW64_STATUS_INVALID_ARGUMENT);
    map.abi_version = LAT_WOW64_ABI_VERSION + 1;
    g_assert_cmpint(lat_wow64_memory_event(process, &map), ==,
                    LAT_WOW64_STATUS_VERSION_MISMATCH);
    g_assert_cmpuint(lat_wow64_mapping_generation(process), ==, 1);
    g_assert_cmpint(lat_wow64_process_destroy(process), ==, LAT_WOW64_STATUS_OK);
    g_mutex_clear(&backend.lock);
}

static void test_i386_pc_bounds(void)
{
    TestBackend backend = { .use_exit_reason = true,
                            .exit_reason = LAT_WOW64_EXIT_EXCEPTION };
    LatWow64Process *process;
    LatWow64Thread *thread;
    LatWow64Context initial = make_context(1), saved;
    LatWow64Exit event = { .size = sizeof(event),
                          .abi_version = LAT_WOW64_ABI_VERSION };
    LatWow64Reply reply = { .size = sizeof(reply),
                           .abi_version = LAT_WOW64_ABI_VERSION,
                           .action = LAT_WOW64_REPLY_REDIRECT };
    const uint64_t invalid_pcs[] = { UINT64_C(0x100000000),
                                     UINT64_C(0x100001234), UINT64_MAX - 1 };
    unsigned int i;

    g_mutex_init(&backend.lock);
    process = create_process(&backend);
    g_assert_cmpint(lat_wow64_thread_create(process, &initial, &thread), ==,
                   LAT_WOW64_STATUS_OK);
    for (i = 0; i < G_N_ELEMENTS(invalid_pcs); ++i) {
        g_assert_cmpint(lat_wow64_enter(thread, invalid_pcs[i], &event), ==,
                       LAT_WOW64_STATUS_INVALID_ARGUMENT);
    }
    g_assert_cmpuint(backend.last_guest_pc, ==, 0);
    g_assert_cmpint(lat_wow64_get_context(thread, &saved), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(saved.gpr[LAT_WOW64_EAX], ==, initial.gpr[LAT_WOW64_EAX]);

    /* High-bit-set i386 PCs remain valid and must be zero-extended. */
    g_assert_cmpint(lat_wow64_enter(thread, UINT32_MAX, &event), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(backend.last_guest_pc, ==, UINT32_MAX);
    reply.sequence = event.sequence;
    for (i = 0; i < G_N_ELEMENTS(invalid_pcs); ++i) {
        reply.detail = invalid_pcs[i];
        g_assert_cmpint(lat_wow64_reply(thread, &reply), ==,
                       LAT_WOW64_STATUS_INVALID_ARGUMENT);
    }
    reply.detail = UINT32_MAX;
    g_assert_cmpint(lat_wow64_reply(thread, &reply), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_get_context(thread, &saved), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(saved.eip, ==, UINT32_MAX);
    backend.use_exit_reason = false;
    g_assert_cmpint(lat_wow64_enter(thread, LAT_WOW64_ENTER_CURRENT_PC, &event), ==,
                   LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(backend.last_guest_pc, ==, UINT32_MAX);
    g_assert_cmpint(lat_wow64_thread_destroy(thread), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_process_destroy(process), ==, LAT_WOW64_STATUS_OK);
    g_mutex_clear(&backend.lock);
}

static void test_memory_generation(void)
{
    TestBackend backend = { 0 };
    LatWow64Process *process;
    LatWow64MemoryEvent event = {
        .size = sizeof(event),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .type = LAT_WOW64_MEMORY_MAP,
        .address = 0x1000,
        .length = 0x2000,
        .protection = LAT_WOW64_PROT_READ | LAT_WOW64_PROT_EXEC,
    };

    g_mutex_init(&backend.lock);
    process = create_process(&backend);
    g_assert_cmpint(lat_wow64_memory_event(process, &event), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(lat_wow64_mapping_generation(process), ==, 2);
    g_assert_cmpuint(backend.invalidations, ==, 1);

    event.type = LAT_WOW64_MEMORY_FLUSH;
    event.length = 0x1000;
    g_assert_cmpint(lat_wow64_memory_event(process, &event), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(backend.invalidations, ==, 2);

    event.type = LAT_WOW64_MEMORY_WRITE;
    g_assert_cmpint(lat_wow64_memory_event(process, &event), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(backend.invalidations, ==, 3);

    event.type = LAT_WOW64_MEMORY_PROTECT;
    event.address = 0x1800;
    event.length = 0x800;
    event.protection = LAT_WOW64_PROT_READ;
    g_assert_cmpint(lat_wow64_memory_event(process, &event), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(backend.invalidations, ==, 4);

    event.type = LAT_WOW64_MEMORY_UNMAP;
    g_assert_cmpint(lat_wow64_memory_event(process, &event), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_memory_event(process, &event), ==,
                    LAT_WOW64_STATUS_NOT_FOUND);
    g_assert_cmpint(lat_wow64_process_destroy(process), ==, LAT_WOW64_STATUS_OK);
    g_mutex_clear(&backend.lock);
}

static void test_memory_multi_mapping_range(void)
{
    TestBackend backend = { 0 };
    LatWow64Process *process;
    LatWow64MemoryEvent event = {
        .size = sizeof(event),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .type = LAT_WOW64_MEMORY_MAP,
        .address = 0x1000,
        .length = 0x1000,
        .protection = LAT_WOW64_PROT_READ | LAT_WOW64_PROT_EXEC,
    };

    g_mutex_init(&backend.lock);
    process = create_process(&backend);
    g_assert_cmpint(lat_wow64_memory_event(process, &event), ==,
                    LAT_WOW64_STATUS_OK);
    event.address = 0x2000;
    event.protection = LAT_WOW64_PROT_READ;
    g_assert_cmpint(lat_wow64_memory_event(process, &event), ==,
                    LAT_WOW64_STATUS_OK);

    event.type = LAT_WOW64_MEMORY_FLUSH;
    event.address = 0x1800;
    event.length = 0x1000;
    g_assert_cmpint(lat_wow64_memory_event(process, &event), ==,
                    LAT_WOW64_STATUS_OK);

    event.type = LAT_WOW64_MEMORY_PROTECT;
    event.address = 0x1800;
    event.length = 0x1000;
    event.protection = LAT_WOW64_PROT_READ | LAT_WOW64_PROT_WRITE;
    g_assert_cmpint(lat_wow64_memory_event(process, &event), ==,
                    LAT_WOW64_STATUS_OK);
    event.type = LAT_WOW64_MEMORY_UNMAP;
    event.address = 0x1000;
    event.length = 0x2000;
    g_assert_cmpint(lat_wow64_memory_event(process, &event), ==,
                    LAT_WOW64_STATUS_OK);

    event.type = LAT_WOW64_MEMORY_MAP;
    event.address = 0x1000;
    event.length = 0x800;
    event.protection = 0;
    g_assert_cmpint(lat_wow64_memory_event(process, &event), ==,
                    LAT_WOW64_STATUS_OK);
    event.address = 0x2000;
    g_assert_cmpint(lat_wow64_memory_event(process, &event), ==,
                    LAT_WOW64_STATUS_OK);
    event.type = LAT_WOW64_MEMORY_PROTECT;
    event.address = 0x1000;
    event.length = 0x1800;
    g_assert_cmpint(lat_wow64_memory_event(process, &event), ==,
                    LAT_WOW64_STATUS_NOT_FOUND);
    event.type = LAT_WOW64_MEMORY_FLUSH;
    g_assert_cmpint(lat_wow64_memory_event(process, &event), ==,
                    LAT_WOW64_STATUS_NOT_FOUND);
    g_assert_cmpint(lat_wow64_process_destroy(process), ==, LAT_WOW64_STATUS_OK);
    g_mutex_clear(&backend.lock);
}

typedef struct ThreadTestData {
    LatWow64Thread *thread;
    uint64_t expected;
} ThreadTestData;

static gpointer run_thread(gpointer opaque)
{
    ThreadTestData *data = opaque;
    LatWow64Context context;
    LatWow64Exit exit_info = {
        .size = sizeof(exit_info),
        .abi_version = LAT_WOW64_ABI_VERSION,
    };

    g_assert_cmpint(lat_wow64_enter(data->thread, 0x4000, &exit_info), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpint(exit_info.reason, ==, LAT_WOW64_EXIT_RETURN);
    g_assert_cmpint(lat_wow64_get_context(data->thread, &context), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(context.gpr[LAT_WOW64_EAX], ==, data->expected);
    g_assert_cmpuint(context.fxsave[160], ==, data->expected - 1);
    g_assert_cmpuint(context.fxsave[32], ==, data->expected + 0x3f);
    g_assert_cmpuint(context.generation, ==, exit_info.context_generation);
    g_assert_cmpuint(context.generation, >, 1);
    return NULL;
}

static void test_thread_isolation(void)
{
    TestBackend backend = { 0 };
    LatWow64Process *process;
    LatWow64Context contexts[4];
    LatWow64Thread *threads[4];
    ThreadTestData data[4];
    GThread *hosts[4];
    unsigned int i;

    g_mutex_init(&backend.lock);
    process = create_process(&backend);
    for (i = 0; i < G_N_ELEMENTS(threads); ++i) {
        contexts[i] = make_context(10 + i * 10);
        g_assert_cmpint(lat_wow64_thread_create(process, &contexts[i], &threads[i]),
                        ==, LAT_WOW64_STATUS_OK);
        data[i].thread = threads[i];
        data[i].expected = 11 + i * 10;
        hosts[i] = g_thread_new("lat-wow64-simd", run_thread, &data[i]);
    }
    for (i = 0; i < G_N_ELEMENTS(hosts); ++i) g_thread_join(hosts[i]);
    g_assert_cmpint(lat_wow64_process_destroy(process), ==, LAT_WOW64_STATUS_BUSY);
    for (i = 0; i < G_N_ELEMENTS(threads); ++i)
        g_assert_cmpint(lat_wow64_thread_destroy(threads[i]), ==,
                        LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_process_destroy(process), ==, LAT_WOW64_STATUS_OK);
    g_mutex_clear(&backend.lock);
}

static void test_thread_serial(void)
{
    TestBackend backend = { 0 };
    LatWow64Process *process;
    LatWow64Context context = make_context(1);
    LatWow64Thread *first;
    LatWow64Thread *second;
    uint64_t first_serial;
    uint64_t second_serial;

    g_mutex_init(&backend.lock);
    process = create_process(&backend);
    g_assert_cmpint(lat_wow64_thread_create(process, &context, &first), ==,
                    LAT_WOW64_STATUS_OK);
    first_serial = lat_wow64_thread_serial(first);
    g_assert_cmpuint(first_serial, >, 0);
    g_assert_cmpint(lat_wow64_thread_destroy(first), ==, LAT_WOW64_STATUS_OK);

    g_assert_cmpint(lat_wow64_thread_create(process, &context, &second), ==,
                    LAT_WOW64_STATUS_OK);
    second_serial = lat_wow64_thread_serial(second);
    g_assert_cmpuint(second_serial, >, first_serial);
    g_assert_cmpint(lat_wow64_thread_destroy(second), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_process_destroy(process), ==, LAT_WOW64_STATUS_OK);
    g_mutex_clear(&backend.lock);
}

static void test_nested_enter(void)
{
    TestBackend backend = {
        .nested = true,
        .nested_outer_exception = true,
    };
    LatWow64Process *process;
    LatWow64Context initial = make_context(1);
    LatWow64Context result;
    LatWow64Thread *thread;
    LatWow64ThreadInfo info = {
        .size = sizeof(info),
        .abi_version = LAT_WOW64_ABI_VERSION,
    };
    LatWow64Exit exit_info = {
        .size = sizeof(exit_info),
        .abi_version = LAT_WOW64_ABI_VERSION,
    };

    g_mutex_init(&backend.lock);
    process = create_process(&backend);
    g_assert_cmpint(lat_wow64_thread_create(process, &initial, &thread), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_enter(thread, 0x1000, &exit_info), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(exit_info.reason, ==, LAT_WOW64_EXIT_EXCEPTION);
    g_assert_cmpint(lat_wow64_get_context(thread, &result), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(result.gpr[LAT_WOW64_EAX], ==, 2);
    g_assert_cmpuint(result.fxsave[160], ==, 1);
    g_assert_cmpuint(result.fxsave[32], ==, 0x41);
    g_assert_cmpuint(result.generation, ==, exit_info.context_generation);
    g_assert_cmpint(lat_wow64_thread_get_info(thread, &info), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(info.state, ==, LAT_WOW64_THREAD_WAITING_REPLY);
    g_assert_cmpuint(info.enter_depth, ==, 0);
    {
        LatWow64Reply reply = {
            .size = sizeof(reply),
            .abi_version = LAT_WOW64_ABI_VERSION,
            .sequence = exit_info.sequence,
            .action = LAT_WOW64_REPLY_CONTINUE,
        };

        g_assert_cmpint(lat_wow64_reply(thread, &reply), ==, LAT_WOW64_STATUS_OK);
    }
    info.size = sizeof(info);
    info.abi_version = LAT_WOW64_ABI_VERSION;
    g_assert_cmpint(lat_wow64_thread_get_info(thread, &info), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(info.state, ==, LAT_WOW64_THREAD_READY);
    g_assert_cmpint(lat_wow64_thread_destroy(thread), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_process_destroy(process), ==, LAT_WOW64_STATUS_OK);
    g_mutex_clear(&backend.lock);
}

static void test_thread_control(void)
{
    TestBackend backend = { .check_destroying = true };
    LatWow64Process *process;
    LatWow64Context initial = make_context(5);
    LatWow64Context current;
    LatWow64Context stale;
    LatWow64Thread *thread;
    LatWow64ThreadInfo info = {
        .size = sizeof(info),
        .abi_version = LAT_WOW64_ABI_VERSION,
    };
    LatWow64Exit exit_info = {
        .size = sizeof(exit_info),
        .abi_version = LAT_WOW64_ABI_VERSION,
    };

    g_mutex_init(&backend.lock);
    process = create_process(&backend);
    initial.size = 464;
    g_assert_cmpint(lat_wow64_thread_create(process, &initial, &thread), ==,
                    LAT_WOW64_STATUS_VERSION_MISMATCH);
    initial.size = sizeof(initial);
    initial.generation = 1;
    g_assert_cmpint(lat_wow64_thread_create(process, &initial, &thread), ==,
                    LAT_WOW64_STATUS_INVALID_ARGUMENT);
    initial.generation = 0;
    g_assert_cmpint(lat_wow64_thread_create(process, &initial, &thread), ==,
                    LAT_WOW64_STATUS_OK);

    g_assert_cmpint(lat_wow64_thread_get_info(thread, &info), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(info.state, ==, LAT_WOW64_THREAD_READY);
    g_assert_cmpuint(info.context_generation, ==, 1);
    stale = initial;
    g_assert_cmpint(lat_wow64_set_context(thread, &stale), ==,
                    LAT_WOW64_STATUS_INVALID_STATE);

    g_assert_cmpint(lat_wow64_get_context(thread, &current), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(current.generation, ==, 1);
    g_assert_cmpuint(current.fxsave[160], ==, 5);
    g_assert_cmpuint(current.fxsave[32], ==, 0x45);
    g_assert_cmpint(lat_wow64_thread_suspend(thread), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_thread_suspend(thread), ==,
                    LAT_WOW64_STATUS_INVALID_STATE);
    info.size = sizeof(info);
    info.abi_version = LAT_WOW64_ABI_VERSION;
    g_assert_cmpint(lat_wow64_thread_get_info(thread, &info), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(info.state, ==, LAT_WOW64_THREAD_SUSPENDED);
    g_assert_cmpint(lat_wow64_enter(thread, 0x1000, &exit_info), ==,
                    LAT_WOW64_STATUS_INVALID_STATE);

    current.gpr[LAT_WOW64_EAX] = 9;
    current.fxsave[287] = 0xa5;
    current.fxsave[159] = 0x5a;
    stale = current;
    g_assert_cmpint(lat_wow64_set_context(thread, &current), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_set_context(thread, &stale), ==,
                    LAT_WOW64_STATUS_INVALID_STATE);
    g_assert_cmpint(lat_wow64_get_context(thread, &current), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(current.generation, ==, 2);
    g_assert_cmpuint(current.fxsave[287], ==, 0xa5);
    g_assert_cmpuint(current.fxsave[159], ==, 0x5a);
    g_assert_cmpint(lat_wow64_thread_resume(thread), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_thread_resume(thread), ==,
                    LAT_WOW64_STATUS_INVALID_STATE);
    g_assert_cmpint(lat_wow64_enter(thread, 0x1000, &exit_info), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(exit_info.reason, ==, LAT_WOW64_EXIT_RETURN);
    g_assert_cmpuint(exit_info.context_generation, ==, 3);

    info.size = offsetof(LatWow64ThreadInfo, context_generation);
    info.abi_version = LAT_WOW64_ABI_VERSION;
    g_assert_cmpint(lat_wow64_thread_get_info(thread, &info), ==,
                    LAT_WOW64_STATUS_VERSION_MISMATCH);
    info.size = sizeof(info);
    info.abi_version = LAT_WOW64_ABI_VERSION + 1;
    g_assert_cmpint(lat_wow64_thread_get_info(thread, &info), ==,
                    LAT_WOW64_STATUS_VERSION_MISMATCH);

    g_assert_cmpint(lat_wow64_thread_destroy(thread), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_process_destroy(process), ==, LAT_WOW64_STATUS_OK);
    g_mutex_clear(&backend.lock);
}

static LatWow64Reply make_reply(const LatWow64Exit *exit_info,
                             LatWow64ReplyAction action)
{
    LatWow64Reply reply = {
        .size = sizeof(reply),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .sequence = exit_info->sequence,
        .action = action,
    };

    return reply;
}

typedef struct BarrierThreadData {
    TestBackend *backend;
    LatWow64Process *process;
    LatWow64Thread *thread;
    uint32_t guest_pc;
    LatWow64MemoryEvent event;
    LatWow64Status status;
    LatWow64Exit exit_info;
} BarrierThreadData;

static gpointer run_entry_handle(gpointer opaque)
{
    BarrierThreadData *data = opaque;

    data->exit_info.size = sizeof(data->exit_info);
    data->exit_info.abi_version = LAT_WOW64_ABI_VERSION;
    data->status = lat_wow64_enter(data->thread, data->guest_pc,
                                      &data->exit_info);
    return NULL;
}

static gpointer run_memory_event(gpointer opaque)
{
    BarrierThreadData *data = opaque;

    data->status = lat_wow64_memory_event(data->process, &data->event);
    g_mutex_lock(&data->backend->lock);
    data->backend->memory_event_finished = true;
    g_cond_broadcast(&data->backend->cond);
    g_mutex_unlock(&data->backend->lock);
    return NULL;
}

static void test_unload_barrier(void)
{
    TestBackend backend = { .use_exit_reason = true,
                            .exit_reason = LAT_WOW64_EXIT_STOPPED };
    LatWow64Process *process;
    LatWow64Context initial = make_context(7);
    LatWow64Thread *thread;
    LatWow64MemoryEvent map = {
        .size = sizeof(map),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .type = LAT_WOW64_MEMORY_MAP,
        .address = 0x1000,
        .length = 0x1000,
        .protection = LAT_WOW64_PROT_READ | LAT_WOW64_PROT_EXEC,
    };
    BarrierThreadData entry_data = { 0 }, event_data = { 0 };
    GThread *entry_host, *event_host;

    g_mutex_init(&backend.lock);
    g_cond_init(&backend.cond);
    process = create_process(&backend);
    g_assert_cmpint(lat_wow64_memory_event(process, &map), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_thread_create(process, &initial, &thread), ==,
                    LAT_WOW64_STATUS_OK);

    backend.block_execute = true;
    entry_data.thread = thread;
    entry_data.guest_pc = 0x1000;
    entry_host = g_thread_new("lat-wow64-entry", run_entry_handle, &entry_data);

    g_mutex_lock(&backend.lock);
    while (!backend.execute_started) {
        g_cond_wait(&backend.cond, &backend.lock);
    }
    g_mutex_unlock(&backend.lock);

    event_data.backend = &backend;
    event_data.process = process;
    event_data.event = map;
    event_data.event.type = LAT_WOW64_MEMORY_UNMAP;
    event_host = g_thread_new("lat-wow64-unload", run_memory_event, &event_data);

    g_mutex_lock(&backend.lock);
    while (backend.invalidations < 2) {
        g_cond_wait(&backend.cond, &backend.lock);
    }
    g_assert_false(backend.memory_event_finished);
    backend.allow_execute_return = true;
    g_cond_broadcast(&backend.cond);
    g_mutex_unlock(&backend.lock);

    g_thread_join(entry_host);
    g_thread_join(event_host);
    g_assert_cmpint(entry_data.status, ==, LAT_WOW64_STATUS_STALE_MAPPING);
    g_assert_cmpint(entry_data.exit_info.reason, ==,
                    LAT_WOW64_EXIT_STALE_MAPPING);
    g_assert_cmpint(event_data.status, ==, LAT_WOW64_STATUS_OK);
    g_assert_true(backend.memory_event_finished);

    g_assert_cmpint(lat_wow64_thread_destroy(thread), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_process_destroy(process), ==, LAT_WOW64_STATUS_OK);
    g_cond_clear(&backend.cond);
    g_mutex_clear(&backend.lock);
}

static void test_inflight_stale_context(gconstpointer data)
{
    uint32_t reason = GPOINTER_TO_UINT(data);
    TestBackend backend = { .use_exit_reason = true, .exit_reason = reason };
    LatWow64Process *process;
    LatWow64Context initial = make_context(7), saved;
    LatWow64Thread *thread;
    LatWow64MemoryEvent map = {
        .size = sizeof(map),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .type = LAT_WOW64_MEMORY_MAP,
        .address = 0x1000,
        .length = 0x1000,
        .protection = LAT_WOW64_PROT_READ | LAT_WOW64_PROT_EXEC,
    };
    BarrierThreadData entry_data = { 0 }, event_data = { 0 };
    GThread *entry_host, *event_host;

    g_mutex_init(&backend.lock);
    g_cond_init(&backend.cond);
    process = create_process(&backend);
    g_assert_cmpint(lat_wow64_memory_event(process, &map), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_thread_create(process, &initial, &thread), ==,
                    LAT_WOW64_STATUS_OK);

    backend.block_execute = true;
    entry_data.thread = thread;
    entry_data.guest_pc = 0x1000;
    entry_host = g_thread_new("lat-wow64-stale-entry", run_entry_handle,
                              &entry_data);

    g_mutex_lock(&backend.lock);
    while (!backend.execute_started) {
        g_cond_wait(&backend.cond, &backend.lock);
    }
    g_mutex_unlock(&backend.lock);

    event_data.backend = &backend;
    event_data.process = process;
    event_data.event = map;
    event_data.event.type = LAT_WOW64_MEMORY_FLUSH;
    event_host = g_thread_new("lat-wow64-stale-flush", run_memory_event,
                              &event_data);

    g_mutex_lock(&backend.lock);
    while (backend.invalidations < 2) {
        g_cond_wait(&backend.cond, &backend.lock);
    }
    g_assert_false(backend.memory_event_finished);
    backend.allow_execute_return = true;
    g_cond_broadcast(&backend.cond);
    g_mutex_unlock(&backend.lock);

    g_thread_join(entry_host);
    g_thread_join(event_host);
    if (reason == LAT_WOW64_EXIT_STOPPED) {
        g_assert_cmpint(entry_data.status, ==, LAT_WOW64_STATUS_STALE_MAPPING);
        g_assert_cmpuint(entry_data.exit_info.reason, ==,
                        LAT_WOW64_EXIT_STALE_MAPPING);
    } else {
        g_assert_cmpint(entry_data.status, ==, LAT_WOW64_STATUS_OK);
        g_assert_cmpuint(entry_data.exit_info.reason, ==, reason);
        if (entry_data.exit_info.reply_mask) {
            LatWow64Reply reply = {
                .size = sizeof(reply), .abi_version = LAT_WOW64_ABI_VERSION,
                .sequence = entry_data.exit_info.sequence,
                .action = LAT_WOW64_REPLY_CONTINUE,
            };
            g_assert_cmpint(lat_wow64_reply(thread, &reply), ==,
                           LAT_WOW64_STATUS_OK);
        }
    }
    g_assert_cmphex(entry_data.exit_info.guest_pc, ==, 0x1000);
    g_assert_cmpint(event_data.status, ==, LAT_WOW64_STATUS_OK);
    g_assert_true(backend.memory_event_finished);

    g_assert_cmpint(lat_wow64_get_context(thread, &saved), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmphex(saved.eip, ==, entry_data.exit_info.guest_pc);
    g_assert_cmpuint(saved.gpr[LAT_WOW64_EAX], ==, 8);

    backend.use_exit_reason = false;
    entry_data.exit_info.size = sizeof(entry_data.exit_info);
    entry_data.exit_info.abi_version = LAT_WOW64_ABI_VERSION;
    g_assert_cmpint(lat_wow64_enter(thread, saved.eip, &entry_data.exit_info), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(entry_data.exit_info.reason, ==, LAT_WOW64_EXIT_RETURN);
    g_assert_cmpint(lat_wow64_get_context(thread, &saved), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpuint(saved.gpr[LAT_WOW64_EAX], ==, 9);

    g_assert_cmpint(lat_wow64_thread_destroy(thread), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_process_destroy(process), ==, LAT_WOW64_STATUS_OK);
    g_cond_clear(&backend.cond);
    g_mutex_clear(&backend.lock);
}

static void test_exit_protocol(void)
{
    static const struct {
        LatWow64ExitReason reason;
        uint32_t reply_mask;
    } cases[] = {
        { LAT_WOW64_EXIT_RETURN, 0 },
        { LAT_WOW64_EXIT_CALLBACK,
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_CONTINUE) |
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_REDIRECT) |
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_STOP) },
        { LAT_WOW64_EXIT_SYSCALL,
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_CONTINUE) |
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_REDIRECT) |
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_STOP) },
        { LAT_WOW64_EXIT_EXCEPTION,
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_CONTINUE) |
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_NOT_HANDLED) |
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_RETRY) |
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_REDIRECT) |
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_STOP) },
        { LAT_WOW64_EXIT_BREAKPOINT,
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_CONTINUE) |
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_NOT_HANDLED) |
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_RETRY) |
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_REDIRECT) |
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_STOP) },
        { LAT_WOW64_EXIT_SINGLE_STEP,
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_CONTINUE) |
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_NOT_HANDLED) |
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_REDIRECT) |
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_STOP) },
        { LAT_WOW64_EXIT_STOPPED,
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_CONTINUE) |
          LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_STOP) },
        { LAT_WOW64_EXIT_SYSCALL_UNROUTED, 0 },
    };
    TestBackend backend = { .use_exit_reason = true };
    LatWow64Process *process;
    LatWow64Context initial = make_context(1);
    LatWow64Thread *thread;
    uint64_t last_sequence = 0;
    unsigned int i;

    g_mutex_init(&backend.lock);
    process = create_process(&backend);
    g_assert_cmpint(lat_wow64_thread_create(process, &initial, &thread), ==,
                    LAT_WOW64_STATUS_OK);

    for (i = 0; i < G_N_ELEMENTS(cases); ++i) {
        LatWow64Exit exit_info = {
            .size = sizeof(exit_info),
            .abi_version = LAT_WOW64_ABI_VERSION,
        };
        LatWow64Reply reply;

        backend.exit_reason = cases[i].reason;
        g_assert_cmpint(lat_wow64_enter(thread, 0x1000 + i * 0x10, &exit_info),
                        ==, LAT_WOW64_STATUS_OK);
        g_assert_cmpuint(exit_info.reason, ==, cases[i].reason);
        g_assert_cmpuint(exit_info.flags & LAT_WOW64_EXIT_FLAG_SAFE_POINT, !=, 0);
        g_assert_cmpuint(exit_info.sequence, >, last_sequence);
        g_assert_cmpuint(exit_info.reply_mask, ==, cases[i].reply_mask);
        last_sequence = exit_info.sequence;

        reply = make_reply(&exit_info, LAT_WOW64_REPLY_CONTINUE);
        if (!cases[i].reply_mask) {
            g_assert_cmpint(lat_wow64_reply(thread, &reply), ==,
                            LAT_WOW64_STATUS_INVALID_STATE);
            continue;
        }
        g_assert_cmpuint(exit_info.flags & LAT_WOW64_EXIT_FLAG_REPLY_REQUIRED,
                         !=, 0);
        {
            LatWow64ThreadInfo info = {
                .size = sizeof(info),
                .abi_version = LAT_WOW64_ABI_VERSION,
            };

            g_assert_cmpint(lat_wow64_thread_get_info(thread, &info), ==,
                            LAT_WOW64_STATUS_OK);
            g_assert_cmpuint(info.state, ==,
                             LAT_WOW64_THREAD_WAITING_REPLY);
            g_assert_cmpuint(info.pending_sequence, ==, exit_info.sequence);
            g_assert_cmpint(lat_wow64_thread_suspend(thread), ==,
                            LAT_WOW64_STATUS_INVALID_STATE);
            g_assert_cmpint(lat_wow64_thread_destroy(thread), ==,
                            LAT_WOW64_STATUS_BUSY);
        }
        {
            LatWow64Context safe_point_context;

            g_assert_cmpint(lat_wow64_get_context(thread, &safe_point_context),
                            ==, LAT_WOW64_STATUS_OK);
            g_assert_cmpint(lat_wow64_set_context(thread, &safe_point_context),
                            ==, LAT_WOW64_STATUS_OK);
        }
        g_assert_cmpint(lat_wow64_enter(thread, 0x2000, &exit_info), ==,
                        LAT_WOW64_STATUS_INVALID_STATE);
        if (cases[i].reply_mask &
            LAT_WOW64_REPLY_MASK(LAT_WOW64_REPLY_NOT_HANDLED)) {
            reply.action = LAT_WOW64_REPLY_NONE;
        } else {
            reply.action = LAT_WOW64_REPLY_NOT_HANDLED;
        }
        g_assert_cmpint(lat_wow64_reply(thread, &reply), ==,
                        LAT_WOW64_STATUS_INVALID_ARGUMENT);
        reply.action = LAT_WOW64_REPLY_CONTINUE;
        --reply.sequence;
        g_assert_cmpint(lat_wow64_reply(thread, &reply), ==,
                        LAT_WOW64_STATUS_INVALID_STATE);
        ++reply.sequence;
        g_assert_cmpint(lat_wow64_reply(thread, &reply), ==, LAT_WOW64_STATUS_OK);
    }

    g_assert_cmpint(lat_wow64_thread_destroy(thread), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_process_destroy(process), ==, LAT_WOW64_STATUS_OK);
    g_mutex_clear(&backend.lock);
}

static void test_invalid_protocol(void)
{
    TestBackend backend = {
        .use_exit_reason = true,
        .exit_reason = UINT32_MAX,
    };
    LatWow64Process *process;
    LatWow64Context initial = make_context(1);
    LatWow64Thread *thread;
    LatWow64Exit exit_info = {
        .size = offsetof(LatWow64Exit, trap_number),
        .abi_version = LAT_WOW64_ABI_VERSION,
    };
    LatWow64Reply reply;

    g_mutex_init(&backend.lock);
    process = create_process(&backend);
    g_assert_cmpint(lat_wow64_thread_create(process, &initial, &thread), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_enter(thread, 0x1000, &exit_info), ==,
                    LAT_WOW64_STATUS_VERSION_MISMATCH);
    exit_info.size = sizeof(exit_info);
    exit_info.abi_version = LAT_WOW64_ABI_VERSION + 1;
    g_assert_cmpint(lat_wow64_enter(thread, 0x1000, &exit_info), ==,
                    LAT_WOW64_STATUS_VERSION_MISMATCH);
    exit_info.abi_version = LAT_WOW64_ABI_VERSION;
    g_assert_cmpint(lat_wow64_enter(thread, 0x1000, &exit_info), ==,
                    LAT_WOW64_STATUS_PROTOCOL_ERROR);
    g_assert_cmpuint(exit_info.reason, ==, LAT_WOW64_EXIT_NONE);

    backend.exit_reason = LAT_WOW64_EXIT_SYSCALL;
    backend.exit_flags = 1u << 31;
    g_assert_cmpint(lat_wow64_enter(thread, 0x1000, &exit_info), ==,
                    LAT_WOW64_STATUS_PROTOCOL_ERROR);
    backend.exit_flags = 0;
    exit_info.size = sizeof(exit_info);
    exit_info.abi_version = LAT_WOW64_ABI_VERSION;
    g_assert_cmpint(lat_wow64_enter(thread, 0x1000, &exit_info), ==,
                    LAT_WOW64_STATUS_OK);
    reply = make_reply(&exit_info, LAT_WOW64_REPLY_NOT_HANDLED);
    g_assert_cmpint(lat_wow64_reply(thread, &reply), ==,
                    LAT_WOW64_STATUS_INVALID_ARGUMENT);
    reply.action = LAT_WOW64_REPLY_CONTINUE;
    reply.flags = 1;
    g_assert_cmpint(lat_wow64_reply(thread, &reply), ==,
                    LAT_WOW64_STATUS_INVALID_ARGUMENT);
    reply.flags = 0;
    reply.detail = 1;
    g_assert_cmpint(lat_wow64_reply(thread, &reply), ==,
                    LAT_WOW64_STATUS_INVALID_ARGUMENT);
    reply.action = LAT_WOW64_REPLY_REDIRECT;
    reply.detail = LAT_WOW64_ENTER_CURRENT_PC;
    g_assert_cmpint(lat_wow64_reply(thread, &reply), ==,
                    LAT_WOW64_STATUS_INVALID_ARGUMENT);
    reply.action = LAT_WOW64_REPLY_CONTINUE;
    reply.detail = 0;
    reply.size = offsetof(LatWow64Reply, detail);
    g_assert_cmpint(lat_wow64_reply(thread, &reply), ==,
                    LAT_WOW64_STATUS_VERSION_MISMATCH);
    reply.size = sizeof(reply);
    reply.abi_version = LAT_WOW64_ABI_VERSION + 1;
    g_assert_cmpint(lat_wow64_reply(thread, &reply), ==,
                    LAT_WOW64_STATUS_VERSION_MISMATCH);
    reply.abi_version = LAT_WOW64_ABI_VERSION;
    reply.action = LAT_WOW64_REPLY_STOP;
    g_assert_cmpint(lat_wow64_reply(thread, &reply), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_enter(thread, 0x1000, &exit_info), ==,
                    LAT_WOW64_STATUS_INVALID_STATE);
    {
        LatWow64ThreadInfo info = {
            .size = sizeof(info),
            .abi_version = LAT_WOW64_ABI_VERSION,
        };

        g_assert_cmpint(lat_wow64_thread_get_info(thread, &info), ==,
                        LAT_WOW64_STATUS_OK);
        g_assert_cmpuint(info.state, ==, LAT_WOW64_THREAD_STOPPED);
        g_assert_cmpint(lat_wow64_thread_resume(thread), ==,
                        LAT_WOW64_STATUS_INVALID_STATE);
    }

    g_assert_cmpint(lat_wow64_thread_destroy(thread), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_process_destroy(process), ==, LAT_WOW64_STATUS_OK);
    g_mutex_clear(&backend.lock);
}

static void test_resume_actions(void)
{
    TestBackend backend = {
        .use_exit_reason = true,
        .exit_reason = LAT_WOW64_EXIT_EXCEPTION,
    };
    LatWow64Process *process;
    LatWow64Context context = make_context(1);
    LatWow64Thread *thread;
    LatWow64Exit exit_info = {
        .size = sizeof(exit_info),
        .abi_version = LAT_WOW64_ABI_VERSION,
    };
    LatWow64Reply reply;

    g_mutex_init(&backend.lock);
    process = create_process(&backend);
    g_assert_cmpint(lat_wow64_thread_create(process, &context, &thread), ==,
                    LAT_WOW64_STATUS_OK);

    g_assert_cmpint(lat_wow64_enter(thread, 0x1000, &exit_info), ==,
                    LAT_WOW64_STATUS_OK);
    reply = make_reply(&exit_info, LAT_WOW64_REPLY_RETRY);
    g_assert_cmpint(lat_wow64_reply(thread, &reply), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_get_context(thread, &context), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmphex(context.eip, ==, 0x1000);
    g_assert_cmpint(lat_wow64_enter(thread, LAT_WOW64_ENTER_CURRENT_PC, &exit_info),
                    ==, LAT_WOW64_STATUS_OK);
    g_assert_cmphex(backend.last_guest_pc, ==, 0x1000);

    reply = make_reply(&exit_info, LAT_WOW64_REPLY_REDIRECT);
    reply.detail = 0x3000;
    g_assert_cmpint(lat_wow64_reply(thread, &reply), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_get_context(thread, &context), ==,
                    LAT_WOW64_STATUS_OK);
    g_assert_cmphex(context.eip, ==, 0x3000);
    g_assert_cmpint(lat_wow64_enter(thread, LAT_WOW64_ENTER_CURRENT_PC, &exit_info),
                    ==, LAT_WOW64_STATUS_OK);
    g_assert_cmphex(backend.last_guest_pc, ==, 0x3000);

    g_assert_cmpint(lat_wow64_get_context(thread, &context), ==,
                    LAT_WOW64_STATUS_OK);
    context.eip = 0x4000;
    g_assert_cmpint(lat_wow64_set_context(thread, &context), ==,
                    LAT_WOW64_STATUS_OK);
    reply = make_reply(&exit_info, LAT_WOW64_REPLY_CONTINUE);
    g_assert_cmpint(lat_wow64_reply(thread, &reply), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_enter(thread, LAT_WOW64_ENTER_CURRENT_PC, &exit_info),
                    ==, LAT_WOW64_STATUS_OK);
    g_assert_cmphex(backend.last_guest_pc, ==, 0x4000);
    reply = make_reply(&exit_info, LAT_WOW64_REPLY_NOT_HANDLED);
    g_assert_cmpint(lat_wow64_reply(thread, &reply), ==, LAT_WOW64_STATUS_OK);

    g_assert_cmpint(lat_wow64_thread_destroy(thread), ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_process_destroy(process), ==, LAT_WOW64_STATUS_OK);
    g_mutex_clear(&backend.lock);
}

typedef struct MemoryOrder {
    GMutex lock;
    GCond cond;
    LatWow64Process *process;
    bool unmap_waiting;
    bool release_unmap;
    bool reordered;
    bool second_started;
    LatWow64Status status[2];
} MemoryOrder;

static void ordered_memory_event(void *opaque, const LatWow64MemoryEvent *event,
                                 uint64_t generation, int invalidate)
{
    MemoryOrder *order = opaque;

    g_mutex_lock(&order->lock);
    if (event->type == LAT_WOW64_MEMORY_UNMAP) {
        order->unmap_waiting = true;
        g_cond_broadcast(&order->cond);
        while (!order->release_unmap) {
            g_cond_wait(&order->cond, &order->lock);
        }
    } else if (order->unmap_waiting && !order->release_unmap) {
        order->reordered = true;
        g_cond_broadcast(&order->cond);
    }
    g_mutex_unlock(&order->lock);
}

static gpointer ordered_update(gpointer opaque)
{
    MemoryOrder *order = opaque;
    LatWow64MemoryEvent event = {
        .size = sizeof(event), .abi_version = LAT_WOW64_ABI_VERSION,
        .type = LAT_WOW64_MEMORY_UNMAP, .address = 0x1000, .length = 0x1000,
    };
    order->status[0] = lat_wow64_memory_event(order->process, &event);
    return NULL;
}

static gpointer ordered_remap(gpointer opaque)
{
    MemoryOrder *order = opaque;
    LatWow64MemoryEvent event = {
        .size = sizeof(event), .abi_version = LAT_WOW64_ABI_VERSION,
        .type = LAT_WOW64_MEMORY_MAP, .address = 0x1000, .length = 0x1000,
        .protection = LAT_WOW64_PROT_READ | LAT_WOW64_PROT_EXEC,
    };
    g_mutex_lock(&order->lock);
    order->second_started = true;
    g_cond_broadcast(&order->cond);
    g_mutex_unlock(&order->lock);
    order->status[1] = lat_wow64_memory_event(order->process, &event);
    return NULL;
}

static void test_memory_event_order(void)
{
    MemoryOrder order = {0};
    LatWow64BackendOps ops = {
        .size = sizeof(ops), .abi_version = LAT_WOW64_ABI_VERSION,
        .execute_guest = execute_guest, .memory_event = ordered_memory_event,
    };
    LatWow64ProcessConfig config = {
        .size = sizeof(config), .abi_version = LAT_WOW64_ABI_VERSION,
    };
    LatWow64MemoryEvent event = {
        .size = sizeof(event), .abi_version = LAT_WOW64_ABI_VERSION,
        .type = LAT_WOW64_MEMORY_MAP, .address = 0x1000, .length = 0x1000,
        .protection = LAT_WOW64_PROT_READ,
    };
    GThread *unmap, *remap;
    gint64 deadline;

    g_mutex_init(&order.lock);
    g_cond_init(&order.cond);
    g_assert_cmpint(lat_wow64_process_create(&ops, &order, &config, &order.process),
                    ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_memory_event(order.process, &event), ==,
                    LAT_WOW64_STATUS_OK);
    unmap = g_thread_new("unmap", ordered_update, &order);
    g_mutex_lock(&order.lock);
    while (!order.unmap_waiting) g_cond_wait(&order.cond, &order.lock);
    g_mutex_unlock(&order.lock);
    remap = g_thread_new("remap", ordered_remap, &order);
    g_mutex_lock(&order.lock);
    while (!order.second_started) g_cond_wait(&order.cond, &order.lock);
    deadline = g_get_monotonic_time() + 100 * G_TIME_SPAN_MILLISECOND;
    while (!order.reordered && g_cond_wait_until(&order.cond, &order.lock, deadline)) {}
    order.release_unmap = true;
    g_cond_broadcast(&order.cond);
    g_mutex_unlock(&order.lock);
    g_thread_join(unmap);
    g_thread_join(remap);
    g_assert_false(order.reordered);
    g_assert_cmpint(order.status[0], ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(order.status[1], ==, LAT_WOW64_STATUS_OK);
    g_assert_cmpint(lat_wow64_process_destroy(order.process), ==, LAT_WOW64_STATUS_OK);
    g_cond_clear(&order.cond);
    g_mutex_clear(&order.lock);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/lat-wow64/abi/validation", test_abi_validation);
    g_test_add_func("/lat-wow64/abi/i386-pc-bounds", test_i386_pc_bounds);
    g_test_add_func("/lat-wow64/memory/generation", test_memory_generation);
    g_test_add_func("/lat-wow64/memory/event-order", test_memory_event_order);
    g_test_add_func("/lat-wow64/memory/multi-mapping-range",
                    test_memory_multi_mapping_range);
    g_test_add_func("/lat-wow64/thread/isolation", test_thread_isolation);
    g_test_add_func("/lat-wow64/thread/serial", test_thread_serial);
    g_test_add_func("/lat-wow64/thread/backend-create-failure",
                    test_thread_create_backend_failure);
    g_test_add_func("/lat-wow64/thread/nested-enter", test_nested_enter);
    g_test_add_func("/lat-wow64/thread/control", test_thread_control);
    g_test_add_func("/lat-wow64/memory/unload-barrier", test_unload_barrier);
    g_test_add_data_func("/lat-wow64/memory/inflight-stale-context",
                        GUINT_TO_POINTER(LAT_WOW64_EXIT_STOPPED),
                        test_inflight_stale_context);
    g_test_add_data_func("/lat-wow64/memory/inflight-syscall",
                        GUINT_TO_POINTER(LAT_WOW64_EXIT_SYSCALL),
                        test_inflight_stale_context);
    g_test_add_data_func("/lat-wow64/memory/inflight-exception",
                        GUINT_TO_POINTER(LAT_WOW64_EXIT_EXCEPTION),
                        test_inflight_stale_context);
    g_test_add_data_func("/lat-wow64/memory/inflight-breakpoint",
                        GUINT_TO_POINTER(LAT_WOW64_EXIT_BREAKPOINT),
                        test_inflight_stale_context);
    g_test_add_data_func("/lat-wow64/memory/inflight-return",
                        GUINT_TO_POINTER(LAT_WOW64_EXIT_RETURN),
                        test_inflight_stale_context);
    g_test_add_func("/lat-wow64/exit/protocol", test_exit_protocol);
    g_test_add_func("/lat-wow64/exit/invalid-protocol",
                    test_invalid_protocol);
    g_test_add_func("/lat-wow64/exit/resume-actions", test_resume_actions);
    return g_test_run();
}
