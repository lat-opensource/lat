
#ifndef LOONGARCH_HOSTDEP_H
#define LOONGARCH_HOSTDEP_H

/* We have a safe-syscall.inc.S */
#define HAVE_SAFE_SYSCALL

#ifndef __ASSEMBLER__

/* These are defined by the safe-syscall.inc.S file */
extern char safe_syscall_start[];
extern char safe_syscall_end[];

/* Adjust the signal context to rewind out of safe-syscall if we're in it */
static inline void rewind_if_in_safe_syscall(void *puc)
{
    ucontext_t *uc = puc;
    uintptr_t  *pcreg = (uintptr_t *)&(uc->uc_mcontext.__pc);

    if (*pcreg > (uintptr_t)safe_syscall_start
        && *pcreg < (uintptr_t)safe_syscall_end) {
        *pcreg = (uintptr_t)safe_syscall_start;
    }
}

#endif /* __ASSEMBLER__ */

#endif
