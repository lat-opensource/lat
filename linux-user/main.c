/*
 *  qemu user main
 *
 *  Copyright (c) 2003-2008 Fabrice Bellard
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "library_private.h"
#include "qemu/osdep.h"
#include "qemu-common.h"
#include "qemu/units.h"
#include "qemu/accel.h"
#include "sysemu/tcg.h"
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/shm.h>
#include <linux/binfmts.h>
#include <stdlib.h>

#include "qapi/error.h"
#include "qemu.h"
#include "qemu/path.h"
#include "qemu/queue.h"
#include "qemu/config-file.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "qemu/help_option.h"
#include "qemu/module.h"
#include "qemu/plugin.h"
#include "exec/exec-all.h"
#include "tcg/tcg.h"
#include "qemu/timer.h"
#include "qemu/envlist.h"
#include "qemu/guest-random.h"
#include "elf.h"
#include "trace/control.h"
#include "target_elf.h"
#include "cpu_loop-common.h"
#include "crypto/init.h"
int mydebug = 1;

#ifdef CONFIG_LATX
#include "latx-config.h"
#include "latx-options.h"
#include "aot.h"
#include <openssl/evp.h>
#endif
#ifdef CONFIG_LATX_PERF
#include "latx-perf.h"
#endif
#ifdef CONFIG_LATX
#include "debug.h"
#include "fileutils.h"
#include "box64context.h"
#include "elfloader.h"
#include "elfloader_private.h"
#include "khash.h"
#include "elfload_dump.h"
#include "librarian.h"
#include "wrapper.h"
#if defined(CONFIG_LATX_KZT)
#include "wrappertbbridge.h"
box64context_t* my_context = NULL;
elfheader_t* elf_header = NULL;
int relocation_log = 0; //LOG_NONE;
int relocation_dump = 0;
int box64_pagesize;
uintptr_t box64_load_addr = 0;
int dlsym_error = 0;
int kzt_call_log = 0;
int cycle_log = 0;
int allow_missing_libs = 1;
int box64_nogtk = 0;
int box64_prefer_emulated = 0;
int box64_prefer_wrapped = 0;
int fix_64bit_inodes = 0;
int box64_nopulse = 0;
int box64_novulkan = 0;
char* libGL = NULL;
int kzt_init(char** argv, int argc,char** target_argv, int  target_argc,
    struct linux_binprm* bprm);
void kzt_bridge_init(void);
int is_user_map = 0;
#endif
uintptr_t fmod_smc_start = 0;
uintptr_t fmod_smc_end = 0;
int jit_gdb = 0;
int box64_tcmalloc_minimal = 0;
#endif

#ifndef AT_FLAGS_PRESERVE_ARGV0
#define AT_FLAGS_PRESERVE_ARGV0_BIT 0
#define AT_FLAGS_PRESERVE_ARGV0 (1 << AT_FLAGS_PRESERVE_ARGV0_BIT)
#endif

char *exec_path;
char *real_path;
#ifdef CONFIG_LATX_AOT
char *aot_file_path;
char *aot_file_lock;
#endif

#ifdef TARGET_X86_64
int latx_wine;
#endif
int singlestep;
static const char *argv0;
static const char *gdbstub;
static envlist_t *envlist;
static const char *cpu_model;
static const char *cpu_type;
static const char *seed_optarg;
#ifdef CONFIG_LATX_AOT
const char *aot_file_size_optarg;
const char *aot_left_file_minsize_optarg;
char ** latx_aot_wine_pefiles_cache = NULL;
#endif
unsigned long mmap_min_addr;
#ifdef CONFIG_GUEST_BASE_ZERO
uintptr_t guest_base = 0;
bool have_guest_base = 1;
#else
uintptr_t guest_base;
bool have_guest_base;
#endif
struct image_info info1, *info = &info1;

/*
 * Used to implement backwards-compatibility for the `-strace`, and
 * QEMU_STRACE options. Without this, the QEMU_LOG can be overwritten by
 * -strace, or vice versa.
 */
static bool enable_strace;
static bool enable_strace_error;

/*
 * The last log mask given by the user in an environment variable or argument.
 * Used to support command line arguments overriding environment variables.
 */
static int last_log_mask;

/*
 * When running 32-on-64 we should make sure we can fit all of the possible
 * guest address space into a contiguous chunk of virtual host memory.
 *
 * This way we will never overlap with our own libraries or binaries or stack
 * or anything else that QEMU maps.
 *
 * Many cpus reserve the high bit (or more than one for some 64-bit cpus)
 * of the address for the kernel.  Some cpus rely on this and user space
 * uses the high bit(s) for pointer tagging and the like.  For them, we
 * must preserve the expected address space.
 */
#ifndef MAX_RESERVED_VA
# if HOST_LONG_BITS > TARGET_VIRT_ADDR_SPACE_BITS
#  if TARGET_VIRT_ADDR_SPACE_BITS == 32 && \
      (TARGET_LONG_BITS == 32 || defined(TARGET_ABI32))
/* There are a number of places where we assign reserved_va to a variable
   of type abi_ulong and expect it to fit.  Avoid the last page.  */
#   define MAX_RESERVED_VA(CPU)  (0xfffffffful & TARGET_PAGE_MASK)
#  else
#   define MAX_RESERVED_VA(CPU)  (1ul << TARGET_VIRT_ADDR_SPACE_BITS)
#  endif
# else
#  define MAX_RESERVED_VA(CPU)  0
# endif
#endif

unsigned long reserved_va;

#if defined(CONFIG_LATX_DEBUG) || defined(CONFIG_DEBUG_TCG)
static void usage(int exitcode);
#endif

const char *interp_prefix = CONFIG_QEMU_INTERP_PREFIX;
const char *qemu_uname_release;

/* XXX: on x86 MAP_GROWSDOWN only works if ESP <= address + 32, so
   we allocate a bigger stack. Need a better solution, for example
   by remapping the process stack directly at the right place */
unsigned long real_guest_stack_size = 128 * 1024 * 1024UL;
unsigned long vir_guest_stack_size = 8 * 1024 * 1024UL;
unsigned long vir_rlimit_as = RLIM_INFINITY;
unsigned long vir_rlimit_as_old = RLIM_INFINITY;
unsigned long vir_rlimit_as_acc = 0;

#if defined(TARGET_I386)
int cpu_get_pic_interrupt(CPUX86State *env)
{
    return -1;
}
#endif

/***********************************************************/
/* Helper routines for implementing atomic operations.  */

/* Make sure everything is in a consistent state for calling fork().  */
void fork_start(void)
{
    start_exclusive();
    mmap_fork_start();
    sigact_fork_start();
    cpu_list_lock();
}

