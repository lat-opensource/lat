/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/accel.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "qemu/rcu.h"
#include "qemu.h"
#include "cpu_loop-common.h"
#include "cpu.h"
#include "exec/cpu-all.h"
#include "exec/exec-all.h"
#include "exec/cpu_ldst.h"
#include "exec/fasttb.h"
#include "hw/core/cpu.h"
#include "latx-config.h"
#include "latx-options.h"
#include "latx-signal.h"
#include "lsenv.h"
#include "translate.h"
#include "wow64-internal.h"
#include "wow64-x86-state.h"
#include "sysemu/tcg.h"
#include "tcg/tcg.h"
#include "tcg/helper-tcg.h"
#ifdef CONFIG_LATX_CAPSTONE_GIT
#include "capstone_git/arch/X86/X86DisassemblerDecoder.h"
#else
#include "capStone/arch/X86/X86DisassemblerDecoder.h"
#endif

int lat_wow64_cpu_exec(CPUState *cpu);
void lat_wow64_init_fpu_regs(CPUX86State *env);

typedef struct LatWow64Backend {
    GMutex lock;
    GHashTable *cpus;
    CPUState *initial_cpu;
    LatWow64Process *process;
    uint64_t return_address;
} LatWow64Backend;

static GMutex plugin_lock;
static GHashTable *providers;
static bool engine_initialized;
char lat_wow64_provider_plugin_active;
__thread char lat_wow64_provider_guest_active;
static __thread void *lat_wow64_signal_altstack;
static __thread size_t lat_wow64_signal_altstack_size;
static __thread bool lat_wow64_host_signal_altstack_usable;

#define LAT_WOW64_SIGNAL_CPU_SLOTS 256

typedef struct LatWow64SignalCpuSlot {
    int tid;
    CPUState *cpu;
    TranslationBlock *entry_tb;
    uintptr_t *helper_retaddr;
    uint32_t float_status;
} LatWow64SignalCpuSlot;

static LatWow64SignalCpuSlot lat_wow64_signal_cpus[LAT_WOW64_SIGNAL_CPU_SLOTS];

static LatWow64SignalCpuSlot *lat_wow64_provider_find_signal_cpu_slot(bool claim)
{
    int tid = qemu_get_thread_id();
    unsigned int start = (unsigned int)tid % LAT_WOW64_SIGNAL_CPU_SLOTS;
    unsigned int i;

    for (i = 0; i < LAT_WOW64_SIGNAL_CPU_SLOTS; ++i) {
        LatWow64SignalCpuSlot *slot = &lat_wow64_signal_cpus[
            (start + i) % LAT_WOW64_SIGNAL_CPU_SLOTS];
        int owner = qatomic_read(&slot->tid);

        if (owner == tid) return slot;
        if (!claim || owner != 0) continue;
        if (qatomic_cmpxchg(&slot->tid, 0, tid) == 0) return slot;
    }
    return NULL;
}

static bool lat_wow64_provider_reserve_signal_cpu(void)
{
    LatWow64SignalCpuSlot *slot = lat_wow64_provider_find_signal_cpu_slot(true);

    if (!slot) return false;
    qatomic_set(&slot->entry_tb, NULL);
    qatomic_set(&slot->cpu, NULL);
    slot->float_status = 0;
    qatomic_set(&slot->helper_retaddr, &helper_retaddr);
    return true;
}

static void lat_wow64_provider_publish_signal_cpu(CPUState *cpu)
{
    LatWow64SignalCpuSlot *slot = lat_wow64_provider_find_signal_cpu_slot(false);

    g_assert(slot);
    qatomic_set(&slot->cpu, cpu);
}

static uint32_t lat_wow64_provider_clear_signal_cpu(void)
{
    LatWow64SignalCpuSlot *slot = lat_wow64_provider_find_signal_cpu_slot(false);
    uint32_t status;

    g_assert(slot);
    status = slot->float_status;
    qatomic_set(&slot->entry_tb, NULL);
    qatomic_set(&slot->cpu, NULL);
    qatomic_set(&slot->helper_retaddr, NULL);
    qatomic_set(&slot->tid, 0);
    return status;
}

