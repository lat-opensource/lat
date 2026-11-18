/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define WIN32_NO_STATUS
#include <windef.h>
#include <winternl.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <wine/unixlib.h>

#ifdef LATX_CPU_UNIX
#include "wow64-unix-bridge.h"
#define BOP_OPCODE "0x050f"
#else
#include "wow64-pe.h"
#include "wine/loongarch64-exception.h"
#define BOP_OPCODE "0x2ecd"
#endif

NTSTATUS WINAPI Wow64SystemServiceEx(UINT number, UINT *args);
void WINAPI Wow64PassExceptionToGuest(EXCEPTION_POINTERS *ptrs);

NTSTATUS (WINAPI *__wine_unix_call_dispatcher)(unixlib_handle_t, unsigned int, void *);
extern const uint16_t syscall_stub;
extern const uint16_t unix_stub;

/* The i386 ntdll calls these executable addresses. */
__asm__(".text\n"
        ".p2align 2\n"
        ".globl syscall_stub\n"
        "syscall_stub: .short " BOP_OPCODE "\n"
        ".globl unix_stub\n"
        "unix_stub: .short " BOP_OPCODE "\n");

#ifdef LATX_CPU_UNIX
unixlib_handle_t __wine_unixlib_handle;
#endif

static UINT latx_protection(ULONG protection)
{
    if (protection & PAGE_GUARD) {
        return 0;
    }

    switch (protection & 0xff) {
    case PAGE_READONLY:
        return LAT_WOW64_PROT_READ;
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
        return LAT_WOW64_PROT_READ | LAT_WOW64_PROT_WRITE;
    case PAGE_EXECUTE:
        return LAT_WOW64_PROT_EXEC;
    case PAGE_EXECUTE_READ:
        return LAT_WOW64_PROT_READ | LAT_WOW64_PROT_EXEC;
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return LAT_WOW64_PROT_READ | LAT_WOW64_PROT_WRITE | LAT_WOW64_PROT_EXEC;
    default:
        return 0;
    }
}

static NTSTATUS memory_event(UINT type, uintptr_t base, SIZE_T size, ULONG protect)
{
    LatWow64MemoryEvent event = {
        .size = sizeof(event),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .type = type,
        .protection = latx_protection(protect),
        .address = base,
        .length = size,
    };

    if (size && base <= UINT32_MAX && size <= UINT64_C(0x100000000) - base) {
#ifdef LATX_CPU_UNIX
        return WINE_UNIX_CALL(WOWLATX_MEMORY, &event);
#else
        lat_wow64_pe_memory_event(&event);
#endif
    }
    return STATUS_SUCCESS;
}

static NTSTATUS notify_views(uintptr_t position, uint64_t end, void *allocation, UINT type)
{
    MEMORY_BASIC_INFORMATION info;
    NTSTATUS status;

    while (position < end) {
        uint64_t next;
        status = NtQueryVirtualMemory((HANDLE)(LONG_PTR)-1, (void *)position,
                                      MemoryBasicInformation, &info, sizeof(info), NULL);
        if (status) {
            return status == STATUS_INVALID_PARAMETER ? STATUS_SUCCESS : status;
        }
        if (allocation && info.AllocationBase != allocation) {
            break;
        }
        next = (uintptr_t)info.BaseAddress + info.RegionSize;
        if (next <= position) {
            return STATUS_INVALID_ADDRESS;
        }
        if (info.State == MEM_COMMIT) {
            status = memory_event(type, position, min(end, next) - position, info.Protect);
            if (status && status != STATUS_NOT_FOUND) {
                return status;
            }
        }
        position = next;
    }
    return STATUS_SUCCESS;
}

