/*
 *  User emulator execution
 *
 *  Copyright (c) 2003-2005 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include "qemu/osdep.h"
#include "hw/core/tcg-cpu-ops.h"
#include "disas/disas.h"
#include "exec/exec-all.h"
#include "tcg/tcg.h"
#include "qemu/bitops.h"
#include "exec/cpu_ldst.h"
#include "exec/translate-all.h"
#include "exec/helper-proto.h"
#include "qemu/atomic128.h"
#include "trace/trace-root.h"
#include "trace/mem.h"
#include <dlfcn.h>
#include "loongarch-extcontext.h"

#ifdef CONFIG_LATX
#include "qemu.h"
#include "latx-options.h"
#include "latx-backtrace.h"
#include "latx-smc.h"
#endif
#ifdef CONFIG_LATX_DEBUG
#include "latx-debug.h"
#if defined(CONFIG_LATX_KZT)
#include "debug.h"
#endif
#endif

#undef EAX
#undef ECX
#undef EDX
#undef EBX
#undef ESP
#undef EBP
#undef ESI
#undef EDI
#undef EIP
#ifdef __linux__
#include <sys/ucontext.h>
#endif

__thread uintptr_t helper_retaddr;

//#define DEBUG_SIGNAL

/* exit the current TB from a signal handler. The host registers are
   restored in a state compatible with the CPU emulator
 */
static void QEMU_NORETURN cpu_exit_tb_from_sighandler(CPUState *cpu,
                                                      sigset_t *old_set)
{
    /* XXX: use siglongjmp ? */
    sigprocmask(SIG_SETMASK, old_set, NULL);
    cpu_loop_exit_noexc(cpu);
}

#if defined(CONFIG_LATX_DEBUG)
const char *latx_demangling(char *symbol);
static void latx_guest_backtrace(CPUArchState *env)
{
    int index = env->func_index;
    printf("\n============ LATX Guest Backtrace ============\n");
    for (int i = 0; i < FUNC_DEPTH; i++) {
        fprintf(stderr, "#%2d  %s\n", i + 1,
            latx_demangling(env->call_func[index % FUNC_DEPTH]));
        index++;
    }
    latx_guest_stack_init(env);
}

static void show_sig_host_regs(ucontext_t *uc)
{
    fprintf(stderr, "================= GPR INFO ===================\n");
    fprintf(stderr, "r0   0x%016llx  r1   0x%016llx  r2   0x%016llx  r3   0x%016llx\n",
            UC_GR(uc)[0], UC_GR(uc)[1],
            UC_GR(uc)[2], UC_GR(uc)[3]);
    fprintf(stderr, "r4   0x%016llx  r5   0x%016llx  r6   0x%016llx  r7   0x%016llx\n",
            UC_GR(uc)[4], UC_GR(uc)[5],
            UC_GR(uc)[6], UC_GR(uc)[7]);
    fprintf(stderr, "r8   0x%016llx  r9   0x%016llx  r10  0x%016llx  r11  0x%016llx\n",
            UC_GR(uc)[8], UC_GR(uc)[9],
            UC_GR(uc)[10], UC_GR(uc)[11]);
    fprintf(stderr, "r12  0x%016llx  r13  0x%016llx  r14  0x%016llx  r15  0x%016llx\n",
            UC_GR(uc)[12], UC_GR(uc)[13],
            UC_GR(uc)[14], UC_GR(uc)[15]);
    fprintf(stderr, "r16  0x%016llx  r17  0x%016llx  r18  0x%016llx  r19  0x%016llx\n",
            UC_GR(uc)[16], UC_GR(uc)[17],
            UC_GR(uc)[18], UC_GR(uc)[19]);
    fprintf(stderr, "r20  0x%016llx  r21  0x%016llx  r22  0x%016llx  r23  0x%016llx\n",
            UC_GR(uc)[20], UC_GR(uc)[21],
            UC_GR(uc)[22], UC_GR(uc)[23]);
    fprintf(stderr, "r24  0x%016llx  r25  0x%016llx  r26  0x%016llx  r27  0x%016llx\n",
            UC_GR(uc)[24], UC_GR(uc)[25],
            UC_GR(uc)[26], UC_GR(uc)[27]);
    fprintf(stderr, "r28  0x%016llx  r29  0x%016llx  r30  0x%016llx  r31  0x%016llx\n",
            UC_GR(uc)[28], UC_GR(uc)[29],
            UC_GR(uc)[30], UC_GR(uc)[31]);
}

static void show_sig_x86_eflags(ucontext_t *uc)
{
    fprintf(stderr, "=================== EFLAGS ===================\n");
    unsigned int eflags;
#if defined(__loongarch__)
    __asm__ __volatile__ (
            "    .word 0x17 << 18 | 0x3f << 10 | 0 << 5 | 0xc\n"
            "    or %[eflags], $zero, $t0\n"
            : [eflags] "=&r" (eflags)
            : );
#elif defined(__mips__)
    __asm__ __volatile__ (
            "    .word ((0x70004034) | (0xc) << 16) | (0x3f) << 6\n"
            "    or %[eflags], $zero, $t0\n"
            : [eflags] "=&r" (eflags)
            : );
#endif
    fprintf(stderr, "[ ");
    if (eflags & 0x1) {
        fprintf(stderr, "CF ");
    }
    if (eflags & 0x4) {
        fprintf(stderr, "PF ");
    }
    if (eflags & 0x10) {
        fprintf(stderr, "AF ");
    }
    if (eflags & 0x40) {
        fprintf(stderr, "ZF ");
    }
    if (eflags & 0x80) {
        fprintf(stderr, "SF ");
    }
    if (eflags & 0x800) {
        fprintf(stderr, "OF ");
    }
    fprintf(stderr, "]\n");
}

static void show_latx_signal_debuginfo(siginfo_t *info, uintptr_t pc,
                                       ucontext_t *uc)
{
    X86CPU *cpu = X86_CPU(current_cpu);
    CPUX86State *env = &cpu->env;

    latx_guest_backtrace(env);
    fprintf(stderr, "\n================= SIG INFO ===================\n");
    fprintf(stderr, "pid%d cpu%d signo %d si_code %d si_errno %d si_addr %p epc 0x%lx\n",
            getpid(), current_cpu->cpu_index, info->si_signo, info->si_code,
            info->si_errno, info->si_addr, pc);
    TranslationBlock *current_tb = tcg_tb_lookup(pc);
    if ((current_tb != NULL) && (info->si_addr != (void *)0xde)) {
        int count =  current_tb->tc.size / 4;
        unsigned int *ins_p = (unsigned int *)current_tb->tc.ptr;
        fprintf(stderr, "================ CURRENT TB ==================\n");
        fprintf(stderr, "This cpu%d tb_exec_count = %ld\n",
                current_cpu->cpu_index, env->tb_exec_count);
        fprintf(stderr, "current_tb start pc = 0x" TARGET_FMT_lx "\n", current_tb->pc);
        for (int i = 0; i < count; i++) {
            if ((unsigned long)(ins_p + i) == pc) {
                fprintf(stderr, " epc => ");
            } else {
                fprintf(stderr, "\t");
            }
            fprintf(stderr, "%p:\t0x%x\n", ins_p + i, *(ins_p + i));
        }
    }
    show_sig_host_regs(uc);
    show_sig_x86_eflags(uc);
    if (info->si_addr == (void *)0x1) {
        abort();
    }
    fprintf(stderr, "============== LATX TRACE MEM ================\n");
    fprintf(stderr, "Last store latx_trace_mem insn 0x%016lx\n", env->last_store_insn);
    fprintf(stderr, "============ LATX HOST BACKTRACE =============\n");
    print_stack_trace();
#if defined(CONFIG_LATX_KZT) && defined(CONFIG_LATX_DEBUG)
    latx_kzt_debuginfo_check();
#endif
}
#endif

#ifdef CONFIG_LATX_SMC_OPT

