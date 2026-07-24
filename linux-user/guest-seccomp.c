#include "qemu/osdep.h"
#include "cpu.h"
#include "qemu.h"
#include "guest-seccomp.h"
#include "signal-common.h"

#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>

#ifndef SECCOMP_RET_USER_NOTIF
#define SECCOMP_RET_USER_NOTIF 0x7fc00000U
#endif

typedef struct GuestSeccompData {
    int32_t nr;
    uint32_t arch;
    uint64_t instruction_pointer;
    uint64_t args[6];
} GuestSeccompData;

typedef struct GuestSeccompFilter {
    struct GuestSeccompFilter *previous;
    unsigned int len;
    struct sock_filter insns[];
} GuestSeccompFilter;

static bool seccomp_jump_valid(unsigned int pc, uint32_t offset,
                               unsigned int len)
{
    return offset < len && pc < len - offset - 1;
}

static bool seccomp_filter_valid(const struct sock_filter *insns,
                                 unsigned int len)
{
    unsigned int i;

    if (len == 0 || len > BPF_MAXINSNS ||
        BPF_CLASS(insns[len - 1].code) != BPF_RET) {
        return false;
    }

    for (i = 0; i < len; i++) {
        const struct sock_filter *insn = &insns[i];

        switch (insn->code) {
        case BPF_LD | BPF_W | BPF_ABS:
            if ((insn->k & 3) ||
                insn->k > sizeof(GuestSeccompData) - sizeof(uint32_t)) {
                return false;
            }
            break;
        case BPF_LD | BPF_W | BPF_IMM:
        case BPF_LDX | BPF_W | BPF_IMM:
            break;
        case BPF_LD | BPF_W | BPF_MEM:
        case BPF_LDX | BPF_W | BPF_MEM:
        case BPF_ST:
        case BPF_STX:
            if (insn->k >= BPF_MEMWORDS) {
                return false;
            }
            break;
        case BPF_ALU | BPF_ADD | BPF_K:
        case BPF_ALU | BPF_ADD | BPF_X:
        case BPF_ALU | BPF_SUB | BPF_K:
        case BPF_ALU | BPF_SUB | BPF_X:
        case BPF_ALU | BPF_MUL | BPF_K:
        case BPF_ALU | BPF_MUL | BPF_X:
        case BPF_ALU | BPF_DIV | BPF_X:
        case BPF_ALU | BPF_OR | BPF_K:
        case BPF_ALU | BPF_OR | BPF_X:
        case BPF_ALU | BPF_AND | BPF_K:
        case BPF_ALU | BPF_AND | BPF_X:
        case BPF_ALU | BPF_LSH | BPF_K:
        case BPF_ALU | BPF_LSH | BPF_X:
        case BPF_ALU | BPF_RSH | BPF_K:
        case BPF_ALU | BPF_RSH | BPF_X:
        case BPF_ALU | BPF_MOD | BPF_X:
        case BPF_ALU | BPF_XOR | BPF_K:
        case BPF_ALU | BPF_XOR | BPF_X:
        case BPF_ALU | BPF_NEG:
            break;
        case BPF_ALU | BPF_DIV | BPF_K:
        case BPF_ALU | BPF_MOD | BPF_K:
            if (insn->k == 0) {
                return false;
            }
            break;
        case BPF_JMP | BPF_JA:
            if (!seccomp_jump_valid(i, insn->k, len)) {
                return false;
            }
            break;
        case BPF_JMP | BPF_JEQ | BPF_K:
        case BPF_JMP | BPF_JEQ | BPF_X:
        case BPF_JMP | BPF_JGT | BPF_K:
        case BPF_JMP | BPF_JGT | BPF_X:
        case BPF_JMP | BPF_JGE | BPF_K:
        case BPF_JMP | BPF_JGE | BPF_X:
        case BPF_JMP | BPF_JSET | BPF_K:
        case BPF_JMP | BPF_JSET | BPF_X:
            if (!seccomp_jump_valid(i, insn->jt, len) ||
                !seccomp_jump_valid(i, insn->jf, len)) {
                return false;
            }
            break;
        case BPF_RET | BPF_K:
        case BPF_RET | BPF_A:
        case BPF_MISC | BPF_TAX:
        case BPF_MISC | BPF_TXA:
            break;
        default:
            return false;
        }
    }
    return true;
}

