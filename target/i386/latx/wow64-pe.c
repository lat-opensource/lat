/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/cpu_ldst.h"
#include "exec/cpu-common.h"
#include "hw/core/tcg-cpu-ops.h"
#include "qapi/error.h"
#include "qemu/rcu.h"
#include "sysemu/tcg.h"
#include "wow64-pe-host.h"
#include "exec/translate-all.h"
#include "qemu/log.h"
#include "qemu-common.h"
#include "reg-alloc.h"
#include "qemu/timer.h"
#include "qemu/units.h"
#include "tcg/tcg.h"
#include "trace/control.h"
#include "X86DisassemblerDecoder.h"
#include "latx-options.h"
#include "latx-config.h"
#include "lsenv.h"
#include "translate.h"
#include "wow64-pe.h"
#include "wow64-x86-state.h"

LONG WINAPI NtCompareObjects(HANDLE first, HANDLE second);
LONG WINAPI NtQueryVirtualMemory(HANDLE process, const void *address, ULONG info_class,
                                 void *buffer, SIZE_T size, SIZE_T *returned);

/* MEMORY_REGION_INFORMATION from the native memory query. */
typedef struct PeMemoryRegion {
    void *allocation_base;
    ULONG allocation_protect;
    ULONG region_type;
    SIZE_T region_size;
    SIZE_T commit_size;
    ULONG_PTR partition_id;
    ULONG_PTR node_preference;
} PeMemoryRegion;

typedef struct PeHostTls PeHostTls;

typedef struct PeThread {
    struct rcu_head rcu;
    X86CPU x86;
    ENV lsenv_data;
    TRANSLATION_DATA translation;
    uint64_t gdt[64];
    PeHostTls *host;
    HANDLE control;
    unsigned int execution;
    uint32_t float_status;
} PeThread;

enum { PE_EXECUTING = 1, PE_PAUSE_REQUESTED = 2 };

/* A terminated native thread may have been waiting to acquire either lock. */
static QemuSpin mmap_mutex;
static QemuSpin thread_control_lock;
struct PeHostTls {
    CPUState *cpu;
    uintptr_t helper_ra;
    uintptr_t unmap_address;
    size_t unmap_size;
    unsigned int lock_depth;
    bool process_term_lock;
    struct rcu_reader_data reader;
    bool rcu_registered;
    void *qemu_thread;
};

static DWORD pe_host_slot = TLS_OUT_OF_INDEXES;

static PeHostTls *pe_host_tls(void)
{
    PeHostTls *host = TlsGetValue(pe_host_slot);

    if (!host) {
        host = g_new0(PeHostTls, 1);
        if (!TlsSetValue(pe_host_slot, host)) {
            abort();
        }
    }
    return host;
}

CPUState **lat_wow64_pe_current_cpu_slot(void)
{
    return &pe_host_tls()->cpu;
}

uintptr_t *lat_wow64_pe_helper_retaddr_slot(void)
{
    return &pe_host_tls()->helper_ra;
}

struct rcu_reader_data *lat_wow64_pe_rcu_reader(void)
{
    return &pe_host_tls()->reader;
}

bool *lat_wow64_pe_rcu_registered(void)
{
    return &pe_host_tls()->rcu_registered;
}

void *lat_wow64_pe_qemu_thread_data(void)
{
    return pe_host_tls()->qemu_thread;
}

void lat_wow64_pe_set_qemu_thread_data(void *data)
{
    pe_host_tls()->qemu_thread = data;
}

#define mmap_depth (pe_host_tls()->lock_depth)
/* Wow64 initializes the CPU DLL before the native PE loader sets up static TLS. */
static DWORD thread_slot = TLS_OUT_OF_INDEXES;
uintptr_t qemu_host_page_size;
uintptr_t qemu_real_host_page_size;
intptr_t qemu_host_page_mask;
uintptr_t guest_base;
int64_t clock_freq;
unsigned long reserved_va;
int singlestep;
bool message_with_timestamp;
int qemu_icache_linesize = 64;
int qemu_dcache_linesize = 64;
int trace_events_enabled_count;
uint16_t _TRACE_XTM_TR_TB_DSTATE;
uint16_t _TRACE_EXEC_TB_DSTATE, _TRACE_EXEC_TB_EXIT_DSTATE;