void fork_end(int child)
{
    mmap_fork_end(child);
    sigact_fork_end(child);
    if (child) {
        CPUState *cpu, *next_cpu;
        /* Child processes created by fork() only have a single thread.
           Discard information about the parent threads.  */
        CPU_FOREACH_SAFE(cpu, next_cpu) {
            if (cpu != thread_cpu) {
                QTAILQ_REMOVE_RCU(&cpus, cpu, node);
            }
        }
        qemu_init_cpu_list();
        gdbserver_fork(thread_cpu);
        /* qemu_init_cpu_list() takes care of reinitializing the
         * exclusive state, so we don't need to end_exclusive() here.
         */
    } else {
        cpu_list_unlock();
        end_exclusive();
    }
}

__thread CPUState *thread_cpu;

bool qemu_cpu_is_self(CPUState *cpu)
{
    return thread_cpu == cpu;
}

void qemu_cpu_kick(CPUState *cpu)
{
    cpu_exit(cpu);
}

void task_settid(TaskState *ts)
{
    if (ts->ts_tid == 0) {
        ts->ts_tid = (pid_t)syscall(SYS_gettid);
    }
}

void stop_all_tasks(void)
{
    /*
     * We trust that when using NPTL, start_exclusive()
     * handles thread stopping correctly.
     */
    start_exclusive();
}

/* Assumes contents are already zeroed.  */
void init_task_state(TaskState *ts)
{
    ts->used = 1;
    ts->sigaltstack_used = (struct target_sigaltstack) {
        .ss_sp = 0,
        .ss_size = 0,
        .ss_flags = TARGET_SS_DISABLE,
    };
}

CPUArchState *cpu_copy(CPUArchState *env)
{
    CPUState *cpu = env_cpu(env);
    CPUState *new_cpu = cpu_create(cpu_type);
    CPUArchState *new_env = new_cpu->env_ptr;
    CPUBreakpoint *bp;
    CPUWatchpoint *wp;

    /* Reset non arch specific state */
    cpu_reset(new_cpu);

    new_cpu->tcg_cflags = cpu->tcg_cflags;
    memcpy(new_env, env, sizeof(CPUArchState));

    /*
     * NOTE: Current QEMU only has one and only one gdt_table ptr.
     * On the other hand, Wine leverage fs reg for TEB storage, detail information
     * please refer to link: https://wiki.winehq.org/Wine_Developer%27s_Guide/Kernel_modules
     * Current QEMU has one and only one gdt for all threads, that will lead to wine cannot get
     * correct ldt in gdt table. The new thread ldt will overwrite the previous thread ldt addr.
     * To solve this issue, create a per-thread gdt for each thread to make sure ldt entry safe.
     */
    new_env->gdt.base = target_mmap(0, sizeof(uint64_t) * TARGET_GDT_ENTRIES,
                                    PROT_READ|PROT_WRITE,
                                    MAP_ANONYMOUS|MAP_PRIVATE, -1, 0, 0);
    assert(new_env->gdt.base > 0);
    memcpy(g2h(cpu, new_env->gdt.base), g2h(cpu, env->gdt.base),
                                    sizeof(uint64_t) * TARGET_GDT_ENTRIES);
    /* Clone all break/watchpoints.
       Note: Once we support ptrace with hw-debug register access, make sure
       BP_CPU break/watchpoints are handled correctly on clone. */
    QTAILQ_INIT(&new_cpu->breakpoints);
    QTAILQ_INIT(&new_cpu->watchpoints);
    QTAILQ_FOREACH(bp, &cpu->breakpoints, entry) {
        cpu_breakpoint_insert(new_cpu, bp->pc, bp->flags, NULL);
    }
    QTAILQ_FOREACH(wp, &cpu->watchpoints, entry) {
        cpu_watchpoint_insert(new_cpu, wp->vaddr, wp->len, wp->flags, NULL);
    }

#ifdef CONFIG_LATX
    new_env->tb_jmp_cache_ptr = new_cpu->tb_jmp_cache;
#endif
    return new_env;
}

#if defined(CONFIG_LATX_DEBUG) || defined(CONFIG_DEBUG_TCG)
#ifdef CONFIG_LATX
#include "latx-options.h"

static void handle_range_trace_begin(const char *arg)
{
    qemu_strtol(arg, NULL, 16, (long *)&option_begin_trace_addr);
}

static void handle_range_trace_end(const char *arg)
{
    qemu_strtol(arg, NULL, 16, (long *)&option_end_trace_addr);
}

static void handle_arg_latx_print(const char *arg)
{
    options_parse_dump(arg);
    qemu_set_log(0);
}

static void handle_arg_latx_show_tb(const char *arg)
{
    options_parse_show_tb(arg);
    qemu_set_log(0);
}

static void handle_arg_latx_trace_mem(const char *arg)
{
    options_parse_trace_mem(arg);
}

static void handle_arg_latx_break_insn(const char *arg)
{
    options_parse_break_insn(arg);
}

static void handle_arg_debug_lative(const char *arg)
{
    options_parse_debug_lative();
    qemu_set_log(0);
}

static void handle_arg_enable_fcsr_exc(const char *arg)
{
    option_enable_fcsr_exc = 1;
    if (!option_save_xmm) {
        option_save_xmm = 0xff;
    }
}

static void handle_arg_latx_unlink(const char *arg)
{
    options_parse_latx_unlink(arg);
}

static void handle_arg_latx_em_debug(const char *arg)
{
    option_em_debug = 1;
}

static void handle_arg_latx_trace(const char *arg)
{
    options_parse_trace(arg);
}

static void handle_arg_latx_check(const char *arg)
{
    option_check = 1;
}

static void handle_arg_latx_tb_dump(const char *arg)
{
    option_dump_all_tb = 1;
}

static void handle_arg_latx_disassemble_trace_cmp(const char *arg)
{
    options_parse_latx_disassemble_trace_cmp(arg);
}

#endif

static void handle_arg_help(const char *arg)
{
    usage(EXIT_SUCCESS);
}

static void handle_arg_imm_skip_pc(const char *arg) {
  imm_skip_pc = strtol(arg, NULL, 16);
}

static void handle_arg_log(const char *arg)
{
    last_log_mask = qemu_str_to_log_mask(arg);
    if (!last_log_mask) {
        qemu_print_log_usage(stdout);
        exit(EXIT_FAILURE);
    }
}

static void handle_arg_dfilter(const char *arg)
{
    qemu_set_dfilter_ranges(arg, &error_fatal);
}


