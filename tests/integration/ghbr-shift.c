#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static __attribute__((noinline)) uint32_t shl32_low(uint32_t value,
                                                   uint32_t count)
{
    uint32_t result;

    __asm__ volatile("shll %b2, %0"
                     : "=r"(result)
                     : "0"(value), "c"(count)
                     : "cc");
    return result;
}

static __attribute__((noinline)) uint64_t shl32_zero_full(uint32_t value)
{
    uint64_t result;

    __asm__ volatile(
        "movl %1, %%eax\n\t"
        "shll $0, %%eax\n\t"
        "movq %%rax, %0"
        : "=r"(result)
        : "r"(value)
        : "rax", "cc");
    return result;
}

static __attribute__((noinline)) uint64_t sar32_full(uint32_t value,
                                                     uint32_t count)
{
    uint64_t result;

    __asm__ volatile(
        "movl %1, %%eax\n\t"
        "sarl %b2, %%eax\n\t"
        "movq %%rax, %0"
        : "=r"(result)
        : "r"(value), "c"(count)
        : "rax", "cc");
    return result;
}

int main(void)
{
    uint32_t shl = shl32_low(0x12345678, 4);
    uint64_t zero = shl32_zero_full(0x89abcdef);
    uint64_t sar = sar32_full(0x80000000, 1);

    printf("shift=%08" PRIx32 ":%016" PRIx64 ":%016" PRIx64 "\n",
           shl, zero, sar);
    return shl != 0x23456780U || zero != 0x0000000089abcdefULL ||
           sar != 0x00000000c0000000ULL;
}