uint16_t _TRACE_QEMU_MEMALIGN_DSTATE, _TRACE_QEMU_VFREE_DSTATE;
uint16_t _TRACE_QEMU_ANON_RAM_ALLOC_DSTATE, _TRACE_QEMU_ANON_RAM_FREE_DSTATE;
uint16_t _TRACE_QEMU_MUTEX_LOCK_DSTATE, _TRACE_QEMU_MUTEX_LOCKED_DSTATE;
uint16_t _TRACE_QEMU_MUTEX_UNLOCK_DSTATE;

/* Bind the Win32 synchronization primitives without the lock profiler. */
QemuMutexLockFunc qemu_mutex_lock_func = qemu_mutex_lock_impl;
QemuMutexTrylockFunc qemu_mutex_trylock_func = qemu_mutex_trylock_impl;
QemuCondWaitFunc qemu_cond_wait_func = qemu_cond_wait_impl;
QemuCondTimedWaitFunc qemu_cond_timedwait_func = qemu_cond_timedwait_impl;

/* Wine's ntdll longjmp unwinds the stack. LATX's generated frame has no PE
 * unwind information, so restore the matching _setjmpex register frame here. */
QEMU_BUILD_BUG_ON(offsetof(_JUMP_BUFFER, S) != 8 ||
                  offsetof(_JUMP_BUFFER, Sp) != 88 ||
                  offsetof(_JUMP_BUFFER, Ra) != 96 ||
                  offsetof(_JUMP_BUFFER, Fcsr) != 104 ||
                  offsetof(_JUMP_BUFFER, F) != 112);
asm(".text\n"
    ".p2align 2\n"
    ".globl lat_wow64_pe_longjmp\n"
    "lat_wow64_pe_longjmp:\n"
    "ld.d $fp, $a0, 8\n"
    "ld.d $s0, $a0, 16\n"
    "ld.d $s1, $a0, 24\n"
    "ld.d $s2, $a0, 32\n"
    "ld.d $s3, $a0, 40\n"
    "ld.d $s4, $a0, 48\n"
    "ld.d $s5, $a0, 56\n"
    "ld.d $s6, $a0, 64\n"
    "ld.d $s7, $a0, 72\n"
    "ld.d $s8, $a0, 80\n"
    "ld.d $sp, $a0, 88\n"
    "ld.d $ra, $a0, 96\n"
    "ld.w $t0, $a0, 104\n"
    "movgr2fcsr $fcsr0, $t0\n"
    "fld.d $f24, $a0, 112\n"
    "fld.d $f25, $a0, 120\n"
    "fld.d $f26, $a0, 128\n"
    "fld.d $f27, $a0, 136\n"
    "fld.d $f28, $a0, 144\n"
    "fld.d $f29, $a0, 152\n"
    "fld.d $f30, $a0, 160\n"
    "fld.d $f31, $a0, 168\n"
    "sltui $t0, $a1, 1\n"
    "or $a0, $a1, $t0\n"
    "ret\n");

static PeThread *get_pe_thread(void)
{
    return TlsGetValue(thread_slot);
}

static void pe_enter_execution(PeThread *thread)
{
    while (qatomic_cmpxchg(&thread->execution, 0, PE_EXECUTING)) {
        Sleep(0);
    }
}

void lat_wow64_pe_end_execute(void)
{
    qatomic_and(&get_pe_thread()->execution, ~PE_EXECUTING);
}

ENV *lat_wow64_pe_get_lsenv(void)
{
    return &get_pe_thread()->lsenv_data;
}

