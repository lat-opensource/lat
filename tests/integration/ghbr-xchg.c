#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static __attribute__((noinline)) uint32_t xchg32_low(uint32_t lhs,
                                                     uint32_t rhs)
{
    __asm__ volatile("xchgl %0, %1"
                     : "+r"(lhs), "+r"(rhs)
                     :
                     : "memory");
    return lhs ^ rhs;
}

static __attribute__((noinline)) uint64_t xchg32_full(uint64_t lhs,
                                                      uint64_t rhs)
{
    uint64_t lhs_out;
    uint64_t rhs_out;

    __asm__ volatile(
        "movq %[lhs], %%rax\n\t"
        "movq %[rhs], %%rbx\n\t"
        "xchgl %%eax, %%ebx\n\t"
        "movq %%rax, %[lhs_out]\n\t"
        "movq %%rbx, %[rhs_out]"
        : [lhs_out] "=r"(lhs_out), [rhs_out] "=r"(rhs_out)
        : [lhs] "r"(lhs), [rhs] "r"(rhs)
        : "rax", "rbx", "cc", "memory");
    return lhs_out ^ rhs_out;
}

int main(void)
{
    uint32_t low = xchg32_low(0x11223344, 0x55667788);
    uint64_t full = xchg32_full(0xdeadbeef11223344ULL,
                                0xfeedface55667788ULL);

    printf("xchg32=%08" PRIx32 ":%016" PRIx64 "\n",
           low, full);
    return low != 0x444444ccU || full != 0x00000000444444ccULL;
}