#ifdef LATX_CPU_UNIX
static NTSTATUS init_unix_backend(void)
{
    const UNICODE_STRING name = RTL_CONSTANT_STRING(L"wowlatx-unix.dll");
    LatWow64ProcessConfig config = {
        .size = sizeof(config),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .max_enter_depth = 8,
        .return_address = (uintptr_t)&syscall_stub,
    };
    UINT64 library[2];
    NTSTATUS status;

    status = NtQueryVirtualMemory((HANDLE)(LONG_PTR)-1, &name,
                                  MemoryWineLoadUnixLibByName, library,
                                  sizeof(library), NULL);
    if (status) {
        return status;
    }
    __wine_unixlib_handle = library[1];
    status = WINE_UNIX_CALL(WOWLATX_INIT, &config);
    if (!status) {
        status = notify_views(0, UINT64_C(0x100000000), NULL, LAT_WOW64_MEMORY_MAP);
    }
    return status;
}

static void thread_exit(void)
{
    if (__wine_unixlib_handle) {
        WINE_UNIX_CALL(WOWLATX_THREAD_EXIT, NULL);
    }
}

#else
static void thread_exit(void)
{
    lat_wow64_pe_thread_exit();
}

static uint32_t host_float_status(NTSTATUS code)
{
    switch (code) {
    case STATUS_FLOAT_INVALID_OPERATION:
    case STATUS_FLOAT_STACK_CHECK: return 1;
    case STATUS_FLOAT_DENORMAL_OPERAND: return 2;
    case STATUS_FLOAT_DIVIDE_BY_ZERO: return 4;
    case STATUS_FLOAT_OVERFLOW: return 8;
    case STATUS_FLOAT_UNDERFLOW: return 16;
    case STATUS_FLOAT_INEXACT_RESULT: return 32;
    default: return 0;
    }
}

__declspec(dllexport) NTSTATUS WINAPI BTCpuResetToConsistentState(EXCEPTION_POINTERS *ptrs)
{
    const EXCEPTION_RECORD *record = ptrs->ExceptionRecord;
    CONTEXT *context = ptrs->ContextRecord;
    const struct wine_loongarch64_exception_frame *frame = (const void *)context;
    uint32_t float_status = host_float_status(record->ExceptionCode);
    if (!(context->ContextFlags & WINE_LOONGARCH64_EXCEPTION_XSTATE) ||
        (!float_status && (record->ExceptionCode != STATUS_ACCESS_VIOLATION ||
                          record->NumberParameters != 2))) {
        return STATUS_SUCCESS;
    }

    LatWow64PeFaultContext state = {
        .gpr = context->Gpr,
        .vector = frame->xstate.vector,
        .fcsr = context->Fcsr,
        .eflags = frame->xstate.lbt_eflags,
        .ftop = frame->xstate.lbt_ftop,
    };
    lat_wow64_pe_fault(&state, context->Pc, record->ExceptionInformation[1],
                       record->ExceptionInformation[0], float_status);
    return STATUS_SUCCESS;
}

#endif

__declspec(dllexport) void WINAPI BTCpuNotifyMemoryAlloc(void *addr, SIZE_T size,
    ULONG type, ULONG protect, BOOL post, NTSTATUS status)
{
    if (post && !status && (type & MEM_COMMIT)) {
        notify_views((uintptr_t)addr, (uintptr_t)addr + size, NULL, LAT_WOW64_MEMORY_MAP);
    }
}

__declspec(dllexport) void WINAPI BTCpuNotifyMemoryProtect(void *addr, SIZE_T size,
    ULONG protect, BOOL post, NTSTATUS status)
{
    if (post && !status) {
        memory_event(LAT_WOW64_MEMORY_PROTECT, (uintptr_t)addr, size, protect);
    }
}