void mmap_lock(void)
{
    if (mmap_depth++ == 0) {
        qemu_spin_lock(&mmap_mutex);
    }
}

void mmap_unlock(void)
{
    if (--mmap_depth == 0) {
        qemu_spin_unlock(&mmap_mutex);
    }
}

bool have_mmap_lock(void)
{
    return mmap_depth != 0;
}

void lat_wow64_pe_prepare_exit(void)
{
    const unsigned int keep_depth = pe_host_tls()->process_term_lock;

    if (mmap_depth > keep_depth) {
        tr_fini(false);
        do {
            mmap_unlock();
        } while (mmap_depth > keep_depth);
    }
}

void lat_wow64_pe_process_term(bool post, bool success)
{
    if (!post) {
        mmap_lock();
        pe_host_tls()->process_term_lock = true;
    } else if (!success) {
        pe_host_tls()->process_term_lock = false;
        mmap_unlock();
    }
    /* On success the other threads have been terminated. Keep the lock
     * until process exit so killed waiters never have to be woken. The
     * remaining thread can still translate DLL detach code recursively. */
}

void lat_wow64_pe_memory_event(const LatWow64MemoryEvent *event)
{
    target_ulong start = event->address & TARGET_PAGE_MASK;
    target_ulong end = TARGET_PAGE_ALIGN(event->address + event->length);

    mmap_lock();
    if (event->type != LAT_WOW64_MEMORY_FLUSH &&
        event->type != LAT_WOW64_MEMORY_WRITE) {
        int flags = event->type == LAT_WOW64_MEMORY_UNMAP ? 0 : PAGE_VALID;

        if (flags && (event->protection & LAT_WOW64_PROT_READ)) {
            flags |= PAGE_READ;
        }
        if (flags && (event->protection & LAT_WOW64_PROT_WRITE)) {
            flags |= PAGE_WRITE | PAGE_WRITE_ORG;
        }
        if (flags && (event->protection & LAT_WOW64_PROT_EXEC)) {
            flags |= PAGE_EXEC;
        }
        page_set_flags(start, end, flags);
    }
    tb_invalidate_phys_range(start, end);
    mmap_unlock();
}

void lat_wow64_pe_begin_unmap(void *address)
{
    const uint64_t guest_end = UINT64_C(0x100000000);
    const ULONG memory_region_information = 3;
    PeHostTls *host = pe_host_tls();
    PeMemoryRegion region;

    /* The syscall accepts an interior address and may remove the entire view. */
    mmap_lock();
    host->unmap_address = 0;
    host->unmap_size = guest_end;
    if (!NtQueryVirtualMemory(GetCurrentProcess(), address, memory_region_information,
                              &region, sizeof(region), NULL) && region.region_size) {
        uintptr_t base = (uintptr_t)region.allocation_base;

        host->unmap_address = base;
        host->unmap_size = base < guest_end ? MIN(region.region_size, guest_end - base) : 0;
    }
}

void lat_wow64_pe_end_unmap(bool success)
{
    PeHostTls *host = pe_host_tls();

    if (success && host->unmap_size) {
        LatWow64MemoryEvent event = {
            .type = LAT_WOW64_MEMORY_UNMAP,
            .address = host->unmap_address,
            .length = host->unmap_size,
        };

        lat_wow64_pe_memory_event(&event);
    }
    mmap_unlock();
}

void cpu_exit(CPUState *cpu)
{
    qatomic_set(&cpu->exit_request, 1);
    smp_wmb();
    qatomic_set(&cpu_neg(cpu)->icount_decr.u16.high, -1);
}

bool qemu_cpu_is_self(CPUState *cpu)
{
    return current_cpu == cpu;
}

int qemu_mprotect_none(void *base, size_t size)
{
    DWORD old;
    return VirtualProtect(base, size, PAGE_NOACCESS, &old) ? 0 : -1;
}