static uint64_t parse_guest_store(siginfo_t *info, ucontext_t *uc, int *size)
{
    uint32_t inst = *(uint32_t *)UC_PC(uc);
    int rj = (inst >> 5) & 0x1f;
    int64_t imm, real_guest_addr;
    static uint64_t unsup = 0, unsup_sc = 0;
    int s = -1; /* unsupport inst */
    int check_cross_host = 0;

    /* sc.w/d/q can not be interpreted here in a sample way.
     * Treat as unsupport inst and profile this case
     * to indicate futher optimization. */
    switch (inst >> 15) {
    case 0x70ae: /* SC.Q */ s = -2; break;
    default: break;
    }
    switch (inst >> 24) {
    case 0x21: /* SC.W */ s = -2; break;
    case 0x23: /* SC.D */ s = -2; break;
    case 0x25: /* STPTR.W */ s = 4; break;
    case 0x27: /* STPTR.D */ s = 8; break;
    default: break;
    }
    if (s > 0) goto do_rj_imm14;

    switch (inst >> 22) {
    case 0xa4: /* ST.B */ s = 1; break;
    case 0xa5: /* ST.H */ s = 2; break;
    case 0xa6: /* ST.W */ s = 4; break;
    case 0xa7: /* ST.D */ s = 8; break;
    case 0xb1: /* VST */ s = 16; break;
    case 0xb3: /* XVST */ s = 32; break;
    case 0xad: /* FST.S */ s = 4; check_cross_host = 1; break;
    case 0xaf: /* FST.D */ s = 8; check_cross_host = 1; break;
    default: break;
    }
    if (s > 0) goto do_rj_imm12;

    /* handle unsupport inst here */
    unsup += 1;
    if (s == -2) unsup_sc += 1;

    if (unsup < 1024) {
        fprintf(stderr, "%s:%d SMC unsupport inst %08x count %lu %lu\n",
                __func__, __LINE__, inst, unsup, unsup_sc);
    } else if (!(unsup & 0x3ff)) {
        fprintf(stderr, "%s:%d SMC unsupport inst %08x count %lu %lu\n",
                __func__, __LINE__, inst, unsup, unsup_sc);
        fprintf(stderr, "%s:%d SMC unsupport too much !!! "
                "Try export LAT_SMC = 0 and report this problem please.\n",
                __func__, __LINE__);
    }
    fflush(stderr);
    goto do_return;

do_rj_imm14:
    imm = (inst >> 10) & 0x3fff;
    imm = (int16_t)(imm << 2);
    real_guest_addr = UC_GR(uc)[rj] + imm;
    goto do_return;

do_rj_imm12:
    imm = (inst >> 10) & 0xfff;
    imm = (int16_t)(imm << 4) >> 4;
    real_guest_addr = UC_GR(uc)[rj] + imm;

do_return:
    *size = s;
    if (check_cross_host) {
        if (((real_guest_addr)     & qemu_host_page_mask) !=
            ((real_guest_addr + s) & qemu_host_page_mask))
        {
            /* In some cases, the store inst can not be interpreted
             * if the memory address cross the host page boundary. */
            *size = -3;
        }
    }
    return real_guest_addr;
}

static void write_by_byte(int64_t addr, int64_t value, int byte_count)
{
    for (int i = 0; i < byte_count; i++) {
        *(char *)addr = (char)(value >> (i * 8) & 0xff);
        addr += 1;
    }
}

static void smc_store_shadow_page(int64_t mem_addr, int64_t value, int deal_byte_count)
{
    int64_t page2_addr = (mem_addr & TARGET_PAGE_MASK) + TARGET_PAGE_SIZE;
    int page1_byte_count = page2_addr - mem_addr;
    if (deal_byte_count < page1_byte_count) {
        page1_byte_count = deal_byte_count;
    }
    ShadowPageDesc *spd = page_get_target_data(mem_addr);
    if (spd) {
        mem_addr += spd->access_off;
    }
    /* Write page1. */
    write_by_byte(mem_addr, value, page1_byte_count);
    /* Write page2. */
    if (page1_byte_count < deal_byte_count) {
        deal_byte_count -= page1_byte_count;
        value >>= (page1_byte_count << 3);
        spd = page_get_target_data(page2_addr);
        if (spd) {
            page2_addr += spd->access_off;
        }
        write_by_byte(page2_addr, value, deal_byte_count);
    }
}

static int smc_store_interpret(siginfo_t *info, ucontext_t *uc,
        uint64_t real_guest_addr)
{
    uint32_t inst, rd, fd;
    int64_t value, mem_addr;
#ifndef CONFIG_LOONGARCH_NEW_WORLD
    uint32_t rj;
    ShadowPageDesc *spd;
#endif
    /* UC_PC(uc) += size after interpret */
    int size = 0x4;

#ifdef CONFIG_LOONGARCH_NEW_WORLD
    struct extctx_layout extctx;
    memset(&extctx, 0, sizeof(extctx));
    /* we need to parse the extcontext data */
    parse_extcontext(uc, &extctx);
#endif

    inst = *(uint32_t *)UC_PC(uc);
    rd = inst & 0x1f;
    mem_addr = real_guest_addr;

    switch (inst >> 15) {
    case 0x70ae: /* SC.Q */
        g_assert_not_reached();
    default:
        break;
    }
    switch (inst >> 24) {
    case 0x21: /* SC.W */
        g_assert_not_reached();
    case 0x23: /* SC.D */
        g_assert_not_reached();
    case 0x25: /* STPTR.W */
        value = UC_GR(uc)[rd];
        smc_store_shadow_page(mem_addr, value, 4);
        goto do_return;
    case 0x27: /* STPTR.D */
        value = UC_GR(uc)[rd];
        smc_store_shadow_page(mem_addr, value, 8);
        goto do_return;
    default:
        break;
    }

    switch (inst >> 22) {
    case 0xa4: /* ST.B */
        value = UC_GR(uc)[rd];
        smc_store_shadow_page(mem_addr, value, 1);
        break;
    case 0xa5: /* ST.H */
        value = UC_GR(uc)[rd];
        smc_store_shadow_page(mem_addr, value, 2);
        break;
    case 0xa6: /* ST.W */
        value = UC_GR(uc)[rd];
        smc_store_shadow_page(mem_addr, value, 4);
        break;
    case 0xa7: /* ST.D */
        value = UC_GR(uc)[rd];
        smc_store_shadow_page(mem_addr, value, 8);
        break;
#ifndef CONFIG_LOONGARCH_NEW_WORLD
    case 0xad: /* FST.S */
        rj = (inst >> 5) & 0x1f;
        if (((mem_addr)     & qemu_host_page_mask) ==
            ((mem_addr + 4) & qemu_host_page_mask))
        {
            smc_set_interpret_glue_code(uc, inst, rj);
            spd = page_get_target_data(mem_addr);
            UC_GR(uc)[rj] += spd ->access_off;
            size = 0; /* no change UC_PC(uc) again */
        } else {
            g_assert_not_reached();
        }
        break;
    case 0xaf: /* FST.D */
        rj = (inst >> 5) & 0x1f;
        if (((mem_addr)     & qemu_host_page_mask) ==
            ((mem_addr + 8) & qemu_host_page_mask))
        {
            smc_set_interpret_glue_code(uc, inst, rj);
            spd = page_get_target_data(mem_addr);
            UC_GR(uc)[rj] += spd ->access_off;
            size = 0; /* no change UC_PC(uc) again */
        } else {
            g_assert_not_reached();
        }
        break;
    case 0xb1: /* VST */
        fd = rd + 1;
        value = UC_FREG(uc)[fd].__val64[0];
        smc_store_shadow_page(mem_addr, value, 8);
        value = UC_FREG(uc)[fd].__val64[1];
        smc_store_shadow_page(mem_addr + 8, value, 8);
        break;
    case 0xb3: /* XVST */
        fd = rd + 1;
        value = UC_FREG(uc)[fd].__val64[0];
        smc_store_shadow_page(mem_addr, value, 8);
        value = UC_FREG(uc)[fd].__val64[1];
        smc_store_shadow_page(mem_addr + 8, value, 8);
        value = UC_FREG(uc)[fd].__val64[2];
        smc_store_shadow_page(mem_addr + 16, value, 8);
        value = UC_FREG(uc)[fd].__val64[3];
        smc_store_shadow_page(mem_addr + 24, value, 8);
        break;
#else
    case 0xad: /* FST.S */
        fd = rd;
        fd = fd < 8 ? (fd + UC_GET_FTOP(&extctx, uint32_t)) & 7 : fd;
        value = UC_GET_FPR(&extctx, fd, uint32_t);
        smc_store_shadow_page(mem_addr, value, 4);
        break;
    case 0xaf: /* FST.D */
        fd = rd;
        fd = fd < 8 ? (fd + UC_GET_FTOP(&extctx, uint32_t)) & 7 : fd;
        value = UC_GET_FPR(&extctx, fd, uint64_t);
        smc_store_shadow_page(mem_addr, value, 8);
        break;
    case 0xb1: /* VST */
        fd = rd;
        value = UC_GET_LSX(&extctx, fd, 0, uint64_t);
        smc_store_shadow_page(mem_addr, value, 8);
        value = UC_GET_LSX(&extctx, fd, 1, uint64_t);
        smc_store_shadow_page(mem_addr + 8, value, 8);
        break;
    case 0xb3: /* XVST */
        fd = rd;
        value = UC_GET_LSX(&extctx, fd, 0, uint64_t);
        smc_store_shadow_page(mem_addr, value, 8);
        value = UC_GET_LSX(&extctx, fd, 1, uint64_t);
        smc_store_shadow_page(mem_addr + 8, value, 8);
        value = UC_GET_LSX(&extctx, fd, 2, uint64_t);
        smc_store_shadow_page(mem_addr + 16, value, 8);
        value = UC_GET_LSX(&extctx, fd, 3, uint64_t);
        smc_store_shadow_page(mem_addr + 24, value, 8);
        break;
#endif
    default:
        fprintf(stderr, "%s:%d SMC unsupport inst %08x\n",
                __func__, __LINE__, inst);
        fflush(stderr);
        g_assert_not_reached();
        break;
    }

do_return:
    /* UC_PC(uc) += size after interpret */
    return size;
}