__declspec(dllexport) void WINAPI BTCpuNotifyMemoryFree(void *addr, SIZE_T size,
    ULONG type, BOOL post, NTSTATUS status)
{
    if ((type & MEM_RELEASE) && !size) {
#ifndef LATX_CPU_UNIX
        if (post && !status) {
            memory_event(LAT_WOW64_MEMORY_UNMAP, (uintptr_t)addr,
                         UINT64_C(0x100000000) - (uintptr_t)addr, 0);
            return;
        }
#endif
        if (!post || status) {
            notify_views((uintptr_t)addr, UINT64_C(0x100000000), addr,
                         post ? LAT_WOW64_MEMORY_MAP : LAT_WOW64_MEMORY_UNMAP);
        }
    } else if (post && !status) {
        memory_event(LAT_WOW64_MEMORY_UNMAP, (uintptr_t)addr, size, 0);
    }
}

__declspec(dllexport) NTSTATUS WINAPI BTCpuNotifyMapViewOfSection(void *section, void *addr,
    void *unknown, SIZE_T size, ULONG alloc, ULONG protect)
{
    return notify_views((uintptr_t)addr, (uintptr_t)addr + size, NULL, LAT_WOW64_MEMORY_MAP);
}

__declspec(dllexport) void WINAPI BTCpuNotifyUnmapViewOfSection(void *addr, BOOL post,
    NTSTATUS status)
{
#ifndef LATX_CPU_UNIX
    if (post) {
        lat_wow64_pe_end_unmap(!status);
    } else {
        lat_wow64_pe_begin_unmap(addr);
    }
#else
    if (!post || status) {
        notify_views((uintptr_t)addr, UINT64_C(0x100000000), addr,
                     post ? LAT_WOW64_MEMORY_MAP : LAT_WOW64_MEMORY_UNMAP);
    }
#endif
}

__declspec(dllexport) void WINAPI BTCpuFlushInstructionCache2(void *addr, SIZE_T size)
{
    if (!addr && !size) {
        notify_views(0, UINT64_C(0x100000000), NULL, LAT_WOW64_MEMORY_FLUSH);
    } else {
        memory_event(LAT_WOW64_MEMORY_FLUSH, (uintptr_t)addr, size, 0);
    }
}

#ifndef LATX_CPU_UNIX
__declspec(dllexport) void WINAPI BTCpuNotifyMemoryDirty(void *addr, SIZE_T size)
{
    memory_event(LAT_WOW64_MEMORY_WRITE, (uintptr_t)addr, size, 0);
}
#endif

__declspec(dllexport) void WINAPI BTCpuFlushInstructionCacheHeavy(void *addr, SIZE_T size)
{
    BTCpuFlushInstructionCache2(addr, size);
}
__declspec(dllexport) const void *WINAPI BTCpuGetBopCode(void)
{
    return &syscall_stub;
}

__declspec(dllexport) const void *WINAPI __wine_get_unix_opcode(void)
{
    return &unix_stub;
}

__declspec(dllexport) NTSTATUS WINAPI BTCpuProcessInit(void)
{
    HMODULE module;
    UNICODE_STRING name;
    void **dispatcher;
    NTSTATUS status;

    RtlInitUnicodeString(&name, L"ntdll.dll");
    if ((uintptr_t)&syscall_stub > UINT32_MAX ||
        LdrGetDllHandle(NULL, 0, &name, &module)) {
        RtlRaiseStatus(STATUS_INVALID_ADDRESS);
    }
    dispatcher = RtlFindExportedRoutineByName(module, "__wine_unix_call_dispatcher");
    if (!dispatcher || !*dispatcher) {
        status = STATUS_PROCEDURE_NOT_FOUND;
    } else {
        __wine_unix_call_dispatcher = *dispatcher;
#ifdef LATX_CPU_UNIX
        status = init_unix_backend();
#else
        status = lat_wow64_pe_init() ? STATUS_SUCCESS : STATUS_DLL_INIT_FAILED;
#endif
    }
    if (status) {
        NtTerminateProcess((HANDLE)(LONG_PTR)-1, status);
    } else {
        WOW64INFO *wow64info = NtCurrentTeb()->TlsSlots[WOW64_TLS_WOW64INFO];
        wow64info->CpuFlags |= WOW64_CPUFLAGS_SOFTWARE;
    }
    return status;
}