void cpu_dump_state(CPUState *cpu, FILE *file, int flags)
{
    CPUX86State *env = cpu->env_ptr;
    fprintf(file, "EIP=%08" PRIx32 " EFLAGS=%08" PRIx32 "\n",
            (uint32_t)env->eip, (uint32_t)env->eflags);
}

int qemu_printf(const char *format, ...)
{
    va_list ap;
    int ret;

    va_start(ap, format);
    ret = vfprintf(stdout, format, ap);
    va_end(ap);
    return ret;
}

int error_printf(const char *format, ...)
{
    va_list ap;
    int ret;

    va_start(ap, format);
    ret = vfprintf(stderr, format, ap);
    va_end(ap);
    return ret;
}

int cpu_memory_rw_debug(CPUState *cpu, target_ulong addr,
                        void *buf, size_t len, bool is_write)
{
    SIZE_T count;
    BOOL ok;

    if (is_write) {
        ok = WriteProcessMemory(GetCurrentProcess(), (void *)(uintptr_t)addr,
                                buf, len, &count);
    } else {
        ok = ReadProcessMemory(GetCurrentProcess(), (void *)(uintptr_t)addr,
                               buf, len, &count);
    }
    return ok && count == len ? 0 : -1;
}

void error_report(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fputc('\n', stderr);
}

void warn_report(const char *format, ...) __attribute__((alias("error_report")));

void lat_wow64_pe_fault(LatWow64PeFaultContext *context, uintptr_t pc,
                        uintptr_t address, unsigned int access,
                        uint32_t float_status)
{
    PeThread *thread = get_pe_thread();
    CPUState *cpu = thread ? (CPUState *)&thread->x86 : NULL;
    CPUX86State *env = thread ? &thread->x86.env : NULL;
    bool in_jit = cpu && cpu->running && context->gpr[31] == (uintptr_t)env &&
                  tcg_tb_lookup(pc);
    uintptr_t restore_pc = in_jit ? pc + GETPC_ADJ : helper_retaddr;

    if (!cpu || !cpu->running || (!in_jit && !restore_pc)) {
        return;
    }
    helper_retaddr = 0;
    /* Helpers have already saved their guest registers to env. */
    env->puc = in_jit ? context : NULL;
    if (!cpu_restore_state(cpu, restore_pc, true)) {
        env->puc = NULL;
        return;
    }
    env->exception_is_int = false;
    env->exception_next_eip = -1;
    /* A fault in a locked instruction skips the generated unlock. */
    for (size_t i = 0; i < ARRAY_SIZE(lat_lock); i++) {
        qatomic_cmpxchg(&lat_lock[i].lock, cpu->cpu_index + 1, 0);
    }
    if (float_status) {
        thread->float_status = float_status;
        cpu->exception_index = EXCP10_COPR;
    } else {
        env->cr[2] = address;
        env->error_code = PG_ERROR_U_MASK;
        if (access == 1) {
            env->error_code |= PG_ERROR_W_MASK;
        } else if (access == 8) {
            env->error_code |= PG_ERROR_I_D_MASK;
        }
        cpu->exception_index = EXCP0E_PAGE;
    }
    cpu_loop_exit(cpu);
}

void qemu_cpu_kick(CPUState *cpu)
{
    cpu_exit(cpu);
}


void page_size_init(void)
{
    qemu_host_page_mask = -(intptr_t)qemu_host_page_size;
}


uint64_t cpu_get_tsc(CPUX86State *env)
{
    return cpu_get_host_ticks();
}

/* A single i686 processor with the features enabled by this runtime. */
void cpu_x86_cpuid(CPUX86State *env, uint32_t index, uint32_t count,
                   uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    *eax = *ebx = *ecx = *edx = 0;
    if (index == 0) {
        *eax = env->cpuid_level;
        *ebx = env->cpuid_vendor1;
        *ecx = env->cpuid_vendor3;
        *edx = env->cpuid_vendor2;
    } else if (index == 1) {
        *eax = env->cpuid_version;
        *ebx = 1 << 16;
        *ecx = env->features[FEAT_1_ECX];
        *edx = env->features[FEAT_1_EDX];
    }
}