#endif

/* 'pc' is the host PC at which the exception was raised. 'address' is
   the effective address of the memory exception. 'is_write' is 1 if a
   write caused the exception and otherwise 0'. 'old_set' is the
   signal set which should be restored */
static inline int handle_cpu_signal(uintptr_t pc, siginfo_t *info,
                                    int is_write, sigset_t *old_set)
{
    CPUState *cpu = current_cpu;
    CPUClass *cc;
    unsigned long address = (unsigned long)info->si_addr;
    MMUAccessType access_type = is_write ? MMU_DATA_STORE : MMU_DATA_LOAD;

#ifdef CONFIG_LATX_SMC_OPT
    int *emu = NULL;
    int size = 0;
    int emu_store = 0;
    uint64_t guest_store_address;
#endif

#if defined(CONFIG_LATX_DEBUG)
    uintptr_t epc = pc;
#endif
#if defined(CONFIG_LATX_SMC_OPT) || defined(CONFIG_LATX_DEBUG)
#ifndef CONFIG_LOONGARCH_NEW_WORLD
    mcontext_t * uc_mctx = (void *)old_set - 0x1540;
    ucontext_t *uc = container_of(uc_mctx, ucontext_t, uc_mcontext);
#else
    ucontext_t *uc = container_of(old_set, ucontext_t, uc_sigmask);
#endif
#endif
    switch (helper_retaddr) {
    default:
        /*
         * Fault during host memory operation within a helper function.
         * The helper's host return address, saved here, gives us a
         * pointer into the generated code that will unwind to the
         * correct guest pc.
         */
        pc = helper_retaddr;
        break;

    case 0:
        /*
         * Fault during host memory operation within generated code.
         * (Or, a unrelated bug within qemu, but we can't tell from here).
         *
         * We take the host pc from the signal frame.  However, we cannot
         * use that value directly.  Within cpu_restore_state_from_tb, we
         * assume PC comes from GETPC(), as used by the helper functions,
         * so we adjust the address by -GETPC_ADJ to form an address that
         * is within the call insn, so that the address does not accidentally
         * match the beginning of the next guest insn.  However, when the
         * pc comes from the signal frame it points to the actual faulting
         * host memory insn and not the return from a call insn.
         *
         * Therefore, adjust to compensate for what will be done later
         * by cpu_restore_state_from_tb.
         */
        pc += GETPC_ADJ;
        break;

    case 1:
        /*
         * Fault during host read for translation, or loosely, "execution".
         *
         * The guest pc is already pointing to the start of the TB for which
         * code is being generated.  If the guest translator manages the
         * page crossings correctly, this is exactly the correct address
         * (and if the translator doesn't handle page boundaries correctly
         * there's little we can do about that here).  Therefore, do not
         * trigger the unwinder.
         *
         * Like tb_gen_code, release the memory lock before cpu_loop_exit.
         */
        pc = 0;
        access_type = MMU_INST_FETCH;
        mmap_unlock();
        break;
    }

#if defined(DEBUG_SIGNAL)
    printf("qemu: SIGSEGV pc=0x%08lx address=%08lx w=%d oldset=0x%08lx\n",
           pc, address, is_write, *(unsigned long *)old_set);
#endif
    /* XXX: locking issue */
    /* Note that it is important that we don't call page_unprotect() unless
     * this is really a "write to nonwriteable page" fault, because
     * page_unprotect() assumes that if it is called for an access to
     * a page that's writeable this means we had two threads racing and
     * another thread got there first and already made the page writeable;
     * so we will retry the access. If we were to call page_unprotect()
     * for some other kind of fault that should really be passed to the
     * guest, we'd end up in an infinite loop of retrying the faulting
     * access.
     */
    if (is_write && info->si_signo == SIGSEGV && info->si_code == SEGV_ACCERR &&
        h2g_valid(address)) {
#ifdef CONFIG_LATX_SMC_OPT
        if (latx_smc_inv_page()) {
            guest_store_address = h2g(address);
            /* set @emu = NULL to invalidate all TB in this page */
            emu = NULL;
        } else {
            /*
             * pass @emu to page_unprotect() to only invalidate the those TB
             * that is affected by the smc store.
             * The *emu should contain the size of the store inst.
             * when return:
             *     if *emu = 1 : page is not writable, interpret the store
             *     if *emu = 0 : page is writable, ok to return
             *
             * if parse_guest_store return with size < 0, it means the
             * inst is unsupported or sc.w/d/q.
             * To avoid abort, fallback to invalidate all TB on this page
             * to make this page writable and return.
             */
            guest_store_address = parse_guest_store(info, uc, &size);
            if (size < 0) {
                guest_store_address = h2g(address);
                emu = NULL;
            } else {
                emu_store = size;
                emu = &emu_store;
            }
        }
        switch (page_unprotect(guest_store_address, pc, emu)) {
#else
        switch (page_unprotect(h2g(address), pc, NULL)) {
#endif
        case 0:
            /* Fault not caused by a page marked unwritable to protect
             * cached translations, must be the guest binary's problem.
             */
            break;
        case 1:
#ifdef CONFIG_LATX_SMC_OPT
            if (emu_store) {
                /*
                 * The page is not writable because there still are TB in this
                 * page. The original store instruction needs to be interpret
                 * here. The shadow page is used to allow write on this page.
                 * During page_unprotect(), the shadow page will be created for
                 * the page to be writen. So does the 2nd page if the store
                 * cross the guest page boundary.
                 *
                 * The smc_store_interpret return an offset which is used to
                 * update the UC_PC(uc) after signal return. On default the
                 * offset is 0x4 to bypass the original the store inst.
                 * In some cases, the glue code is used to interpret the
                 * store inst. The UC_PC(uc) will point to the glue code.
                 * So the offset will be 0x0.
                 */
                mmap_lock();
                size = smc_store_interpret(info, uc, guest_store_address);
                UC_PC(uc) += size;
                mmap_unlock();
            }
#endif
            /* Fault caused by protection of cached translation; TBs
             * invalidated, so resume execution.  Retain helper_retaddr
             * for a possible second fault.
             */
            return 1;
        case 2:
            /* Fault caused by protection of cached translation, and the
             * currently executing TB was modified and must be exited
             * immediately.  Clear helper_retaddr for next execution.
             */
            clear_helper_retaddr();
            cpu_exit_tb_from_sighandler(cpu, old_set);
            /* NORETURN */

        default:
            g_assert_not_reached();
        }
    }

#if defined(CONFIG_LATX_DEBUG)
    show_latx_signal_debuginfo(info, epc, uc);
#endif

    /* For synchronous signals we expect to be coming from the vCPU
     * thread (so current_cpu should be valid) and either from running
     * code or during translation which can fault as we cross pages.
     *
     * If neither is true then something has gone wrong and we should
     * abort rather than try and restart the vCPU execution.
     */
    if (!cpu || !cpu->running) {
#if defined(CONFIG_LATX_DEBUG)
        printf("qemu:%s received signal outside vCPU context @ pc=0x%"
               PRIxPTR "\n",  __func__, pc);
#endif
        abort();
    }

    /* Convert forcefully to guest address space, invalid addresses
       are still valid segv ones */
    address = h2g_nocheck(address);

    /*
     * There is no way the target can handle this other than raising
     * an exception.  Undo signal and retaddr state prior to longjmp.
     */
    sigprocmask(SIG_SETMASK, old_set, NULL);
    clear_helper_retaddr();

    cc = CPU_GET_CLASS(cpu);
    cc->tcg_ops->tlb_fill(cpu, address, 0, access_type,
                          MMU_USER_IDX, false, pc, info);
    g_assert_not_reached();
}

static int probe_access_internal(CPUArchState *env, target_ulong addr,
                                 int fault_size, MMUAccessType access_type,
                                 bool nonfault, uintptr_t ra)
{
    int flags;

    switch (access_type) {
    case MMU_DATA_STORE:
        flags = PAGE_WRITE;
        break;
    case MMU_DATA_LOAD:
        flags = PAGE_READ;
        break;
    case MMU_INST_FETCH:
        flags = PAGE_EXEC;
        break;
    default:
        g_assert_not_reached();
    }

    if (!guest_addr_valid_untagged(addr) ||
        !page_check_range(addr, 1, flags)) {
        if (nonfault) {
            return TLB_INVALID_MASK;
        } else {
            CPUState *cpu = env_cpu(env);
            CPUClass *cc = CPU_GET_CLASS(cpu);
            cc->tcg_ops->tlb_fill(cpu, addr, fault_size, access_type,
                                  MMU_USER_IDX, false, ra, NULL);
            g_assert_not_reached();
        }
    }
    return 0;
}

int probe_access_flags(CPUArchState *env, target_ulong addr,
                       MMUAccessType access_type, int mmu_idx,
                       bool nonfault, void **phost, uintptr_t ra)
{
    int flags;

    flags = probe_access_internal(env, addr, 0, access_type, nonfault, ra);
    *phost = flags ? NULL : g2h(env_cpu(env), addr);
    return flags;
}

void *probe_access(CPUArchState *env, target_ulong addr, int size,
                   MMUAccessType access_type, int mmu_idx, uintptr_t ra)
{
    int flags;

    g_assert(-(addr | TARGET_PAGE_MASK) >= size);
    flags = probe_access_internal(env, addr, size, access_type, false, ra);
    g_assert(flags == 0);

    return size ? g2h(env_cpu(env), addr) : NULL;
}

tb_page_addr_t get_page_addr_code_hostp(CPUArchState *env, target_ulong addr,
                                        void **hostp)
{
    if (hostp) {
        *hostp = g2h_untagged(addr);
    }
    return addr;
}

#if defined(__i386__)

#if defined(__NetBSD__)
#include <ucontext.h>

#define EIP_sig(context)     ((context)->uc_mcontext.__gregs[_REG_EIP])
#define TRAP_sig(context)    ((context)->uc_mcontext.__gregs[_REG_TRAPNO])
#define ERROR_sig(context)   ((context)->uc_mcontext.__gregs[_REG_ERR])
#define MASK_sig(context)    ((context)->uc_sigmask)
#elif defined(__FreeBSD__) || defined(__DragonFly__)
#include <ucontext.h>

#define EIP_sig(context)  (*((unsigned long *)&(context)->uc_mcontext.mc_eip))
#define TRAP_sig(context)    ((context)->uc_mcontext.mc_trapno)
#define ERROR_sig(context)   ((context)->uc_mcontext.mc_err)
#define MASK_sig(context)    ((context)->uc_sigmask)
#elif defined(__OpenBSD__)
#define EIP_sig(context)     ((context)->sc_eip)
#define TRAP_sig(context)    ((context)->sc_trapno)
#define ERROR_sig(context)   ((context)->sc_err)
#define MASK_sig(context)    ((context)->sc_mask)
#else
#define EIP_sig(context)     ((context)->uc_mcontext.gregs[REG_EIP])
#define TRAP_sig(context)    ((context)->uc_mcontext.gregs[REG_TRAPNO])
#define ERROR_sig(context)   ((context)->uc_mcontext.gregs[REG_ERR])
#define MASK_sig(context)    ((context)->uc_sigmask)
#endif

int cpu_signal_handler(int host_signum, void *pinfo,
                       void *puc)
{
    siginfo_t *info = pinfo;
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__DragonFly__)
    ucontext_t *uc = puc;
#elif defined(__OpenBSD__)
    struct sigcontext *uc = puc;
#else
    ucontext_t *uc = puc;
#endif
    unsigned long pc;
    int trapno;

#ifndef REG_EIP
/* for glibc 2.1 */
#define REG_EIP    EIP
#define REG_ERR    ERR
#define REG_TRAPNO TRAPNO
#endif
    pc = EIP_sig(uc);
    trapno = TRAP_sig(uc);
    return handle_cpu_signal(pc, info,
                             trapno == 0xe ? (ERROR_sig(uc) >> 1) & 1 : 0,
                             &MASK_sig(uc));
}