static uint32_t seccomp_run_filter(const GuestSeccompFilter *filter,
                                   const GuestSeccompData *data)
{
    uint32_t accumulator = 0;
    uint32_t index = 0;
    uint32_t memory[BPF_MEMWORDS] = { 0 };
    unsigned int pc = 0;

    while (pc < filter->len) {
        const struct sock_filter *insn = &filter->insns[pc++];
        uint32_t operand = BPF_SRC(insn->code) == BPF_X ? index : insn->k;
        bool condition;

        switch (insn->code) {
        case BPF_LD | BPF_W | BPF_ABS:
            memcpy(&accumulator, (const uint8_t *)data + insn->k,
                   sizeof(accumulator));
            break;
        case BPF_LD | BPF_W | BPF_IMM:
            accumulator = insn->k;
            break;
        case BPF_LDX | BPF_W | BPF_IMM:
            index = insn->k;
            break;
        case BPF_LD | BPF_W | BPF_MEM:
            accumulator = memory[insn->k];
            break;
        case BPF_LDX | BPF_W | BPF_MEM:
            index = memory[insn->k];
            break;
        case BPF_ST:
            memory[insn->k] = accumulator;
            break;
        case BPF_STX:
            memory[insn->k] = index;
            break;
        case BPF_ALU | BPF_ADD | BPF_K:
        case BPF_ALU | BPF_ADD | BPF_X:
            accumulator += operand;
            break;
        case BPF_ALU | BPF_SUB | BPF_K:
        case BPF_ALU | BPF_SUB | BPF_X:
            accumulator -= operand;
            break;
        case BPF_ALU | BPF_MUL | BPF_K:
        case BPF_ALU | BPF_MUL | BPF_X:
            accumulator *= operand;
            break;
        case BPF_ALU | BPF_DIV | BPF_K:
        case BPF_ALU | BPF_DIV | BPF_X:
            if (operand == 0) {
                return SECCOMP_RET_KILL_THREAD;
            }
            accumulator /= operand;
            break;
        case BPF_ALU | BPF_OR | BPF_K:
        case BPF_ALU | BPF_OR | BPF_X:
            accumulator |= operand;
            break;
        case BPF_ALU | BPF_AND | BPF_K:
        case BPF_ALU | BPF_AND | BPF_X:
            accumulator &= operand;
            break;
        case BPF_ALU | BPF_LSH | BPF_K:
        case BPF_ALU | BPF_LSH | BPF_X:
            accumulator = operand < 32 ? accumulator << operand : 0;
            break;
        case BPF_ALU | BPF_RSH | BPF_K:
        case BPF_ALU | BPF_RSH | BPF_X:
            accumulator = operand < 32 ? accumulator >> operand : 0;
            break;
        case BPF_ALU | BPF_NEG:
            accumulator = -accumulator;
            break;
        case BPF_ALU | BPF_MOD | BPF_K:
        case BPF_ALU | BPF_MOD | BPF_X:
            if (operand == 0) {
                return SECCOMP_RET_KILL_THREAD;
            }
            accumulator %= operand;
            break;
        case BPF_ALU | BPF_XOR | BPF_K:
        case BPF_ALU | BPF_XOR | BPF_X:
            accumulator ^= operand;
            break;
        case BPF_JMP | BPF_JA:
            pc += insn->k;
            break;
        case BPF_JMP | BPF_JEQ | BPF_K:
        case BPF_JMP | BPF_JEQ | BPF_X:
            condition = accumulator == operand;
            pc += condition ? insn->jt : insn->jf;
            break;
        case BPF_JMP | BPF_JGT | BPF_K:
        case BPF_JMP | BPF_JGT | BPF_X:
            condition = accumulator > operand;
            pc += condition ? insn->jt : insn->jf;
            break;
        case BPF_JMP | BPF_JGE | BPF_K:
        case BPF_JMP | BPF_JGE | BPF_X:
            condition = accumulator >= operand;
            pc += condition ? insn->jt : insn->jf;
            break;
        case BPF_JMP | BPF_JSET | BPF_K:
        case BPF_JMP | BPF_JSET | BPF_X:
            condition = (accumulator & operand) != 0;
            pc += condition ? insn->jt : insn->jf;
            break;
        case BPF_RET | BPF_K:
            return insn->k;
        case BPF_RET | BPF_A:
            return accumulator;
        case BPF_MISC | BPF_TAX:
            index = accumulator;
            break;
        case BPF_MISC | BPF_TXA:
            accumulator = index;
            break;
        default:
            g_assert_not_reached();
        }
    }
    return SECCOMP_RET_KILL_THREAD;
}