void cpu_abort(CPUState *cpu, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    abort();
}

static unsigned int pe_code_page_flags(target_ulong addr)
{
    MEMORY_BASIC_INFORMATION info;
    DWORD protect;
    unsigned int flags = PAGE_VALID | PAGE_EXEC;

    if (addr > UINT32_MAX ||
        !VirtualQuery((const void *)(uintptr_t)addr, &info, sizeof(info)) ||
        info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD)) {
        return 0;
    }
    protect = info.Protect & 0xff;
    if (!(protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                     PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) {
        return 0;
    }
    if (protect != PAGE_EXECUTE) {
        flags |= PAGE_READ;
    }
    if (protect & (PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) {
        flags |= PAGE_WRITE | PAGE_WRITE_ORG;
    }
    return flags;
}

static bool code_can_execute(target_ulong addr)
{
    return pe_code_page_flags(addr) != 0;
}

static void code_fault(CPUX86State *env, uint32_t pc, uint32_t addr)
{
    env->eip = pc;
    env->cr[2] = addr;
    env->error_code = PG_ERROR_U_MASK | PG_ERROR_I_D_MASK;
    env->exception_is_int = false;
    env->exception_next_eip = -1;
    env_cpu(env)->exception_index = EXCP0E_PAGE;
    cpu_loop_exit(env_cpu(env));
}

tb_page_addr_t get_page_addr_code_hostp(CPUX86State *env, target_ulong addr,
                                        void **hostp)
{
    unsigned int flags = pe_code_page_flags(addr);
    target_ulong page = addr & TARGET_PAGE_MASK;

    if (!flags) {
        code_fault(env, addr, addr);
    }
    if (have_mmap_lock() && !(page_get_flags(addr) & PAGE_VALID)) {
        page_set_flags(page, page + TARGET_PAGE_SIZE, flags);
    }
    if (hostp) {
        *hostp = (void *)(uintptr_t)addr;
    }
    /* Win32 can modify an executable writable page without a callback. */
    return (flags & PAGE_WRITE) ? -1 : addr;
}

uint32_t cpu_ldub_code(CPUArchState *env, abi_ptr addr)
{
    if (!code_can_execute(addr)) {
        code_fault(env, addr, addr);
    }
    return ldub_p((void *)(uintptr_t)addr);
}

typedef struct PeCodeReader {
    uint8_t *bytes;
    uint32_t pc;
    uint32_t fault;
    bool faulted;
} PeCodeReader;

static int read_code_byte(const struct reader_info *opaque, uint8_t *value,
                          uint64_t address)
{
    PeCodeReader *reader = (PeCodeReader *)opaque;
    uint64_t off = address - reader->pc;
    uint32_t addr = address;

    if (off >= 15) {
        return -1;
    }
    if ((addr & TARGET_PAGE_MASK) != (reader->pc & TARGET_PAGE_MASK) &&
        !code_can_execute(addr)) {
        reader->fault = addr;
        reader->faulted = true;
        return -1;
    }
    *value = reader->bytes[off] = ldub_p((void *)(uintptr_t)addr);
    return 0;
}

void lat_wow64_pe_fetch_code(void *opaque, uint32_t pc, uint8_t bytes[15], bool first)
{
    CPUX86State *env = opaque;
    size_t available = TARGET_PAGE_SIZE - (pc & (TARGET_PAGE_SIZE - 1));

    /* The decoder stops the block before it can fetch from a second page. */
    if (first && !code_can_execute(pc)) {
        code_fault(env, pc, pc);
    }
    if (available >= 15) {
        memcpy(bytes, (const void *)(uintptr_t)pc, 15);
        return;
    }
    PeCodeReader reader = { .bytes = bytes, .pc = pc };
    struct InternalInstruction insn = {0};

    memset(bytes, 0, 15);
    if (decodeInstruction(&insn, read_code_byte, (const void *)&reader,
                          pc, MODE_32BIT) && reader.faulted) {
        code_fault(env, pc, reader.fault);
    }
}

/* LATX helpers can also operate on native scratch storage (FXSAVE/FXRSTOR). */
#define PE_LD_DATA(name, type, load)                                      \
    type name(CPUArchState *env, abi_ptr addr, uintptr_t ra)               \
    {                                                                    \
        type value;                                                      \
        set_helper_retaddr(ra);                                          \
        value = load((const void *)(uintptr_t)addr);                       \
        clear_helper_retaddr();                                          \
        return value;                                                    \
    }
#define PE_ST_DATA(name, type, store)                                    \
    void name(CPUArchState *env, abi_ptr addr, type value, uintptr_t ra)    \
    {                                                                    \
        set_helper_retaddr(ra);                                           \
        store((void *)(uintptr_t)addr, value);                             \
        clear_helper_retaddr();                                          \
    }
PE_LD_DATA(cpu_ldub_data_ra, uint32_t, ldub_p)
PE_LD_DATA(cpu_lduw_le_data_ra, uint32_t, lduw_le_p)
PE_LD_DATA(cpu_ldsw_le_data_ra, int, ldsw_le_p)
PE_LD_DATA(cpu_ldl_le_data_ra, uint32_t, ldl_le_p)
PE_LD_DATA(cpu_ldq_le_data_ra, uint64_t, ldq_le_p)
PE_ST_DATA(cpu_stb_data_ra, uint32_t, stb_p)
PE_ST_DATA(cpu_stw_le_data_ra, uint32_t, stw_le_p)
PE_ST_DATA(cpu_stl_le_data_ra, uint32_t, stl_le_p)
PE_ST_DATA(cpu_stq_le_data_ra, uint64_t, stq_le_p)
#undef PE_ST_DATA
#undef PE_LD_DATA

static void pe_cpu_do_interrupt(CPUState *cpu)
{
    CPUX86State *env = cpu->env_ptr;

    if (env->exception_is_int) {
        env->eip = env->exception_next_eip;
    }
    env->old_exception = -1;
}

static struct TCGCPUOps pe_tcg_ops = { .do_interrupt = pe_cpu_do_interrupt };
static CPUClass pe_cpu_class = { .tcg_ops = &pe_tcg_ops };

static bool pe_thread_init(void)
{
    PeThread *thread = g_try_malloc0(sizeof(*thread));
    CPUState *cpu;
    CPUX86State *env;

    if (!thread) {
        return false;
    }
    if (!TlsSetValue(thread_slot, thread)) {
        g_free(thread);
        return false;
    }
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
                         &thread->control, THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT,
                         FALSE, 0)) {
        TlsSetValue(thread_slot, NULL);
        g_free(thread);
        return false;
    }
    thread->host = pe_host_tls();
    thread->lsenv_data.tr_data = &thread->translation;
    cpu = (CPUState *)&thread->x86;
    env = &thread->x86.env;
    cpu->env_ptr = env;
    OBJECT(cpu)->class = (ObjectClass *)&pe_cpu_class;
    cpu->cpu_index = UNASSIGNED_CPU_INDEX;
    cpu->cflags_next_tb = -1;
    cpu->icount_decr_ptr = &cpu_neg(cpu)->icount_decr;
    env->tb_jmp_cache_ptr = cpu->tb_jmp_cache;
    QTAILQ_INIT(&cpu->breakpoints);
    QTAILQ_INIT(&cpu->watchpoints);
    QSIMPLEQ_INIT(&cpu->work_list);
    qemu_mutex_init(&cpu->work_mutex);
    cpu->nr_cores = cpu->nr_threads = 1;
    cpu->tcg_cflags = CF_PARALLEL;
    env->cr[0] = CR0_PE_MASK;
    env->cr[4] = CR4_OSFXSR_MASK;
    env->hflags = 3 | HF_PE_MASK | HF_OSFXSR_MASK;
    env->cpuid_level = 1;
    env->cpuid_version = 0x00000663;
    env->cpuid_vendor1 = CPUID_VENDOR_INTEL_1;
    env->cpuid_vendor2 = CPUID_VENDOR_INTEL_2;
    env->cpuid_vendor3 = CPUID_VENDOR_INTEL_3;
    env->features[FEAT_1_EDX] = CPUID_FP87 | CPUID_TSC | CPUID_CX8 |
        CPUID_CMOV | CPUID_MMX | CPUID_FXSR | CPUID_SSE | CPUID_SSE2;
    env->gdt.base = (uintptr_t)thread->gdt;
    env->gdt.limit = sizeof(thread->gdt) - 1;
    latx_lsenv_init(env);
    mmap_lock();
    if (!rcu_thread_registered()) {
        rcu_register_thread();
    }
    cpu_list_add(cpu);
    mmap_unlock();
    return true;
}