#elif defined(__x86_64__)

#ifdef __NetBSD__
#define PC_sig(context)       _UC_MACHINE_PC(context)
#define TRAP_sig(context)     ((context)->uc_mcontext.__gregs[_REG_TRAPNO])
#define ERROR_sig(context)    ((context)->uc_mcontext.__gregs[_REG_ERR])
#define MASK_sig(context)     ((context)->uc_sigmask)
#elif defined(__OpenBSD__)
#define PC_sig(context)       ((context)->sc_rip)
#define TRAP_sig(context)     ((context)->sc_trapno)
#define ERROR_sig(context)    ((context)->sc_err)
#define MASK_sig(context)     ((context)->sc_mask)
#elif defined(__FreeBSD__) || defined(__DragonFly__)
#include <ucontext.h>

#define PC_sig(context)  (*((unsigned long *)&(context)->uc_mcontext.mc_rip))
#define TRAP_sig(context)     ((context)->uc_mcontext.mc_trapno)
#define ERROR_sig(context)    ((context)->uc_mcontext.mc_err)
#define MASK_sig(context)     ((context)->uc_sigmask)
#else
#define PC_sig(context)       ((context)->uc_mcontext.gregs[REG_RIP])
#define TRAP_sig(context)     ((context)->uc_mcontext.gregs[REG_TRAPNO])
#define ERROR_sig(context)    ((context)->uc_mcontext.gregs[REG_ERR])
#define MASK_sig(context)     ((context)->uc_sigmask)
#endif

int cpu_signal_handler(int host_signum, void *pinfo,
                       void *puc)
{
    siginfo_t *info = pinfo;
    unsigned long pc;
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__DragonFly__)
    ucontext_t *uc = puc;
#elif defined(__OpenBSD__)
    struct sigcontext *uc = puc;
#else
    ucontext_t *uc = puc;
#endif

    pc = PC_sig(uc);
    return handle_cpu_signal(pc, info,
                             TRAP_sig(uc) == 0xe ? (ERROR_sig(uc) >> 1) & 1 : 0,
                             &MASK_sig(uc));
}

#elif defined(_ARCH_PPC)

/***********************************************************************
 * signal context platform-specific definitions
 * From Wine
 */
#ifdef linux
/* All Registers access - only for local access */
#define REG_sig(reg_name, context)              \
    ((context)->uc_mcontext.regs->reg_name)
/* Gpr Registers access  */
#define GPR_sig(reg_num, context)              REG_sig(gpr[reg_num], context)
/* Program counter */
#define IAR_sig(context)                       REG_sig(nip, context)
/* Machine State Register (Supervisor) */
#define MSR_sig(context)                       REG_sig(msr, context)
/* Count register */
#define CTR_sig(context)                       REG_sig(ctr, context)
/* User's integer exception register */
#define XER_sig(context)                       REG_sig(xer, context)
/* Link register */
#define LR_sig(context)                        REG_sig(link, context)
/* Condition register */
#define CR_sig(context)                        REG_sig(ccr, context)

