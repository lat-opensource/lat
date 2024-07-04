#include <stdarg.h>
#include "callback.h"
#include "lsenv.h"
#include "qemu.h"

#ifdef TARGET_X86_64
static int64_t Pop64(CPUX86State *cpu)
{
    uint64_t *st = ((uint64_t*)(cpu->regs[R_ESP]));
    cpu->regs[R_ESP] += 8;

    return *st;
}

static void Push64(CPUX86State *cpu, uint64_t v)
{
    cpu->regs[R_ESP] -= 8;
    *((uint64_t*)cpu->regs[R_ESP]) = v;

}
#endif

uint64_t RunFunctionWithState(uintptr_t fnc, int nargs, ...)
{
#ifdef TARGET_X86_64
    lsassert(fnc);
    lsassert(CODEIS64);
    CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state;
    CPUState * cs = env_cpu(cpu);

    int align = (nargs>6)?(((nargs-6)&1)):0;
    int stackn = align + ((nargs>6)?(nargs-6):0);

    Push64(cpu, cpu->regs[R_EBP]);
    uintptr_t old_rbp = cpu->regs[R_EBP] = cpu->regs[R_ESP];      // mov rbp, rsp

    Push64(cpu, cpu->regs[R_EDI]);
    Push64(cpu, cpu->regs[R_ESI]);
    Push64(cpu, cpu->regs[R_EDX]);
    Push64(cpu, cpu->regs[R_ECX]);
    Push64(cpu, cpu->regs[R_R8]);
    Push64(cpu, cpu->regs[R_R9]);
    Push64(cpu, cpu->regs[R_R10]);
    Push64(cpu, cpu->regs[R_R11]);
    Push64(cpu, cpu->regs[R_EBX]);
    Push64(cpu, cpu->regs[R_R12]);
    Push64(cpu, cpu->regs[R_R13]);
    Push64(cpu, cpu->regs[R_R14]);
    Push64(cpu, cpu->regs[R_R15]);

    cpu->regs[R_ESP] -= stackn*sizeof(void*);   // need to push in reverse order
    uint64_t *p = (uint64_t*)cpu->regs[R_ESP];

    va_list va;
    va_start (va, nargs);
    for (int i=0; i<nargs; ++i) {
        if(i<6) {
            int nn[] = {R_EDI, R_ESI, R_EDX, R_ECX, R_R8, R_R9};
            cpu->regs[nn[i]] = va_arg(va, uint64_t);
        } else {
            *p = va_arg(va, uint64_t);
            p++;
        }
    }

    Push64(cpu, (uint64_t)&RunFunctionWithState);
    va_end (va);

    uintptr_t oldip = cpu->eip;
    cpu->eip=fnc;
    sigjmp_buf buf;
    memcpy(&buf, &cs->jmp_env, sizeof(sigjmp_buf));

    uintptr_t old_running = qatomic_read(&cs->running);
    cpu_loop(cpu);
    qatomic_set(&cs->running, old_running);

    memcpy(&cs->jmp_env, &buf, sizeof(sigjmp_buf));
    cpu->eip = oldip;

    cpu->regs[R_R15] = Pop64(cpu);
    cpu->regs[R_R14] = Pop64(cpu);
    cpu->regs[R_R13] = Pop64(cpu);
    cpu->regs[R_R12] = Pop64(cpu);
    cpu->regs[R_EBX] = Pop64(cpu);
    cpu->regs[R_R11] = Pop64(cpu);
    cpu->regs[R_R10] = Pop64(cpu);
    cpu->regs[R_R9] = Pop64(cpu);
    cpu->regs[R_R8] = Pop64(cpu);
    cpu->regs[R_ECX] = Pop64(cpu);
    cpu->regs[R_EDX] = Pop64(cpu);
    cpu->regs[R_ESI] = Pop64(cpu);
    cpu->regs[R_EDI] = Pop64(cpu);

    cpu->regs[R_ESP] = old_rbp;          // mov rsp, rbp
    cpu->regs[R_EBP] = Pop64(cpu);     // pop rbp

    return cpu->regs[R_EAX];
#else
    return 0;
#endif
}