static void handle_arg_log_filename(const char *arg)
{
    qemu_set_log_filename(arg, &error_fatal);
}

static void handle_arg_set_env(const char *arg)
{
    char *r, *p, *token;
    r = p = strdup(arg);
    while ((token = strsep(&p, ",")) != NULL) {
        if (envlist_setenv(envlist, token) != 0) {
            usage(EXIT_FAILURE);
        }
    }
    free(r);
}

static void handle_arg_unset_env(const char *arg)
{
    char *r, *p, *token;
    r = p = strdup(arg);
    while ((token = strsep(&p, ",")) != NULL) {
        if (envlist_unsetenv(envlist, token) != 0) {
            usage(EXIT_FAILURE);
        }
    }
    free(r);
}

static void handle_arg_argv0(const char *arg)
{
    argv0 = strdup(arg);
}

static void handle_arg_stack_size(const char *arg)
{
    const char *p;
    unsigned long lv;
    real_guest_stack_size = qemu_strtoul(arg, &p, 0, &lv);
    vir_guest_stack_size = real_guest_stack_size;
    if (vir_guest_stack_size == 0) {
        usage(EXIT_FAILURE);
    }

    if (*p == 'M') {
        real_guest_stack_size *= MiB;
        vir_guest_stack_size *= MiB;
    } else if (*p == 'k' || *p == 'K') {
        real_guest_stack_size *= KiB;
        vir_guest_stack_size *= KiB;
    }
}

static void handle_arg_pagesize(const char *arg)
{
    qemu_host_page_size = atoi(arg);
    if (qemu_host_page_size == 0 ||
        (qemu_host_page_size & (qemu_host_page_size - 1)) != 0) {
        fprintf(stderr, "page size must be a power of two\n");
        exit(EXIT_FAILURE);
    }
}

static void handle_arg_seed(const char *arg)
{
    seed_optarg = arg;
}

static void handle_arg_gdb(const char *arg)
{
    gdbstub = g_strdup(arg);
}

static void handle_arg_uname(const char *arg)
{
    qemu_uname_release = strdup(arg);
}

static void handle_arg_cpu(const char *arg)
{
    cpu_model = strdup(arg);
    if (cpu_model == NULL || is_help_option(cpu_model)) {
        /* XXX: implement xxx_cpu_list for targets that still miss it */
#if defined(cpu_list)
        cpu_list();
#endif
        exit(EXIT_FAILURE);
    }
}

static void handle_arg_guest_base(const char *arg)
{
    guest_base = strtol(arg, NULL, 0);
    have_guest_base = true;
}

static void handle_arg_reserved_va(const char *arg)
{
    char *p;
    int shift = 0;
    reserved_va = strtoul(arg, &p, 0);
    switch (*p) {
    case 'k':
    case 'K':
        shift = 10;
        break;
    case 'M':
        shift = 20;
        break;
    case 'G':
        shift = 30;
        break;
    }
    if (shift) {
        unsigned long unshifted = reserved_va;
        p++;
        reserved_va <<= shift;
        if (reserved_va >> shift != unshifted) {
            fprintf(stderr, "Reserved virtual address too big\n");
            exit(EXIT_FAILURE);
        }
    }
    if (*p) {
        fprintf(stderr, "Unrecognised -R size suffix '%s'\n", p);
        exit(EXIT_FAILURE);
    }
}

static void handle_arg_singlestep(const char *arg)
{
    singlestep = 1;
}

static void handle_arg_strace(const char *arg)
{
    enable_strace = true;
}

static void handle_arg_strace_error(const char *arg)
{
    enable_strace_error = true;
}

static void handle_arg_trace(const char *arg)
{
    trace_opt_parse(arg);
}

#if defined(TARGET_XTENSA)
static void handle_arg_abi_call0(const char *arg)
{
    xtensa_set_abi_call0();
}
#endif

static QemuPluginList plugins = QTAILQ_HEAD_INITIALIZER(plugins);

#ifdef CONFIG_PLUGIN
static void handle_arg_plugin(const char *arg)
{
    qemu_plugin_opt_parse(arg, &plugins);
}
#endif
#endif

#ifdef CONFIG_LATX
static void handle_arg_version(const char *arg)
{
    printf("lat-" TARGET_NAME " " LATX_VERSION
           "\n");
    exit(EXIT_SUCCESS);
}

static void handle_arg_ld_prefix(const char *arg)
{
    interp_prefix = strdup(arg);
}

static void handle_arg_optimize(const char *arg)
{
    options_parse_opt(arg);
}

static void handle_arg_latx_parallel(const char *arg)
{
    close_latx_parallel = strtol(arg, NULL, 0);
}

static void handle_arg_latx_softfpu(const char *arg)
{
    option_softfpu = strtol(arg, NULL, 0);
    if (option_softfpu) {
        option_aot = 0;
    }
}

static void handle_arg_latx_softfpu_fast(const char *arg)
{
    option_softfpu_fast = strtol(arg, NULL, 0);
}

static void handle_arg_latx_prlimit(const char *arg)
{
    option_prlimit = strtol(arg, NULL, 0);
}

#ifdef CONFIG_LATX_AVX_OPT
static void handle_arg_latx_avx_cpuid(const char *arg)
{
    printf("handle_arg_latx_avx_cpuid:arg=%s\n",arg);
    option_avx_cpuid = strtol(arg, NULL, 0);
}
#endif

#if defined(CONFIG_LATX_KZT)
static void handle_arg_latx_kzt(const char *arg)
{
    option_kzt = strtol(arg, NULL, 0);
}
#endif

static void handle_arg_latx_fputag(const char *arg)
{
    option_fputag = strtol(arg, NULL, 0);
}

static void handle_arg_latx_imm_reg(const char *arg)
{
    options_parse_imm_reg(arg);
}

static void handle_arg_save_xmm(const char *arg)
{
    if (strtol(arg, NULL, 0)) {
        option_save_xmm = 0xff;
    }
}

static void handle_arg_latx_jrra(const char *arg)
{
    option_jr_ra = strtol(arg, NULL, 0);
    if (option_jr_ra) {
        option_aot = 0;
    }
}

static void handle_arg_latx_anonym(const char *arg)
{
    option_anonym = strtol(arg, NULL, 0);
#ifdef CONFIG_LATX_AOT
    option_aot = 0;
#endif
}

static void handle_arg_mmap_start(const char *arg)
{
    mmap_next_start = strtol(arg, NULL, 0);
}

static void handle_arg_mmap_fixed(const char *arg)
{
    option_mmap_fixed = strtol(arg, NULL, 0);
}