/* Float Registers access  */
#define FLOAT_sig(reg_num, context)                                     \
    (((double *)((char *)((context)->uc_mcontext.regs + 48 * 4)))[reg_num])
#define FPSCR_sig(context) \
    (*(int *)((char *)((context)->uc_mcontext.regs + (48 + 32 * 2) * 4)))
/* Exception Registers access */
#define DAR_sig(context)                       REG_sig(dar, context)
#define DSISR_sig(context)                     REG_sig(dsisr, context)
#define TRAP_sig(context)                      REG_sig(trap, context)
#endif /* linux */

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <ucontext.h>
#define IAR_sig(context)               ((context)->uc_mcontext.mc_srr0)
#define MSR_sig(context)               ((context)->uc_mcontext.mc_srr1)
#define CTR_sig(context)               ((context)->uc_mcontext.mc_ctr)
#define XER_sig(context)               ((context)->uc_mcontext.mc_xer)
#define LR_sig(context)                ((context)->uc_mcontext.mc_lr)
#define CR_sig(context)                ((context)->uc_mcontext.mc_cr)
/* Exception Registers access */
#define DAR_sig(context)               ((context)->uc_mcontext.mc_dar)
#define DSISR_sig(context)             ((context)->uc_mcontext.mc_dsisr)
#define TRAP_sig(context)              ((context)->uc_mcontext.mc_exc)
#endif /* __FreeBSD__|| __FreeBSD_kernel__ */

int cpu_signal_handler(int host_signum, void *pinfo,
                       void *puc)
{
    siginfo_t *info = pinfo;
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    ucontext_t *uc = puc;
#else
    ucontext_t *uc = puc;
#endif
    unsigned long pc;
    int is_write;

    pc = IAR_sig(uc);
    is_write = 0;
#if 0
    /* ppc 4xx case */
    if (DSISR_sig(uc) & 0x00800000) {
        is_write = 1;
    }
#else
    if (TRAP_sig(uc) != 0x400 && (DSISR_sig(uc) & 0x02000000)) {
        is_write = 1;
    }
#endif
    return handle_cpu_signal(pc, info, is_write, &uc->uc_sigmask);
}

#elif defined(__alpha__)

int cpu_signal_handler(int host_signum, void *pinfo,
                           void *puc)
{
    siginfo_t *info = pinfo;
    ucontext_t *uc = puc;
    uint32_t *pc = uc->uc_mcontext.sc_pc;
    uint32_t insn = *pc;
    int is_write = 0;

    /* XXX: need kernel patch to get write flag faster */
    switch (insn >> 26) {
    case 0x0d: /* stw */
    case 0x0e: /* stb */
    case 0x0f: /* stq_u */
    case 0x24: /* stf */
    case 0x25: /* stg */
    case 0x26: /* sts */
    case 0x27: /* stt */
    case 0x2c: /* stl */
    case 0x2d: /* stq */
    case 0x2e: /* stl_c */
    case 0x2f: /* stq_c */
        is_write = 1;
    }

    return handle_cpu_signal(pc, info, is_write, &uc->uc_sigmask);
}
#elif defined(__sparc__)

int cpu_signal_handler(int host_signum, void *pinfo,
                       void *puc)
{
    siginfo_t *info = pinfo;
    int is_write;
    uint32_t insn;
#if !defined(__arch64__) || defined(CONFIG_SOLARIS)
    uint32_t *regs = (uint32_t *)(info + 1);
    void *sigmask = (regs + 20);
    /* XXX: is there a standard glibc define ? */
    unsigned long pc = regs[1];
#else
#ifdef __linux__
    struct sigcontext *sc = puc;
    unsigned long pc = sc->sigc_regs.tpc;
    void *sigmask = (void *)sc->sigc_mask;
#elif defined(__OpenBSD__)
    struct sigcontext *uc = puc;
    unsigned long pc = uc->sc_pc;
    void *sigmask = (void *)(long)uc->sc_mask;
#elif defined(__NetBSD__)
    ucontext_t *uc = puc;
    unsigned long pc = _UC_MACHINE_PC(uc);
    void *sigmask = (void *)&uc->uc_sigmask;
#endif
#endif

    /* XXX: need kernel patch to get write flag faster */
    is_write = 0;
    insn = *(uint32_t *)pc;
    if ((insn >> 30) == 3) {
        switch ((insn >> 19) & 0x3f) {
        case 0x05: /* stb */
        case 0x15: /* stba */
        case 0x06: /* sth */
        case 0x16: /* stha */
        case 0x04: /* st */
        case 0x14: /* sta */
        case 0x07: /* std */
        case 0x17: /* stda */
        case 0x0e: /* stx */
        case 0x1e: /* stxa */
        case 0x24: /* stf */
        case 0x34: /* stfa */
        case 0x27: /* stdf */
        case 0x37: /* stdfa */
        case 0x26: /* stqf */
        case 0x36: /* stqfa */
        case 0x25: /* stfsr */
        case 0x3c: /* casa */
        case 0x3e: /* casxa */
            is_write = 1;
            break;
        }
    }
    return handle_cpu_signal(pc, info, is_write, sigmask);
}

#elif defined(__arm__)

#if defined(__NetBSD__)
#include <ucontext.h>
#include <sys/siginfo.h>
#endif

int cpu_signal_handler(int host_signum, void *pinfo,
                       void *puc)
{
    siginfo_t *info = pinfo;
#if defined(__NetBSD__)
    ucontext_t *uc = puc;
    siginfo_t *si = pinfo;
#else
    ucontext_t *uc = puc;
#endif
    unsigned long pc;
    uint32_t fsr;
    int is_write;

#if defined(__NetBSD__)
    pc = uc->uc_mcontext.__gregs[_REG_R15];
#elif defined(__GLIBC__) && (__GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ <= 3))
    pc = uc->uc_mcontext.gregs[R15];
#else
    pc = uc->uc_mcontext.arm_pc;
#endif

#ifdef __NetBSD__
    fsr = si->si_trap;
#else
    fsr = uc->uc_mcontext.error_code;
#endif
    /*
     * In the FSR, bit 11 is WnR, assuming a v6 or
     * later processor.  On v5 we will always report
     * this as a read, which will fail later.
     */
    is_write = extract32(fsr, 11, 1);
    return handle_cpu_signal(pc, info, is_write, &uc->uc_sigmask);
}

#elif defined(__aarch64__)

#if defined(__NetBSD__)

#include <ucontext.h>
#include <sys/siginfo.h>

int cpu_signal_handler(int host_signum, void *pinfo, void *puc)
{
    ucontext_t *uc = puc;
    siginfo_t *si = pinfo;
    unsigned long pc;
    int is_write;
    uint32_t esr;

    pc = uc->uc_mcontext.__gregs[_REG_PC];
    esr = si->si_trap;

    /*
     * siginfo_t::si_trap is the ESR value, for data aborts ESR.EC
     * is 0b10010x: then bit 6 is the WnR bit
     */
    is_write = extract32(esr, 27, 5) == 0x12 && extract32(esr, 6, 1) == 1;
    return handle_cpu_signal(pc, si, is_write, &uc->uc_sigmask);
}

#else

#ifndef ESR_MAGIC
/* Pre-3.16 kernel headers don't have these, so provide fallback definitions */
#define ESR_MAGIC 0x45535201
struct esr_context {
    struct _aarch64_ctx head;
    uint64_t esr;
};
#endif

static inline struct _aarch64_ctx *first_ctx(ucontext_t *uc)
{
    return (struct _aarch64_ctx *)&uc->uc_mcontext.__reserved;
}

static inline struct _aarch64_ctx *next_ctx(struct _aarch64_ctx *hdr)
{
    return (struct _aarch64_ctx *)((char *)hdr + hdr->size);
}

