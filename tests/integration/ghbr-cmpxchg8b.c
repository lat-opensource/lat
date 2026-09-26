#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static __attribute__((noinline)) void cmpxchg8b_equal(
    uint64_t *memory, uint64_t *eax_out, uint64_t *edx_out)
{
    uint64_t eax = 0xdeadbeef11223344ULL;
    uint64_t edx = 0xfeedfacedeadbeefULL;
    uint64_t ebx = 0x1111111155667788ULL;
    uint64_t ecx = 0x2222222299aabbccULL;

    __asm__ volatile("cmpxchg8b %[memory]"
                     : "+a"(eax), "+d"(edx), [memory] "+m"(*memory)
                     : "b"(ebx), "c"(ecx)
                     : "cc");
    *eax_out = eax;
    *edx_out = edx;
}

static __attribute__((noinline)) void cmpxchg8b_unequal(
    uint64_t *memory, uint64_t *eax_out, uint64_t *edx_out)
{
    uint64_t eax = 0xdeadbeef11111111ULL;
    uint64_t edx = 0xfeedface22222222ULL;
    uint64_t ebx = 0x1111111155667788ULL;
    uint64_t ecx = 0x2222222299aabbccULL;

    __asm__ volatile("cmpxchg8b %[memory]"
                     : "+a"(eax), "+d"(edx), [memory] "+m"(*memory)
                     : "b"(ebx), "c"(ecx)
                     : "cc");
    *eax_out = eax;
    *edx_out = edx;
}

int main(void)
{
    uint64_t equal_memory = 0xdeadbeef11223344ULL;
    uint64_t unequal_memory = 0x3333333344444444ULL;
    uint64_t equal_eax;
    uint64_t equal_edx;
    uint64_t unequal_eax;
    uint64_t unequal_edx;

    cmpxchg8b_equal(&equal_memory, &equal_eax, &equal_edx);
    cmpxchg8b_unequal(&unequal_memory, &unequal_eax, &unequal_edx);
    printf("cmpxchg8b=%016" PRIx64 ":%016" PRIx64
           ":%016" PRIx64 ":%016" PRIx64
           ":%016" PRIx64 ":%016" PRIx64 "\n",
           equal_memory, equal_eax, equal_edx,
           unequal_memory, unequal_eax, unequal_edx);
    return equal_memory != 0x99aabbcc55667788ULL ||
           equal_eax != 0xdeadbeef11223344ULL ||
           equal_edx != 0xfeedfacedeadbeefULL ||
           unequal_memory != 0x3333333344444444ULL ||
           unequal_eax != 0x0000000044444444ULL ||
           unequal_edx != 0x0000000033333333ULL;
}