void lat_wow64_provider_record_sigfpe(int code)
{
    LatWow64SignalCpuSlot *slot = lat_wow64_provider_find_signal_cpu_slot(false);

    if (!slot) return;
    /* Linux can clear FCSR Cause before saving the signal context. Preserve
     * the hardware fault reason independently of the sticky exception bits. */
    switch (code) {
    case FPE_FLTINV: case FPE_FLTSUB: slot->float_status = 1; break;
    case FPE_FLTDIV: slot->float_status = 4; break;
    case FPE_FLTOVF: slot->float_status = 8; break;
    case FPE_FLTUND: slot->float_status = 0x10; break;
    case FPE_FLTRES: slot->float_status = 0x20; break;
    default: break;
    }
}

int lat_wow64_provider_signal_state(CPUState **cpu, uintptr_t **retaddr)
{
    LatWow64SignalCpuSlot *slot = lat_wow64_provider_find_signal_cpu_slot(false);

    if (!slot || !(*cpu = qatomic_read(&slot->cpu))) return false;
    *retaddr = qatomic_read(&slot->helper_retaddr);
    return *retaddr != NULL;
}

/* Valid only while cpu_tb_exec owns a live TB reference. Clear it on
 * normal return and the cpu_exec longjmp path before any cache mutation. */
void lat_wow64_provider_tb_enter(TranslationBlock *tb)
{
    LatWow64SignalCpuSlot *slot = lat_wow64_provider_find_signal_cpu_slot(false);

    if (slot) qatomic_set(&slot->entry_tb, tb);
}

TranslationBlock *lat_wow64_provider_signal_entry_tb(void)
{
    LatWow64SignalCpuSlot *slot = lat_wow64_provider_find_signal_cpu_slot(false);

    return slot ? qatomic_read(&slot->entry_tb) : NULL;
}

static void plugin_request_interrupt(LatWow64Thread *thread, void *host_signal_context)
{
    CPUState *cpu;
    uintptr_t *retaddr;
    sigset_t set, previous;

    /* Match the signal handler's serialization while unlinking guest code. */
    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, &previous);
    lat_wow64_request_interrupt(thread);
    if (lat_wow64_provider_signal_state(&cpu, &retaddr)) {
        cpu_exit(cpu);
        if (host_signal_context) {
            tb_exit_to_qemu(cpu->env_ptr, host_signal_context);
        }
    }
    sigprocmask(SIG_SETMASK, &previous, NULL);
}

static bool lat_wow64_provider_install_signal_altstack(stack_t *previous,
                                                    bool *restore)
{
    stack_t stack = { 0 };

    *restore = false;
    if (lat_wow64_host_signal_altstack_usable) return true;
    if (sigaltstack(NULL, previous)) return false;
    /* Wine installs a stable per-thread signal stack before entering guests. */
    if (!(previous->ss_flags & SS_DISABLE) &&
        previous->ss_size >= (size_t)SIGSTKSZ) {
        lat_wow64_host_signal_altstack_usable = true;
        return true;
    }
    if (!lat_wow64_signal_altstack) {
        lat_wow64_signal_altstack_size = MAX((size_t)SIGSTKSZ, (size_t)64 * 1024);
        lat_wow64_signal_altstack = g_try_malloc(lat_wow64_signal_altstack_size);
        if (!lat_wow64_signal_altstack) return false;
    }
    stack.ss_size = lat_wow64_signal_altstack_size;
    stack.ss_sp = lat_wow64_signal_altstack;
    if (sigaltstack(&stack, NULL)) {
        g_free(lat_wow64_signal_altstack);
        lat_wow64_signal_altstack = NULL;
        lat_wow64_signal_altstack_size = 0;
        return false;
    }
    *restore = true;
    return true;
}

static bool lat_wow64_provider_restore_signal_altstack(const stack_t *previous)
{
    return !sigaltstack(previous, NULL);
}

static inline uint32_t lat_wow64_host_fcsr_get(void)
{
#ifdef __loongarch__
    uint32_t value;

    __asm__ volatile("movfcsr2gr %0, $fcsr0" : "=r"(value));
    return value;
#else
    return 0;
#endif
}