static void handle_arg_latx_unimp_dump(const char *arg)
{
    last_log_mask = LOG_UNIMP;
    qemu_set_log_filename(arg, &error_fatal);
}

static void handle_arg_latx_mem_test(const char *arg)
{
    option_mem_test = strtol(arg, NULL, 0);
    if (option_mem_test) {
        if (sysconf(_SC_PAGESIZE) != 16384) {
            option_mem_test = 0;
        } else {
            option_aot = 0;
        }
    }
}

static void handle_arg_latx_real_maps(const char *arg)
{
    option_real_maps = strtol(arg, NULL, 0);
    if (option_real_maps) {
        option_aot = 0;
    }
}

static void handle_arg_latx_monitor_shared_mem(const char *arg)
{
    option_monitor_shared_mem = strtol(arg, NULL, 0);
}

#ifdef CONFIG_LATX_AOT
static void handle_arg_latx_aot(const char *arg)
{
    option_aot = strtol(arg, NULL, 0);
    if (option_softfpu || option_mem_test) {
        option_aot = 0;
    }
}

static void handle_arg_latx_aot_wine_pefiles_cache(const char *arg)
{
    if (!arg || !strlen(arg)) {
        return;
    }
    char * tmp[100] = {0};
    int index;
    char *s1 = NULL;
    char * tmp_str = malloc(strlen(arg) + 1);
    strcpy(tmp_str, arg);
    s1 = strtok(tmp_str, ",");
    if (!s1) {
       latx_aot_wine_pefiles_cache = malloc (2 * sizeof(char *));
       latx_aot_wine_pefiles_cache[0] = tmp_str;
       latx_aot_wine_pefiles_cache[1] = NULL;
       return;
    }
    for (index = 0; index < 100 && s1; index++,s1 = strtok(NULL, ",")) {
        tmp[index] = s1;
    }
    tmp[index] = NULL;
    latx_aot_wine_pefiles_cache = malloc ((index + 1) * sizeof(char *));
    memcpy(latx_aot_wine_pefiles_cache, tmp, (index + 1) * sizeof(char *));
    lsassert(!latx_aot_wine_pefiles_cache[index]);
}

#endif

#ifdef CONFIG_LATX_AOT
static void handle_arg_lat_aot_file_size(const char *arg)
{
    aot_file_size_optarg = arg;
}
static void handle_arg_lat_aot_left_file_size(const char *arg)
{
    aot_left_file_minsize_optarg = arg;
}
#endif
#else
static void handle_arg_version(const char *arg)
{
    printf("lat-" TARGET_NAME " " QEMU_FULL_VERSION
           "\n");
    exit(EXIT_SUCCESS);
}
#endif

struct qemu_argument {
    const char *argv;
    const char *env;
    bool has_arg;
    void (*handle_opt)(const char *arg);
    const char *example;
    const char *help;
};