__declspec(dllexport) NTSTATUS WINAPI BTCpuGetContext(HANDLE thread, HANDLE process,
                                                     void *unused, WOW64_CONTEXT *ctx)
{
    return NtQueryInformationThread(thread, ThreadWow64Context, ctx, sizeof(*ctx), NULL);
}

__declspec(dllexport) NTSTATUS WINAPI BTCpuSetContext(HANDLE thread, HANDLE process,
                                                     void *unused, WOW64_CONTEXT *ctx)
{
    return NtSetInformationThread(thread, ThreadWow64Context, ctx, sizeof(*ctx));
}

__declspec(dllexport) void WINAPI BTCpuThreadTerm(HANDLE thread, LONG status)
{
    if (!thread || RtlIsCurrentThread(thread)) {
        thread_exit();
    }
}

#ifndef LATX_CPU_UNIX
static NTSTATUS control_thread(HANDLE handle, ULONG *count, LONG exit_code, bool terminate)
{
    const HANDLE self = (HANDLE)(LONG_PTR)-2;
    const HANDLE process = (HANDLE)(LONG_PTR)-1;
    OBJECT_BASIC_INFORMATION object;
    CONTEXT context = { .ContextFlags = CONTEXT_CONTROL };
    ULONG previous;
    HANDLE thread;
    HANDLE control;
    NTSTATUS status;
    void *paused;

    if (!handle || handle == self) {
        if (!terminate) return NtSuspendThread(handle, count);
        thread_exit();
        return NtTerminateThread(handle, exit_code);
    }
    status = NtDuplicateObject(process, handle, process, &thread, 0, 0, DUPLICATE_SAME_ACCESS);
    if (status) return status;
    status = NtQueryObject(thread, ObjectBasicInformation, &object, sizeof(object), NULL);
    if (status) goto done;
    if (!(object.GrantedAccess & (terminate ? THREAD_TERMINATE : THREAD_SUSPEND_RESUME))) {
        status = STATUS_ACCESS_DENIED;
        goto done;
    }
    if (RtlIsCurrentThread(thread)) {
        NtClose(thread);
        if (terminate) thread_exit();
        return terminate ? NtTerminateThread(self, exit_code) : NtSuspendThread(self, count);
    }
    if (!lat_wow64_pe_pause_thread(thread, &paused, &control)) {
        status = STATUS_DEVICE_BUSY;
        goto done;
    }

    if (!control) {
        status = terminate ? NtTerminateThread(thread, exit_code) : NtSuspendThread(thread, &previous);
    } else if (!(status = NtSuspendThread(control, &previous))) {
        /* Await the native suspension before releasing a thread's translator state. */
        status = NtGetContextThread(control, &context);
        if (!status && terminate) status = NtTerminateThread(thread, exit_code);
        if (status) NtResumeThread(control, NULL);
    }
    lat_wow64_pe_finish_thread(paused, terminate && !status);
    if (!status && count) *count = previous;

done:
    NtClose(thread);
    return status;
}

__declspec(dllexport) NTSTATUS WINAPI BTCpuSuspendLocalThread(HANDLE thread, ULONG *count)
{
    return control_thread(thread, count, 0, false);
}

__declspec(dllexport) NTSTATUS WINAPI __wine_wow64_terminate_thread(HANDLE thread, LONG status)
{
    return control_thread(thread, NULL, status, true);
}
#endif

__declspec(dllexport) void WINAPI BTCpuProcessTerm(HANDLE process, BOOL post, NTSTATUS status)
{
#ifdef LATX_CPU_UNIX
    WINE_UNIX_CALL(WOWLATX_PROCESS_TERM, &post);
#else
    lat_wow64_pe_process_term(post, !status);
#endif
}