static inline void lat_wow64_host_fcsr_set(uint32_t value)
{
#ifdef __loongarch__
    __asm__ volatile("movgr2fcsr $fcsr0, %0" : : "r"(value));
#else
    (void)value;
#endif
}

static uint64_t lat_wow64_latx_teb_base(void)
{
#ifdef __loongarch__
    uintptr_t teb;

    __asm__ volatile("move %0, $r21" : "=r"(teb));
    return teb;
#else
    return 0;
#endif
}

static void lat_wow64_host_teb_set(uint64_t teb)
{
#ifdef __loongarch__
    __asm__ volatile("move $r21, %0" : : "r"(teb) : "$r21");
#else
    (void)teb;
#endif
}


void lat_wow64_provider_check_code(CPUArchState *env, target_ulong pc,
                                    target_ulong address)
{
    int flags = page_get_flags(address);
    CPUState *cpu;

    if ((flags & (PAGE_VALID | PAGE_EXEC)) == (PAGE_VALID | PAGE_EXEC)) {
        return;
    }
    cpu = env_cpu(env);
    env->eip = pc;
    env->cr[2] = address;
    env->error_code = PG_ERROR_U_MASK | PG_ERROR_I_D_MASK |
                      ((flags & PAGE_VALID) ? PG_ERROR_P_MASK : 0);
    env->exception_is_int = 0;
    env->exception_next_eip = -1;
    cpu->exception_index = EXCP0E_PAGE;
    mmap_unlock();
    cpu_loop_exit(cpu);
}

typedef struct LatWow64CodeReader {
    CPUArchState *env;
    target_ulong pc, fault_address;
    uint8_t *code;
    bool fault;
} LatWow64CodeReader;

static int lat_wow64_code_byte(const struct reader_info *arg, uint8_t *byte,
                              uint64_t address)
{
    LatWow64CodeReader *reader = (LatWow64CodeReader *)arg;
    uint64_t offset = address - reader->pc;
    uint32_t guest_address = address;

    if (offset >= 15) return -1;
    if ((page_get_flags(guest_address) & (PAGE_VALID | PAGE_EXEC)) !=
        (PAGE_VALID | PAGE_EXEC)) {
        reader->fault = true;
        reader->fault_address = guest_address;
        return -1;
    }
    *byte = cpu_ldub_code(reader->env, guest_address);
    reader->code[offset] = *byte;
    return 0;
}

void lat_wow64_provider_fetch_code(CPUArchState *env, target_ulong pc,
                                    uint8_t *code)
{
    struct InternalInstruction instruction = { 0 };
    LatWow64CodeReader reader = { .env = env, .pc = pc, .code = code };
    int result;

    memset(code, 0, 15);
    result = decodeInstruction(&instruction, lat_wow64_code_byte, &reader,
                               pc, MODE_32BIT);
    if (result && reader.fault) {
        lat_wow64_provider_check_code(env, pc, reader.fault_address);
    }
}

static bool lat_wow64_latx_trap_has_error_code(int trap)
{
    switch (trap) {
    case EXCP08_DBLE:
    case EXCP0A_TSS:
    case EXCP0B_NOSEG:
    case EXCP0C_STACK:
    case EXCP0D_GPF:
    case EXCP0E_PAGE:
    case EXCP11_ALGN:
        return true;
    default:
        return false;
    }
}

static void destroy_cpu(CPUState *cpu);

static CPUState *create_cpu(void)
{
    struct target_pt_regs regs = {0};
    CPUState *cpu;
    TaskState *task;

    /* CPU realization publishes into the global list before the LATX cache
     * is ready. TB invalidation also takes mmap_lock before traversing it. */
    mmap_lock();
    cpu = cpu_create(parse_cpu_option("qemu64"));
    if (!cpu) {
        mmap_unlock();
        return NULL;
    }
    cpu_reset(cpu);
    target_cpu_copy_regs(cpu->env_ptr, &regs);
    /* The x86_64 Linux initializer installs 16-byte IDT gates.  WoW64
     * executes in protected mode, where software interrupts use 8-byte gates. */
    {
        CPUX86State *env = cpu->env_ptr;
        uint64_t *idt = g2h_untagged(env->idt.base);

        memset(idt, 0, 256 * sizeof(*idt));
        idt[3] = idt[4] = idt[0x80] = (uint64_t)(0x8000 | (3 << 13)) << 32;
        env->idt.limit = 256 * sizeof(*idt) - 1;
    }
    lat_wow64_init_fpu_regs(cpu->env_ptr);
    task = g_new0(TaskState, 1);
    init_task_state(task);
    cpu->opaque = task;
#ifdef CONFIG_LATX_FAST_JMPCACHE
    ((CPUX86State *)cpu->env_ptr)->tb_jmp_cache_ptr = NULL;
    if (!latx_fast_jmp_cache_init(cpu->env_ptr)) {
        destroy_cpu(cpu);
        mmap_unlock();
        return NULL;
    }
#endif
    mmap_unlock();
    return cpu;
}