static const struct qemu_argument arg_table[] = {
#ifdef CONFIG_LATX
    {"latx-optimize",   "LAT_OPTIMIZE",      false, handle_arg_optimize,
    "",           "specify enabled optimize type"},
    {"latx-close-parallel",    "LATX_CLOSE_PARALLEL",     true,  handle_arg_latx_parallel,
    "",           "disable latx parallel"},
    {"latx-softfpu",    "LATX_SOFTFPU",     true,  handle_arg_latx_softfpu,
    "",           "enable softfpu"},
    {"latx-softfpu-fast",    "LATX_SOFTFPU_FAST",     true,  handle_arg_latx_softfpu_fast,
    "",           "enable softfpu fast"},
    {"latx-prlimit",    "LATX_PRLIMIT",     true,  handle_arg_latx_prlimit,
    "",           "enable prlimit"},
#if defined(CONFIG_LATX_AVX_OPT)
    {"latx-avx-cpuid",    "LATX_AVX_CPUID",     true,  handle_arg_latx_avx_cpuid,
    "",           "enable avx cpuid"},
#endif
#if defined(CONFIG_LATX_KZT)
    {"latx-kzt",    "LATX_KZT",     true,  handle_arg_latx_kzt,
    "",           "enable kuzhitong"},
#endif
    {"latx-fputag",    "LATX_FPUTAG",     true,  handle_arg_latx_fputag,
    "",           "enable fputag"},
    {"save-xmm",    "SAVE_XMM",     true,  handle_arg_save_xmm,
    "",           ""},
    {"latx-jrra",    "LATX_JRRA",     true,  handle_arg_latx_jrra,
    "",           "enable jrra"},
    {"latx-imm-reg",    "LATX_IMM_REG",     true,  handle_arg_latx_imm_reg,
    "",           "enable imm reg optimization"},
    {"latx-mem-test",    "LATX_MT",     true,  handle_arg_latx_mem_test,
    "",           "test memory right when memory access"},
    {"latx-real-maps",    "LATX_REAL_MAPS",     true,  handle_arg_latx_real_maps,
    "",           "enable get real self maps"},
    {"latx-monitor-shared-mem",    "LATX_MONITOR_SHARED_MEM",     true,  handle_arg_latx_monitor_shared_mem,
    "",           "monitor shared memory, retranslate self modifying page"},
#ifdef CONFIG_LATX_AOT
    {"latx-aot",    "LATX_AOT",     true,  handle_arg_latx_aot,
    "",           "enable aot"},
    {"latx-aot-file-size", "LAT_AOT_FILE_SIZE", false,
    handle_arg_lat_aot_file_size, "", "set max aot file size by MB."},
    {"latx-aot-left-file-size",   "LAT_AOT_LEFT_FILE_SIZE", false,
    handle_arg_lat_aot_left_file_size, "", "set left aot file size  by MB."},
    {"latx-aot-wine-pefiles-cache",    "LATX_AOT_WINE_PEFILES_CACHE",     true,
    handle_arg_latx_aot_wine_pefiles_cache, "", "aot load pe files"
    "e.g. .dll, .exe, .sys or .drv"},
#endif
    {"latx-anonym",    "LATX_ANONYM",     true,  handle_arg_latx_anonym,
    "",           "anonymize latx for guest"},
    {"latx-mmap_start",    "LATX_MMAP_START",     true,  handle_arg_mmap_start,
    "",           "adjust the initial address of mmap"},
    {"latx-mmap_fixed",    "LATX_MMAP_FIXED",     true,  handle_arg_mmap_fixed,
    "",           "force mmap with address to be MAP_FIXED"},
    {"latx-unimp-dump",     "LATX_UNIMP_DUMP", false, handle_arg_latx_unimp_dump,
    "",                 "LATX dump unsupport syscall"},
#endif
#if defined(CONFIG_LATX_DEBUG) || defined(CONFIG_DEBUG_TCG)
#ifdef CONFIG_LATX
    {"latx-runtime-trace-begin", "LATX_BEGIN_TRACE",
     false, handle_range_trace_begin,
    "", "runtime begin trace per instruction"},
    {"latx-runtime-trace-end", "LATX_END_TRACE", false, handle_range_trace_end,
    "", "runtime end trace per instruction"},
    {"latx-dump",       "LATX_DUMP",         true,  handle_arg_latx_print,
    "bitmap",           "LATX-dump-transalte-info: 5 bits each for func,ir1,ir2,host,profile"},
    {"latx-show-tb",    "LATX_SHOW_TB",      true,  handle_arg_latx_show_tb,
    "",           "show the func,ir1,ir2 and host of tb based on the PC"},
    {"latx-trace-mem",  "LATX_TRACE_MEM",    true,  handle_arg_latx_trace_mem,
    "",           "show the ir1 pc that last written to the addr"},
    {"latx-break-insn", "LATX_BREAK_INSN",   true,  handle_arg_latx_break_insn,
    "",           "triggering a SEGV at the given PC of ir1"},
    {"latx-debug-lative", "LATX_DEBUG_LATIVE", false,  handle_arg_debug_lative,
    "",           "triggering a SEGV at the given PC of ir1"},
    {"latx-enable_fcsr_exc", "LATX_ENABLE_FCSR_EXC", false,  handle_arg_enable_fcsr_exc,
    "",           "enable all exception bit in fcsr"},
    {"latx-unlink",     "LATX_UNLINK",       true,  handle_arg_latx_unlink,
    "",           "unlink_count[,cpu_index]"},
    {"latx-em-debug",    "",                 false,  handle_arg_latx_em_debug,
    "",           ""},
    {"latx-trace",       "",                 true,  handle_arg_latx_trace,
    "bitmap",           "LATX-trace-TB-execution: 2 bits each for TB,ir1,ir2"},
    {"latx-check",       "",                 false, handle_arg_latx_check,
    "",                 "LATX-enable-check"},
    {"latx-tb-dump",     "",                 false, handle_arg_latx_tb_dump,
    "",                 "LATX dump all the run times of all TB"},
    {"latx-disassemble-trace-cmp",     "LATX_DISASSEMBLE_TRACE_CMP",
        true, handle_arg_latx_disassemble_trace_cmp,
        "", "LATX Compare different disassemble."},
#endif
    {"h",          "",                 false, handle_arg_help,
     "",           "print this help"},
    {"help",       "",                 false, handle_arg_help,
     "",           ""},
    {"g",          "LAT_GDB",         true,  handle_arg_gdb,
     "port",       "wait gdb connection to 'port'"},
    {"s",          "LAT_STACK_SIZE",  true,  handle_arg_stack_size,
     "size",       "set the stack size to 'size' bytes"},
    {"cpu",        "LAT_CPU",         true,  handle_arg_cpu,
     "model",      "select CPU (-cpu help for list)"},
    {"E",          "LAT_SET_ENV",     true,  handle_arg_set_env,
     "var=value",  "sets targets environment variable (see below)"},
    {"U",          "LAT_UNSET_ENV",   true,  handle_arg_unset_env,
     "var",        "unsets targets environment variable (see below)"},
    {"0",          "LAT_ARGV0",       true,  handle_arg_argv0,
     "argv0",      "forces target process argv[0] to be 'argv0'"},
    {"r",          "LAT_UNAME",       true,  handle_arg_uname,
     "uname",      "set lat uname release string to 'uname'"},
    {"B",          "LAT_GUEST_BASE",  true,  handle_arg_guest_base,
     "address",    "set guest_base address to 'address'"},
    {"R",          "LAT_RESERVED_VA", true,  handle_arg_reserved_va,
     "size",       "reserve 'size' bytes for guest virtual address space"},
    {"d",          "LAT_LOG",         true,  handle_arg_log,
     "item[,...]", "enable logging of specified items "
     "(use '-d help' for a list of items)"},
    {"imm-skip",   "LAT_IMM_SKIP_PC", true,  handle_arg_imm_skip_pc,
     "address",    "latx imm reg opt skip pc"},
    {"dfilter",    "LAT_DFILTER",     true,  handle_arg_dfilter,
     "range[,...]","filter logging based on address range"},
    {"D",          "LAT_LOG_FILENAME", true, handle_arg_log_filename,
     "logfile",     "write logs to 'logfile' (default stderr)"},
    {"p",          "LAT_PAGESIZE",    true,  handle_arg_pagesize,
     "pagesize",   "set the host page size to 'pagesize'"},
    {"singlestep", "LAT_SINGLESTEP",  false, handle_arg_singlestep,
     "",           "run in singlestep mode"},
    {"strace",     "LAT_STRACE",      false, handle_arg_strace,
     "",           "log system calls"},
    {"strace-error",     "LAT_STRACE_ERROR",      false, handle_arg_strace_error,
     "",           "log system calls"},
    {"seed",       "LAT_RAND_SEED",   true,  handle_arg_seed,
     "",           "Seed for pseudo-random number generator"},
    {"trace",      "LAT_TRACE",       true,  handle_arg_trace,
     "",           "[[enable=]<pattern>][,events=<file>][,file=<file>]"},
#ifdef CONFIG_PLUGIN
    {"plugin",     "LAT_PLUGIN",      true,  handle_arg_plugin,
     "",           "[file=]<file>[,arg=<string>]"},
#endif
#endif
    {"L",          "LAT_LD_PREFIX",   true,  handle_arg_ld_prefix,
     "path",       "set the elf interpreter prefix to 'path'"},
    {"version",    "LAT_VERSION",     false, handle_arg_version,
     "",           "display version information and exit"},
#if defined(TARGET_XTENSA)
    {"xtensa-abi-call0", "QEMU_XTENSA_ABI_CALL0", false, handle_arg_abi_call0,
     "",           "assume CALL0 Xtensa ABI"},
#endif
    {NULL, NULL, false, NULL, NULL, NULL}
};