int cpu_signal_handler(int host_signum, void *pinfo, void *puc)
{
    siginfo_t *info = pinfo;
    ucontext_t *uc = puc;
    uintptr_t pc = uc->uc_mcontext.pc;
    bool is_write;
    struct _aarch64_ctx *hdr;
    struct esr_context const *esrctx = NULL;

    /* Find the esr_context, which has the WnR bit in it */
    for (hdr = first_ctx(uc); hdr->magic; hdr = next_ctx(hdr)) {
        if (hdr->magic == ESR_MAGIC) {
            esrctx = (struct esr_context const *)hdr;
            break;
        }
    }

    if (esrctx) {
        /* For data aborts ESR.EC is 0b10010x: then bit 6 is the WnR bit */
        uint64_t esr = esrctx->esr;
        is_write = extract32(esr, 27, 5) == 0x12 && extract32(esr, 6, 1) == 1;
    } else {
        /*
         * Fall back to parsing instructions; will only be needed
         * for really ancient (pre-3.16) kernels.
         */
        uint32_t insn = *(uint32_t *)pc;

        is_write = ((insn & 0xbfff0000) == 0x0c000000   /* C3.3.1 */
                    || (insn & 0xbfe00000) == 0x0c800000   /* C3.3.2 */
                    || (insn & 0xbfdf0000) == 0x0d000000   /* C3.3.3 */
                    || (insn & 0xbfc00000) == 0x0d800000   /* C3.3.4 */
                    || (insn & 0x3f400000) == 0x08000000   /* C3.3.6 */
                    || (insn & 0x3bc00000) == 0x39000000   /* C3.3.13 */
                    || (insn & 0x3fc00000) == 0x3d800000   /* ... 128bit */
                    /* Ignore bits 10, 11 & 21, controlling indexing.  */
                    || (insn & 0x3bc00000) == 0x38000000   /* C3.3.8-12 */
                    || (insn & 0x3fe00000) == 0x3c800000   /* ... 128bit */
                    /* Ignore bits 23 & 24, controlling indexing.  */
                    || (insn & 0x3a400000) == 0x28000000); /* C3.3.7,14-16 */
    }
    return handle_cpu_signal(pc, info, is_write, &uc->uc_sigmask);
}
#endif

#elif defined(__s390__)

int cpu_signal_handler(int host_signum, void *pinfo,
                       void *puc)
{
    siginfo_t *info = pinfo;
    ucontext_t *uc = puc;
    unsigned long pc;
    uint16_t *pinsn;
    int is_write = 0;

    pc = uc->uc_mcontext.psw.addr;

    /* ??? On linux, the non-rt signal handler has 4 (!) arguments instead
       of the normal 2 arguments.  The 3rd argument contains the "int_code"
       from the hardware which does in fact contain the is_write value.
       The rt signal handler, as far as I can tell, does not give this value
       at all.  Not that we could get to it from here even if it were.  */
    /* ??? This is not even close to complete, since it ignores all
       of the read-modify-write instructions.  */
    pinsn = (uint16_t *)pc;
    switch (pinsn[0] >> 8) {
    case 0x50: /* ST */
    case 0x42: /* STC */
    case 0x40: /* STH */
        is_write = 1;
        break;
    case 0xc4: /* RIL format insns */
        switch (pinsn[0] & 0xf) {
        case 0xf: /* STRL */
        case 0xb: /* STGRL */
        case 0x7: /* STHRL */
            is_write = 1;
        }
        break;
    case 0xe3: /* RXY format insns */
        switch (pinsn[2] & 0xff) {
        case 0x50: /* STY */
        case 0x24: /* STG */
        case 0x72: /* STCY */
        case 0x70: /* STHY */
        case 0x8e: /* STPQ */
        case 0x3f: /* STRVH */
        case 0x3e: /* STRV */
        case 0x2f: /* STRVG */
            is_write = 1;
        }
        break;
    }
    return handle_cpu_signal(pc, info, is_write, &uc->uc_sigmask);
}

#elif defined(__mips__)

#if defined(__misp16) || defined(__mips_micromips)
#error "Unsupported encoding"
#endif

int cpu_signal_handler(int host_signum, void *pinfo,
                       void *puc)
{
    siginfo_t *info = pinfo;
    ucontext_t *uc = puc;
    uintptr_t pc = uc->uc_mcontext.pc;
    uint32_t insn = *(uint32_t *)pc;
    int is_write = 0;

#ifndef CONFIG_SOFTMMU
    mmap_lock();
    if (page_get_target_data((uint64_t)info->si_addr) &&
        info->si_signo == SIGSEGV) {
        int ret = shared_private_interpret(info, uc);
        if (!ret) {
            mmap_unlock();
            return 1;
        }
    }
    mmap_unlock();
#endif

    /* Detect all store instructions at program counter. */
    switch((insn >> 26) & 077) {
    case 050: /* SB */
    case 051: /* SH */
    case 052: /* SWL */
    case 053: /* SW */
    case 054: /* SDL */
    case 055: /* SDR */
    case 056: /* SWR */
    case 070: /* SC */
    case 071: /* SWC1 */
    case 074: /* SCD */
    case 075: /* SDC1 */
    case 077: /* SD */
#if !defined(__mips_isa_rev) || __mips_isa_rev < 6
    case 072: /* SWC2 */
    case 076: /* SDC2 */
#endif
        is_write = 1;
        break;
    case 023: /* COP1X */
        /* Required in all versions of MIPS64 since
           MIPS64r1 and subsequent versions of MIPS32r2. */
        switch (insn & 077) {
        case 010: /* SWXC1 */
        case 011: /* SDXC1 */
        case 015: /* SUXC1 */
            is_write = 1;
        }
        break;
    }

    return handle_cpu_signal(pc, info, is_write, &uc->uc_sigmask);
}

#elif defined(__loongarch__)
#if defined(CONFIG_LATX_KZT)
#include "debug.h"
static int find_stack_func_exist(int func, int index)
{
#include <execinfo.h>
#define BTSIZT 64
    void    * array[BTSIZT] = {0};
    size_t  size;
    int ret = -1;

    size = backtrace(array, BTSIZT);
    if (size < index) {
        return -1;
    }
    Dl_info dl_info;
    if (dladdr (array[index], &dl_info) && dl_info.dli_sname &&  *(int *)dl_info.dli_sname == func) {
        ret = 0;
        printf_log(LOG_NEVER, "%s find func %x\n", __func__, func);
        goto out;
    }
out:
    return ret;
}

static int elf_native_func[] = {0x6f6c6552/*"Relo"*/};
#endif
int cpu_signal_handler(int host_signum, void *pinfo,
                       void *puc)
{
    siginfo_t *info = pinfo;
    ucontext_t *uc = puc;
    greg_t pc = uc->uc_mcontext.__pc;
    uint32_t insn = *(uint32_t *)pc;

#ifndef CONFIG_SOFTMMU
    if (info->si_signo == SIGSEGV &&
        info->si_code == SEGV_MAPERR &&
        (tcg_tb_lookup(pc) || !current_cpu->running)) {
        int prot = page_get_flags((target_ulong)(uint64_t)info->si_addr);
        if (prot & PAGE_BITS) {
            cpu_relax();
            return 1;
        }
    }

    mmap_lock();

    if (page_get_target_data((uint64_t)info->si_addr) &&
        info->si_signo == SIGSEGV) {
        int ret = shared_private_interpret(info, uc);
        if (!ret) {
            mmap_unlock();
            return 1;
        }
    }
    if (info->si_signo == SIGBUS) {
        int ret = lock_interpret(info, uc);
        if (!ret) {
            mmap_unlock();
            return 1;
        }
    }
#if defined(CONFIG_LATX_KZT)
    if (option_kzt && (int64_t)info->si_addr >= info1.start_data &&
        (int64_t)info->si_addr <= info1.end_data &&
        info->si_signo == SIGSEGV) {
        int ret = elf_data_interpret(info, uc);
        if (!ret) {
            mmap_unlock();
            return 1;
        }
    }
    if (option_kzt && info->si_signo == SIGSEGV) {
        for (int i = 0; i < sizeof(elf_native_func) / sizeof(int); i++) {
            if (!find_stack_func_exist(elf_native_func[i], 4)) {
                int ret = elf_data_interpret(info, uc);
                if (!ret) {
                    mmap_unlock();
                    return 1;
                }
            }
        }
    }
#endif
    mmap_unlock();
#endif

    int is_write = 0;
    //TODO use kernel to send write info
    //only st/stl/str handled, may cause bugs

    switch (insn >> 24) {
    case 0x21: /* SC.W */
    case 0x23: /* SC.D */
    case 0x25: /* STPTR.W */
    case 0x27: /* STPTR.D */
    case 0x29: /* ST.{B/H/W/D} */
    case 0x2f: /* STL.{W/D} STR.{W/D} */
        is_write = 1;
        goto insn_parse_end;
    default:
        break;
    }

    switch (insn >> 22) {
    case 0xad: /* FST.S */
    case 0xaf: /* FST.D */
    case 0xb1: /* VST */
    case 0xb3: /* XVST */
        is_write = 1;
        goto insn_parse_end;
    default:
        break;
    }

    switch (insn >> 18) {
    case 0xe04: /* STX.B */
    case 0xe05: /* STX.H */
    case 0xe06: /* STX.W */
    case 0xe07: /* STX.D */
    case 0xe0e: /* FSTX.S */
    case 0xe0f: /* FSTX.D */
    case 0xe11: /* VSTX */
    case 0xe13: /* XVSTX */
        is_write = 1;
        goto insn_parse_end;
    default:
        break;
    }

    switch (insn >> 15) {
    case 0x70c0 ... 0x70e3: /* AM* */
        is_write = 1;
        break;
    }
insn_parse_end:

#ifdef CONFIG_LATX_DEBUG
    {
    if (option_debug_lative && (info->si_addr == (void *)0xde)) {
        show_sig_host_regs(uc);
        show_sig_x86_eflags(uc);
        uc->uc_mcontext.__pc += 4;
        return 1;
    }
    }
#endif

#ifndef CONFIG_LOONGARCH_NEW_WORLD
    /* TODO:workaround of user/kernel uapi mismatch */
    {
    sigset_t *puc_sigmask = (sigset_t *)((void *)&uc->uc_mcontext+0x1540);
    return handle_cpu_signal(pc, info, is_write, puc_sigmask);
    }
#else
    return handle_cpu_signal(pc, info, is_write, &uc->uc_sigmask);
#endif
}