typedef struct RetiredCpu {
    struct rcu_head rcu;
    CPUState *cpu;
} RetiredCpu;

static void free_cpu(RetiredCpu *retired)
{
    CPUState *cpu = retired->cpu;
    CPUX86State *env = cpu->env_ptr;

    mmap_lock();
#ifdef CONFIG_LATX_FAST_JMPCACHE
    latx_fast_jmp_cache_free_rcu(env->tb_jmp_cache_ptr);
    env->tb_jmp_cache_ptr = NULL;
#endif
    if (env->gdt.base) {
        target_munmap(env->gdt.base, sizeof(uint64_t) * TARGET_GDT_ENTRIES, 0);
    }
    if (env->idt.base) {
        /* target_cpu_copy_regs allocates 512 eight-byte entries. */
        target_munmap(env->idt.base, 512 * sizeof(uint64_t), 0);
    }
    g_free(cpu->opaque);
    cpu->opaque = NULL;
    object_property_set_bool(OBJECT(cpu), "realized", false, NULL);
    object_unparent(OBJECT(cpu));
    object_unref(OBJECT(cpu));
    mmap_unlock();
    g_free(retired);
}

static void destroy_cpu(CPUState *cpu)
{
    RetiredCpu *retired = g_new(RetiredCpu, 1);

    if (thread_cpu == cpu) {
        thread_cpu = NULL;
    }
    mmap_lock();
    cpu_list_remove(cpu);
    mmap_unlock();
    retired->cpu = cpu;
    /* Other guests may hold RCU read references for an arbitrarily long run.
     * Removing this CPU must not wait for those guests to return to Wine. */
    call_rcu(retired, free_cpu, rcu);
}

static LatWow64Status backend_thread_create(void *opaque, LatWow64Thread *thread)
{
    LatWow64Backend *backend = opaque;
    CPUState *cpu;

    cpu = create_cpu();
    g_mutex_lock(&backend->lock);
    if (cpu) {
        g_hash_table_insert(backend->cpus, thread, cpu);
    }
    g_mutex_unlock(&backend->lock);
    return cpu ? LAT_WOW64_STATUS_OK : LAT_WOW64_STATUS_NO_MEMORY;
}

static void backend_thread_destroy(void *opaque, LatWow64Thread *thread)
{
    LatWow64Backend *backend = opaque;
    CPUState *cpu;

    g_mutex_lock(&backend->lock);
    cpu = g_hash_table_lookup(backend->cpus, thread);
    g_hash_table_remove(backend->cpus, thread);
    g_mutex_unlock(&backend->lock);
    if (cpu) {
        destroy_cpu(cpu);
    }
}

