#include <elf.h>
#include <stddef.h>
#include <stdint.h>

__thread uint64_t tls_value = 42;

static long x86_syscall6(long number, long a1, long a2, long a3,
                         long a4, long a5, long a6)
{
    register long r10 __asm__("r10") = a4;
    register long r8 __asm__("r8") = a5;
    register long r9 __asm__("r9") = a6;
    long result;
    __asm__ volatile("syscall" : "=a"(result) : "a"(number), "D"(a1),
                     "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9) :
                     "rcx", "r11", "memory");
    return result;
}

static int fail(void)
{
    return 94;
}

int guest_main(uint64_t *stack)
{
    uint64_t argc = *stack++;
    stack += argc + 1;
    while (*stack++) {}
    Elf64_auxv_t *auxv = (void *)stack;
    const Elf64_Phdr *phdr = NULL;
    uint64_t phnum = 0;
    for (; auxv->a_type != AT_NULL; auxv++) {
        if (auxv->a_type == AT_PHDR) phdr = (void *)auxv->a_un.a_val;
        if (auxv->a_type == AT_PHNUM) phnum = auxv->a_un.a_val;
    }
    if (!phdr || !phnum) return fail();
    const Elf64_Phdr *tls = NULL;
    for (uint64_t i = 0; i < phnum; i++) {
        if (phdr[i].p_type == PT_TLS) tls = &phdr[i];
    }
    if (!tls || !tls->p_memsz || tls->p_filesz > tls->p_memsz) return fail();
    uint64_t alignment = tls->p_align ? tls->p_align : 1;
    uint64_t tls_size = (tls->p_memsz + alignment - 1) & ~(alignment - 1);
    unsigned char *memory = (void *)(uintptr_t)x86_syscall6(
        9, 0, 4096, 3, 0x22, -1, 0);
    if ((uintptr_t)memory >= (uintptr_t)-4095) return fail();
    unsigned char *thread_pointer = memory + tls_size;
    unsigned char *destination = thread_pointer - tls_size;
    const unsigned char *source = (void *)(uintptr_t)tls->p_vaddr;
    for (uint64_t i = 0; i < tls->p_filesz; i++) destination[i] = source[i];
    for (uint64_t i = tls->p_filesz; i < tls_size; i++) destination[i] = 0;
    if (x86_syscall6(158, 0x1002, (long)thread_pointer, 0, 0, 0, 0)) {
        return fail();
    }
    if (tls_value != 42) return fail();
    static const char message[] = "TLS OK\n";
    if (x86_syscall6(1, 1, (long)message, sizeof(message) - 1, 0, 0, 0) !=
        sizeof(message) - 1) {
        return fail();
    }
    return 42;
}