static void import_context(LatWow64Context *lat, const WOW64_CONTEXT *ctx)
{
    TEB *teb = NtCurrentTeb();

    *lat = (LatWow64Context){
        .size = sizeof(*lat),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .gpr = {
            [LAT_WOW64_EAX] = ctx->Eax, [LAT_WOW64_ECX] = ctx->Ecx,
            [LAT_WOW64_EDX] = ctx->Edx, [LAT_WOW64_EBX] = ctx->Ebx,
            [LAT_WOW64_ESP] = ctx->Esp, [LAT_WOW64_EBP] = ctx->Ebp,
            [LAT_WOW64_ESI] = ctx->Esi, [LAT_WOW64_EDI] = ctx->Edi,
        },
        .eip = ctx->Eip, .eflags = ctx->EFlags,
        .cs = ctx->SegCs, .ss = ctx->SegSs, .ds = ctx->SegDs,
        .es = ctx->SegEs, .fs = ctx->SegFs, .gs = ctx->SegGs,
        .fs_base = PtrToUlong((char *)teb + teb->WowTebOffset),
        .dr0 = ctx->Dr0, .dr1 = ctx->Dr1, .dr2 = ctx->Dr2, .dr3 = ctx->Dr3,
        .dr6 = ctx->Dr6, .dr7 = ctx->Dr7,
    };
    memcpy(lat->fxsave, ctx->ExtendedRegisters, sizeof(lat->fxsave));
}

static void export_context(WOW64_CONTEXT *ctx, const LatWow64Context *lat)
{
    ctx->Eax = lat->gpr[LAT_WOW64_EAX];
    ctx->Ecx = lat->gpr[LAT_WOW64_ECX];
    ctx->Edx = lat->gpr[LAT_WOW64_EDX];
    ctx->Ebx = lat->gpr[LAT_WOW64_EBX];
    ctx->Esp = lat->gpr[LAT_WOW64_ESP];
    ctx->Ebp = lat->gpr[LAT_WOW64_EBP];
    ctx->Esi = lat->gpr[LAT_WOW64_ESI];
    ctx->Edi = lat->gpr[LAT_WOW64_EDI];
    ctx->Eip = lat->eip;
    ctx->EFlags = lat->eflags;
    ctx->SegCs = lat->cs;
    ctx->SegSs = lat->ss;
    ctx->SegDs = lat->ds;
    ctx->SegEs = lat->es;
    ctx->SegFs = lat->fs;
    ctx->SegGs = lat->gs;
    ctx->Dr0 = lat->dr0;
    ctx->Dr1 = lat->dr1;
    ctx->Dr2 = lat->dr2;
    ctx->Dr3 = lat->dr3;
    ctx->Dr6 = lat->dr6;
    ctx->Dr7 = lat->dr7;
    memcpy(ctx->ExtendedRegisters, lat->fxsave, sizeof(lat->fxsave));
}

static void dispatch_guest_exception(const LatWow64Exit *exit, const LatWow64Context *lat)
{
    EXCEPTION_RECORD record = {0};
    CONTEXT native_context = {0};
    EXCEPTION_POINTERS ptrs = {&record, &native_context};

    record.ExceptionAddress = ULongToPtr(exit->guest_pc);
    switch (exit->trap_number) {
    case 0:  record.ExceptionCode = STATUS_INTEGER_DIVIDE_BY_ZERO; break;
    case 1:  record.ExceptionCode = STATUS_SINGLE_STEP; break;
    case 3:  record.ExceptionCode = STATUS_BREAKPOINT; break;
    case 4:  record.ExceptionCode = STATUS_INTEGER_OVERFLOW; break;
    case 5:  record.ExceptionCode = STATUS_ARRAY_BOUNDS_EXCEEDED; break;
    case 6:  record.ExceptionCode = STATUS_ILLEGAL_INSTRUCTION; break;
    case 12: record.ExceptionCode = STATUS_STACK_OVERFLOW; break;
    case 17: record.ExceptionCode = STATUS_DATATYPE_MISALIGNMENT; break;
    case 9:
    case 16: record.ExceptionCode = STATUS_FLOAT_INVALID_OPERATION; break;
    case 19: {
        uint32_t mxcsr, pending;

        memcpy(&mxcsr, lat->fxsave + 24, sizeof(mxcsr));
        pending = mxcsr & ~(mxcsr >> 7) & 0x3f;
        record.ExceptionCode = (pending & 5) || !pending ? STATUS_FLOAT_MULTIPLE_TRAPS
                                                        : STATUS_FLOAT_MULTIPLE_FAULTS;
        record.NumberParameters = 2;
        record.ExceptionInformation[1] = mxcsr;
        break;
    }
    default:
        record.ExceptionCode = STATUS_ACCESS_VIOLATION;
        record.NumberParameters = 2;
        record.ExceptionInformation[1] = ~0u;
        if (exit->flags & LAT_WOW64_EXIT_FLAG_FAULT_ADDRESS_VALID) {
            record.ExceptionInformation[0] = (exit->error_code >> 1) & 9;
            record.ExceptionInformation[1] = exit->fault_address;
        }
        break;
    }
    Wow64PassExceptionToGuest(&ptrs);
}