static LatWow64Status backend_execute(void *opaque, LatWow64Thread *thread,
                                      LatWow64Context *context,
                                      LatWow64Exit *exit_info)
{
    LatWow64Backend *backend = opaque;
    CPUState *cpu;
    CPUX86State *env;
    stack_t previous_altstack;
    bool restore_altstack, unregister_rcu;
    bool run_guest;
    uint64_t host_teb;
    uint32_t host_fcsr;
    uint32_t float_status;
    unsigned int lock_depth = 0;
    int trap;

    /* The core holds an active reference to this thread until we return. */
    g_mutex_lock(&backend->lock);
    cpu = g_hash_table_lookup(backend->cpus, thread);
    g_mutex_unlock(&backend->lock);
    if (!cpu) {
        return LAT_WOW64_STATUS_INVALID_STATE;
    }
    if (!lat_wow64_provider_install_signal_altstack(&previous_altstack,
                                                   &restore_altstack)) {
        return LAT_WOW64_STATUS_NO_MEMORY;
    }
    if (!lat_wow64_provider_reserve_signal_cpu()) {
        if (restore_altstack) {
            lat_wow64_provider_restore_signal_altstack(&previous_altstack);
        }
        return LAT_WOW64_STATUS_NO_MEMORY;
    }
    env = cpu->env_ptr;
    thread_cpu = cpu;
    unregister_rcu = !rcu_thread_registered();
    if (unregister_rcu) {
        rcu_register_thread();
    }
    if (!tcg_ctx) {
        tcg_register_thread();
    }
    if (!lsenv) {
        latx_lsenv_init(env);
    } else {
        lsenv->cpu_state = env;
    }
    cpu->tcg_cflags |= CF_PARALLEL;
    lat_wow64_context_to_cpu(env, context);
    while (have_mmap_lock()) {
        mmap_unlock();
        ++lock_depth;
    }
    host_teb = lat_wow64_latx_teb_base();
    host_fcsr = lat_wow64_host_fcsr_get();
    qatomic_set(&((TaskState *)cpu->opaque)->ts_tid, qemu_get_thread_id());
    qatomic_set(&cpu->exit_request, 0);
    cpu_exec_start(cpu);
    g_mutex_lock(&backend->lock);
    run_guest = lat_wow64_mapping_generation(backend->process) ==
                exit_info->mapping_generation;
    if (run_guest) {
        lat_wow64_provider_publish_signal_cpu(cpu);
        lat_wow64_provider_guest_active = true;
    }
    g_mutex_unlock(&backend->lock);
    /* A request received before publication cannot kick this CPU. Consume it
     * only after publication so a later request is handled by the signal path. */
    if (lat_wow64_consume_interrupt(thread)) {
        run_guest = false;
    }
    trap = run_guest ? lat_wow64_cpu_exec(cpu) : EXCP_INTERRUPT;
    lat_wow64_host_teb_set(host_teb);
    lat_wow64_host_fcsr_set(host_fcsr);
    lat_wow64_provider_guest_active = false;
    float_status = lat_wow64_provider_clear_signal_cpu();
    /* Execution has returned, so requests received while it ran have been
     * satisfied. They must not stop the next entry a second time. */
    lat_wow64_consume_interrupt(thread);
    cpu_exec_end(cpu);
    process_queued_cpu_work(cpu);
    while (have_mmap_lock()) {
        mmap_unlock();
    }
    while (lock_depth--) {
        mmap_lock();
    }
    if (trap == EXCP00_DIVZ && float_status) {
        trap = lat_wow64_float_trap(env, EXCP10_COPR);
    }
    lat_wow64_context_from_cpu(context, env, trap, float_status);
    if (unregister_rcu) {
        rcu_unregister_thread();
    }
    if (restore_altstack &&
        !lat_wow64_provider_restore_signal_altstack(&previous_altstack)) {
        error_report("failed to restore host signal stack: %s", strerror(errno));
    }

    exit_info->guest_pc = context->eip;
    exit_info->detail = trap;
    switch (trap) {
    case EXCP03_INT3:
        if (context->eip == backend->return_address + 1) {
            exit_info->reason = LAT_WOW64_EXIT_RETURN;
            break;
        }
        exit_info->guest_pc--;
        exit_info->reason = LAT_WOW64_EXIT_BREAKPOINT;
        exit_info->flags = LAT_WOW64_EXIT_FLAG_TRAP_NUMBER_VALID;
        exit_info->trap_number = trap;
        break;
    case EXCP_SYSCALL:
    case 0x80:
        exit_info->reason = LAT_WOW64_EXIT_SYSCALL;
        exit_info->flags = LAT_WOW64_EXIT_FLAG_TRAP_NUMBER_VALID |
                           LAT_WOW64_EXIT_FLAG_SERVICE_NUMBER_VALID;
        exit_info->trap_number = trap;
        exit_info->service_number = context->gpr[LAT_WOW64_EAX];
        break;
    case EXCP01_DB:
        if (!(context->dr6 & 0xf)) {
            context->dr6 |= DR6_BS;
        }
        exit_info->reason = LAT_WOW64_EXIT_SINGLE_STEP;
        exit_info->flags = LAT_WOW64_EXIT_FLAG_TRAP_NUMBER_VALID;
        exit_info->trap_number = trap;
        break;
    case EXCP_DEBUG:
        exit_info->reason = LAT_WOW64_EXIT_BREAKPOINT;
        exit_info->flags = LAT_WOW64_EXIT_FLAG_TRAP_NUMBER_VALID;
        exit_info->trap_number = trap;
        break;
    case EXCP_INTERRUPT:
        exit_info->reason = LAT_WOW64_EXIT_STOPPED;
        break;
    default:
        exit_info->reason = LAT_WOW64_EXIT_EXCEPTION;
        exit_info->flags = LAT_WOW64_EXIT_FLAG_TRAP_NUMBER_VALID;
        exit_info->trap_number = trap;
        if (lat_wow64_latx_trap_has_error_code(trap)) {
            exit_info->flags |= LAT_WOW64_EXIT_FLAG_ERROR_CODE_VALID;
            exit_info->error_code = env->error_code;
        }
        if (trap == EXCP0E_PAGE) {
            exit_info->flags |= LAT_WOW64_EXIT_FLAG_FAULT_ADDRESS_VALID;
            exit_info->fault_address = env->cr[2];
        }
        break;
    }
    return LAT_WOW64_STATUS_OK;
}

