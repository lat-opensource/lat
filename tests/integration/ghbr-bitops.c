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

static __attribute__((noinline)) uint64_t popcnt32_alias(uint64_t value)
{
    uint64_t result;

    __asm__ volatile(
        "movq %1, %%rax\n\t"
        "popcntl %%eax, %%eax\n\t"
        "movq %%rax, %0"
        : "=r"(result)
        : "r"(value)
        : "rax");
    return result;
}

static __attribute__((noinline)) uint64_t bsr32_alias(uint64_t value)
{
    uint64_t result;

    __asm__ volatile(
        "movq %1, %%rax\n\t"
        "bsrl %%eax, %%eax\n\t"
        "movq %%rax, %0"
        : "=r"(result)
        : "r"(value)
        : "rax", "cc");
    return result;
}

#define DEFINE_BITOP16(name, instruction)                                  \
    static __attribute__((noinline)) uint64_t name(uint64_t initial,        \
                                                    uint16_t value)         \
    {                                                                       \
        uint64_t result;                                                    \
                                                                            \
        __asm__ volatile(                                                   \
            "movq %1, %%rax\n\t"                                          \
            instruction " %w2, %%ax\n\t"                                  \
            "movq %%rax, %0"                                               \
            : "=r"(result)                                                 \
            : "r"(initial), "r"(value)                                    \
            : "rax", "cc");                                               \
        return result;                                                      \
    }

DEFINE_BITOP16(popcnt16_full, "popcntw")
DEFINE_BITOP16(tzcnt16_full, "tzcntw")
DEFINE_BITOP16(lzcnt16_full, "lzcntw")
DEFINE_BITOP16(bsf16_full, "bsfw")
DEFINE_BITOP16(bsr16_full, "bsrw")

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
    uint64_t popcnt_alias = popcnt32_alias(0xdeadbeef00001001ULL);
    uint64_t bsr_alias = bsr32_alias(0xfeedface00001000ULL);
    uint64_t initial = 0xdeadbeef89abcdefULL;
    uint64_t popcnt16 = popcnt16_full(initial, 0xf0f1);
    uint64_t tzcnt16 = tzcnt16_full(initial, 0x1000);
    uint64_t lzcnt16 = lzcnt16_full(initial, 0x1000);
    uint64_t bsf16 = bsf16_full(initial, 0x1000);
    uint64_t bsr16 = bsr16_full(initial, 0x1000);

    printf("bitops=%08" PRIx32 ":%u:%u:%u:%u:%u:%016" PRIx64
           ":%016" PRIx64 ":%016" PRIx64 "\n",
           bswap, popcnt, tzcnt, lzcnt, bsf, bsr, bswap_full,
           popcnt_alias, bsr_alias);
    printf("bitops16=%016" PRIx64 ":%016" PRIx64 ":%016" PRIx64
           ":%016" PRIx64 ":%016" PRIx64 "\n",
           popcnt16, tzcnt16, lzcnt16, bsf16, bsr16);

    return bswap != 0x44332211U || popcnt != 9 || tzcnt != 12 ||
           lzcnt != 19 || bsf != 12 || bsr != 12 ||
           bswap_full != 0x0000000044332211ULL ||
           popcnt_alias != 2 || bsr_alias != 12 ||
           popcnt16 != 0xdeadbeef89ab0009ULL ||
           tzcnt16 != 0xdeadbeef89ab000cULL ||
           lzcnt16 != 0xdeadbeef89ab0003ULL ||
           bsf16 != 0xdeadbeef89ab000cULL ||
           bsr16 != 0xdeadbeef89ab000cULL;
}