#if defined(CONFIG_LATX_DEBUG) || defined(CONFIG_DEBUG_TCG)
static void usage(int exitcode)
{
    const struct qemu_argument *arginfo;
    int maxarglen;
    int maxenvlen;

    printf("usage: lat-" TARGET_NAME " [options] program [arguments...]\n"
           "Linux CPU emulator (compiled for " TARGET_NAME " emulation)\n"
           "\n"
           "Options and associated environment variables:\n"
           "\n");

    /* Calculate column widths. We must always have at least enough space
     * for the column header.
     */
    maxarglen = strlen("Argument");
    maxenvlen = strlen("Env-variable");

    for (arginfo = arg_table; arginfo->handle_opt != NULL; arginfo++) {
        int arglen = strlen(arginfo->argv);
        if (arginfo->has_arg) {
            arglen += strlen(arginfo->example) + 1;
        }
        if (strlen(arginfo->env) > maxenvlen) {
            maxenvlen = strlen(arginfo->env);
        }
        if (arglen > maxarglen) {
            maxarglen = arglen;
        }
    }

    printf("%-*s %-*s Description\n", maxarglen+1, "Argument",
            maxenvlen, "Env-variable");

    for (arginfo = arg_table; arginfo->handle_opt != NULL; arginfo++) {
        if (arginfo->has_arg) {
            printf("-%s %-*s %-*s %s\n", arginfo->argv,
                   (int)(maxarglen - strlen(arginfo->argv) - 1),
                   arginfo->example, maxenvlen, arginfo->env, arginfo->help);
        } else {
            printf("-%-*s %-*s %s\n", maxarglen, arginfo->argv,
                    maxenvlen, arginfo->env,
                    arginfo->help);
        }
    }

    printf("\n"
           "Defaults:\n"
           "LAT_LD_PREFIX  = %s\n"
           "LAT_STACK_SIZE = %ld byte\n",
           interp_prefix,
           vir_guest_stack_size);

    printf("\n"
           "You can use -E and -U options or the LAT_SET_ENV and\n"
           "LAT_UNSET_ENV environment variables to set and unset\n"
           "environment variables for the target process.\n"
           "It is possible to provide several variables by separating them\n"
           "by commas in getsubopt(3) style. Additionally it is possible to\n"
           "provide the -E and -U options multiple times.\n"
           "The following lines are equivalent:\n"
           "    -E var1=val2 -E var2=val2 -U LD_PRELOAD -U LD_DEBUG\n"
           "    -E var1=val2,var2=val2 -U LD_PRELOAD,LD_DEBUG\n"
           "    LAT_SET_ENV=var1=val2,var2=val2 LAT_UNSET_ENV=LD_PRELOAD,LD_DEBUG\n"
           "Note that if you provide several changes to a single variable\n"
           "the last change will stay in effect.\n"
           "\n");

    exit(exitcode);
}
#endif