static void backend_memory_event(void *opaque, const LatWow64MemoryEvent *event,
                                  uint64_t generation, int invalidate)
{
    LatWow64Backend *backend = opaque;
    target_ulong start = event->address & TARGET_PAGE_MASK;
    target_ulong end = TARGET_PAGE_ALIGN(event->address + event->length);
    int flags = PAGE_VALID;

    if (event->protection & LAT_WOW64_PROT_READ) {
        flags |= PAGE_READ;
    }
    if (event->protection & LAT_WOW64_PROT_WRITE) {
        flags |= PAGE_WRITE | PAGE_WRITE_ORG;
    }
    if (event->protection & LAT_WOW64_PROT_EXEC) {
        flags |= PAGE_EXEC;
    }
    if (invalidate) {
        GHashTableIter iter;
        gpointer cpu;

        /* Long-running guests must leave before the core's unload barrier. */
        g_mutex_lock(&backend->lock);
        g_hash_table_iter_init(&iter, backend->cpus);
        while (g_hash_table_iter_next(&iter, NULL, &cpu)) {
            if (qatomic_read(&((CPUState *)cpu)->running)) {
                qemu_cpu_kick(cpu);
            }
        }
        g_mutex_unlock(&backend->lock);
    }
    mmap_lock();
    if (event->type == LAT_WOW64_MEMORY_MAP ||
        event->type == LAT_WOW64_MEMORY_PROTECT ||
        event->type == LAT_WOW64_MEMORY_UNMAP) {
        page_set_flags(start, end,
                       event->type == LAT_WOW64_MEMORY_UNMAP ? 0 : flags);
    }
    if (invalidate) {
        tb_invalidate_phys_range(event->address, event->address + event->length);
    }
    mmap_unlock();
}

