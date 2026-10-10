#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static __attribute__((noinline)) uint32_t bts32_low(uint32_t value,
                                                    uint32_t bit)
{
    uint32_t result;

    __asm__ volatile("btsl %2, %0"
                     : "=r"(result)
                     : "0"(value), "r"(bit)
                     : "cc");
    return result;
}

static __attribute__((noinline)) uint64_t btr32_full(uint32_t value,
                                                     uint32_t bit)
{
    uint64_t result;

    __asm__ volatile(
        "movl %1, %%eax\n\t"
        "btrl %2, %%eax\n\t"
        "movq %%rax, %0"
        : "=r"(result)
        : "r"(value), "r"(bit)
        : "rax", "cc");
    return result;
}

static __attribute__((noinline)) uint32_t btc32_low(uint32_t value,
                                                    uint32_t bit)
{
    uint32_t result;

    __asm__ volatile("btcl %2, %0"
                     : "=r"(result)
                     : "0"(value), "r"(bit)
                     : "cc");
    return result;
}

int main(void)
{
    uint32_t bts = bts32_low(0x00001000, 4);
    uint64_t btr = btr32_full(0x00001000, 12);
    uint32_t btc = btc32_low(0x00001000, 12);

    printf("btx=%08" PRIx32 ":%016" PRIx64 ":%08" PRIx32 "\n",
           bts, btr, btc);
    return bts != 0x00001010 || btr != 0 || btc != 0;
}
