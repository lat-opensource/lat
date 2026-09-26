#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static __attribute__((noinline)) uint32_t bswap32(uint32_t value)
{
    __asm__ volatile("bswapl %0" : "+r"(value));
    return value;
}

static __attribute__((noinline)) uint32_t popcnt32(uint32_t value)
{
    uint32_t result;

    __asm__ volatile("popcntl %1, %0" : "=r"(result) : "r"(value));
    return result;
}

static __attribute__((noinline)) uint32_t tzcnt32(uint32_t value)
{
    uint32_t result;

    __asm__ volatile("tzcntl %1, %0" : "=r"(result) : "r"(value));
    return result;
}

static __attribute__((noinline)) uint32_t lzcnt32(uint32_t value)
{
    uint32_t result;

    __asm__ volatile("lzcntl %1, %0" : "=r"(result) : "r"(value));
    return result;
}

static __attribute__((noinline)) uint32_t bsf32(uint32_t value)
{
    uint32_t result;

    __asm__ volatile("bsfl %1, %0" : "=r"(result) : "r"(value));
    return result;
}

static __attribute__((noinline)) uint32_t bsr32(uint32_t value)
{
    uint32_t result;

    __asm__ volatile("bsrl %1, %0" : "=r"(result) : "r"(value));
    return result;
}

static __attribute__((noinline)) uint64_t bswap32_full(uint32_t value)
{
    uint64_t result;

    __asm__ volatile(
        "movl %1, %%eax\n\t"
        "bswapl %%eax\n\t"
        "movq %%rax, %0"
        : "=r"(result)
        : "r"(value)
        : "rax");
    return result;
}

int main(void)
{
    uint32_t value = 0x00001000;
    uint32_t bswap = bswap32(0x11223344);
    uint32_t popcnt = popcnt32(0xf0f00001);
    uint32_t tzcnt = tzcnt32(value);
    uint32_t lzcnt = lzcnt32(value);
    uint32_t bsf = bsf32(value);
    uint32_t bsr = bsr32(value);
    uint64_t bswap_full = bswap32_full(0x11223344);

    printf("bitops=%08" PRIx32 ":%u:%u:%u:%u:%u:%016" PRIx64 "\n",
           bswap, popcnt, tzcnt, lzcnt, bsf, bsr, bswap_full);
    return bswap != 0x44332211U || popcnt != 9 || tzcnt != 12 ||
           lzcnt != 19 || bsf != 12 || bsr != 12 ||
           bswap_full != 0x0000000044332211ULL;
}