static LatWow64Status plugin_process_create(const LatWow64ProcessConfig *config,
                                            LatWow64Process **out)
{
    static const LatWow64BackendOps ops = {
        .size = sizeof(ops),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .execute_guest = backend_execute,
        .memory_event = backend_memory_event,
        .thread_create = backend_thread_create,
        .thread_destroy = backend_thread_destroy,
    };
    LatWow64Backend *backend;
    LatWow64Status status;

    if (!config || !out || config->size < sizeof(*config)) {
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }
    if (config->abi_version != LAT_WOW64_ABI_VERSION) {
        return LAT_WOW64_STATUS_VERSION_MISMATCH;
    }
    *out = NULL;
    g_mutex_lock(&plugin_lock);
    option_aot = 0;
    option_load_aot = 0;
    if (!engine_initialized) {
        AccelClass *ac;
        const char *value;

        /* The Linux launcher normally parses these before engine setup.
         * This library has no Linux guest argv or launcher configuration. */
        value = getenv("LATX_SOFTFPU");
        if (value) find_option("LATX_SOFTFPU", value);
        value = getenv("LATX_SOFTFPU_FAST");
        if (value) find_option("LATX_SOFTFPU_FAST", value);

        error_init("latx-wow64");
        module_call_init(MODULE_INIT_TRACE);
        qemu_init_cpu_list();
        module_call_init(MODULE_INIT_QOM);
        ac = ACCEL_GET_CLASS(current_accel());
        if (ac->init_machine(NULL)) {
            g_mutex_unlock(&plugin_lock);
            return LAT_WOW64_STATUS_BACKEND_ERROR;
        }
        accel_init_interfaces(ac);
    }
    backend = g_new0(LatWow64Backend, 1);
    g_mutex_init(&backend->lock);
    backend->cpus = g_hash_table_new(g_direct_hash, g_direct_equal);
    backend->return_address = config->return_address;
    backend->initial_cpu = create_cpu();
    if (!backend->initial_cpu) {
        status = LAT_WOW64_STATUS_NO_MEMORY;
        goto fail;
    }
    thread_cpu = backend->initial_cpu;
    if (!lsenv) {
        latx_lsenv_init(thread_cpu->env_ptr);
    } else {
        lsenv->cpu_state = thread_cpu->env_ptr;
    }
    if (!engine_initialized) {
        tcg_prologue_init(tcg_ctx);
        tcg_region_init();
        latx_dt_init();
        lat_wow64_provider_plugin_active = true;
        signal_init();
        engine_initialized = true;
    }
    /* Wine owns mappings and has no Linux ELF/AOT loading stage. */
    option_anonym = 1;
    option_aot = 0;
    option_load_aot = 0;
    status = lat_wow64_process_create(&ops, backend, config, &backend->process);
    if (status != LAT_WOW64_STATUS_OK) {
        destroy_cpu(backend->initial_cpu);
        goto fail;
    }
    if (!providers) {
        providers = g_hash_table_new(g_direct_hash, g_direct_equal);
    }
    g_hash_table_insert(providers, backend->process, backend);
    *out = backend->process;
    g_mutex_unlock(&plugin_lock);
    return LAT_WOW64_STATUS_OK;

fail:
    g_hash_table_destroy(backend->cpus);
    g_mutex_clear(&backend->lock);
    g_free(backend);
    g_mutex_unlock(&plugin_lock);
    return status;
}

static LatWow64Status plugin_process_destroy(LatWow64Process *process)
{
    LatWow64Backend *backend;
    LatWow64Status status;

    g_mutex_lock(&plugin_lock);
    backend = providers ? g_hash_table_lookup(providers, process) : NULL;
    if (!backend) {
        g_mutex_unlock(&plugin_lock);
        return LAT_WOW64_STATUS_INVALID_ARGUMENT;
    }
    status = lat_wow64_process_destroy(process);
    if (status == LAT_WOW64_STATUS_OK) {
        destroy_cpu(backend->initial_cpu);
        g_hash_table_remove(providers, process);
        g_hash_table_destroy(backend->cpus);
        g_mutex_clear(&backend->lock);
        g_free(backend);
    }
    g_mutex_unlock(&plugin_lock);
    return status;
}

const LatWow64Api *lat_wow64_get_api(uint32_t abi_version)
{
    static const LatWow64Api api = {
        .size = sizeof(api),
        .abi_version = LAT_WOW64_ABI_VERSION,
        .process_create = plugin_process_create,
        .process_destroy = plugin_process_destroy,
        .thread_create = lat_wow64_thread_create,
        .thread_destroy = lat_wow64_thread_destroy,
        .get_context = lat_wow64_get_context,
        .set_context = lat_wow64_set_context,
        .enter = lat_wow64_enter,
        .reply = lat_wow64_reply,
        .memory_event = lat_wow64_memory_event,
        .thread_get_info = lat_wow64_thread_get_info,
        .thread_suspend = lat_wow64_thread_suspend,
        .thread_resume = lat_wow64_thread_resume,
        .request_interrupt = plugin_request_interrupt,
    };

    return abi_version == LAT_WOW64_ABI_VERSION ? &api : NULL;
}