#elif defined(__riscv)

int cpu_signal_handler(int host_signum, void *pinfo,
                       void *puc)
{
    siginfo_t *info = pinfo;
    ucontext_t *uc = puc;
    greg_t pc = uc->uc_mcontext.__gregs[REG_PC];
    uint32_t insn = *(uint32_t *)pc;
    int is_write = 0;

    /* Detect store by reading the instruction at the program
       counter. Note: we currently only generate 32-bit
       instructions so we thus only detect 32-bit stores */
    switch (((insn >> 0) & 0b11)) {
    case 3:
        switch (((insn >> 2) & 0b11111)) {
        case 8:
            switch (((insn >> 12) & 0b111)) {
            case 0: /* sb */
            case 1: /* sh */
            case 2: /* sw */
            case 3: /* sd */
            case 4: /* sq */
                is_write = 1;
                break;
            default:
                break;
            }
            break;
        case 9:
            switch (((insn >> 12) & 0b111)) {
            case 2: /* fsw */
            case 3: /* fsd */
            case 4: /* fsq */
                is_write = 1;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }

    /* Check for compressed instructions */
    switch (((insn >> 13) & 0b111)) {
    case 7:
        switch (insn & 0b11) {
        case 0: /*c.sd */
        case 2: /* c.sdsp */
            is_write = 1;
            break;
        default:
            break;
        }
        break;
    case 6:
        switch (insn & 0b11) {
        case 0: /* c.sw */
        case 3: /* c.swsp */
            is_write = 1;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    return handle_cpu_signal(pc, info, is_write, &uc->uc_sigmask);
}

#else

#error host CPU specific signal handler needed

#endif

/* The softmmu versions of these helpers are in cputlb.c.  */

uint32_t cpu_ldub_data(CPUArchState *env, abi_ptr ptr)
{
    uint32_t ret;
    uint16_t meminfo = trace_mem_get_info(MO_UB, MMU_USER_IDX, false);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    ret = ldub_p(g2h(env_cpu(env), ptr));
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
    return ret;
}

int cpu_ldsb_data(CPUArchState *env, abi_ptr ptr)
{
    int ret;
    uint16_t meminfo = trace_mem_get_info(MO_SB, MMU_USER_IDX, false);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    ret = ldsb_p(g2h(env_cpu(env), ptr));
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
    return ret;
}

uint32_t cpu_lduw_be_data(CPUArchState *env, abi_ptr ptr)
{
    uint32_t ret;
    uint16_t meminfo = trace_mem_get_info(MO_BEUW, MMU_USER_IDX, false);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    ret = lduw_be_p(g2h(env_cpu(env), ptr));
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
    return ret;
}

int cpu_ldsw_be_data(CPUArchState *env, abi_ptr ptr)
{
    int ret;
    uint16_t meminfo = trace_mem_get_info(MO_BESW, MMU_USER_IDX, false);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    ret = ldsw_be_p(g2h(env_cpu(env), ptr));
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
    return ret;
}

uint32_t cpu_ldl_be_data(CPUArchState *env, abi_ptr ptr)
{
    uint32_t ret;
    uint16_t meminfo = trace_mem_get_info(MO_BEUL, MMU_USER_IDX, false);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    ret = ldl_be_p(g2h(env_cpu(env), ptr));
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
    return ret;
}

uint64_t cpu_ldq_be_data(CPUArchState *env, abi_ptr ptr)
{
    uint64_t ret;
    uint16_t meminfo = trace_mem_get_info(MO_BEQ, MMU_USER_IDX, false);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    ret = ldq_be_p(g2h(env_cpu(env), ptr));
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
    return ret;
}

uint32_t cpu_lduw_le_data(CPUArchState *env, abi_ptr ptr)
{
    uint32_t ret;
    uint16_t meminfo = trace_mem_get_info(MO_LEUW, MMU_USER_IDX, false);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    ret = lduw_le_p(g2h(env_cpu(env), ptr));
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
    return ret;
}

int cpu_ldsw_le_data(CPUArchState *env, abi_ptr ptr)
{
    int ret;
    uint16_t meminfo = trace_mem_get_info(MO_LESW, MMU_USER_IDX, false);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    ret = ldsw_le_p(g2h(env_cpu(env), ptr));
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
    return ret;
}

uint32_t cpu_ldl_le_data(CPUArchState *env, abi_ptr ptr)
{
    uint32_t ret;
    uint16_t meminfo = trace_mem_get_info(MO_LEUL, MMU_USER_IDX, false);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    ret = ldl_le_p(g2h(env_cpu(env), ptr));
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
    return ret;
}

uint64_t cpu_ldq_le_data(CPUArchState *env, abi_ptr ptr)
{
    uint64_t ret;
    uint16_t meminfo = trace_mem_get_info(MO_LEQ, MMU_USER_IDX, false);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    ret = ldq_le_p(g2h(env_cpu(env), ptr));
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
    return ret;
}

uint32_t cpu_ldub_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t retaddr)
{
    uint32_t ret;

    set_helper_retaddr(retaddr);
    ret = cpu_ldub_data(env, ptr);
    clear_helper_retaddr();
    return ret;
}

int cpu_ldsb_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t retaddr)
{
    int ret;

    set_helper_retaddr(retaddr);
    ret = cpu_ldsb_data(env, ptr);
    clear_helper_retaddr();
    return ret;
}

uint32_t cpu_lduw_be_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t retaddr)
{
    uint32_t ret;

    set_helper_retaddr(retaddr);
    ret = cpu_lduw_be_data(env, ptr);
    clear_helper_retaddr();
    return ret;
}

int cpu_ldsw_be_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t retaddr)
{
    int ret;

    set_helper_retaddr(retaddr);
    ret = cpu_ldsw_be_data(env, ptr);
    clear_helper_retaddr();
    return ret;
}

uint32_t cpu_ldl_be_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t retaddr)
{
    uint32_t ret;

    set_helper_retaddr(retaddr);
    ret = cpu_ldl_be_data(env, ptr);
    clear_helper_retaddr();
    return ret;
}

uint64_t cpu_ldq_be_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t retaddr)
{
    uint64_t ret;

    set_helper_retaddr(retaddr);
    ret = cpu_ldq_be_data(env, ptr);
    clear_helper_retaddr();
    return ret;
}

