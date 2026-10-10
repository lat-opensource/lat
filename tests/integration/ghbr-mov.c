#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static __attribute__((noinline)) uint32_t mov32_low(uint32_t value)
{
    uint32_t result;

    __asm__ volatile("movl %1, %0" : "=r"(result) : "r"(value));
    return result;
}

static __attribute__((noinline)) uint64_t mov32_full(uint32_t value)
{
    uint64_t result;

    __asm__ volatile(
        "movl %1, %%eax\n\t"
        "movq %%rax, %0"
        : "=r"(result)
        : "r"(value)
        : "rax");
    return result;
}

static __attribute__((noinline)) uint64_t mov32_self_full(uint64_t value)
{
    uint64_t result;

    __asm__ volatile(
        "movq %1, %%rax\n\t"
        "movl %%eax, %%eax\n\t"
        "movq %%rax, %0"
        : "=r"(result)
        : "r"(value)
        : "rax");
    return result;
}

int main(void)
{
    uint32_t value = 0x89abcdef;
    uint32_t low = mov32_low(value);
    uint64_t full = mov32_full(value);
    uint64_t self = mov32_self_full(0xdeadbeef89abcdefULL);

    printf("mov32=%08" PRIx32 ":%016" PRIx64 ":%016" PRIx64 "\n",
           low, full, self);
    return low != 0x89abcdefU || full != 0x0000000089abcdefULL ||
           self != 0x0000000089abcdefULL;
}
