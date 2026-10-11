__attribute__((visibility("default")))
int semantic_preload_marker(void)
{
    return 77;
}

__attribute__((visibility("default")))
int semantic_hook(void)
{
    return 100;
}

__attribute__((naked, visibility("default"), noreturn))
void semantic_preload_entry(void)
{
    __asm__ volatile(
        "mov $60, %eax\n\t"
        "xor %edi, %edi\n\t"
        "syscall\n\t"
        "1: jmp 1b\n\t");
}
