/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "latx-wow64.h"

static const LatWow64Api *api;
static volatile sig_atomic_t host_signals;
static volatile sig_atomic_t restart_signals;

static void host_signal(int sig)
{
    if (sig == SIGUSR1) {
        ++host_signals;
    } else if (sig == SIGRTMIN + 1) {
        ++restart_signals;
    }
}

static void ok(LatWow64Status status)
{
    if (status != LAT_WOW64_STATUS_OK) {
        fprintf(stderr, "WoW64 provider status: %d\n", status);
        _exit(1);
    }
}

static void reply(LatWow64Thread *thread, const LatWow64Exit *event)
{
    LatWow64Reply r = {
        .size = sizeof(r),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .sequence = event->sequence,
        .action = LAT_WOW64_REPLY_CONTINUE,
    };
    ok(api->reply(thread, &r));
}

struct worker {
    LatWow64Thread *thread;
    uintptr_t pc;
    uint32_t expected;
};

static void *run_worker(void *arg)
{
    struct worker *worker = arg;
    LatWow64Context context;
    LatWow64Exit event = {
        .size = sizeof(event), .abi_version = LAT_WOW64_ABI_VERSION,
    };

    do {
        LatWow64Status status = api->enter(worker->thread, worker->pc, &event);
        assert(status == LAT_WOW64_STATUS_OK ||
               status == LAT_WOW64_STATUS_STALE_MAPPING);
        if (event.reason == LAT_WOW64_EXIT_STOPPED) reply(worker->thread, &event);
        worker->pc = LAT_WOW64_ENTER_CURRENT_PC;
    } while (event.reason == LAT_WOW64_EXIT_STOPPED ||
             event.reason == LAT_WOW64_EXIT_STALE_MAPPING);
    assert(event.reason == LAT_WOW64_EXIT_SYSCALL);
    ok(api->get_context(worker->thread, &context));
    assert(context.gpr[LAT_WOW64_EAX] == worker->expected);
    reply(worker->thread, &event);
    return NULL;
}

static __thread LatWow64Thread *interrupted_thread;

static void interrupt_guest(int signal, siginfo_t *info, void *context)
{
    int saved_errno = errno;
    (void)signal;
    (void)info;
    api->request_interrupt(interrupted_thread, context);
    errno = saved_errno;
}

static void *run_until_interrupted(void *opaque)
{
    struct worker *worker = opaque;
    LatWow64Exit event = { .size = sizeof(event), .abi_version = LAT_WOW64_ABI_VERSION };
    interrupted_thread = worker->thread;
    ok(api->enter(worker->thread, worker->pc, &event));
    assert(event.reason == LAT_WOW64_EXIT_STOPPED);
    reply(worker->thread, &event);
    return NULL;
}

struct host_read {
    int fd;
    int ready;
    ssize_t result;
    int error;
};

static void *host_read(void *opaque)
{
    struct host_read *call = opaque;
    char byte;
    __atomic_store_n(&call->ready, 1, __ATOMIC_RELEASE);
    call->result = read(call->fd, &byte, 1);
    call->error = errno;
    return NULL;
}

static void test_host_restart(void)
{
    int pipefd[2];
    pthread_t host;
    struct host_read call = { 0 };
    assert(!pipe(pipefd));
    call.fd = pipefd[0];
    assert(!pthread_create(&host, NULL, host_read, &call));
    while (!__atomic_load_n(&call.ready, __ATOMIC_ACQUIRE)) usleep(100);
    usleep(20000);
    assert(!pthread_kill(host, SIGRTMIN + 1));
    while (!restart_signals) usleep(100);
    assert(write(pipefd[1], "x", 1) == 1);
    assert(!pthread_join(host, NULL));
    fprintf(stderr, "host read after forwarded signal: result=%zd errno=%d\n",
            call.result, call.error);
    assert(call.result == 1);
    close(pipefd[0]);
    close(pipefd[1]);
}

