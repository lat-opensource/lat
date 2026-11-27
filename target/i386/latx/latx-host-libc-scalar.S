/*
 * Keep LAT's host-side libc string and memory operations on baseline
 * implementations.  Some LSX-only hosts trap on unaligned vector memory,
 * while the vendor glibc IFUNC resolvers select LSX using only HWCAP_LSX.
 */

.text

.macro LATX_SCALAR_LIBC_ALIAS public, implementation
    .globl \public
    .type \public, @function
\public:
    b \implementation
    .size \public, .-\public
.endm

LATX_SCALAR_LIBC_ALIAS memcpy, __memcpy_aligned
LATX_SCALAR_LIBC_ALIAS memmove, __memmove_aligned
LATX_SCALAR_LIBC_ALIAS memset, __memset_aligned
LATX_SCALAR_LIBC_ALIAS memcmp, __memcmp_aligned
LATX_SCALAR_LIBC_ALIAS memchr, __memchr_aligned
LATX_SCALAR_LIBC_ALIAS rawmemchr, __rawmemchr_aligned
LATX_SCALAR_LIBC_ALIAS __rawmemchr, __rawmemchr_aligned
LATX_SCALAR_LIBC_ALIAS strlen, __strlen_aligned
LATX_SCALAR_LIBC_ALIAS strnlen, __strnlen_aligned
LATX_SCALAR_LIBC_ALIAS __strnlen, __strnlen_aligned
LATX_SCALAR_LIBC_ALIAS strcmp, __strcmp_aligned
LATX_SCALAR_LIBC_ALIAS strncmp, __strncmp_aligned
LATX_SCALAR_LIBC_ALIAS strchr, __strchr_aligned
LATX_SCALAR_LIBC_ALIAS strchrnul, __strchrnul_aligned
LATX_SCALAR_LIBC_ALIAS __strchrnul, __strchrnul_aligned
LATX_SCALAR_LIBC_ALIAS strrchr, __strrchr_aligned
LATX_SCALAR_LIBC_ALIAS strcpy, __strcpy_aligned
LATX_SCALAR_LIBC_ALIAS stpcpy, __stpcpy_aligned
LATX_SCALAR_LIBC_ALIAS __stpcpy, __stpcpy_aligned
