#include <stdint.h>

typedef int (*latc_callback_fn)(int value);

__attribute__((visibility("default")))
int latc_plugin_apply(int value, latc_callback_fn callback)
{
    return callback(value) + 7;
}

__attribute__((naked, visibility("default"), noreturn))
void latc_plugin_entry(void)
{
    __asm__ volatile(
        "mov $60, %eax\n\t"
        "xor %edi, %edi\n\t"
        "syscall\n\t"
        "1: jmp 1b\n\t");
}