static abi_long seccomp_load_filter(abi_ulong program,
                                    GuestSeccompFilter **result)
{
    struct target_sock_fprog *target_program;
    struct target_sock_filter *target_insns;
    GuestSeccompFilter *filter;
    abi_ulong target_filter;
    unsigned int len;
    unsigned int i;

    if (!program) {
        return -TARGET_EFAULT;
    }
    if (!lock_user_struct(VERIFY_READ, target_program, program, 1)) {
        return -TARGET_EFAULT;
    }
    len = tswap16(target_program->len);
    target_filter = tswapal(target_program->filter);
    unlock_user_struct(target_program, program, 0);

    if (len == 0 || len > BPF_MAXINSNS) {
        return -TARGET_EINVAL;
    }
    target_insns = lock_user(VERIFY_READ, target_filter,
                             len * sizeof(*target_insns), 1);
    if (!target_insns) {
        return -TARGET_EFAULT;
    }

    filter = g_malloc(sizeof(*filter) + len * sizeof(filter->insns[0]));
    filter->previous = NULL;
    filter->len = len;
    for (i = 0; i < len; i++) {
        filter->insns[i].code = tswap16(target_insns[i].code);
        filter->insns[i].jt = target_insns[i].jt;
        filter->insns[i].jf = target_insns[i].jf;
        filter->insns[i].k = tswap32(target_insns[i].k);
    }
    unlock_user(target_insns, target_filter, 0);

    if (!seccomp_filter_valid(filter->insns, filter->len)) {
        g_free(filter);
        return -TARGET_EINVAL;
    }
    *result = filter;
    return 0;
}

static abi_long seccomp_install_filter(CPUArchState *env, abi_ulong flags,
                                       abi_ulong program)
{
    const abi_ulong supported_flags = SECCOMP_FILTER_FLAG_TSYNC |
                                      SECCOMP_FILTER_FLAG_LOG |
                                      SECCOMP_FILTER_FLAG_SPEC_ALLOW;
    CPUState *cpu = env_cpu(env);
    TaskState *task = cpu->opaque;
    GuestSeccompFilter *filter;
    abi_long ret;

    if (flags & ~supported_flags) {
        return -TARGET_EINVAL;
    }
    if (prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) != 1) {
        return -TARGET_EACCES;
    }
    ret = seccomp_load_filter(program, &filter);
    if (ret) {
        return ret;
    }
    filter->previous = task->seccomp_filter;

    if (flags & SECCOMP_FILTER_FLAG_TSYNC) {
        CPUState *other_cpu;

        start_exclusive();
        cpu_list_lock();
        CPU_FOREACH(other_cpu) {
            TaskState *other_task = other_cpu->opaque;

            if (other_task->seccomp_filter != task->seccomp_filter) {
                ret = other_task->ts_tid;
                break;
            }
        }
        if (ret == 0) {
            CPU_FOREACH(other_cpu) {
                TaskState *other_task = other_cpu->opaque;

                other_task->seccomp_filter = filter;
            }
        }
        cpu_list_unlock();
        end_exclusive();
        if (ret != 0) {
            g_free(filter);
        }
        return ret;
    }

    task->seccomp_filter = filter;
    return 0;
}