static void pe_free_thread(PeThread *thread)
{
    mmap_lock();
    qemu_mutex_destroy(&((CPUState *)&thread->x86)->work_mutex);
    latx_lsenv_destroy(&thread->lsenv_data);
    CloseHandle(thread->control);
    g_free(thread->host);
    g_free(thread);
    mmap_unlock();
}

/* Own mmap_lock until the native suspend/terminate operation is finished.
 * Never wait for a CPU to leave execution while holding the translation lock. */
bool lat_wow64_pe_pause_thread(void *handle, void **paused, void **control)
{
    PeThread *thread = NULL;
    CPUState *cpu;

    qemu_spin_lock(&thread_control_lock);
    mmap_lock();
    CPU_FOREACH(cpu) {
        PeThread *entry = container_of((X86CPU *)cpu, PeThread, x86);

        if (!NtCompareObjects(handle, entry->control)) {
            thread = entry;
            break;
        }
    }
    *paused = thread;
    *control = thread ? thread->control : NULL;
    if (thread) {
        const ULONGLONG deadline = GetTickCount64() + 3000;

        qatomic_or(&thread->execution, PE_PAUSE_REQUESTED);
        mmap_unlock();
        while (qatomic_load_acquire(&thread->execution) & PE_EXECUTING) {
            cpu_exit(cpu);
            if (GetTickCount64() >= deadline) {
                qatomic_and(&thread->execution, ~PE_PAUSE_REQUESTED);
                qemu_spin_unlock(&thread_control_lock);
                return false;
            }
            Sleep(0);
        }
        mmap_lock();
    }
    return true;
}

