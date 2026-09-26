#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static __attribute__((noinline)) uint32_t lea32_low(uint64_t base,
                                                   uint32_t index)
{
    uint32_t result;

    __asm__ volatile("leal (%[base],%[index],4), %k[result]"
                     : [result] "=r"(result)
                     : [base] "r"(base), [index] "r"((uint64_t)index));
    return result;
}

static __attribute__((noinline)) uint64_t lea32_full(uint64_t base,
                                                    uint32_t index)
{
    uint64_t result;

    __asm__ volatile("leal (%[base],%[index],4), %k[result]"
                     : [result] "=r"(result)
                     : [base] "r"(base), [index] "r"((uint64_t)index));
    return result;
}

int main(void)
{
    uint32_t low = lea32_low(0xdeadbeef10000000ULL, 0x02000000);
    uint64_t full = lea32_full(0xdeadbeef10000000ULL, 0x02000000);

    printf("lea32=%08" PRIx32 ":%016" PRIx64 "\n",
           low, full);
    return low != 0x18000000U || full != 0x0000000018000000ULL;
}