static void *load_provider(void *path)
{
    return dlopen(path, RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
}

static void test_cache_pressure(LatWow64Process *process,
                                LatWow64Thread *thread,
                                LatWow64Context *context, size_t page)
{
    const unsigned int blocks = 1u << 22;
    size_t length = ((size_t)blocks * 8 + 2 + page - 1) & ~(page - 1);
    unsigned char *code = mmap((void *)0x38000000, length,
                              PROT_READ | PROT_WRITE | PROT_EXEC,
                              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                              -1, 0);
    LatWow64MemoryEvent map = {
        .size = sizeof(map), .abi_version = LAT_WOW64_ABI_VERSION,
        .type = LAT_WOW64_MEMORY_MAP, .address = (uintptr_t)code,
        .length = length,
        .protection = LAT_WOW64_PROT_READ | LAT_WOW64_PROT_WRITE |
                      LAT_WOW64_PROT_EXEC,
    };
    LatWow64Exit event = {
        .size = sizeof(event), .abi_version = LAT_WOW64_ABI_VERSION,
    };
    unsigned int stops = 0;
    uintptr_t pc = (uintptr_t)code;
    assert(code != MAP_FAILED);
    for (unsigned int i = 0; i < blocks; ++i) {
        /* inc eax; add edx,8; jmp edx: indirect targets prevent TU joining. */
        memcpy(code + i * 8, "\x40\x83\xc2\x08\xff\xe2\x90\x90", 8);
    }
    code[blocks * 8] = 0xcd;
    code[blocks * 8 + 1] = 0x80;
    ok(api->memory_event(process, &map));
    ok(api->get_context(thread, context));
    context->gpr[LAT_WOW64_EAX] = 0;
    context->gpr[LAT_WOW64_EDX] = (uintptr_t)code;
    ok(api->set_context(thread, context));
    for (;;) {
        ok(api->enter(thread, pc, &event));
        pc = LAT_WOW64_ENTER_CURRENT_PC;
        if (event.reason != LAT_WOW64_EXIT_STOPPED) break;
        assert(++stops < 100);
        reply(thread, &event);
    }
    fprintf(stderr, "cache pressure: stops=%u service=%u\n", stops,
            event.service_number);
    assert(stops && event.reason == LAT_WOW64_EXIT_SYSCALL);
    assert(event.service_number == blocks);
    reply(thread, &event);
    map.type = LAT_WOW64_MEMORY_UNMAP;
    ok(api->memory_event(process, &map));
    assert(!munmap(code, length));
}

static void test_i386_address_width(LatWow64Process *process,
                                    LatWow64Thread *thread,
                                    unsigned char *memory, size_t page)
{
    unsigned char wrap[] = { 0xbb,0xf0,0xff,0xff,0xff,0x8b,0x83,0,0,0,0,0xcd,0x80 };
    unsigned char high_code[] = {
        0xc7,0x05,0,0,0,0,0x78,0x56,0x34,0x12,0xa1,0,0,0,0,0xcd,0x80,
    };
    LatWow64Exit event = { .size = sizeof(event), .abi_version = LAT_WOW64_ABI_VERSION };
    LatWow64Context context;
    LatWow64MemoryEvent map = {
        .size = sizeof(map), .abi_version = LAT_WOW64_ABI_VERSION,
        .type = LAT_WOW64_MEMORY_WRITE, .address = (uintptr_t)memory, .length = page,
    };
    uint32_t address = (uintptr_t)(memory + page + 768);
    uint32_t displacement = address + 16;
    unsigned char *high;

    *(uint32_t *)(uintptr_t)address = 0x76543210;
    memcpy(wrap + 7, &displacement, sizeof(displacement));
    memcpy(memory + 192, wrap, sizeof(wrap));
    ok(api->memory_event(process, &map));
    ok(api->enter(thread, (uintptr_t)(memory + 192), &event));
    assert(event.reason == LAT_WOW64_EXIT_SYSCALL && event.service_number == 0x76543210);
    reply(thread, &event);

    high = mmap((void *)0xf0000000, page, PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    assert(high != MAP_FAILED);
    address = (uintptr_t)(high + page - 4);
    memcpy(high_code + 2, &address, sizeof(address));
    memcpy(high_code + 11, &address, sizeof(address));
    memcpy(high, high_code, sizeof(high_code));
    map.type = LAT_WOW64_MEMORY_MAP;
    map.address = (uintptr_t)high;
    map.protection = LAT_WOW64_PROT_READ | LAT_WOW64_PROT_WRITE | LAT_WOW64_PROT_EXEC;
    ok(api->memory_event(process, &map));
    ok(api->enter(thread, (uintptr_t)high, &event));
    assert(event.reason == LAT_WOW64_EXIT_SYSCALL && event.service_number == 0x12345678);
    ok(api->get_context(thread, &context));
    assert(context.eip == (uintptr_t)high + sizeof(high_code));
    reply(thread, &event);
    map.type = LAT_WOW64_MEMORY_UNMAP;
    ok(api->memory_event(process, &map));
    assert(!munmap(high, page));
    puts("i386 address wrap and high-bit addresses PASS");
}

static void test_i386_execute_permissions(LatWow64Process *process,
                                         LatWow64Thread *thread,
                                         unsigned char *memory, size_t page)
{
    unsigned char *code = mmap((void *)0x40000000, 2 * page,
                               PROT_READ | PROT_WRITE | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    unsigned char *next = code + page;
    LatWow64Exit event = { .size = sizeof(event), .abi_version = LAT_WOW64_ABI_VERSION };
    LatWow64MemoryEvent map = {
        .size = sizeof(map), .abi_version = LAT_WOW64_ABI_VERSION,
        .type = LAT_WOW64_MEMORY_MAP, .address = (uintptr_t)code, .length = 2 * page,
        .protection = LAT_WOW64_PROT_READ | LAT_WOW64_PROT_WRITE | LAT_WOW64_PROT_EXEC,
    };
    LatWow64Context saved, context;
    uint32_t address = (uintptr_t)next;

    assert(code != MAP_FAILED);
    ok(api->get_context(thread, &saved));
    memset(code, 0x90, 2 * page);
    memcpy(next, "\xcd\x80", 2);
    /* An indirect jump must report the destination, not the jump's PC. */
    code[0] = 0xb8;
    memcpy(code + 1, &address, 4);
    memcpy(code + 5, "\xff\xe0", 2);
    ok(api->memory_event(process, &map));
    map.type = LAT_WOW64_MEMORY_PROTECT;
    map.address = (uintptr_t)next;
    map.length = page;
    map.protection &= ~LAT_WOW64_PROT_EXEC;
    ok(api->memory_event(process, &map));
    for (unsigned int kind = 0; kind < 4; ++kind) {
        uintptr_t pc = kind == 0 ? (uintptr_t)next :
                       kind == 1 ? (uintptr_t)code : (uintptr_t)(next - 2);
        if (kind == 3) {
            /* mov eax,imm32 straddles the RX/RW boundary. */
            next[-2] = 0xb8;
            next[-1] = 0x12;
        }
        LatWow64MemoryEvent write = map;
        write.type = LAT_WOW64_MEMORY_WRITE;
        write.address = (uintptr_t)code;
        write.length = 2 * page;
        ok(api->memory_event(process, &write));
        ok(api->enter(thread, pc, &event));
        fprintf(stderr, "execute permission case %u: trap=%u pc=%#llx address=%#llx error=%#x\n",
                kind, event.trap_number, (unsigned long long)event.guest_pc,
                (unsigned long long)event.fault_address, event.error_code);
        assert(event.reason == LAT_WOW64_EXIT_EXCEPTION && event.trap_number == 14);
        assert(event.guest_pc == (kind == 3 ? (uintptr_t)(next - 2) : (uintptr_t)next));
        assert(event.fault_address == (uintptr_t)next && (event.error_code & 0x14) == 0x14);
        reply(thread, &event);
    }
    /* The same crossing instruction succeeds once both pages are executable. */
    memcpy(next, "\x34\x56\x78\xcd\x80", 5);
    map.protection |= LAT_WOW64_PROT_EXEC;
    ok(api->memory_event(process, &map));
    ok(api->enter(thread, (uintptr_t)(next - 2), &event));
    fprintf(stderr, "cross-page executable: reason=%u trap=%u service=%#llx pc=%#llx\n",
            event.reason, event.trap_number, (unsigned long long)event.service_number,
            (unsigned long long)event.guest_pc);
    assert(event.reason == LAT_WOW64_EXIT_SYSCALL && event.service_number == 0x78563412);
    reply(thread, &event);
    /* Previously translated code must stop executing after EXEC is removed. */
    map.protection &= ~LAT_WOW64_PROT_EXEC;
    ok(api->memory_event(process, &map));
    ok(api->enter(thread, (uintptr_t)(next - 2), &event));
    assert(event.reason == LAT_WOW64_EXIT_EXCEPTION && event.trap_number == 14);
    reply(thread, &event);

    /* A final RET does not fetch any byte from the following RW/NOACCESS page. */
    next[-1] = 0xc3;
    memcpy(code + 64, "\xb8\x2a\0\0\0\xcd\x80", 7);
    for (unsigned int noaccess = 0; noaccess < 2; ++noaccess) {
        LatWow64MemoryEvent write = map;
        write.type = LAT_WOW64_MEMORY_WRITE;
        write.address = (uintptr_t)code;
        ok(api->memory_event(process, &write));
        map.protection = noaccess ? 0 : LAT_WOW64_PROT_READ | LAT_WOW64_PROT_WRITE;
        ok(api->memory_event(process, &map));
        ok(api->get_context(thread, &context));
        context.gpr[LAT_WOW64_ESP] = (uintptr_t)(memory + page + 1024);
        *(uint32_t *)(memory + page + 1024) = (uintptr_t)(code + 64);
        ok(api->set_context(thread, &context));
        ok(api->enter(thread, (uintptr_t)(next - 1), &event));
        assert(event.reason == LAT_WOW64_EXIT_SYSCALL && event.service_number == 42);
        reply(thread, &event);
    }
    ok(api->get_context(thread, &context));
    saved.generation = context.generation;
    ok(api->set_context(thread, &saved));
    map.type = LAT_WOW64_MEMORY_UNMAP;
    map.address = (uintptr_t)code;
    map.length = 2 * page;
    ok(api->memory_event(process, &map));
    assert(!munmap(code, 2 * page));
    puts("i386 execute permissions and page boundaries PASS");
}

static void test_i386_x87_fault_retry(LatWow64Process *process,
                                     LatWow64Thread *thread,
                                     unsigned char *memory, size_t page)
{
    unsigned char fild[] = {0xdb, 0x05, 0, 0, 0, 0, 0xcd, 0x80};
    unsigned char *operand = mmap((void *)0x50000000, page, PROT_NONE,
                                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    uint32_t address = (uintptr_t)operand;
    LatWow64Context context, saved;
    LatWow64Exit event = { .size = sizeof(event), .abi_version = LAT_WOW64_ABI_VERSION };
    LatWow64MemoryEvent map = {
        .size = sizeof(map), .abi_version = LAT_WOW64_ABI_VERSION,
        .type = LAT_WOW64_MEMORY_MAP, .address = (uintptr_t)operand, .length = page,
    };
    LatWow64MemoryEvent write = map;

    assert(operand != MAP_FAILED);
    ok(api->get_context(thread, &saved));
    context = saved;
    memset(context.fxsave, 0, sizeof(context.fxsave));
    context.fxsave[0] = 0x7f;
    context.fxsave[1] = 3;
    context.fxsave[3] = 0x18; /* TOP=3, empty stack. */
    context.fxsave[24] = 0x80;
    context.fxsave[25] = 0x1f;
    ok(api->set_context(thread, &context));
    memcpy(fild + 2, &address, sizeof(address));
    memcpy(memory + 128, fild, sizeof(fild));
    write.type = LAT_WOW64_MEMORY_WRITE;
    write.address = (uintptr_t)memory;
    ok(api->memory_event(process, &write));
    ok(api->memory_event(process, &map));
    ok(api->enter(thread, (uintptr_t)(memory + 128), &event));
    assert(event.reason == LAT_WOW64_EXIT_EXCEPTION && event.trap_number == 14);
    assert(event.guest_pc == (uintptr_t)(memory + 128) && event.fault_address == address);
    ok(api->get_context(thread, &context));
    fprintf(stderr, "FILD fault TOP=%u (expected 3)\n", (context.fxsave[3] >> 3) & 7);
    assert(((context.fxsave[3] >> 3) & 7) == 3);
    reply(thread, &event);
    assert(!mprotect(operand, page, PROT_READ | PROT_WRITE));
    *(int32_t *)operand = 42;
    map.type = LAT_WOW64_MEMORY_PROTECT;
    map.protection = LAT_WOW64_PROT_READ | LAT_WOW64_PROT_WRITE;
    ok(api->memory_event(process, &map));
    ok(api->enter(thread, LAT_WOW64_ENTER_CURRENT_PC, &event));
    assert(event.reason == LAT_WOW64_EXIT_SYSCALL);
    ok(api->get_context(thread, &context));
    assert(((context.fxsave[3] >> 3) & 7) == 2);
    assert(context.fxsave[39] == 0xa8 && context.fxsave[40] == 4 && context.fxsave[41] == 0x40);
    reply(thread, &event);
    saved.generation = context.generation;
    ok(api->set_context(thread, &saved));
    map.type = LAT_WOW64_MEMORY_UNMAP;
    ok(api->memory_event(process, &map));
    assert(!munmap(operand, page));
    puts("x87 FILD page fault and retry PASS");
}

extern unsigned long wow64_check_host_fprs(LatWow64Status (*callback)(void *),
                                           void *opaque,
                                           LatWow64Status *status);
struct checked_call {
    LatWow64ProcessConfig *config;
    LatWow64Process **process;
    LatWow64Context *context;
    LatWow64Thread **thread;
    uintptr_t pc;
    LatWow64Exit *event;
};

static LatWow64Status checked_create_process(void *opaque)
{
    struct checked_call *call = opaque;
    return api->process_create(call->config, call->process);
}

static LatWow64Status checked_create_thread(void *opaque)
{
    struct checked_call *call = opaque;
    return api->thread_create(*call->process, call->context, call->thread);
}

static LatWow64Status checked_enter(void *opaque)
{
    struct checked_call *call = opaque;
    return api->enter(*call->thread, call->pc, call->event);
}

static void checked(LatWow64Status (*callback)(void *), struct checked_call *call)
{
    LatWow64Status status;
    assert(wow64_check_host_fprs(callback, call, &status) == 0);
    ok(status);
}

int main(int argc, char **argv)
{
    /* mov eax,42; mov edx,fs:[0]; int 80; int3 */
    static const unsigned char code[] = {
        0xb8, 42, 0, 0, 0, 0x0f, 0xa0, 0x0f, 0xa1, 0x64, 0x8b, 0x15, 0, 0, 0, 0,
        0xcd, 0x80, 0xcc,
    };
    const LatWow64Api *(*get_api)(uint32_t);
    LatWow64Process *process;
    LatWow64Thread *thread;
    LatWow64Context context = {
        .size = sizeof(context),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .eflags = 0x202,
        .cs = 0x23, .ss = 0x2b, .ds = 0x2b, .es = 0x2b, .fs = 0x53,
    };
    LatWow64Exit event = {
        .size = sizeof(event), .abi_version = LAT_WOW64_ABI_VERSION,
    };
    LatWow64ProcessConfig config = {
        .size = sizeof(config), .abi_version = LAT_WOW64_ABI_VERSION,
    };
    LatWow64MemoryEvent map = {
        .size = sizeof(map), .abi_version = LAT_WOW64_ABI_VERSION,
        .type = LAT_WOW64_MEMORY_MAP,
        .protection = LAT_WOW64_PROT_READ | LAT_WOW64_PROT_WRITE |
                      LAT_WOW64_PROT_EXEC,
    };
    size_t page = sysconf(_SC_PAGESIZE);
    unsigned char *memory;
    void *module;

    assert(argc == 2 || argc == 3);
    struct sigaction restart_action = { .sa_handler = host_signal,
                                        .sa_flags = SA_RESTART };
    sigemptyset(&restart_action.sa_mask);
    assert(!sigaction(SIGRTMIN + 1, &restart_action, NULL));
    assert(signal(SIGUSR1, host_signal) != SIG_ERR);
    if (argc == 3 && !strcmp(argv[2], "--worker-loader")) {
        pthread_t loader;
        assert(!pthread_create(&loader, NULL, load_provider, argv[1]));
        assert(!pthread_join(loader, &module));
    } else {
        module = load_provider(argv[1]);
    }
    if (!module) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return 1;
    }
    get_api = dlsym(module, "lat_wow64_get_api");
    assert(get_api && !get_api(LAT_WOW64_ABI_VERSION + 1));
    api = get_api(LAT_WOW64_ABI_VERSION);
    assert(api && api->size == sizeof(*api));
    memory = mmap((void *)0x30000000, 4 * page, PROT_READ | PROT_WRITE | PROT_EXEC,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    assert(memory != MAP_FAILED);
    memcpy(memory, code, sizeof(code));
    *(uint32_t *)(memory + page) = 0x1234abcd;
    context.fs_base = (uintptr_t)(memory + page);
    context.gpr[LAT_WOW64_ESP] = (uintptr_t)(memory + 3 * page - 16);
    context.fxsave[0] = 0x7f;
    context.fxsave[1] = 3;
    context.fxsave[24] = 0x80;
    context.fxsave[25] = 0x1f;
    config.return_address = (uintptr_t)(memory + sizeof(code) - 1);
    struct checked_call call = {
        .config = &config, .process = &process, .context = &context,
        .thread = &thread, .pc = (uintptr_t)memory, .event = &event,
    };
    checked(checked_create_process, &call);
    map.address = (uintptr_t)memory;
    map.length = 3 * page;
    ok(api->memory_event(process, &map));
    context.dr7 = 1;
    assert(api->thread_create(process, &context, &thread) ==
           LAT_WOW64_STATUS_INVALID_ARGUMENT);
    context.dr7 = 0;
    checked(checked_create_thread, &call);
    if (argc == 3 && !strcmp(argv[2], "--cache-pressure")) {
        test_cache_pressure(process, thread, &context, page);
        ok(api->thread_destroy(thread));
        ok(api->process_destroy(process));
        assert(!munmap(memory, 4 * page));
        puts("WoW64 provider cache pressure PASS");
        return 0;
    }
    checked(checked_enter, &call);
    assert(event.reason == LAT_WOW64_EXIT_SYSCALL);
    assert(event.service_number == 42);
    ok(api->get_context(thread, &context));
    assert(context.gpr[LAT_WOW64_EDX] == 0x1234abcd);
    reply(thread, &event);
    ok(api->enter(thread, LAT_WOW64_ENTER_CURRENT_PC, &event));
    assert(event.reason == LAT_WOW64_EXIT_RETURN);

    test_i386_address_width(process, thread, memory, page);
    test_i386_execute_permissions(process, thread, memory, page);
    test_i386_x87_fault_retry(process, thread, memory, page);

    /* Code writes must invalidate the previously generated translation. */
    memory[1] = 84;
    map.type = LAT_WOW64_MEMORY_WRITE;
    ok(api->memory_event(process, &map));
    ok(api->enter(thread, (uintptr_t)memory, &event));
    assert(event.reason == LAT_WOW64_EXIT_SYSCALL);
    assert(event.service_number == 84);
    reply(thread, &event);

    /* fld1; fld1; faddp; movaps xmm0,xmm1; int 80 */
    {
        static const unsigned char floating[] = {
            0xd9, 0xe8, 0xd9, 0xe8, 0xde, 0xc1,
            0x0f, 0x28, 0xc1, 0xcd, 0x80,
        };
        memcpy(memory + 64, floating, sizeof(floating));
        ok(api->memory_event(process, &map));
        ok(api->get_context(thread, &context));
        memset(context.fxsave + 176, 0x5a, 16);
        ok(api->set_context(thread, &context));
        ok(api->enter(thread, (uintptr_t)(memory + 64), &event));
        assert(event.reason == LAT_WOW64_EXIT_SYSCALL);
        ok(api->get_context(thread, &context));
        assert(context.fxsave[39] == 0x80 && context.fxsave[41] == 0x40);
        for (unsigned int i = 0; i < 16; ++i) {
            assert(context.fxsave[160 + i] == 0x5a);
        }
        reply(thread, &event);
    }

    /* Soft x87 modes keep native SSE exceptions in the saved FCSR. */
    if (getenv("LATX_SOFTFPU") &&
        (!strcmp(getenv("LATX_SOFTFPU"), "1") ||
         !strcmp(getenv("LATX_SOFTFPU"), "2"))) {
        unsigned char divide_sse[] = {
            0xf3, 0x0f, 0x10, 0x05, 0, 0, 0, 0, /* movss xmm0,[one] */
            0x0f, 0x57, 0xc9,                   /* xorps xmm1,xmm1 */
            0xf3, 0x0f, 0x5e, 0xc1,             /* divss xmm0,xmm1 */
            0xcd, 0x80,
        };
        uint32_t address = (uintptr_t)(memory + page + 512);
        *(uint32_t *)(memory + page + 512) = 0x3f800000;
        memcpy(divide_sse + 4, &address, sizeof(address));
        memcpy(memory + 96, divide_sse, sizeof(divide_sse));
        ok(api->memory_event(process, &map));
        ok(api->enter(thread, (uintptr_t)(memory + 96), &event));
        assert(event.reason == LAT_WOW64_EXIT_SYSCALL);
        ok(api->get_context(thread, &context));
        fprintf(stderr, "SSE divide MXCSR=%02x%02x\n",
                context.fxsave[25], context.fxsave[24]);
        assert(context.fxsave[24] & 4);
        assert(!(context.fxsave[2] & 4));
        reply(thread, &event);

        /* An x87 exception imported from Windows must not become SSE state
         * when no SSE arithmetic executes on the next entry. */
        context.fxsave[2] |= 4;
        context.fxsave[24] = 0x80;
        context.fxsave[25] = 0x1f;
        ok(api->set_context(thread, &context));
        memcpy(memory + 96, "\xcd\x80", 2);
        ok(api->memory_event(process, &map));
        ok(api->enter(thread, (uintptr_t)(memory + 96), &event));
        assert(event.reason == LAT_WOW64_EXIT_SYSCALL);
        ok(api->get_context(thread, &context));
        fprintf(stderr, "x87/SSE isolation: status=%02x MXCSR=%02x%02x\n",
                context.fxsave[2], context.fxsave[25], context.fxsave[24]);
        assert(context.fxsave[2] & 4);
        assert(!(context.fxsave[24] & 4));
        reply(thread, &event);
    }

    if (getenv("LATX_SOFTFPU") && !strcmp(getenv("LATX_SOFTFPU"), "2")) {
        unsigned char precise[] = {
            0xdb, 0xe3, 0xd9, 0xe8, 0xdb, 0x2d, 0, 0, 0, 0,
            0xde, 0xc1, 0xcd, 0x80,
        };
        static const unsigned char tiny[] = {
            0, 0, 0, 0, 0, 0, 0, 0x80, 0xc0, 0x3f,
        };
        uint32_t address = (uintptr_t)(memory + page + 528);
        memcpy(memory + page + 528, tiny, sizeof(tiny));
        memcpy(precise + 6, &address, sizeof(address));
        memcpy(memory + 96, precise, sizeof(precise));
        ok(api->memory_event(process, &map));
        ok(api->enter(thread, (uintptr_t)(memory + 96), &event));
        assert(event.reason == LAT_WOW64_EXIT_SYSCALL);
        ok(api->get_context(thread, &context));
        /* 1 + 2^-63: the low bit is lost by the default double precision. */
        assert(context.fxsave[32] == 1 && context.fxsave[39] == 0x80);
        assert(context.fxsave[40] == 0xff && context.fxsave[41] == 0x3f);
        fprintf(stderr, "80-bit x87 precision sentinel PASS\n");
        reply(thread, &event);
    }

    /* Masked x87 exceptions must survive context export and reentry. */
    {
        static const unsigned char divide[] = {
            0xd9, 0xe8, 0xd9, 0xee, 0xde, 0xf9, 0xcd, 0x80,
        };
        memcpy(memory + 96, divide, sizeof(divide));
        ok(api->memory_event(process, &map));
        ok(api->enter(thread, (uintptr_t)(memory + 96), &event));
        assert(event.reason == LAT_WOW64_EXIT_SYSCALL);
        ok(api->get_context(thread, &context));
        fprintf(stderr, "x87 divide status=%02x%02x\n",
                context.fxsave[3], context.fxsave[2]);
        assert(context.fxsave[2] & 4);
        reply(thread, &event);
        ok(api->enter(thread, (uintptr_t)(memory + sizeof(code) - 1), &event));
        assert(event.reason == LAT_WOW64_EXIT_RETURN);
        ok(api->get_context(thread, &context));
        assert(context.fxsave[2] & 4);

        /* 1/3 also sets the precision flag, without an explicit status-word
         * instruction that could hide a missing export synchronization. */
        static const unsigned char inexact[] = {
            0xdb, 0xe3, 0xd9, 0xe8, 0xd9, 0xe8, 0xde, 0xc1,
            0xd9, 0xe8, 0xde, 0xc1, 0xd9, 0xe8, 0xd9, 0xc9,
            0xde, 0xf9, 0xcd, 0x80,
        };
        memcpy(memory + 96, inexact, sizeof(inexact));
        ok(api->memory_event(process, &map));
        ok(api->enter(thread, (uintptr_t)(memory + 96), &event));
        assert(event.reason == LAT_WOW64_EXIT_SYSCALL);
        ok(api->get_context(thread, &context));
        fprintf(stderr, "x87 inexact status=%02x%02x\n",
                context.fxsave[3], context.fxsave[2]);
        assert(context.fxsave[2] & 0x20);
        reply(thread, &event);
    }

    /* A hardware divide must leave the caller's signal mask unchanged. */
    {
        static const unsigned char divide[] = {0x31, 0xc9, 0xf7, 0xf1};
        sigset_t before, after;

        memcpy(memory + 128, divide, sizeof(divide));
        ok(api->memory_event(process, &map));
        assert(!pthread_sigmask(SIG_SETMASK, NULL, &before));
        ok(api->enter(thread, (uintptr_t)(memory + 128), &event));
        assert(!pthread_sigmask(SIG_SETMASK, NULL, &after));
        assert(!pthread_sigmask(SIG_SETMASK, &before, NULL));
        assert(event.reason == LAT_WOW64_EXIT_EXCEPTION);
        assert(event.trap_number == 0);
        assert(event.guest_pc == (uintptr_t)(memory + 130));
        for (int sig = 1; sig < NSIG; ++sig) {
            if (sigismember(&before, sig) != sigismember(&after, sig)) {
                fprintf(stderr, "signal %d mask changed after divide\n", sig);
                abort();
            }
        }
        reply(thread, &event);
        puts("hardware divide signal mask PASS");
    }

    /* Unmasked x87 and SSE divides use different architectural traps. */
    {
        static const unsigned char x87[] = {
            0xd9, 0xe8, 0xd9, 0xee, 0xde, 0xf9, 0x9b, 0xcd, 0x80,
        };
        static const unsigned char sse[] = {
            0xb8, 0, 0, 0x80, 0x3f,       /* mov eax,1.0f */
            0x66, 0x0f, 0x6e, 0xc0,       /* movd xmm0,eax */
            0x0f, 0x57, 0xc9,             /* xorps xmm1,xmm1 */
            0xf3, 0x0f, 0x5e, 0xc1,       /* divss xmm0,xmm1 */
            0xcd, 0x80,
        };
        const char *softfpu = getenv("LATX_SOFTFPU");
        bool use_softfpu = softfpu && (!strcmp(softfpu, "1") || !strcmp(softfpu, "2"));
        for (unsigned int simd = 0; simd < 2; ++simd) {
            ok(api->get_context(thread, &context));
            memset(context.fxsave, 0, sizeof(context.fxsave));
            /* The default translator must honor MXCSR with x87 masked. */
            context.fxsave[0] = simd && !use_softfpu ? 0x7f : 0x7b;
            context.fxsave[1] = 3;
            context.fxsave[24] = 0x80;
            context.fxsave[25] = simd ? 0x1d : 0x1f;
            ok(api->set_context(thread, &context));
            memcpy(memory + 128, simd ? sse : x87, simd ? sizeof(sse) : sizeof(x87));
            ok(api->memory_event(process, &map));
            ok(api->enter(thread, (uintptr_t)(memory + 128), &event));
            ok(api->get_context(thread, &context));
            fprintf(stderr, "unmasked %s: reason=%u trap=%u pc=%#llx x87=%02x%02x MXCSR=%02x%02x\n",
                    simd ? "SSE" : "x87", event.reason, event.trap_number,
                    (unsigned long long)event.guest_pc, context.fxsave[3], context.fxsave[2],
                    context.fxsave[25], context.fxsave[24]);
            assert(event.reason == LAT_WOW64_EXIT_EXCEPTION);
            assert(event.trap_number == (simd ? 19 : 16));
            assert(context.fxsave[simd ? 24 : 2] & 4);
            assert(!(context.fxsave[simd ? 2 : 24] & 4));
            reply(thread, &event);
        }
        memset(context.fxsave, 0, sizeof(context.fxsave));
        context.fxsave[0] = 0x7f;
        context.fxsave[1] = 3;
        context.fxsave[24] = 0x80;
        context.fxsave[25] = 0x1f;
        ok(api->set_context(thread, &context));
    }

    /* A guest access fault must return its PC and fault address to Wine. */
    {
        static const unsigned char fault[] = {0xa1, 0, 0, 0, 0};
        memcpy(memory + 128, fault, sizeof(fault));
        ok(api->memory_event(process, &map));
        ok(api->enter(thread, (uintptr_t)(memory + 128), &event));
        assert(event.reason == LAT_WOW64_EXIT_EXCEPTION);
        assert(event.trap_number == 14 && event.fault_address == 0);
        assert(event.guest_pc == (uintptr_t)(memory + 128));
        reply(thread, &event);
    }
    ok(api->thread_destroy(thread));

    /* Each guest waits for the other inside LATX, proving concurrent execution. */
    {
        static const unsigned char concurrent[] = {
            0x64, 0xa1, 0, 0, 0, 0, 0xf0, 0xff, 0x07,
            0x83, 0x3f, 0x02, 0x75, 0xfb, 0xcd, 0x80,
        };
        struct worker workers[2];
        pthread_t hosts[2];

        memcpy(memory + 256, concurrent, sizeof(concurrent));
        ok(api->memory_event(process, &map));
        *(uint32_t *)(memory + page + 128) = 0;
        for (unsigned int i = 0; i < 2; ++i) {
            context.generation = 0;
            context.fs_base = (uintptr_t)(memory + page + i * 16);
            context.gpr[LAT_WOW64_EDI] = (uintptr_t)(memory + page + 128);
            context.gpr[LAT_WOW64_ESP] = (uintptr_t)(memory + 3 * page - 16 - i * 256);
            workers[i].expected = 0x11223300 + i;
            *(uint32_t *)(memory + page + i * 16) = workers[i].expected;
            workers[i].pc = (uintptr_t)(memory + 256);
            ok(api->thread_create(process, &context, &workers[i].thread));
        }
        for (unsigned int i = 0; i < 2; ++i) {
            assert(!pthread_create(&hosts[i], NULL, run_worker, &workers[i]));
        }
        for (unsigned int i = 0; i < 2; ++i) {
            assert(!pthread_join(hosts[i], NULL));
            ok(api->thread_destroy(workers[i].thread));
        }
    }
    /* Creating and destroying another CPU must not wait for this guest. */
    {
        static const unsigned char waiting[] = {
            0x64, 0xa1, 0, 0, 0, 0,       /* mov eax,fs:[0] */
            0xc7, 0x47, 4, 1, 0, 0, 0,  /* mov [edi+4],1 */
            0x83, 0x3f, 0, 0x74, 0xfb,   /* wait for [edi] != 0 */
            0xcd, 0x80,
        };
        struct worker worker = { .pc = (uintptr_t)(memory + 512),
                                 .expected = 0x33445566 };
        LatWow64Thread *idle;
        pthread_t host;
        uint32_t *control = (uint32_t *)(memory + page + 128);

        memcpy(memory + 512, waiting, sizeof(waiting));
        ok(api->memory_event(process, &map));
        control[0] = control[1] = 0;
        context.generation = 0;
        context.fs_base = (uintptr_t)(memory + page);
        *(uint32_t *)(memory + page) = worker.expected;
        context.gpr[LAT_WOW64_EDI] = (uintptr_t)control;
        ok(api->thread_create(process, &context, &worker.thread));
        assert(!pthread_create(&host, NULL, run_worker, &worker));
        while (!__atomic_load_n(control + 1, __ATOMIC_ACQUIRE)) {
            usleep(100);
        }
        assert(!pthread_kill(host, SIGUSR1));
        while (!host_signals) {
            usleep(100);
        }
        ok(api->thread_create(process, &context, &idle));
        ok(api->thread_destroy(idle));
        /* Updating an unrelated executable page must interrupt linked code. */
        {
            LatWow64MemoryEvent unrelated = map;
            unrelated.type = LAT_WOW64_MEMORY_MAP;
            unrelated.address = (uintptr_t)(memory + 3 * page);
            unrelated.length = page;
            unrelated.protection = LAT_WOW64_PROT_READ | LAT_WOW64_PROT_EXEC;
            ok(api->memory_event(process, &unrelated));
        }
        __atomic_store_n(control, 1, __ATOMIC_RELEASE);
        assert(!pthread_join(host, NULL));
        ok(api->thread_destroy(worker.thread));
    }
    /* Requests both before CPU publication and inside linked guest code must
     * return normally, preserving a usable thread and no second pending stop. */
    {
        static const unsigned char loop[] = {
            0xc7, 0x07, 1, 0, 0, 0, /* mov dword [edi],1 */
            0xeb, 0xfe,             /* jmp $ */
        };
        struct worker worker = { .pc = (uintptr_t)(memory + 768) };
        struct sigaction action = { .sa_sigaction = interrupt_guest,
                                    .sa_flags = SA_SIGINFO };
        pthread_t host;
        uint32_t *started = (uint32_t *)(memory + page + 128);

        sigfillset(&action.sa_mask);
        assert(!sigaction(SIGUSR2, &action, NULL));
        memcpy(memory + 768, loop, sizeof(loop));
        memcpy(memory + 800, "\xcd\x80", 2);
        ok(api->memory_event(process, &map));
        context.generation = 0;
        context.gpr[LAT_WOW64_EDI] = (uintptr_t)started;
        ok(api->thread_create(process, &context, &worker.thread));
        api->request_interrupt(worker.thread, NULL);
        ok(api->enter(worker.thread, worker.pc, &event));
        assert(event.reason == LAT_WOW64_EXIT_STOPPED);
        reply(worker.thread, &event);
        *started = 0;
        assert(!pthread_create(&host, NULL, run_until_interrupted, &worker));
        while (!__atomic_load_n(started, __ATOMIC_ACQUIRE)) usleep(100);
        assert(!pthread_kill(host, SIGUSR2));
        assert(!pthread_join(host, NULL));
        ok(api->enter(worker.thread, (uintptr_t)(memory + 800), &event));
        assert(event.reason == LAT_WOW64_EXIT_SYSCALL);
        reply(worker.thread, &event);
        ok(api->thread_destroy(worker.thread));
        puts("WoW64 pre-entry and linked-loop host interruption PASS");
    }
    test_host_restart();
    map.type = LAT_WOW64_MEMORY_UNMAP;
    ok(api->memory_event(process, &map));
    ok(api->process_destroy(process));
    munmap(memory, 4 * page);
    puts("WoW64 i386 provider: syscall, FS, x87/SSE, fault, parallel, return and invalidation PASS");
    return 0;
}