void lat_wow64_pe_finish_thread(void *paused, bool terminated)
{
    PeThread *thread = paused;

    if (thread && terminated) {
        cpu_list_remove((CPUState *)&thread->x86);
        lat_wow64_pe_remove_rcu_reader(&thread->host->reader);
        call_rcu(thread, pe_free_thread, rcu);
    } else if (thread) {
        qatomic_and(&thread->execution, ~PE_PAUSE_REQUESTED);
    }
    mmap_unlock();
    qemu_spin_unlock(&thread_control_lock);
}

void lat_wow64_pe_thread_exit(void)
{
    PeThread *thread = get_pe_thread();

    if (!thread || pe_host_tls()->process_term_lock) {
        return;
    }
    mmap_lock();
    while (qatomic_read(&thread->execution) & PE_PAUSE_REQUESTED) {
        mmap_unlock();
        Sleep(0);
        mmap_lock();
    }
    cpu_list_remove((CPUState *)&thread->x86);
    rcu_unregister_thread();
    TlsSetValue(thread_slot, NULL);
    call_rcu(thread, pe_free_thread, rcu);
    mmap_unlock();
    TlsSetValue(pe_host_slot, NULL);
}

bool lat_wow64_pe_init(void)
{
    SYSTEM_INFO info;
    LARGE_INTEGER frequency;

    GetSystemInfo(&info);
    if (!QueryPerformanceFrequency(&frequency)) {
        return false;
    }
    clock_freq = frequency.QuadPart;
    qemu_host_page_size = qemu_real_host_page_size = info.dwPageSize;
    thread_slot = TlsAlloc();
    pe_host_slot = TlsAlloc();
    if (thread_slot == TLS_OUT_OF_INDEXES || pe_host_slot == TLS_OUT_OF_INDEXES) {
        return false;
    }
    lat_wow64_pe_rcu_init();
    qemu_init_cpu_list();
    options_init();
    option_anonym = 1;
    latx_dt_init();
    if (!pe_thread_init()) {
        return false;
    }
    tcg_exec_init(64 * MiB, 0);
    tcg_prologue_init(tcg_ctx);
    tcg_region_init();
    return true;
}