static int parse_args(int argc, char **argv)
{
    const char *r;
    int optind;
    const struct qemu_argument *arginfo;

    for (arginfo = arg_table; arginfo->handle_opt != NULL; arginfo++) {
        if (arginfo->env == NULL) {
            continue;
        }

        r = getenv(arginfo->env);
        if (r != NULL) {
            arginfo->handle_opt(r);
        }
    }

    optind = 1;
    for (;;) {
        if (optind >= argc) {
            break;
        }
        r = argv[optind];
        if (r[0] != '-') {
            break;
        }
        optind++;
        r++;
        if (!strcmp(r, "-")) {
            break;
        }
        /* Treat --foo the same as -foo.  */
        if (r[0] == '-') {
            r++;
        }

        for (arginfo = arg_table; arginfo->handle_opt != NULL; arginfo++) {
            if (!strcmp(r, arginfo->argv)) {
                if (arginfo->has_arg) {
                    if (optind >= argc) {
                        (void) fprintf(stderr,
                            "missing argument for option '%s'\n", r);
                        exit(EXIT_FAILURE);
                    }
                    arginfo->handle_opt(argv[optind]);
                    optind++;
                } else {
                    arginfo->handle_opt(NULL);
                }
                break;
            }
        }

        /* no option matched the current argv */
        if (arginfo->handle_opt == NULL) {
            (void) fprintf(stderr, "unknown option '%s'\n", r);
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        (void) fprintf(stderr, "no user program specified\n");
        exit(EXIT_FAILURE);
    }

    exec_path = argv[optind];
    real_path = realpath(exec_path, (char *)malloc(MAX_PATH));

    return optind;
}

int main(int argc, char **argv, char **envp)
{
    struct target_pt_regs regs1, *regs = &regs1;
    struct linux_binprm bprm;
    TaskState *ts;
    CPUArchState *env;
    CPUState *cpu;
    int optind;
    char **target_environ, **wrk;
    char **target_argv;
    int target_argc;
    int i;
    int ret;
    int execfd;
    int log_mask;
    unsigned long max_reserved_va;
    bool preserve_argv0;

#if defined(CONFIG_LATX) && defined(__loongarch__)
    /* Lets check hwcap */
    #include <asm/hwcap.h>
    int need_cap, hwcap;
    need_cap = HWCAP_LOONGARCH_LSX | HWCAP_LOONGARCH_LBT_X86;
    hwcap = qemu_getauxval(AT_HWCAP);
    if (need_cap != (hwcap & need_cap)) {
        fprintf(stderr, "LAT needs LSX/LASX and LBT extension support.\n");
        fprintf(stderr, "Extension not found:");
        if (!(hwcap & HWCAP_LOONGARCH_LSX))
            fprintf(stderr, " LSX");
        if (!(hwcap & HWCAP_LOONGARCH_LBT_X86))
            fprintf(stderr, " LBT_X86");
        fprintf(stderr, ". Please check KERNEL and HARDWARE.\n");
    }
    if (!(hwcap & HWCAP_LOONGARCH_LASX)) {
            fprintf(stderr, "not found LASX, use 128-bit vectors\n");
            option_enable_lasx = 0;
    }
#endif

    optind = parse_args(argc, argv);

    error_init(argv[0]);
    module_call_init(MODULE_INIT_TRACE);
    qemu_init_cpu_list();
    module_call_init(MODULE_INIT_QOM);

    envlist = envlist_create();

    /* add current environment into the list */
    for (wrk = environ; *wrk != NULL; wrk++) {
        (void) envlist_setenv(envlist, *wrk);
    }

    /* Read the stack limit from the kernel.  If it's "unlimited",
       then we can do little else besides use the default.  */
    {
        struct rlimit lim;
        if (getrlimit(RLIMIT_STACK, &lim) == 0
            && lim.rlim_cur != RLIM_INFINITY
            && lim.rlim_cur == (target_long)lim.rlim_cur) {
            vir_guest_stack_size = lim.rlim_cur;
        }
    }

    cpu_model = NULL;

    qemu_add_opts(&qemu_trace_opts);
    qemu_plugin_add_opts();

    if (argc >= 5 && !strcmp(argv[1], argv[2])) {
        long long hash = 0;
        for (int i = 4; i < argc; ++i) {
            for (int j = 0; argv[i][j] != '\0'; ++j) {
                hash += argv[i][j];
            }
        }
        if (hash == atoll(argv[3])) {
            for (int i = 4; i < argc; ++i) {
                argv[i - 3] = argv[i];
            }
            argv[argc - 1] = argv[argc - 2] = argv[argc - 3] = NULL;
            argc = argc - 3;
        }
    }
    log_mask = last_log_mask | (enable_strace ? LOG_STRACE : 0)
                             | (enable_strace_error ? LOG_STRACE_ERROR : 0);
    if (log_mask) {
        qemu_log_needs_buffers();
        qemu_set_log(log_mask);
    }

    if (!trace_init_backends()) {
        exit(1);
    }
    trace_init_file();
#ifdef CONFIG_LATX_DEBUG
    qemu_plugin_load_list(&plugins, &error_fatal);
#endif

    /* Zero out regs */
    memset(regs, 0, sizeof(struct target_pt_regs));

    /* Zero out image_info */
    memset(info, 0, sizeof(struct image_info));

    memset(&bprm, 0, sizeof (bprm));

    /* Scan interp_prefix dir for replacement files. */
    init_paths(interp_prefix);

    init_qemu_uname_release();

    /*
     * Manage binfmt-misc open-binary flag
     */
    execfd = qemu_getauxval(AT_EXECFD);
    if (execfd == 0) {
        execfd = open(exec_path, O_RDONLY);
        if (execfd < 0) {
            printf("Error while loading %s: %s\n", exec_path, strerror(errno));
            _exit(EXIT_FAILURE);
        }
    }

    /*
     * get binfmt_misc flags
     */
    preserve_argv0 = !!(qemu_getauxval(AT_FLAGS) & AT_FLAGS_PRESERVE_ARGV0);

    /*
     * Manage binfmt-misc preserve-arg[0] flag
     *    argv[optind]     full path to the binary
     *    argv[optind + 1] original argv[0]
     */
    if (optind + 1 < argc && preserve_argv0) {
        optind++;
    }

    if (cpu_model == NULL) {
        cpu_model = cpu_get_model(get_elf_eflags(execfd));
    }
    cpu_type = parse_cpu_option(cpu_model);

    /* init tcg before creating CPUs and to get qemu_host_page_size */
    {
        AccelClass *ac = ACCEL_GET_CLASS(current_accel());

        ac->init_machine(NULL);
        accel_init_interfaces(ac);
    }
    cpu = cpu_create(cpu_type);
    env = cpu->env_ptr;
    cpu_reset(cpu);

#ifdef CONFIG_LATX
    latx_init_fpu_regs(env);
    latx_lsenv_init(env);
    tcg_prologue_init(tcg_ctx);
    tcg_region_init();
    latx_dt_init();
    latx_handle_args(exec_path);
#endif
    thread_cpu = cpu;

    /*
     * Reserving too much vm space via mmap can run into problems
     * with rlimits, oom due to page table creation, etc.  We will
     * still try it, if directed by the command-line option, but
     * not by default.
     */
    max_reserved_va = MAX_RESERVED_VA(cpu);
    if (reserved_va != 0) {
        if (max_reserved_va && reserved_va > max_reserved_va) {
            fprintf(stderr, "Reserved virtual address too big\n");
            exit(EXIT_FAILURE);
        }
    } else if (HOST_LONG_BITS == 64 && TARGET_VIRT_ADDR_SPACE_BITS <= 32) {
        /*
         * reserved_va must be aligned with the host page size
         * as it is used with mmap()
         */
        reserved_va = max_reserved_va & qemu_host_page_mask;
    } else if (HOST_LONG_BITS == 64 && TARGET_VIRT_ADDR_SPACE_BITS == 47) {
        /*
         * For now, the maximum address space bit we support is 39, so set the
         * max_reserved_va to 512G - 64G in x86-64 mode.
         */
        struct rlimit limit;
        getrlimit(RLIMIT_STACK, &limit);
        if((unsigned long)limit.rlim_cur==RLIM_INFINITY) {
            /* rlimit stack to inf cause annoymous mmap to TASK_SIZE/3,
             * so we shrink reserved_va to avoid conflict.
             */
            reserved_va = 0x4000000000;
        } else {
            reserved_va = ((max_reserved_va >> 8) & qemu_host_page_mask) - ((max_reserved_va >> 11) & qemu_host_page_mask);
        }
    }

    {
        Error *err = NULL;
        if (seed_optarg != NULL) {
            qemu_guest_random_seed_main(seed_optarg, &err);
        } else {
            qcrypto_init(&err);
        }
        if (err) {
            error_reportf_err(err, "cannot initialize crypto: ");
            exit(1);
        }
    }

    target_environ = envlist_to_environ(envlist, NULL);
    envlist_free(envlist);

#define KERNEL_CONFIG_LSM_MMAP_MIN_ADDR 65536
    /*
     * Read in mmap_min_addr kernel parameter.  This value is used
     * When loading the ELF image to determine whether guest_base
     * is needed.  It is also used in mmap_find_vma.
     */
    {
        FILE *fp;

        if ((fp = fopen("/proc/sys/vm/mmap_min_addr", "r")) != NULL) {
            unsigned long tmp;
            if (fscanf(fp, "%lu", &tmp) == 1) {
                mmap_min_addr = tmp;
                /*
                 * We prefer to not make NULL pointers accessible to QEMU.
                 * If we're in a chroot with no /proc, fall back to 1 page.
                 */
                if (mmap_min_addr < qemu_host_page_size) {
                    mmap_min_addr = qemu_host_page_size;
                }

                /* get the mmap_min_addr from kernel */
                void *addr = mmap(
                    (void *)mmap_min_addr, qemu_host_page_size,
                    PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if (addr != MAP_FAILED) {
                    munmap(addr, qemu_host_page_size);
                    assert(!((unsigned long)addr > KERNEL_CONFIG_LSM_MMAP_MIN_ADDR &&
                            (unsigned long)addr > mmap_min_addr));
                    mmap_min_addr = (unsigned long)addr;
                    qemu_log_mask(CPU_LOG_PAGE, "host mmap_min_addr=0x%lx\n",
                                  mmap_min_addr);
                } else {
                    /* error */
                    qemu_log_mask(CPU_LOG_PAGE, "get mmap_min_addr error\n");
                }
            }
            fclose(fp);
        }
    }

    /*
     * Prepare copy of argv vector for target.
     */
    target_argc = argc - optind;
    target_argv = calloc(target_argc + 1, sizeof (char *));
    if (target_argv == NULL) {
        (void) fprintf(stderr, "Unable to allocate memory for target_argv\n");
        exit(EXIT_FAILURE);
    }

    /*
     * If argv0 is specified (using '-0' switch) we replace
     * argv[0] pointer with the given one.
     */
    i = 0;
    if (argv0 != NULL) {
        target_argv[i++] = strdup(argv0);
    }

#ifdef CONFIG_LATX_AOT
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    char *buf;
    char real[PATH_MAX], *temp;
    char *aot_dir;

    /* mkdir ~/.cache/latx/ */
    aot_dir = malloc(PATH_MAX * sizeof(char));
    char *home = getenv("HOME");
    if (likely(home)) {
        snprintf(aot_dir, PATH_MAX, "%s%s", home, "/.cache/latx/");
    } else {
        snprintf(aot_dir, PATH_MAX, "%s", "/.cache/latx/");
    }
    aot_dir[PATH_MAX - 1] = 0;
    mk_aot_dir(aot_dir);

    /* sha1 and fill aot_file_path */
    aot_file_path = calloc(PATH_MAX * sizeof(char), 1);
    buf = aot_file_path;
    strcat(buf, aot_dir);
    buf += strlen(aot_dir);
    free(aot_dir);
    temp = realpath(exec_path, real);
    if (temp == NULL) {
        lsassertm(0, "%s error!", __func__);
    }
    EVP_DigestInit_ex(ctx, EVP_sha1(), NULL);
    EVP_DigestUpdate(ctx, real, strlen(real));
    int aotac = 4;
    for (; i < target_argc; i++) {
        target_argv[i] = strdup(argv[optind + i]);
        if (option_aot_wine == 0 && strstr(target_argv[i], "wine")) {
            option_aot_wine = 1;
        }
        int arglen = strlen(target_argv[i]);
        if (i < aotac) {
            if ((i == 1) && (arglen > strlen("/bin/wine")) &&
                (!strstr(target_argv[i] +
                    (arglen - strlen("/bin/wine")),
                    "/bin/wine"))) {
                aotac = 2;
                continue;
            }
            EVP_DigestUpdate(ctx, target_argv[i], arglen);
        }
    }
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);
    for (i = 0; i < hash_len; i++) {
        sprintf(buf, "%02x", hash[i] & 0xff);
        buf += 2;
    }
    aot_file_lock = malloc(PATH_MAX);
    strcpy(aot_file_lock, aot_file_path);
    strcat(aot_file_lock, ".lock");
    strcat(aot_file_path, ".aot");
#else
    for (; i < target_argc; i++) {
        target_argv[i] = strdup(argv[optind + i]);
    }
#endif
    target_argv[target_argc] = NULL;

#ifdef TARGET_X86_64
    {
        const char* prog = target_argv[0];
        if(strstr(prog, "wine-preloader")==(prog+strlen(prog)-strlen("wine-preloader"))
         || strstr(prog, "wine64-preloader")==(prog+strlen(prog)-strlen("wine64-preloader"))) {
            prog = target_argv[1];
        }
        //printf("prog %s\n", prog);
        if(strstr(prog, "wine")) {
            latx_wine = 1;
        }
    }
#endif

    ts = g_new0(TaskState, 1);
    init_task_state(ts);
    /* build Task State */
    ts->info = info;
    ts->bprm = &bprm;
    cpu->opaque = ts;
    task_settid(ts);
#ifdef CONFIG_LATX_AOT
    aot_init();
#endif
    ret = loader_exec(execfd, exec_path, target_argv, target_environ, regs,
        info, &bprm);
    if (ret != 0) {
        printf("Error while loading %s: %s\n", exec_path, strerror(-ret));
        _exit(EXIT_FAILURE);
    }
#ifdef CONFIG_LATX
    if (!close_latx_parallel) {
        latx_fast_jmp_cache_init(env);
    }
#endif
#if defined(CONFIG_LATX_KZT) && defined(TARGET_X86_64)
    kzt_init(argv, argc, target_argv, target_argc, &bprm);
#endif
    for (wrk = target_environ; *wrk; wrk++) {
        g_free(*wrk);
    }

    g_free(target_environ);

    if (qemu_loglevel_mask(CPU_LOG_PAGE)) {
        qemu_log("guest_base  %p\n", (void *)guest_base);
        log_page_dump("binary load");

        qemu_log("start_brk   0x" TARGET_ABI_FMT_lx "\n", info->start_brk);
        qemu_log("end_code    0x" TARGET_ABI_FMT_lx "\n", info->end_code);
        qemu_log("start_code  0x" TARGET_ABI_FMT_lx "\n", info->start_code);
        qemu_log("start_data  0x" TARGET_ABI_FMT_lx "\n", info->start_data);
        qemu_log("end_data    0x" TARGET_ABI_FMT_lx "\n", info->end_data);
        qemu_log("start_stack 0x" TARGET_ABI_FMT_lx "\n", info->start_stack);
        qemu_log("brk         0x" TARGET_ABI_FMT_lx "\n", info->brk);
        qemu_log("entry       0x" TARGET_ABI_FMT_lx "\n", info->entry);
        qemu_log("argv_start  0x" TARGET_ABI_FMT_lx "\n", info->arg_start);
        qemu_log("env_start   0x" TARGET_ABI_FMT_lx "\n",
                 info->arg_end + (abi_ulong)sizeof(abi_ulong));
        qemu_log("auxv_start  0x" TARGET_ABI_FMT_lx "\n", info->saved_auxv);
    }

    target_set_brk(info->brk);
    syscall_init();
    signal_init();

#ifndef CONFIG_LATX
    /* Now that we've loaded the binary, GUEST_BASE is fixed.  Delay
       generating the prologue until now so that the prologue can take
       the real value of GUEST_BASE into account.  */
    tcg_prologue_init(tcg_ctx);
    tcg_region_init();
#endif

    target_cpu_copy_regs(env, regs);

    if (gdbstub) {
        if (gdbserver_start(gdbstub) < 0) {
            fprintf(stderr, "could not open gdbserver on %s\n",
                    gdbstub);
            exit(EXIT_FAILURE);
        }
        gdb_handlesig(cpu, 0);
    }

#if defined(CONFIG_LATX_KZT) && defined(TARGET_X86_64)
    kzt_bridge_init();
    is_user_map = 1;
#endif

#ifdef CONFIG_LATX_PERF
    latx_timer_start(TIMER_PROCESS);
#endif
    cpu_loop(env);
    /* never exits */
    return 0;
}