__declspec(dllexport) void WINAPI BTCpuSimulate(void)
{
    WOW64_CPURESERVED *cpu = NtCurrentTeb()->TlsSlots[WOW64_TLS_CPURESERVED];
    WOW64_CONTEXT *ctx = (WOW64_CONTEXT *)(cpu + 1);
    LatWow64Context lat;
    LatWow64Exit exit_info;

    for (;;) {
        const uint32_t *stack;
        const void *opcode;
        NTSTATUS status;

#ifdef LATX_CPU_UNIX
        struct wowlatx_run run;

        import_context(&run.context, ctx);
        status = WINE_UNIX_CALL(WOWLATX_RUN, &run);
        if (status) {
            RtlRaiseStatus(status);
        }
        lat = run.context;
        exit_info = run.exit;
#else
        import_context(&lat, ctx);
        if (!lat_wow64_pe_execute(&lat, &exit_info)) {
            RtlRaiseStatus(STATUS_UNSUCCESSFUL);
        }
#endif
        export_context(ctx, &lat);
#ifndef LATX_CPU_UNIX
        lat_wow64_pe_end_execute();
        if (exit_info.reason == LAT_WOW64_EXIT_STOPPED) continue;
#endif
        if (exit_info.reason == LAT_WOW64_EXIT_EXCEPTION ||
            exit_info.reason == LAT_WOW64_EXIT_BREAKPOINT ||
            exit_info.reason == LAT_WOW64_EXIT_SINGLE_STEP) {
            dispatch_guest_exception(&exit_info, &lat);
            continue;
        }
        opcode = ULongToPtr(ctx->Eip - sizeof(syscall_stub));
        if (exit_info.reason != LAT_WOW64_EXIT_SYSCALL ||
            (opcode != &syscall_stub && opcode != &unix_stub)) {
            RtlRaiseStatus(STATUS_ILLEGAL_INSTRUCTION);
        }
        stack = ULongToPtr(ctx->Esp);
        ctx->Eip = stack[0];
        ctx->Esp += 4;
        cpu->Flags = 0;
        if (opcode == &unix_stub) {
            unixlib_handle_t handle = stack[1] | (uint64_t)stack[2] << 32;
            ctx->Esp += 16;
            status = __wine_unix_call_dispatcher(handle, stack[3], ULongToPtr(stack[4]));
        } else {
            status = Wow64SystemServiceEx(ctx->Eax, (UINT *)stack + 2);
        }
        if (!(cpu->Flags & WOW64_CPURESERVED_FLAG_RESET_STATE)) {
            ctx->Eax = status;
        }
        cpu->Flags &= ~WOW64_CPURESERVED_FLAG_RESET_STATE;
    }
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_THREAD_DETACH) {
        thread_exit();
    }
    return TRUE;
}