bool lat_wow64_pe_execute(LatWow64Context *context, LatWow64Exit *exit_info)
{
    PeThread *thread;
    CPUX86State *env;
    CPUState *cpu;
    uint32_t host_fcsr;
    int trap;

    thread = get_pe_thread();
    if (!thread) {
        if (!pe_thread_init()) {
            return false;
        }
        thread = get_pe_thread();
    }
    pe_enter_execution(thread);
    env = &thread->x86.env;
    cpu = env_cpu(env);
    cpu->exception_index = -1;
    env->exception_is_int = false;
    env->old_exception = -1;
    lat_wow64_context_to_cpu(env, context);
    thread->float_status = 0;
    asm volatile("movfcsr2gr %0, $fcsr0" : "=r"(host_fcsr));
    current_cpu = cpu;
    do {
        process_queued_cpu_work(cpu);
        cpu_exec_start(cpu);
        trap = cpu_exec(cpu);
        cpu_exec_end(cpu);
        if (trap == EXCP_ATOMIC) {
            cpu_exec_step_atomic(cpu);
            trap = cpu->exception_index;
        }
    } while ((trap == EXCP_INTERRUPT || trap < 0) &&
             !(qatomic_read(&thread->execution) & PE_PAUSE_REQUESTED));
    asm volatile("movgr2fcsr $fcsr0, %0" : : "r"(host_fcsr));
    if (thread->float_status) {
        trap = lat_wow64_float_trap(env, trap);
    }
    lat_wow64_context_from_cpu(context, env, trap, thread->float_status);
    *exit_info = (LatWow64Exit){
        .size = sizeof(*exit_info),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .reason = LAT_WOW64_EXIT_EXCEPTION,
        .flags = LAT_WOW64_EXIT_FLAG_TRAP_NUMBER_VALID,
        .trap_number = trap,
        .guest_pc = context->eip,
    };
    if (env->exception_is_int && trap == 0x2e) {
        exit_info->reason = LAT_WOW64_EXIT_SYSCALL;
    } else if (trap == EXCP_INTERRUPT || trap < 0) {
        exit_info->reason = LAT_WOW64_EXIT_STOPPED;
    } else if (trap == EXCP03_INT3) {
        exit_info->reason = LAT_WOW64_EXIT_BREAKPOINT;
        exit_info->guest_pc--;
    } else if (trap == EXCP01_DB) {
        exit_info->reason = LAT_WOW64_EXIT_SINGLE_STEP;
    } else if (trap == EXCP0E_PAGE) {
        exit_info->flags |= LAT_WOW64_EXIT_FLAG_ERROR_CODE_VALID |
                            LAT_WOW64_EXIT_FLAG_FAULT_ADDRESS_VALID;
        exit_info->error_code = env->error_code;
        exit_info->fault_address = env->cr[2];
    }
    return true;
}

#undef UC_GR
#include "../tcg/restore-latx.inc"