uint32_t cpu_lduw_le_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t retaddr)
{
    uint32_t ret;

    set_helper_retaddr(retaddr);
    ret = cpu_lduw_le_data(env, ptr);
    clear_helper_retaddr();
    return ret;
}

int cpu_ldsw_le_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t retaddr)
{
    int ret;

    set_helper_retaddr(retaddr);
    ret = cpu_ldsw_le_data(env, ptr);
    clear_helper_retaddr();
    return ret;
}

uint32_t cpu_ldl_le_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t retaddr)
{
    uint32_t ret;

    set_helper_retaddr(retaddr);
    ret = cpu_ldl_le_data(env, ptr);
    clear_helper_retaddr();
    return ret;
}

uint64_t cpu_ldq_le_data_ra(CPUArchState *env, abi_ptr ptr, uintptr_t retaddr)
{
    uint64_t ret;

    set_helper_retaddr(retaddr);
    ret = cpu_ldq_le_data(env, ptr);
    clear_helper_retaddr();
    return ret;
}

void cpu_stb_data(CPUArchState *env, abi_ptr ptr, uint32_t val)
{
    uint16_t meminfo = trace_mem_get_info(MO_UB, MMU_USER_IDX, true);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    stb_p(g2h(env_cpu(env), ptr), val);
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
}

void cpu_stw_be_data(CPUArchState *env, abi_ptr ptr, uint32_t val)
{
    uint16_t meminfo = trace_mem_get_info(MO_BEUW, MMU_USER_IDX, true);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    stw_be_p(g2h(env_cpu(env), ptr), val);
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
}

void cpu_stl_be_data(CPUArchState *env, abi_ptr ptr, uint32_t val)
{
    uint16_t meminfo = trace_mem_get_info(MO_BEUL, MMU_USER_IDX, true);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    stl_be_p(g2h(env_cpu(env), ptr), val);
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
}

void cpu_stq_be_data(CPUArchState *env, abi_ptr ptr, uint64_t val)
{
    uint16_t meminfo = trace_mem_get_info(MO_BEQ, MMU_USER_IDX, true);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    stq_be_p(g2h(env_cpu(env), ptr), val);
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
}

void cpu_stw_le_data(CPUArchState *env, abi_ptr ptr, uint32_t val)
{
    uint16_t meminfo = trace_mem_get_info(MO_LEUW, MMU_USER_IDX, true);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    stw_le_p(g2h(env_cpu(env), ptr), val);
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
}

void cpu_stl_le_data(CPUArchState *env, abi_ptr ptr, uint32_t val)
{
    uint16_t meminfo = trace_mem_get_info(MO_LEUL, MMU_USER_IDX, true);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    stl_le_p(g2h(env_cpu(env), ptr), val);
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
}

void cpu_stq_le_data(CPUArchState *env, abi_ptr ptr, uint64_t val)
{
    uint16_t meminfo = trace_mem_get_info(MO_LEQ, MMU_USER_IDX, true);

    trace_guest_mem_before_exec(env_cpu(env), ptr, meminfo);
    stq_le_p(g2h(env_cpu(env), ptr), val);
    qemu_plugin_vcpu_mem_cb(env_cpu(env), ptr, meminfo);
}

void cpu_stb_data_ra(CPUArchState *env, abi_ptr ptr,
                     uint32_t val, uintptr_t retaddr)
{
    set_helper_retaddr(retaddr);
    cpu_stb_data(env, ptr, val);
    clear_helper_retaddr();
}

void cpu_stw_be_data_ra(CPUArchState *env, abi_ptr ptr,
                        uint32_t val, uintptr_t retaddr)
{
    set_helper_retaddr(retaddr);
    cpu_stw_be_data(env, ptr, val);
    clear_helper_retaddr();
}

void cpu_stl_be_data_ra(CPUArchState *env, abi_ptr ptr,
                        uint32_t val, uintptr_t retaddr)
{
    set_helper_retaddr(retaddr);
    cpu_stl_be_data(env, ptr, val);
    clear_helper_retaddr();
}

void cpu_stq_be_data_ra(CPUArchState *env, abi_ptr ptr,
                        uint64_t val, uintptr_t retaddr)
{
    set_helper_retaddr(retaddr);
    cpu_stq_be_data(env, ptr, val);
    clear_helper_retaddr();
}

void cpu_stw_le_data_ra(CPUArchState *env, abi_ptr ptr,
                        uint32_t val, uintptr_t retaddr)
{
    set_helper_retaddr(retaddr);
    cpu_stw_le_data(env, ptr, val);
    clear_helper_retaddr();
}

void cpu_stl_le_data_ra(CPUArchState *env, abi_ptr ptr,
                        uint32_t val, uintptr_t retaddr)
{
    set_helper_retaddr(retaddr);
    cpu_stl_le_data(env, ptr, val);
    clear_helper_retaddr();
}

void cpu_stq_le_data_ra(CPUArchState *env, abi_ptr ptr,
                        uint64_t val, uintptr_t retaddr)
{
    set_helper_retaddr(retaddr);
    cpu_stq_le_data(env, ptr, val);
    clear_helper_retaddr();
}

uint32_t cpu_ldub_code(CPUArchState *env, abi_ptr ptr)
{
    uint32_t ret;

    set_helper_retaddr(1);
    ret = ldub_p(g2h_untagged(ptr));
    clear_helper_retaddr();
    return ret;
}

uint32_t cpu_lduw_code(CPUArchState *env, abi_ptr ptr)
{
    uint32_t ret;

    set_helper_retaddr(1);
    ret = lduw_p(g2h_untagged(ptr));
    clear_helper_retaddr();
    return ret;
}

uint32_t cpu_ldl_code(CPUArchState *env, abi_ptr ptr)
{
    uint32_t ret;

    set_helper_retaddr(1);
    ret = ldl_p(g2h_untagged(ptr));
    clear_helper_retaddr();
    return ret;
}

uint64_t cpu_ldq_code(CPUArchState *env, abi_ptr ptr)
{
    uint64_t ret;

    set_helper_retaddr(1);
    ret = ldq_p(g2h_untagged(ptr));
    clear_helper_retaddr();
    return ret;
}

/* Do not allow unaligned operations to proceed.  Return the host address.  */
static void *atomic_mmu_lookup(CPUArchState *env, target_ulong addr,
                               int size, uintptr_t retaddr)
{
    /* Enforce qemu required alignment.  */
    if (unlikely(addr & (size - 1))) {
        cpu_loop_exit_atomic(env_cpu(env), retaddr);
    }
    void *ret = g2h(env_cpu(env), addr);
    set_helper_retaddr(retaddr);
    return ret;
}

/* Macro to call the above, with local variables from the use context.  */
#define ATOMIC_MMU_DECLS do {} while (0)
#define ATOMIC_MMU_LOOKUP  atomic_mmu_lookup(env, addr, DATA_SIZE, GETPC())
#define ATOMIC_MMU_CLEANUP do { clear_helper_retaddr(); } while (0)
#define ATOMIC_MMU_IDX MMU_USER_IDX

#define ATOMIC_NAME(X)   HELPER(glue(glue(atomic_ ## X, SUFFIX), END))
#define EXTRA_ARGS

#include "atomic_common.c.inc"

#define DATA_SIZE 1
#include "atomic_template.h"

#define DATA_SIZE 2
#include "atomic_template.h"

#define DATA_SIZE 4
#include "atomic_template.h"

#ifdef CONFIG_ATOMIC64
#define DATA_SIZE 8
#include "atomic_template.h"
#endif

/* The following is only callable from other helpers, and matches up
   with the softmmu version.  */

#if HAVE_ATOMIC128 || HAVE_CMPXCHG128

#undef EXTRA_ARGS
#undef ATOMIC_NAME
#undef ATOMIC_MMU_LOOKUP

#define EXTRA_ARGS     , TCGMemOpIdx oi, uintptr_t retaddr
#define ATOMIC_NAME(X) \
    HELPER(glue(glue(glue(atomic_ ## X, SUFFIX), END), _mmu))
#define ATOMIC_MMU_LOOKUP  atomic_mmu_lookup(env, addr, DATA_SIZE, retaddr)

#define DATA_SIZE 16
#include "atomic_template.h"
#endif
