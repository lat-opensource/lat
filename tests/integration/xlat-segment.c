/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * XLAT must honor the address-size override and the FS/GS bases.  The two
 * halves of the buffer hold different bytes so an ignored override or an
 * untruncated base produces a wrong AL instead of a crash.
 */
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long u64;

static struct {
    u8 a[256];
    u8 b[256];
} buf;

#ifndef __x86_64__
static u8 wide[0x10001];
#endif

#ifdef __x86_64__
static long syscall1(long n, long a)
{
    long ret;

    __asm__ volatile("syscall" : "=a"(ret) : "a"(n), "D"(a)
                     : "rcx", "r11", "memory");
    return ret;
}

static long syscall2(long n, long a, long b)
{
    long ret;

    __asm__ volatile("syscall" : "=a"(ret) : "a"(n), "D"(a), "S"(b)
                     : "rcx", "r11", "memory");
    return ret;
}

#define SYS_ARCH_PRCTL 158
#define SYS_EXIT 60
#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002

static int set_seg_base(int which, u64 base)
{
    return (int)syscall2(SYS_ARCH_PRCTL, which, (long)base);
}
#else
static long syscall1(long n, long a)
{
    long ret;

    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n), "b"(a)
                     : "memory");
    return ret;
}

#define SYS_SET_THREAD_AREA 243
#define SYS_EXIT 1

struct user_desc {
    u32 entry_number;
    u32 base_addr;
    u32 limit;
    u32 seg_32bit:1;
    u32 contents:2;
    u32 read_exec_only:1;
    u32 limit_in_pages:1;
    u32 seg_not_present:1;
    u32 useable:1;
};

static int set_fs_base(u32 base)
{
    struct user_desc ud = {0};
    u32 sel;
    long ret;

    ud.entry_number = (u32)-1;
    ud.base_addr = base;
    ud.limit = 0xfffff;
    ud.seg_32bit = 1;
    ud.limit_in_pages = 1;
    ud.useable = 1;

    ret = syscall1(SYS_SET_THREAD_AREA, (long)&ud);
    if (ret) {
        return (int)ret;
    }

    sel = (ud.entry_number << 3) | 3;
    __asm__ volatile("movw %0, %%fs" : : "r"(sel));
    return 0;
}
#endif

static u8 xlat_default(u64 base, u8 idx)
{
    u8 al = idx;

    __asm__ volatile("xlatb" : "+a"(al) : "b"(base) : "memory");
    return al;
}

static u8 xlat_ds(u64 base, u8 idx)
{
    u8 al = idx;

    __asm__ volatile("ds xlatb" : "+a"(al) : "b"(base) : "memory");
    return al;
}

static u8 xlat_fs(u64 base, u8 idx)
{
    u8 al = idx;

    __asm__ volatile("fs xlatb" : "+a"(al) : "b"(base) : "memory");
    return al;
}

#ifndef __x86_64__
static u8 xlat_fs_addr16(u64 base, u8 idx)
{
    u8 al = idx;

    __asm__ volatile("addr16 fs xlatb" : "+a"(al) : "b"(base) : "memory");
    return al;
}
#endif

#ifdef __x86_64__
static u8 xlat_gs(u64 base, u8 idx)
{
    u8 al = idx;

    __asm__ volatile("gs xlatb" : "+a"(al) : "b"(base) : "memory");
    return al;
}

static u8 xlat_addr32(u64 base, u8 idx)
{
    u8 al = idx;

    __asm__ volatile("addr32 xlatb" : "+a"(al) : "b"(base) : "memory");
    return al;
}

static u8 xlat_fs_addr32(u64 base, u8 idx)
{
    u8 al = idx;

    __asm__ volatile("addr32 fs xlatb" : "+a"(al) : "b"(base) : "memory");
    return al;
}
#endif

void _start(void)
{
    u64 pa = (u64)(unsigned long)buf.a;
    u64 pb = (u64)(unsigned long)buf.b;
    u64 delta = pb - pa;
    int fail = 0;
    int i;

    for (i = 0; i < 256; i++) {
        buf.a[i] = (u8)(i ^ 0xa5);
        buf.b[i] = (u8)(i ^ 0x5a);
    }

    for (i = 0; i < 256; i += 0x3f) {
        fail |= xlat_default(pa, (u8)i) != buf.a[i] ? 1 : 0;
        fail |= xlat_ds(pa, (u8)i) != buf.a[i] ? 2 : 0;
    }

#ifdef __x86_64__
    fail |= set_seg_base(ARCH_SET_FS, (long)delta) ? 4 : 0;
    fail |= set_seg_base(ARCH_SET_GS, (long)delta) ? 8 : 0;

    for (i = 0; i < 256; i += 0x3f) {
        fail |= xlat_fs(pa, (u8)i) != buf.b[i] ? 0x10 : 0;
        fail |= xlat_gs(pa, (u8)i) != buf.b[i] ? 0x20 : 0;
        fail |= xlat_addr32(0x100000000ULL + pa, (u8)i) != buf.a[i] ?
                0x40 : 0;
        fail |= xlat_fs_addr32(0x100000000ULL + pa, (u8)i) != buf.b[i] ?
                0x80 : 0;
    }
#else
    fail |= set_fs_base((u32)delta) ? 4 : 0;

    for (i = 0; i < 256; i += 0x3f) {
        fail |= xlat_fs(pa, (u8)i) != buf.b[i] ? 0x10 : 0;
    }

    /*
     * BX + AL wraps at 64K before the FS base is added.  wide[0x10000]
     * holds a different byte so a missing wrap reads the wrong value.
     */
    wide[0] = 0x42;
    wide[0x10000] = 0x99;
    fail |= set_fs_base((u32)(unsigned long)wide) ? 0x20 : 0;
    fail |= xlat_fs_addr16(0xffff, 1) != wide[0] ? 0x20 : 0;
#endif

    syscall1(SYS_EXIT, fail);
    for (;;) {
        __asm__ volatile("pause");
    }
}
