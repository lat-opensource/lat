extern int semantic_hook(void);

static __thread int semantic_tls = 4;

__attribute__((visibility("default")))
int semantic_tls_add(int value)
{
    semantic_tls += value;
    return semantic_tls;
}

static int semantic_ifunc_impl(void)
{
    return 23;
}

static void *semantic_ifunc_resolver(void)
{
    return semantic_ifunc_impl;
}

__attribute__((ifunc("semantic_ifunc_resolver"), visibility("default")))
int semantic_ifunc(void);

int semantic_version_v1(void)
{
    return 31;
}

int semantic_version_v2(void)
{
    return 32;
}

__asm__(".symver semantic_version_v1,semantic_version@LATC_1.0");
__asm__(".symver semantic_version_v2,semantic_version@@LATC_2.0");

__attribute__((visibility("default")))
int semantic_interposed(void)
{
    return semantic_hook();
}

__attribute__((visibility("default"), noinline))
int semantic_pingpong(int loops)
{
    int sum = 0;

    for (int i = 0; i < loops; i++) {
        sum += semantic_hook();
    }
    return sum;
}

__attribute__((naked, visibility("default"), noreturn))
void semantic_plugin_entry(void)
{
    __asm__ volatile(
        "mov $60, %eax\n\t"
        "xor %edi, %edi\n\t"
        "syscall\n\t"
        "1: jmp 1b\n\t");
}
