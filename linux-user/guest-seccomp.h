#ifndef LINUX_USER_GUEST_SECCOMP_H
#define LINUX_USER_GUEST_SECCOMP_H

typedef enum GuestSeccompAction {
    GUEST_SECCOMP_CONTINUE,
    GUEST_SECCOMP_RETURN,
    GUEST_SECCOMP_KILL_THREAD,
    GUEST_SECCOMP_KILL_PROCESS,
} GuestSeccompAction;

abi_long guest_seccomp_prctl(CPUArchState *env, abi_long option,
                             abi_long mode, abi_ulong program);
abi_long guest_seccomp_syscall(CPUArchState *env, abi_long operation,
                               abi_ulong flags, abi_ulong args);
uint32_t guest_seccomp_target_arch(void);
GuestSeccompAction guest_seccomp_filter_syscall(CPUArchState *env, int num,
                                                uint32_t arch,
                                                const abi_long args[6],
                                                abi_long *result);

#endif