abi_long guest_seccomp_prctl(CPUArchState *env, abi_long option,
                             abi_long mode, abi_ulong program)
{
    TaskState *task = env_cpu(env)->opaque;

    if (option == PR_GET_SECCOMP) {
        return task->seccomp_filter ? SECCOMP_MODE_FILTER :
                                      SECCOMP_MODE_DISABLED;
    }
    if (mode != SECCOMP_MODE_FILTER) {
        return -TARGET_EINVAL;
    }
    return seccomp_install_filter(env, 0, program);
}

abi_long guest_seccomp_syscall(CPUArchState *env, abi_long operation,
                               abi_ulong flags, abi_ulong args)
{
    switch (operation) {
    case SECCOMP_SET_MODE_FILTER:
        return seccomp_install_filter(env, flags, args);
    case SECCOMP_GET_ACTION_AVAIL:
    {
        uint32_t action;

        if (flags != 0) {
            return -TARGET_EINVAL;
        }
        if (get_user_u32(action, args)) {
            return -TARGET_EFAULT;
        }
        switch (action) {
        case SECCOMP_RET_KILL_PROCESS:
        case SECCOMP_RET_KILL_THREAD:
        case SECCOMP_RET_TRAP:
        case SECCOMP_RET_ERRNO:
        case SECCOMP_RET_TRACE:
        case SECCOMP_RET_LOG:
        case SECCOMP_RET_ALLOW:
            return 0;
        default:
            return -TARGET_EOPNOTSUPP;
        }
    }
    default:
        return -TARGET_EINVAL;
    }
}

uint32_t guest_seccomp_target_arch(void)
{
#ifdef TARGET_X86_64
    return AUDIT_ARCH_X86_64;
#else
    return AUDIT_ARCH_I386;
#endif
}

GuestSeccompAction guest_seccomp_filter_syscall(CPUArchState *env, int num,
                                                uint32_t arch,
                                                const abi_long args[6],
                                                abi_long *result)
{
    TaskState *task = env_cpu(env)->opaque;
    GuestSeccompFilter *filter = task->seccomp_filter;
    GuestSeccompData data;
    uint32_t decision = SECCOMP_RET_ALLOW;
    unsigned int i;

    if (!filter) {
        return GUEST_SECCOMP_CONTINUE;
    }

    data.nr = num;
    data.arch = arch;
    data.instruction_pointer = env->eip;
    for (i = 0; i < ARRAY_SIZE(data.args); i++) {
        data.args[i] = (abi_ulong)args[i];
    }

    for (; filter; filter = filter->previous) {
        uint32_t current = seccomp_run_filter(filter, &data);
        int32_t current_action = current & SECCOMP_RET_ACTION_FULL;
        int32_t decision_action = decision & SECCOMP_RET_ACTION_FULL;

        if (current_action < decision_action) {
            decision = current;
        }
    }

    switch (decision & SECCOMP_RET_ACTION_FULL) {
    case SECCOMP_RET_ALLOW:
    case SECCOMP_RET_LOG:
        return GUEST_SECCOMP_CONTINUE;
    case SECCOMP_RET_ERRNO:
        *result = -(abi_long)MIN(decision & SECCOMP_RET_DATA, 4095U);
        return GUEST_SECCOMP_RETURN;
    case SECCOMP_RET_TRAP:
    {
        target_siginfo_t info = {
            .si_signo = TARGET_SIGSYS,
            .si_errno = decision & SECCOMP_RET_DATA,
            .si_code = TARGET_SYS_SECCOMP,
            ._sifields._sigsys._call_addr = env->eip,
            ._sifields._sigsys._syscall = num,
            ._sifields._sigsys._arch = data.arch,
        };

        queue_signal(env, TARGET_SIGSYS, QEMU_SI_SYS, &info);
        *result = num;
        return GUEST_SECCOMP_RETURN;
    }
    case SECCOMP_RET_TRACE:
    case SECCOMP_RET_USER_NOTIF:
        *result = -TARGET_ENOSYS;
        return GUEST_SECCOMP_RETURN;
    case SECCOMP_RET_KILL_PROCESS:
        return GUEST_SECCOMP_KILL_PROCESS;
    case SECCOMP_RET_KILL_THREAD:
        return GUEST_SECCOMP_KILL_THREAD;
    default:
        return GUEST_SECCOMP_KILL_PROCESS;
    }
}
