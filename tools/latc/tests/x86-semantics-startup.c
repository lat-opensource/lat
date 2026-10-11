typedef int (*semantic_callback_fn)(int);

__attribute__((visibility("default")))
int semantic_startup_apply(int value, semantic_callback_fn callback)
{
    return callback(value) + 1;
}

__attribute__((naked, visibility("default"), noreturn))
void semantic_startup_entry(void)
{
    __asm__ volatile(
        "mov $60, %eax\n\t"
        "xor %edi, %edi\n\t"
        "syscall\n\t"
        "1: jmp 1b\n\t");
}
