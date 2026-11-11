#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static __attribute__((noinline)) uint32_t imul32_low(uint32_t lhs,
                                                    uint32_t rhs)
{
    uint32_t result;

    __asm__ volatile("imull %2, %0"
                     : "=r"(result)
                     : "0"(lhs), "r"(rhs)
                     : "cc");
    return result;
}

static __attribute__((noinline)) uint64_t imul32_full(uint32_t lhs,
                                                     uint32_t rhs)
{
    uint64_t result;

    __asm__ volatile(
        "movl %1, %%eax\n\t"
        "imull %2, %%eax\n\t"
        "movq %%rax, %0"
        : "=r"(result)
        : "r"(lhs), "r"(rhs)
        : "rax", "cc");
    return result;
}

int main(void)
{
    uint32_t lhs = 0x12345678;
    uint32_t rhs = 0x1337;
    uint32_t low = imul32_low(lhs, rhs);
    uint64_t full = imul32_full(lhs, rhs);

    printf("imul32=%08" PRIx32 ":%016" PRIx64 "\n",
           low, full);
    return low != 0xcba97bc8U || full != 0x00000000cba97bc8ULL;
}
