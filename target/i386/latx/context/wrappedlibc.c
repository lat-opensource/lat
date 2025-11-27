#include "config-host.h"
#define _LARGEFILE_SOURCE 1
#define _FILE_OFFSET_BITS 64
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include <dlfcn.h>
#include <signal.h>
#include <errno.h>
#include <err.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <glob.h>
#include <ctype.h>
#include <dirent.h>
#include <search.h>
#include <sys/types.h>
#include <poll.h>
#include <sys/epoll.h>
#include <ftw.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <setjmp.h>
#include <sys/vfs.h>
#include <spawn.h>
#include <fts.h>
#include <syslog.h>
#include <malloc.h>
#include <getopt.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#undef LOG_INFO
#undef LOG_DEBUG

#include "wrappedlibs.h"

#include "debug.h"
#include "wrapper.h"
#include "bridge.h"
#include "callback.h"
#include "librarian.h"
#include "library_private.h"
#include "box64context.h"
#include "myalign.h"
#include "fileutils.h"
#include "elf.h"
#include "elfloader.h"
#include "elfloader_private.h"
#include "bridge.h"
#include "globalsymbols.h"

#define LIBNAME libc
const char* libcName = 
#ifdef ANDROID
    "libc.so"
#else
    "libc.so.6"
#endif
    ;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
void* x86free;
void* x86realloc;
void* x86pthread_setcanceltype;
typedef int (*iFi_t)(int);
typedef int (*iFp_t)(void*);
typedef int (*iFL_t)(unsigned long);
typedef void (*vFpp_t)(void*, void*);
typedef void (*vFipp_t)(int32_t, void*, void*);
typedef int32_t (*iFpi_t)(void*, int32_t);
typedef int32_t (*iFpp_t)(void*, void*);
typedef int32_t (*iFpL_t)(void*, size_t);
typedef int32_t (*iFiip_t)(int32_t, int32_t, void*);
typedef int32_t (*iFipp_t)(int32_t, void*, void*);
typedef int32_t (*iFppi_t)(void*, void*, int32_t);
typedef int32_t (*iFpup_t)(void*, uint32_t, void*);
typedef int32_t (*iFpuu_t)(void*, uint32_t, uint32_t);
typedef int32_t (*iFiiII_t)(int, int, int64_t, int64_t);
typedef int32_t (*iFiiiV_t)(int, int, int, ...);
typedef int32_t (*iFippi_t)(int32_t, void*, void*, int32_t);
typedef int32_t (*iFpppp_t)(void*, void*, void*, void*);
typedef int32_t (*iFpipp_t)(void*, int32_t, void*, void*);
typedef int32_t (*iFppii_t)(void*, void*, int32_t, int32_t);
typedef int32_t (*iFipuu_t)(int32_t, void*, uint32_t, uint32_t);
typedef int32_t (*iFipiI_t)(int32_t, void*, int32_t, int64_t);
typedef int32_t (*iFipuup_t)(int32_t, void*, uint32_t, uint32_t, void*);
typedef int32_t (*iFiiV_t)(int32_t, int32_t, ...);
typedef void* (*pFp_t)(void*);
typedef void* (*pFpip_t)(void*, int, void*);

#define SUPER() \
    GO(_ITM_addUserCommitAction, iFpup_t)   \
    GO(_IO_file_stat, iFpp_t)               \
    GO(fts64_open, pFpip_t)                 \
    GO(register_printf_specifier, iFipp_t)  \
    GO(register_printf_type, iFp_t)


#include "wrappercallback.h"
// utility functions
#define SUPER() \
GO(0)   \
GO(1)   \
GO(2)   \
GO(3)   \
GO(4)   \
GO(5)   \
GO(6)   \
GO(7)   \
GO(8)   \
GO(9)   \
GO(10)  \
GO(11)  \
GO(12)  \
GO(13)  \
GO(14)  \
GO(15)

// compare
#define GO(A)   \
static uintptr_t my_compare_fct_##A = 0;                                    \
static int my_compare_##A(void* a, void* b)                                 \
{                                                                           \
    return (int)RunFunctionWithState(my_compare_fct_##A, 2, a, b);       \
}
SUPER()
#undef GO
static void* findcompareFct(void* fct)
{
    if(!fct) return NULL;
    void* p;
    if((p = GetNativeFnc((uintptr_t)fct))) return p;
    #define GO(A) if(my_compare_fct_##A == (uintptr_t)fct) return my_compare_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_compare_fct_##A == 0) {my_compare_fct_##A = (uintptr_t)fct; return my_compare_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libc compare callback\n");
    return NULL;
}

// ftw64
#define GO(A)   \
static uintptr_t my_ftw64_fct_##A = 0;                      \
static int my_ftw64_##A(void* fpath, void* sb, int flag)    \
{                                                           \
    struct x64_stat64 x64st;                                \
    UnalignStat64(sb, &x64st);                              \
    return (int)RunFunctionWithState(my_ftw64_fct_##A, 3, fpath, &x64st, flag);         \
}
SUPER()
#undef GO
static void* findftw64Fct(void* fct)
{
    if(!fct) return NULL;
    #define GO(A) if(my_ftw64_fct_##A == (uintptr_t)fct) return my_ftw64_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_ftw64_fct_##A == 0) {my_ftw64_fct_##A = (uintptr_t)fct; return my_ftw64_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libc ftw64 callback\n");
    return NULL;
}

// nftw64
#define GO(A)   \
static uintptr_t my_nftw64_fct_##A = 0;                                     \
static int my_nftw64_##A(void* fpath, void* sb, int flag, void* ftwbuff)    \
{                                                                           \
    struct x64_stat64 x64st;                                                \
    UnalignStat64(sb, &x64st);                                              \
    return (int)RunFunctionWithState(my_nftw64_fct_##A, 4, fpath, &x64st, flag, ftwbuff);          \
}
SUPER()
#undef GO
static void* findnftw64Fct(void* fct)
{
    if(!fct) return NULL;
    #define GO(A) if(my_nftw64_fct_##A == (uintptr_t)fct) return my_nftw64_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_nftw64_fct_##A == 0) {my_nftw64_fct_##A = (uintptr_t)fct; return my_nftw64_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libc nftw64 callback\n");
    return NULL;
}
// globerr
#define GO(A)   \
static uintptr_t my_globerr_fct_##A = 0;                                                \
static int my_globerr_##A(void* epath, int eerrno)                                      \
{                                                                                       \
    return (int)RunFunctionWithState(my_globerr_fct_##A, 2, epath, eerrno);          \
}
SUPER()
#undef GO
static void* findgloberrFct(void* fct)
{
    if(!fct) return NULL;
    void* p;
    if((p = GetNativeFnc((uintptr_t)fct))) return p;
    #define GO(A) if(my_globerr_fct_##A == (uintptr_t)fct) return my_globerr_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_globerr_fct_##A == 0) {my_globerr_fct_##A = (uintptr_t)fct; return my_globerr_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libc globerr callback\n");
    return NULL;
}
// free
#define GO(A)   \
static uintptr_t my_free_fct_##A = 0;                       \
static void my_free_##A(void* p)                            \
{                                                           \
    RunFunctionWithState(my_free_fct_##A, 1, p);          \
}
SUPER()
#undef GO
static void* findfreeFct(void* fct)
{
    if(!fct) return NULL;
    void* p;
    if((p = GetNativeFnc((uintptr_t)fct))) return p;
    #define GO(A) if(my_free_fct_##A == (uintptr_t)fct) return my_free_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_free_fct_##A == 0) {my_free_fct_##A = (uintptr_t)fct; return my_free_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libc free callback\n");
    return NULL;
}

#if 0
#undef dirent
// filter_dir
#define GO(A)   \
static uintptr_t my_filter_dir_fct_##A = 0;                                 \
static int my_filter_dir_##A(const struct dirent* a)                        \
{                                                                           \
    return (int)RunFunctionWithState(my_filter_dir_fct_##A, 1, a);        \
}
SUPER()
#undef GO
static void* findfilter_dirFct(void* fct)
{
    if(!fct) return NULL;
    void* p;
    if((p = GetNativeFnc((uintptr_t)fct))) return p;
    #define GO(A) if(my_filter_dir_fct_##A == (uintptr_t)fct) return my_filter_dir_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_filter_dir_fct_##A == 0) {my_filter_dir_fct_##A = (uintptr_t)fct; return my_filter_dir_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libc filter_dir callback\n");
    return NULL;
}
// compare_dir
#define GO(A)   \
static uintptr_t my_compare_dir_fct_##A = 0;                                    \
static int my_compare_dir_##A(const struct dirent* a, const struct dirent* b)   \
{                                                                               \
    return (int)RunFunctionWithState(my_compare_dir_fct_##A, 2, a, b);       \
}
SUPER()
#undef GO
static void* findcompare_dirFct(void* fct)
{
    if(!fct) return NULL;
    void* p;
    if((p = GetNativeFnc((uintptr_t)fct))) return p;
    #define GO(A) if(my_compare_dir_fct_##A == (uintptr_t)fct) return my_compare_dir_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_compare_dir_fct_##A == 0) {my_compare_dir_fct_##A = (uintptr_t)fct; return my_compare_dir_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libc compare_dir callback\n");
    return NULL;
}
#endif

// filter64
#define GO(A)   \
static uintptr_t my_filter64_fct_##A = 0;                                   \
static int my_filter64_##A(const struct dirent64* a)                        \
{                                                                           \
    return (int)RunFunctionWithState(my_filter64_fct_##A, 1, a);          \
}
SUPER()
#undef GO
static void* findfilter64Fct(void* fct)
{
    if(!fct) return NULL;
    void* p;
    if((p = GetNativeFnc((uintptr_t)fct))) return p;
    #define GO(A) if(my_filter64_fct_##A == (uintptr_t)fct) return my_filter64_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_filter64_fct_##A == 0) {my_filter64_fct_##A = (uintptr_t)fct; return my_filter64_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libc filter64 callback\n");
    return NULL;
}
// compare64
#define GO(A)   \
static uintptr_t my_compare64_fct_##A = 0;                                      \
static int my_compare64_##A(const struct dirent64* a, const struct dirent64* b) \
{                                                                               \
    return (int)RunFunctionWithState(my_compare64_fct_##A, 2, a, b);         \
}
SUPER()
#undef GO
static void* findcompare64Fct(void* fct)
{
    if(!fct) return NULL;
    void* p;
    if((p = GetNativeFnc((uintptr_t)fct))) return p;
    #define GO(A) if(my_compare64_fct_##A == (uintptr_t)fct) return my_compare64_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_compare64_fct_##A == 0) {my_compare64_fct_##A = (uintptr_t)fct; return my_compare64_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libc compare64 callback\n");
    return NULL;
}
// printf_output
#define GO(A)   \
static uintptr_t my_printf_output_fct_##A = 0;                                          \
static int my_printf_output_##A(void* a, void* b, void* c)                              \
{                                                                                       \
    return (int)RunFunctionWithState(my_printf_output_fct_##A, 3, a, b, c);         \
}
SUPER()
#undef GO
static void* findprintf_outputFct(void* fct)
{
    if(!fct) return NULL;
    void* p;
    if((p = GetNativeFnc((uintptr_t)fct))) return p;
    #define GO(A) if(my_printf_output_fct_##A == (uintptr_t)fct) return my_printf_output_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_printf_output_fct_##A == 0) {my_printf_output_fct_##A = (uintptr_t)fct; return my_printf_output_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libc printf_output callback\n");
    return NULL;
}
// printf_arginfo
#define GO(A)   \
static uintptr_t my_printf_arginfo_fct_##A = 0;                                             \
static int my_printf_arginfo_##A(void* a, size_t b, void* c, void* d)                       \
{                                                                                           \
    return (int)RunFunctionWithState(my_printf_arginfo_fct_##A, 4, a, b, c, d);        \
}
SUPER()
#undef GO
static void* findprintf_arginfoFct(void* fct)
{
    if(!fct) return NULL;
    void* p;
    if((p = GetNativeFnc((uintptr_t)fct))) return p;
    #define GO(A) if(my_printf_arginfo_fct_##A == (uintptr_t)fct) return my_printf_arginfo_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_printf_arginfo_fct_##A == 0) {my_printf_arginfo_fct_##A = (uintptr_t)fct; return my_printf_arginfo_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libc printf_arginfo callback\n");
    return NULL;
}
// printf_type
#define GO(A)   \
static uintptr_t my_printf_type_fct_##A = 0;                        \
static void my_printf_type_##A(void* a, va_list* b)                 \
{                                                                   \
    RunFunctionWithState(my_printf_type_fct_##A, 2, a, b);       \
}
SUPER()
#undef GO
static void* findprintf_typeFct(void* fct)
{
    if(!fct) return NULL;
    void* p;
    if((p = GetNativeFnc((uintptr_t)fct))) return p;
    #define GO(A) if(my_printf_type_fct_##A == (uintptr_t)fct) return my_printf_type_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_printf_type_fct_##A == 0) {my_printf_type_fct_##A = (uintptr_t)fct; return my_printf_type_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libc printf_type callback\n");
    return NULL;
}

#undef SUPER

// some my_XXX declare and defines
int32_t my___libc_start_main(int *(main) (int, char * *, char * *),
    int argc, char * * ubp_av, void (*init) (void), void (*fini) (void),
    void (*rtld_fini) (void), void (* stack_end)); // implemented in x64run_private.c
EXPORT void my___libc_init_first(int argc, char* arg0, char** b)
{
    // do nothing specific for now
    (void)argc; (void)arg0; (void)b;
    return;
}
#if 0
void EXPORT my___stack_chk_fail(void)
{
    char buff[200];
    #ifdef HAVE_TRACE
    sprintf(buff, "%p: Stack is corrupted, aborting (prev IP=%p)\n", (void*)emu->old_ip, (void*)emu->prev2_ip);
    #else
    sprintf(buff, "%p: Stack is corrupted, aborting\n", (void*)emu->old_ip);
    #endif
    if(cycle_log) {
        print_cycle_log(LOG_INFO);
    }
    StopEmu(buff, emu->segs[_CS]==0x23);
}
#endif
void EXPORT my___gmon_start__(void)
{
    printf_log(LOG_DEBUG, "__gmon_start__ called (dummy call)\n");
}

#if 0
int EXPORT my___cxa_atexit(void* p, void* a, void* dso_handle)
{
    AddCleanup1Arg(p, a, dso_handle);
    return 0;
}
void EXPORT my___cxa_finalize(void* p)
{
    if(!p) {
        // p is null, call (and remove) all Cleanup functions
        CallAllCleanup();
        return;
    }
    CallCleanup(p);
}
int EXPORT my_atexit(void *p)
{
    AddCleanup(p, NULL);   // should grab current dso_handle?
    return 0;
}
#endif

int my_getcontext(void* ucp);
int my_setcontext(void* ucp);
int my_makecontext(void* ucp, void* fnc, int32_t argc, void* argv);
int my_swapcontext(void* ucp1, void* ucp2);

// All signal and context functions defined in signals.c

// All fts function defined in myfts.c

// getauxval implemented in auxval.c


// this one is defined in elfloader.c
int my_dl_iterate_phdr(void* F, void *data);
#if 0
pid_t EXPORT my_fork(void)
{
    #if 1
    emu->quit = 1;
    emu->fork = 3;  // use regular fork...
    return 0;
    #else
    // execute atforks prepare functions, in reverse order
    for (int i=my_context->atfork_sz-1; i>=0; --i)
        if(my_context->atforks[i].prepare)
            RunFunctionWithState(my_context->atforks[i].prepare, 0);
    int type = emu->type;
    pid_t v;
    v = fork();
    if(type == EMUTYPE_MAIN)
        thread_set_emu();
    if(v<0) {
        printf_log(LOG_NONE, "BOX64: Warning, fork errored... (%d)\n", v);
        // error...
    } else if(v>0) {
        // execute atforks parent functions
        for (int i=0; i<my_context->atfork_sz; --i)
            if(my_context->atforks[i].parent)
                RunFunctionWithState(my_context->atforks[i].parent, 0);

    } else /*if(v==0)*/ {
        // execute atforks child functions
        for (int i=0; i<my_context->atfork_sz; --i)
            if(my_context->atforks[i].child)
                RunFunctionWithState(my_context->atforks[i].child, 0);
    }
    return v;
    #endif
}
pid_t EXPORT my___fork(void) __attribute__((alias("my_fork")));
pid_t EXPORT my_vfork(void)
{
    #if 1
    emu->quit = 1;
    emu->fork = 3;  // use regular fork...
    return 0;
    #else
    return 0;
    #endif
}
#endif
int EXPORT my_uname(struct utsname *buf)
{
    //TODO: check sizeof(struct utsname) == 390
    int ret = uname(buf);
    strcpy(buf->machine, "x86_64");
    return ret;
}

// X86_O_RDONLY 0x00
#define X86_O_WRONLY       0x01     // octal     01
#define X86_O_RDWR         0x02     // octal     02
#define X86_O_CREAT        0x40     // octal     0100
#define X86_O_EXCL         0x80     // octal     0200
#define X86_O_NOCTTY       0x100    // octal     0400
#define X86_O_TRUNC        0x200    // octal    01000
#define X86_O_APPEND       0x400    // octal    02000
#define X86_O_NONBLOCK     0x800    // octal    04000
#define X86_O_SYNC         0x101000 // octal 04010000
#define X86_O_DSYNC        0x1000   // octal   010000
#define X86_O_RSYNC        O_SYNC
#define X86_FASYNC         020000
#define X86_O_DIRECT       040000
#define X86_O_LARGEFILE    0100000
#define X86_O_DIRECTORY    0200000
#define X86_O_NOFOLLOW     0400000
#define X86_O_NOATIME      01000000
#define X86_O_CLOEXEC      02000000
#define X86_O_PATH         010000000
#define X86_O_TMPFILE      020200000

#ifndef O_TMPFILE
#define O_TMPFILE (020000000 | O_DIRECTORY)
#endif

#define SUPER()     \
    GO(O_WRONLY)    \
    GO(O_RDWR)      \
    GO(O_CREAT)     \
    GO(O_EXCL)      \
    GO(O_NOCTTY)    \
    GO(O_TRUNC)     \
    GO(O_APPEND)    \
    GO(O_NONBLOCK)  \
    GO(O_SYNC)      \
    GO(O_DSYNC)     \
    GO(O_RSYNC)     \
    GO(FASYNC)      \
    GO(O_DIRECT)    \
    GO(O_LARGEFILE) \
    GO(O_TMPFILE)   \
    GO(O_DIRECTORY) \
    GO(O_NOFOLLOW)  \
    GO(O_NOATIME)   \
    GO(O_CLOEXEC)   \
    GO(O_PATH)      \

// x86->arm
int of_convert(int a)
{
    if(!a || a==-1) return a;
    int b=0;
    #define GO(A) if((a&(X86_##A))==(X86_##A)) {a&=~(X86_##A); b|=(A);}
    SUPER();
    #undef GO
    if(a) {
        printf_log(LOG_NONE, "Warning, of_convert(...) left over 0x%x, converted 0x%x\n", a, b);
    }
    return a|b;
}

// arm->x86
int of_unconvert(int a)
{
    if(!a || a==-1) return a;
    int b=0;
    #define GO(A) if((a&(A))==(A)) {a&=~(A); b|=(X86_##A);}
    SUPER();
    #undef GO
    // flags 0x20000 unknown?!
    if(a && (a&~0x20000)) {
        printf_log(LOG_NONE, "Warning, of_unconvert(...) left over 0x%x, converted 0x%x\n", a, b);
    }
    return a|b;
}
#undef SUPER

EXPORT void* my__ZGTtnaX (size_t a) { (void)a; printf("warning _ZGTtnaX called\n"); return NULL; }
EXPORT void my__ZGTtdlPv (void* a) { (void)a; printf("warning _ZGTtdlPv called\n"); }
EXPORT uint8_t my__ITM_RU1(const uint8_t * a) { (void)a; printf("warning _ITM_RU1 called\n"); return 0; }
EXPORT uint32_t my__ITM_RU4(const uint32_t * a) { (void)a; printf("warning _ITM_RU4 called\n"); return 0; }
EXPORT uint64_t my__ITM_RU8(const uint64_t * a) { (void)a; printf("warning _ITM_RU8 called\n"); return 0; }
EXPORT void my__ITM_memcpyRtWn(void * a, const void * b, size_t c) { (void)a; (void)b; (void)c; printf("warning _ITM_memcpyRtWn called\n"); }
EXPORT void my__ITM_memcpyRnWt(void * a, const void * b, size_t c) { (void)a; (void)b; (void)c; printf("warning _ITM_memcpyRnWt called\n"); }

#if 0
EXPORT void my_longjmp(/*struct __jmp_buf_tag __env[1]*/void *p, int32_t __val);
EXPORT void my__longjmp(/*struct __jmp_buf_tag __env[1]*/void *p, int32_t __val) __attribute__((alias("my_longjmp")));
EXPORT void my_siglongjmp(/*struct __jmp_buf_tag __env[1]*/void *p, int32_t __val) __attribute__((alias("my_longjmp")));
EXPORT void my___longjmp_chk(/*struct __jmp_buf_tag __env[1]*/void *p, int32_t __val) __attribute__((alias("my_longjmp")));
#endif

//EXPORT int32_t my_setjmp(/*struct __jmp_buf_tag __env[1]*/void *p);
//EXPORT int32_t my__setjmp(/*struct __jmp_buf_tag __env[1]*/void *p) __attribute__((alias("my_setjmp")));
//EXPORT int32_t my___sigsetjmp(/*struct __jmp_buf_tag __env[1]*/void *p) __attribute__((alias("my_setjmp")));

EXPORT int my_printf(void* fmt, void* b) {
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 1);
    PREPARE_VALIST;
    return vprintf((const char*)fmt, VARARGS);
}
EXPORT int my___printf_chk(int chk, void* fmt, void* b)
{
    (void)chk;
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 2);
    PREPARE_VALIST;
    return vprintf((const char*)fmt, VARARGS);
}
EXPORT int my_wprintf(void* fmt, void* b) {
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlignW((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 1);
    PREPARE_VALIST;
    return vwprintf((const wchar_t*)fmt, VARARGS);
}
EXPORT int my___wprintf_chk(int chk, void* fmt, void* b)
{
    (void)chk;
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlignW((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 2);
    PREPARE_VALIST;
    return vwprintf((const wchar_t*)fmt, VARARGS);
}

EXPORT int my_vprintf(void* fmt, x64_va_list_t b) {
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    return vprintf(fmt, VARARGS);
}
EXPORT int my___vprintf_chk(void* fmt, x64_va_list_t b) __attribute__((alias("my_vprintf")));

EXPORT int my_vfprintf(void* F, void* fmt, x64_va_list_t b) {
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    return vfprintf(F, fmt, VARARGS);
}
EXPORT int my___vfprintf_chk(void* F, void* fmt, x64_va_list_t b) __attribute__((alias("my_vfprintf")));
EXPORT int my__IO_vfprintf(void* F, void* fmt, x64_va_list_t b) __attribute__((alias("my_vfprintf")));

EXPORT int my_fprintf(void* F, void* fmt, void* b)  {
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 2);
    PREPARE_VALIST;
    return vfprintf(F, fmt, VARARGS);
}
EXPORT int my___fprintf_chk(void* F, int flag, void* fmt, void* b)  {
    (void)flag;
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 3);
    PREPARE_VALIST;
    return vfprintf(F, fmt, VARARGS);
}

EXPORT int my_vwprintf(void* fmt, x64_va_list_t b) {
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignWValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    int r = vwprintf(fmt, VARARGS);
    return r;
}

EXPORT int my_fwprintf(void* F, void* fmt, void* b)  {
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlignW((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 2);
    PREPARE_VALIST;
    return vfwprintf(F, fmt, VARARGS);
}

EXPORT int my___fwprintf_chk(void* F, int flag, void* fmt, void* b) {
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlignW((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 3);
    PREPARE_VALIST;
    return vfwprintf(F, fmt, VARARGS);
}

EXPORT int my_vfwprintf(void* F, void* fmt, x64_va_list_t  b) {
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignWValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    return vfwprintf(F, fmt, VARARGS);
}
EXPORT int my___vfwprintf_chk(void* F, int flag, void* fmt, x64_va_list_t b)  {
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignWValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    return vfwprintf(F, fmt, VARARGS);
}

EXPORT int my_dprintf(int d, void* fmt, void* b) {
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 2);
    PREPARE_VALIST;
    return vdprintf(d, fmt, VARARGS);
}

EXPORT int my___dprintf_chk(int d, int flag, void* fmt, void* b)  {
    (void)flag;
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 3);
    PREPARE_VALIST;
    return vdprintf(d, fmt, VARARGS);
}


EXPORT int my_vdprintf(int d, void* fmt, x64_va_list_t b) {
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    return vdprintf(d, fmt, VARARGS);
}

EXPORT int my___vdprintf_chk(int d, int flag, void* fmt, x64_va_list_t b)  {
    (void)flag;
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    return vdprintf(d, fmt, VARARGS);
}

#if 0
EXPORT void *my_div(void *result, int numerator, int denominator) {
    *(div_t *)result = div(numerator, denominator);
    return result;
}
#endif

EXPORT int my_snprintf(void* buff, size_t s, void * fmt, uint64_t * b) {
    #ifdef PREFER_CONVERT_VAARG
    char scratch_buff [26*8] = {0};
    CREATE_VALIST_FROM_VAARG(b, (uint64_t *)scratch_buff, 3);
    #else
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 3);
    PREPARE_VALIST;
    #endif
    int r = vsnprintf(buff, s, fmt, VARARGS);
    return r;
}
EXPORT int my___snprintf(void* buff, size_t s, void * fmt, uint64_t * b) __attribute__((alias("my_snprintf")));
EXPORT int my___snprintf_chk(void* buff, size_t s, int flags, size_t maxlen, void * fmt, uint64_t * b)
{
    (void)flags; (void)maxlen;
    #ifdef PREFER_CONVERT_VAARG
    char scratch_buff [26*8] = {0};
    CREATE_VALIST_FROM_VAARG(b, (uint64_t *)scratch_buff, 5);
    #else
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 5);
    PREPARE_VALIST;
    #endif
    int r = vsnprintf(buff, s, fmt, VARARGS);
    return r;
}

EXPORT int my_sprintf(void* buff, void * fmt, void * b) {
    #ifdef PREFER_CONVERT_VAARG
    char scratch_buff [26*8] = {0};
    CREATE_VALIST_FROM_VAARG(b, (uint64_t *)scratch_buff, 2);
    #else
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 2);
    PREPARE_VALIST;
    #endif
    return vsprintf(buff, (const char*)fmt, VARARGS);
}
EXPORT int my___sprintf_chk(void* buff, int flag, size_t l, void * fmt, void * b) {
    (void)flag; (void)l;
    #ifdef PREFER_CONVERT_VAARG
    char scratch_buff [26*8] = {0};
    CREATE_VALIST_FROM_VAARG(b, (uint64_t *)scratch_buff, 4);
    #else
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 4);
    PREPARE_VALIST;
    #endif
    return vsprintf(buff, (const char*)fmt, VARARGS);
}

EXPORT int my_asprintf(void** buff, void * fmt, uint64_t * b) {
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 2);
    PREPARE_VALIST;
    return vasprintf((char**)buff, (char*)fmt, VARARGS);
}
EXPORT int my___asprintf(void** buff, void * fmt, uint64_t * b) __attribute__((alias("my_asprintf")));

EXPORT int my_vasprintf(char** buff, void* fmt, x64_va_list_t b) {
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    return vasprintf(buff, fmt, VARARGS);
}

EXPORT int my_vsprintf(void* buff,  void * fmt, x64_va_list_t b) {
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    return vsprintf(buff, fmt, VARARGS);
}
EXPORT int my___vsprintf_chk(void* buff, void * fmt, x64_va_list_t b) __attribute__((alias("my_vsprintf")));

EXPORT int my_vfscanf(void* stream, void* fmt, x64_va_list_t b)
{
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignScanfValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    return vfscanf(stream, fmt, VARARGS);
}

EXPORT int my_vsscanf(void* stream, void* fmt, x64_va_list_t b)
{
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignScanfValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    return vsscanf(stream, fmt, VARARGS);
}

EXPORT int my__vsscanf(void* stream, void* fmt, void* b) __attribute__((alias("my_vsscanf")));

EXPORT int my_vswscanf(void* stream, void* fmt, x64_va_list_t b)
{
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignScanfWValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    return vswscanf(stream, fmt, VARARGS);
}

EXPORT int my_sscanf(void* stream, void* fmt, uint64_t* b)
{
    char scratch_buff [26*8] = {0};
    myStackAlignScanf((const char*)fmt, b, (uint64_t *)scratch_buff, 2);
    __MY_CPU;
    PREPARE_VALIST;

    return vsscanf(stream, fmt, VARARGS);
}
EXPORT int my__IO_vfscanf(void* stream, void* fmt, void* b) __attribute__((alias("my_vfscanf")));
EXPORT int my___isoc99_vsscanf(void* stream, void* fmt, void* b) __attribute__((alias("my_vsscanf")));
EXPORT int my___isoc99_vswscanf(void* stream, void* fmt, void* b) __attribute__((alias("my_vswscanf")));
EXPORT int my___isoc99_vfscanf(void* stream, void* fmt, void* b) __attribute__((alias("my_vfscanf")));

EXPORT int my___isoc99_fscanf(void* stream, void* fmt, uint64_t* b)
{
  char scratch_buff [26*8] = {0};
  myStackAlignScanf((const char*)fmt, b, (uint64_t *)scratch_buff, 2);
  __MY_CPU;
  PREPARE_VALIST;

  return vfscanf(stream, fmt, VARARGS);
}
EXPORT int my_fscanf(void* stream, void* fmt, uint64_t* b) __attribute__((alias("my___isoc99_fscanf")));

EXPORT int my___isoc99_scanf(void* fmt, uint64_t* b)
{
  char scratch_buff [26*8] = {0};
  myStackAlignScanf((const char*)fmt, b, (uint64_t *)scratch_buff, 1);
  __MY_CPU;
  PREPARE_VALIST;

  return vscanf(fmt, VARARGS);
}

EXPORT int my___isoc99_sscanf(void* stream, void* fmt, uint64_t* b)
{
  char scratch_buff [26*8] = {0};
  myStackAlignScanf((const char*)fmt, b, (uint64_t *)scratch_buff, 2);
  __MY_CPU;
  PREPARE_VALIST;

  return vsscanf(stream, fmt, VARARGS);
}

EXPORT int my___isoc99_swscanf(void* stream, void* fmt, uint64_t* b)
{
  char scratch_buff [26*8] = {0};
  myStackAlignScanf((const char*)fmt, b, (uint64_t *)scratch_buff, 2);
  __MY_CPU;
  PREPARE_VALIST;

  return vswscanf(stream, fmt, VARARGS);
}

EXPORT int my_vsnprintf(void* buff, size_t s, void * fmt, x64_va_list_t b) {
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    int r = vsnprintf(buff, s, fmt, VARARGS);
    return r;
}
EXPORT int my___vsnprintf(void* buff, size_t s, void * fmt, x64_va_list_t b) __attribute__((alias("my_vsnprintf")));
EXPORT int my___vsnprintf_chk(void* buff, size_t s, int flags, size_t slen, void * fmt, x64_va_list_t b) {
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    int r = vsnprintf(buff, s, fmt, VARARGS);
    return r;
}
#if 0
EXPORT int my_vasprintf(void* strp, void* fmt, void* b, va_list V)
{
    #ifndef NOALIGN
    // need to align on arm
    char scratch_buff [26*8] = {0};
    myStackAlign((const char*)fmt, (uint32_t*)b, scratch_buff);
    __MY_CPU;
    PREPARE_VALIST;
    void* f = vasprintf;
    int r = ((iFppp_t)f)(strp, fmt, VARARGS);
    return r;
    #else
    void* f = vasprintf;
    int r = ((iFppp_t)f)(strp, fmt, (uint32_t*)b);
    return r;
    #endif
}
#endif
EXPORT int my___vasprintf_chk(void* buff, int flags, void* fmt, x64_va_list_t b)
{
    (void)flags;
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    int r = vasprintf(buff, fmt, VARARGS);
    return r;
}
EXPORT int my___asprintf_chk(void* result_ptr, int flags, void* fmt, void* b)
{
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 3);
    PREPARE_VALIST;
    return vasprintf((char**)result_ptr, (char*)fmt, VARARGS);
}
EXPORT int my_vswprintf(void* buff, size_t s, void * fmt, x64_va_list_t b) {
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignWValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    int r = vswprintf(buff, s, fmt, VARARGS);
    return r;
}
EXPORT int my___vswprintf(void* buff, size_t s, void * fmt, x64_va_list_t b) __attribute__((alias("my_vswprintf")));
EXPORT int my___vswprintf_chk(void* buff, size_t s, void * fmt, x64_va_list_t b) __attribute__((alias("my_vswprintf")));

EXPORT int my_swscanf(void* stream, void* fmt, uint64_t* b)
{
    char scratch_buff [26*8] = {0};
    myStackAlignScanfW((const char*)fmt, b, (uint64_t *)scratch_buff, 2);
    __MY_CPU;
    PREPARE_VALIST;

    return vswscanf(stream, fmt, VARARGS);
}

#if 0
EXPORT void my_verr(int eval, void* fmt, void* b) {
    #ifndef NOALIGN
    char scratch_buff [26*8] = {0};
    myStackAlignW((const char*)fmt, (uint32_t*)b, scratch_buff);
    __MY_CPU;
    PREPARE_VALIST;
    void* f = verr;
    ((vFipp_t)f)(eval, fmt, VARARGS);
    #else
    void* f = verr;
    ((vFipp_t)f)(eval, fmt, (uint32_t*)b);
    #endif
}

EXPORT void my_vwarn(void* fmt, void* b) {
    #ifndef NOALIGN
    char scratch_buff [26*8] = {0};
    myStackAlignW((const char*)fmt, (uint32_t*)b, scratch_buff);
    __MY_CPU;
    PREPARE_VALIST;
    void* f = vwarn;
    ((vFpp_t)f)(fmt, VARARGS);
    #else
    void* f = vwarn;
    ((vFpp_t)f)(fmt, (uint32_t*)b);
    #endif
}
#endif
EXPORT void my_err(int eval, void* fmt, void* b) {
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 2);
    PREPARE_VALIST;
    verr(eval, (const char*)fmt, VARARGS);
}
EXPORT void my_errx(int eval, void* fmt, void* b) {
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 2);
    PREPARE_VALIST;
    verrx(eval, (const char*)fmt, VARARGS);
}
EXPORT void my_warn(void* fmt, void* b) {
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 1);
    PREPARE_VALIST;
    vwarn((const char*)fmt, VARARGS);
}
EXPORT void my_warnx(void* fmt, void* b) {
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 1);
    PREPARE_VALIST;
    vwarnx((const char*)fmt, VARARGS);
}

EXPORT void my_syslog(int priority, const char* fmt, uint64_t* b)
{
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign(fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 2);
    PREPARE_VALIST;
    return vsyslog(priority, fmt, VARARGS);
}
EXPORT void my___syslog_chk(int priority, int flags, const char* fmt, uint64_t* b)
{
    (void)flags;
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlign(fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 3);
    PREPARE_VALIST;
    return vsyslog(priority, fmt, VARARGS);
}
EXPORT void my_vsyslog(int priority, const char* fmt, x64_va_list_t b)
{
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    return vsyslog(priority, fmt, VARARGS);
}
EXPORT void my___vsyslog_chk(int priority, int flag, const char* fmt, x64_va_list_t b)
{
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(b);
    #else
    char scratch_buff [26*8] = {0};
    myStackAlignValist((const char*)fmt, (uint64_t *)scratch_buff, b);
    __MY_CPU;
    PREPARE_VALIST;
    #endif
    return vsyslog(priority, fmt, VARARGS);
}

EXPORT int my___swprintf_chk(void* s, size_t n, int32_t flag, size_t slen, void* fmt, uint64_t* b)
{
    (void)flag;
    (void)slen;
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlignW((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 5);
    PREPARE_VALIST;
    return vswprintf(s, n, (const wchar_t*)fmt, VARARGS);
}
EXPORT int my_swprintf(void* s, size_t n, void* fmt, uint64_t* b)
{
    char scratch_buff [26*8] = {0};
    __MY_CPU;
    myStackAlignW((const char*)fmt, b, (uint64_t *)scratch_buff, cpu->regs[R_EAX], 3);
    PREPARE_VALIST;
    return vswprintf(s, n, (const wchar_t*)fmt, VARARGS);
}

EXPORT void my__ITM_addUserCommitAction(void* cb, uint32_t b, void* c)
{
    // disabled for now... Are all this _ITM_ stuff really mendatory?
    #if 0
    // quick and dirty... Should store the callback to be removed later....
    libc_my_t *my = (libc_my_t *)my_context->libclib->w.p2;
    x64emu_t *cbemu = AddCallback((uintptr_t)cb, 1, c, NULL, NULL, NULL);
    my->_ITM_addUserCommitAction(libc1ArgCallback, b, cbemu);
    // should keep track of cbemu to remove at some point...
    #else
    (void)cb; (void)b; (void)c;
    printf("warning _ITM_addUserCommitAction called\n");
    #endif
}
EXPORT void my__ITM_registerTMCloneTable(void* p, uint32_t s) { (void)p; (void)s; }
EXPORT void my__ITM_deregisterTMCloneTable(void* p) { (void)p; }


EXPORT int my___fxstat(int vers, int fd, void* buf)
{
    (void)vers;
    struct stat64 st;
    int r = fstat64(fd, buf?&st:buf);
    if(buf)
        UnalignStat64(&st, buf);
    return r;
}

EXPORT int my___fxstat64(int vers, int fd, void* buf)
{
    (void)vers;
    struct stat64 st;
    int r = fstat64(fd, buf?&st:buf);
    if(buf)
        UnalignStat64(&st, buf);
    return r;
}

EXPORT int my___xstat(int v, void* path, void* buf)
{
    (void)v;
    struct stat64 st;
    int r = stat64((const char*)path, buf?&st:buf);
    if(buf)
        UnalignStat64(&st, buf);
    return r;
}

EXPORT int my___xstat64(int v, void* path, void* buf)
{
    (void)v;
    struct stat64 st;
    int r = stat64((const char*)path, buf?&st:buf);
    if(buf)
        UnalignStat64(&st, buf);
    return r;
}

EXPORT int my___lxstat(int v, void* name, void* buf)
{
    (void)v;
    struct stat64 st;
    int r = lstat64((const char*)name, buf?&st:buf);
    if(buf)
        UnalignStat64(&st, buf);
    return r;
}

EXPORT int my___lxstat64(int v, void* name, void* buf)
{
    (void)v;
    struct stat64 st;
    int r = lstat64((const char*)name, buf?&st:buf);
    if(buf)
        UnalignStat64(&st, buf);
    return r;
}

EXPORT int my___fxstatat(int v, int d, void* path, void* buf, int flags)
{
    (void)v;
    struct  stat64 st;
    int r = fstatat64(d, path, &st, flags);
    UnalignStat64(&st, buf);
    return r;
}

EXPORT int my___fxstatat64(int v, int d, void* path, void* buf, int flags)
{
    (void)v;
    struct  stat64 st;
    int r = fstatat64(d, path, &st, flags);
    UnalignStat64(&st, buf);
    return r;
}

EXPORT int my_stat(void* filename, void* buf)
{
    struct stat64 st;
    int r = stat(filename, (struct stat*)&st);
    UnalignStat64(&st, buf);
    return r;
}
EXPORT int my_stat64(void* filename, void* buf) __attribute__((alias("my_stat")));

EXPORT int my_lstat(void* filename, void* buf)
{
    struct stat64 st;
    int r = lstat(filename, (struct stat*)&st);
    UnalignStat64(&st, buf);
    return r;
}
EXPORT int my_lstat64(void* filename, void* buf) __attribute__((alias("my_lstat")));

EXPORT int my_fstat(int fd, void* buf)
{
    struct stat64 st;
    int r = fstat(fd, (struct stat*)&st);
    UnalignStat64(&st, buf);
    return r;
}
EXPORT int my_fstat64(int fd, void* buf) __attribute__((alias("my_fstat")));

EXPORT int my_fstatat(int fd, const char* path, void* buf, int flags)
{
    struct stat64 st;
    int r = fstatat(fd, path, (struct stat*)&st, flags);
    UnalignStat64(&st, buf);
    return r;
}
EXPORT int my_fstatat64(int fd, const char* path, void* buf, int flags) __attribute__((alias("my_fstatat")));

EXPORT int my__IO_file_stat(void* f, void* buf)
{
    struct stat64 st;
    int r = my->_IO_file_stat(f, &st);
    UnalignStat64(&st, buf);
    return r;
}

#if 0
EXPORT int my_fstatfs64(int fd, void* buf)
{
    struct statfs64 st;
    int r = fstatfs64(fd, &st);
    UnalignStatFS64(&st, buf);
    return r;
}

EXPORT int my_statfs64(const char* path, void* buf)
{
    struct statfs64 st;
    int r = statfs64(path, &st);
    UnalignStatFS64(&st, buf);
    return r;
}
#endif

#ifdef ANDROID
typedef int (*__compar_d_fn_t)(const void*, const void*, void*);

static size_t qsort_r_partition(void* base, size_t size, __compar_d_fn_t compar, void* arg, size_t lo, size_t hi)
{
    void* tmp = alloca(size);
    void* pivot = ((char*)base) + lo * size;
    size_t i = lo;
    for (size_t j = lo; j <= hi; j++)
    {
        void* base_i = ((char*)base) + i * size;
        void* base_j = ((char*)base) + j * size;
        if (compar(base_j, pivot, arg) < 0)
        {
            memcpy(tmp, base_i, size);
            memcpy(base_i, base_j, size);
            memcpy(base_j, tmp, size);
            i++;
        }
    }
    void* base_i = ((char *)base) + i * size;
    void* base_hi = ((char *)base) + hi * size;
    memcpy(tmp, base_i, size);
    memcpy(base_i, base_hi, size);
    memcpy(base_hi, tmp, size);
    return i;
}

static void qsort_r_helper(void* base, size_t size, __compar_d_fn_t compar, void* arg, ssize_t lo, ssize_t hi)
{
    if (lo < hi)
    {
        size_t p = qsort_r_partition(base, size, compar, arg, lo, hi);
        qsort_r_helper(base, size, compar, arg, lo, p - 1);
        qsort_r_helper(base, size, compar, arg, p + 1, hi);
    }
}

static void qsort_r(void* base, size_t nmemb, size_t size, __compar_d_fn_t compar, void* arg)
{
    return qsort_r_helper(base, size, compar, arg, 0, nmemb - 1);
}
#endif

typedef struct compare_r_s {
    uintptr_t f;
    void*     data;
    int       r;
} compare_r_t;

static int my_compare_r_cb(void* a, void* b, compare_r_t* arg)
{
    return (int)RunFunctionWithState(arg->f, 2+arg->r, a, b, arg->data);
}
EXPORT void my_qsort(void* base, size_t nmemb, size_t size, void* fnc)
{
    compare_r_t args;
    args.f = (uintptr_t)fnc; args.r = 0; args.data = NULL;
    qsort_r(base, nmemb, size, (__compar_d_fn_t)my_compare_r_cb, &args);
}
EXPORT void my_qsort_r(void* base, size_t nmemb, size_t size, void* fnc, void* data)
{
    compare_r_t args;
    args.f = (uintptr_t)fnc; args.r = 1; args.data = data;
    qsort_r(base, nmemb, size, (__compar_d_fn_t)my_compare_r_cb, &args);
}
EXPORT void* my_bsearch(void* key, void* base, size_t nmemb, size_t size, void* fnc)
{
    return bsearch(key, base, nmemb, size, findcompareFct(fnc));
}

EXPORT void* my_lsearch(void* key, void* base, size_t* nmemb, size_t size, void* fnc)
{
    return lsearch(key, base, nmemb, size, findcompareFct(fnc));
}

EXPORT void* my_tsearch(void* key, void* root, void* fnc)
{
    return tsearch(key, root, findcompareFct(fnc));
}
EXPORT void my_tdestroy(void* root, void* fnc)
{
    tdestroy(root, findfreeFct(fnc));
}
EXPORT void* my_tdelete(void* key, void** root, void* fnc)
{
    return tdelete(key, root, findcompareFct(fnc));
}
EXPORT void* my_tfind(void* key, void** root, void* fnc)
{
    return tfind(key, root, findcompareFct(fnc));
}
EXPORT void* my_lfind(void* key, void* base, size_t* nmemb, size_t size, void* fnc)
{
    return lfind(key, base, nmemb, size, findcompareFct(fnc));
}

EXPORT void* my_fts_open(void* path, int options, void* c)
{
    return fts_open(path, options, findcompareFct(c));
}

EXPORT void* my_fts64_open(void* path, int options, void* c)
{
    return my->fts64_open(path, options, findcompareFct(c));
}

#if 0
struct i386_dirent {
    uint32_t d_ino;
    int32_t  d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[256];
};

EXPORT void* my_readdir(void* dirp)
{
    if (fix_64bit_inodes)
    {
        struct dirent64 *dp64 = readdir64((DIR *)dirp);
        if (!dp64) return NULL;
        uint32_t ino32 = dp64->d_ino ^ (dp64->d_ino >> 32);
        int32_t off32 = dp64->d_off;
        struct i386_dirent *dp32 = (struct i386_dirent *)&(dp64->d_off);
        dp32->d_ino = ino32;
        dp32->d_off = off32;
        dp32->d_reclen -= 8;
        return dp32;
    }
    else
    {
        static pFp_t f = NULL;
        if(!f) {
            library_t* lib = my_lib;
            if(!lib) return NULL;
            f = (pFp_t)dlsym(lib->priv.w.lib, "readdir");
        }

        return f(dirp);
    }
}

EXPORT int32_t my_readdir_r(void* dirp, void* entry, void** result)
{
    struct dirent64 d64, *dp64;
    if (fix_64bit_inodes && (sizeof(d64.d_name) > 1))
    {
        static iFppp_t f = NULL;
        if(!f) {
            library_t* lib = my_lib;
            if(!lib)
            {
                *result = NULL;
                return 0;
            }
            f = (iFppp_t)dlsym(lib->priv.w.lib, "readdir64_r");
        }

        int r = f(dirp, &d64, &dp64);
        if (r || !dp64 || !entry)
        {
            *result = NULL;
            return r;
        }

        struct i386_dirent *dp32 = (struct i386_dirent *)entry;
        int namelen = dp64->d_reclen - offsetof(struct dirent64, d_name);
        if (namelen > sizeof(dp32->d_name))
        {
            *result = NULL;
            return ENAMETOOLONG;
        }

        dp32->d_ino = dp64->d_ino ^ (dp64->d_ino >> 32);
        dp32->d_off = dp64->d_off;
        dp32->d_reclen = namelen + offsetof(struct i386_dirent, d_name);
        dp32->d_type = dp64->d_type;
        memcpy(dp32->d_name, dp64->d_name, namelen);
        *result = dp32;
        return 0;
    }
    else
    {
        static iFppp_t f = NULL;
        if(!f) {
            library_t* lib = my_lib;
            if(!lib)
            {
                *result = NULL;
                return 0;
            }
            f = (iFppp_t)dlsym(lib->priv.w.lib, "readdir_r");
        }

        return f(dirp, entry, result);
    }
}
#endif

static int isProcSelf(const char *path, const char* w)
{
    if(strncmp(path, "/proc/", 6)==0) {
        char tmp[64];
        // check if self ....
        sprintf(tmp, "/proc/self/%s", w);
        if(strcmp((const char*)path, tmp)==0)
            return 1;
        // check if self PID ....
        pid_t pid = getpid();
        sprintf(tmp, "/proc/%d/%s", pid, w);
        if(strcmp((const char*)path, tmp)==0)
            return 1;
    }
    return 0;
}

EXPORT ssize_t my_readlink(void* path, void* buf, size_t sz)
{
    if(isProcSelf((const char*)path, "exe")) {
        // special case for self...
        return strlen(strncpy((char*)buf, my_context->fullpath, sz));
    }
    return readlink((const char*)path, (char*)buf, sz);
}

#ifndef NOALIGN
void CreateCPUInfoFile(int fd)
{
    size_t dummy;
    char buff[600];
    double freq = 600.0; // default to 600 MHz
    // try to get actual ARM max speed:
    FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
    if(f) {
        int r;
        if(1==fscanf(f, "%d", &r))
            freq = r/1000.;
        fclose(f);
    }
    int n = getNCpu();
    // generate fake CPUINFO
//    int gigahertz=(freq>=1000.);
    #define P \
    dummy = write(fd, buff, strlen(buff))
    for (int i=0; i<n; ++i) {
        sprintf(buff, "processor\t: %d\n", i);
        P;
        sprintf(buff, "vendor_id\t: GenuineIntel\n");
        P;
        sprintf(buff, "cpu family\t: 6\n");
        P;
        sprintf(buff, "model\t\t: 1\n");
        P;
        sprintf(buff, "model name\t: %s\n", getBoxCpuName());
        P;
        sprintf(buff, "stepping\t: 1\nmicrocode\t: 0x10\n");
        P;
        sprintf(buff, "cpu MHz\t\t: %g\n", freq);
        P;
        sprintf(buff, "cache size\t: %d\n", 4096);
        P;
        sprintf(buff, "physical id\t: %d\nsiblings\t: %d\n", 0, n);
        P;
        sprintf(buff, "core id\t\t: %d\ncpu cores\t: %d\n", i, n);
        P;
        sprintf(buff, "bogomips\t: %g\n", getBogoMips());
        P;
        sprintf(buff, "flags\t\t: fpu cx8 sep ht cmov clflush mmx sse sse2 syscall tsc lahf_lm ssse3 ht tm lm fma fxsr cpuid pclmulqdq cx16 aes movbe pni sse4_1 sse4_2 lzcnt popcnt\n");
        P;
        sprintf(buff, "address sizes\t: 48 bits physical, 48 bits virtual\n");
        P;
        sprintf(buff, "\n");
        P;
    }
    (void)dummy;
    #undef P
}

#ifdef ANDROID
static int shm_open(const char *name, int oflag, mode_t mode) {
    return -1;
}
static int shm_unlink(const char *name) {
    return -1;
}
#endif

#define TMP_CPUINFO "box64_tmpcpuinfo"
#define TMP_CPUTOPO "box64_tmpcputopo%d"
#endif
#define TMP_MEMMAP  "box64_tmpmemmap"
#define TMP_CMDLINE "box64_tmpcmdline"
EXPORT int32_t my_open(void* pathname, int32_t flags, uint32_t mode)
{
    if(isProcSelf((const char*) pathname, "cmdline")) {
        // special case for self command line...
        #if 0
        char tmpcmdline[200] = {0};
        char tmpbuff[100] = {0};
        sprintf(tmpbuff, "%s/cmdlineXXXXXX", getenv("TMP")?getenv("TMP"):".");
        int tmp = mkstemp(tmpbuff);
        int dummy;
        if(tmp<0) return open(pathname, flags, mode);
        dummy = write(tmp, my_context->fullpath, strlen(my_context->fullpath)+1);
        for (int i=1; i<my_context->argc; ++i)
            dummy = write(tmp, my_context->argv[i], strlen(my_context->argv[i])+1);
        lseek(tmp, 0, SEEK_SET);
        #else
        int tmp = shm_open(TMP_CMDLINE, O_RDWR | O_CREAT, S_IRWXU);
        if(tmp<0) return open(pathname, flags, mode);
        shm_unlink(TMP_CMDLINE);    // remove the shm file, but it will still exist because it's currently in use
        int dummy = write(tmp, my_context->fullpath, strlen(my_context->fullpath)+1);
        (void)dummy;
        for (int i=1; i<my_context->argc; ++i)
            dummy = write(tmp, my_context->argv[i], strlen(my_context->argv[i])+1);
        lseek(tmp, 0, SEEK_SET);
        #endif
        return tmp;
    }
    if(isProcSelf((const char*)pathname, "exe")) {
        return open(my_context->fullpath, flags, mode);
    }
    #ifndef NOALIGN
    if(strcmp((const char*)pathname, "/proc/cpuinfo")==0) {
        // special case for cpuinfo
        int tmp = shm_open(TMP_CPUINFO, O_RDWR | O_CREAT, S_IRWXU);
        if(tmp<0) return open(pathname, flags, mode); // error fallback
        shm_unlink(TMP_CPUINFO);    // remove the shm file, but it will still exist because it's currently in use
        CreateCPUInfoFile(tmp);
        lseek(tmp, 0, SEEK_SET);
        return tmp;
    }
    #endif
    int ret = open(pathname, flags, mode);
    return ret;
}
EXPORT int32_t my___open(void* pathname, int32_t flags, uint32_t mode) __attribute__((alias("my_open")));

//#ifdef DYNAREC
//static int hasDBFromAddress(uintptr_t addr)
//{
//    int idx = (addr>>DYNAMAP_SHIFT);
//    return getDB(idx)?1:0;
//}
//#endif

//EXPORT int32_t my_read(int fd, void* buf, uint32_t count)
//{
//    int ret = read(fd, buf, count);
//#ifdef DYNAREC
//    if(ret!=count && ret>0) {
//        // continue reading...
//        void* p = buf+ret;
//        if(hasDBFromAddress((uintptr_t)p)) {
//            // allow writing the whole block (this happens with HalfLife, libMiles load code directly from .mix and other file like that)
//            unprotectDB((uintptr_t)p, count-ret, 1);
//            int l;
//            do {
//                l = read(fd, p, count-ret);
//                if(l>0) {
//                    p+=l; ret+=l;
//                }
//            } while(l>0);
//        }
//    }
//#endif
//    return ret;
//}

EXPORT int32_t my_open64(void* pathname, int32_t flags, uint32_t mode)
{
    if(isProcSelf((const char*)pathname, "cmdline")) {
        // special case for self command line...
        #if 0
        char tmpcmdline[200] = {0};
        char tmpbuff[100] = {0};
        sprintf(tmpbuff, "%s/cmdlineXXXXXX", getenv("TMP")?getenv("TMP"):".");
        int tmp = mkstemp64(tmpbuff);
        int dummy;
        if(tmp<0) return open64(pathname, flags, mode);
        dummy = write(tmp, my_context->fullpath, strlen(my_context->fullpath)+1);
        for (int i=1; i<my_context->argc; ++i)
            dummy = write(tmp, my_context->argv[i], strlen(my_context->argv[i])+1);
        lseek64(tmp, 0, SEEK_SET);
        #else
        int tmp = shm_open(TMP_CMDLINE, O_RDWR | O_CREAT, S_IRWXU);
        if(tmp<0) return open64(pathname, flags, mode);
        shm_unlink(TMP_CMDLINE);    // remove the shm file, but it will still exist because it's currently in use
        int dummy = write(tmp, my_context->fullpath, strlen(my_context->fullpath)+1);
        (void)dummy;
        for (int i=1; i<my_context->argc; ++i)
            dummy = write(tmp, my_context->argv[i], strlen(my_context->argv[i])+1);
        lseek(tmp, 0, SEEK_SET);
        #endif
        return tmp;
    }
    if(isProcSelf((const char*)pathname, "exe")) {
        return open64(my_context->fullpath, flags, mode);
    }
    #ifndef NOALIGN
    if(strcmp((const char*)pathname, "/proc/cpuinfo")==0) {
        // special case for cpuinfo
        int tmp = shm_open(TMP_CPUINFO, O_RDWR | O_CREAT, S_IRWXU);
        if(tmp<0) return open64(pathname, flags, mode); // error fallback
        shm_unlink(TMP_CPUINFO);    // remove the shm file, but it will still exist because it's currently in use
        CreateCPUInfoFile(tmp);
        lseek(tmp, 0, SEEK_SET);
        return tmp;
    }
    #endif
    return open64(pathname, flags, mode);
}

EXPORT FILE* my_fopen(const char* path, const char* mode)
{
    if(isProcSelf(path, "maps")) {
        // special case for self memory map
        int tmp = shm_open(TMP_MEMMAP, O_RDWR | O_CREAT, S_IRWXU);
        if(tmp<0) return fopen(path, mode); // error fallback
        shm_unlink(TMP_MEMMAP);    // remove the shm file, but it will still exist because it's currently in use
        CreateMemorymapFile(my_context, tmp);
        lseek(tmp, 0, SEEK_SET);
        return fdopen(tmp, mode);
    }
    #ifndef NOALIGN
    if(strcmp(path, "/proc/cpuinfo")==0) {
        // special case for cpuinfo
        int tmp = shm_open(TMP_CPUINFO, O_RDWR | O_CREAT, S_IRWXU);
        if(tmp<0) return fopen(path, mode); // error fallback
        shm_unlink(TMP_CPUINFO);    // remove the shm file, but it will still exist because it's currently in use
        CreateCPUInfoFile(tmp);
        lseek(tmp, 0, SEEK_SET);
        return fdopen(tmp, mode);
    }
    #endif
    if(isProcSelf(path, "exe")) {
        return fopen(my_context->fullpath, mode);
    }
    return fopen(path, mode);
}

EXPORT FILE* my_fopen64(const char* path, const char* mode)
{
    if(isProcSelf(path, "maps")) {
        // special case for self memory map
        int tmp = shm_open(TMP_MEMMAP, O_RDWR | O_CREAT, S_IRWXU);
        if(tmp<0) return fopen64(path, mode); // error fallback
        shm_unlink(TMP_MEMMAP);    // remove the shm file, but it will still exist because it's currently in use
        CreateMemorymapFile(my_context, tmp);
        lseek(tmp, 0, SEEK_SET);
        return fdopen(tmp, mode);
    }
    #ifndef NOALIGN
    if(strcmp(path, "/proc/cpuinfo")==0) {
        // special case for cpuinfo
        int tmp = shm_open(TMP_CPUINFO, O_RDWR | O_CREAT, S_IRWXU);
        if(tmp<0) return fopen64(path, mode); // error fallback
        shm_unlink(TMP_CPUINFO);    // remove the shm file, but it will still exist because it's currently in use
        CreateCPUInfoFile(tmp);
        lseek(tmp, 0, SEEK_SET);
        return fdopen(tmp, mode);
    }
    #endif
    if(isProcSelf(path, "exe")) {
        return fopen64(my_context->fullpath, mode);
    }
    return fopen64(path, mode);
}

#if 0
EXPORT int32_t my_ftw(void* pathname, void* B, int32_t nopenfd)
{
    static iFppi_t f = NULL;
    if(!f) {
        library_t* lib = my_lib;
        if(!lib) return 0;
        f = (iFppi_t)dlsym(lib->priv.w.lib, "ftw");
    }

    return f(pathname, findftwFct(B), nopenfd);
}

EXPORT int32_t my_nftw(void* pathname, void* B, int32_t nopenfd, int32_t flags)
{
    static iFppii_t f = NULL;
    if(!f) {
        library_t* lib = my_lib;
        if(!lib) return 0;
        f = (iFppii_t)dlsym(lib->priv.w.lib, "nftw");
    }

    return f(pathname, findnftwFct(B), nopenfd, flags);
}

EXPORT void* my_ldiv(void* p, int32_t num, int32_t den)
{
    *((ldiv_t*)p) = ldiv(num, den);
    return p;
}
#endif

#ifndef NOALIGN
EXPORT int32_t my_epoll_ctl(int32_t epfd, int32_t op, int32_t fd, void* event)
{
    struct epoll_event _event[1] = {0};
    if(event && (op!=EPOLL_CTL_DEL))
        AlignEpollEvent(_event, event, 1);
    return epoll_ctl(epfd, op, fd, event?_event:NULL);
}
EXPORT int32_t my_epoll_wait(int32_t epfd, void* events, int32_t maxevents, int32_t timeout)
{
    struct epoll_event _events[maxevents];
    int32_t ret = 0;
    //AlignEpollEvent(_events, events, maxevents);
    if (events) {
        ret = epoll_wait(epfd, _events, maxevents, timeout);
    } else {
        ret = -1;
    }
    if(ret>0)
        UnalignEpollEvent(events, _events, ret);
    return ret;
}
EXPORT int32_t my_epoll_pwait(int32_t epfd, void* events, int32_t maxevents, int32_t timeout, const sigset_t *sigmask)
{
    struct epoll_event _events[maxevents];
    int32_t ret = 0;
    //AlignEpollEvent(_events, events, maxevents);
    if (events) {
        ret = epoll_pwait(epfd, _events, maxevents, timeout, sigmask);
    } else {
        ret = -1;
    }
    if(ret>0)
        UnalignEpollEvent(events, _events, ret);
    return ret;
}
#endif

#ifndef ANDROID
EXPORT int32_t my_glob64(void* pat, int32_t flags, void* errfnc, void* pglob)
{
    return glob64(pat, flags, findgloberrFct(errfnc), pglob);
}
EXPORT int32_t my_glob(void* pat, int32_t flags, void* errfnc, void* pglob) __attribute__((alias("my_glob64")));
#endif

EXPORT int my_scandir64(void* dir, void* namelist, void* sel, void* comp)
{
    return scandir64(dir, namelist, findfilter64Fct(sel), findcompare64Fct(comp));
}
EXPORT int my_scandir(void* dir, void* namelist, void* sel, void* comp) __attribute__((alias("my_scandir64")));

EXPORT int my_scandirat(int dirfd, void* dirp, void* namelist, void* sel, void* comp)
{
    return scandirat(dirfd, dirp, namelist, findfilter64Fct(sel), findcompare64Fct(comp));
}

EXPORT int my_ftw64(void* filename, void* func, int descriptors)
{
    return ftw64(filename, findftw64Fct(func), descriptors);
}
EXPORT int my_ftw(void* filename, void* func, int descriptors) __attribute__((alias("my_ftw64")));

EXPORT int32_t my_nftw64(void* pathname, void* B, int32_t nopenfd, int32_t flags)
{
    return nftw64(pathname, findnftw64Fct(B), nopenfd, flags);
}

EXPORT char** my_environ = NULL;
EXPORT char** my__environ = NULL;
EXPORT char** my___environ = NULL;  // all aliases

EXPORT int32_t my_execv(const char* path, char* const argv[])
{
    int self = isProcSelf(path, "exe");
    int x64 = FileIsX64ELF(path);
    int x86 = my_context->box86path?FileIsX86ELF(path):0;
    int script = (my_context->bashpath && FileIsShell(path))?1:0;
    printf_log(LOG_DEBUG, "execv(\"%s\", %p) is x64=%d x86=%d script=%d self=%d\n", path, argv, x64, x86, script, self);
    #if 1
    if (x64 || x86 || script || self) {
        int skip_first = 0;
        if(strlen(path)>=strlen("wine64-preloader") && strcmp(path+strlen(path)-strlen("wine64-preloader"), "wine64-preloader")==0)
            skip_first++;
        // count argv...
        int n=skip_first;
        while(argv[n]) ++n;
        int toadd = script?2:1;
        const char** newargv = (const char**)box_calloc(n+toadd+2, sizeof(char*));
        newargv[0] = x86?my_context->box86path:my_context->box64path;
        if(script) newargv[1] = my_context->bashpath; // script needs to be launched with bash
        memcpy(newargv+toadd, argv+skip_first, sizeof(char*)*(n+toadd));
        if(self)
            newargv[1] = my_context->fullpath;
        else {
            // TODO check if envp is not environ and add the value on a copy
            if(strcmp(newargv[toadd], skip_first?argv[skip_first]:path))
                setenv(x86?"BOX86_ARG0":"BOX64_ARG0", newargv[toadd], 1);
            newargv[toadd] = skip_first?argv[skip_first]:path;
        }
        printf_log(LOG_DEBUG, " => execv(\"%s\", %p [\"%s\", \"%s\", \"%s\"...:%d])\n", newargv[0], newargv, newargv[0], n?newargv[1]:"", (n>1)?newargv[2]:"",n);
        char** envv = NULL;
        if(my_environ!=my_context->envv) envv = my_environ;
        if(my__environ!=my_context->envv) envv = my__environ;
        if(my___environ!=my_context->envv) envv = my___environ;
        int ret;
        if(envv)
            ret = execve(newargv[0], (char* const*)newargv, envv);
        else
            ret = execv(newargv[0], (char* const*)newargv);
        box_free(newargv);
        return ret;
    }
    #endif
    return execv(path, argv);
}

EXPORT int32_t my_execve(const char* path, char* const argv[], char* const envp[])
{
    int self = isProcSelf(path, "exe");
    int x64 = FileIsX64ELF(path);
    int x86 = my_context->box86path?FileIsX86ELF(path):0;
    int script = (my_context->bashpath && FileIsShell(path))?1:0;
    printf_log(LOG_DEBUG, "execve(\"%s\", %p[\"%s\", \"%s\", \"%s\"...], %p) is x64=%d x86=%d script=%d (my_context->envv=%p, environ=%p\n", path, argv, argv[0], argv[1]?argv[1]:"(nil)", argv[2]?argv[2]:"(nil)", envp, x64, x86, script, my_context->envv, environ);
    // hack to update the environ var if needed
    if(envp == my_context->envv && environ) {
        envp = environ;
    }
    #if 1
    if (x64 || x86 || self || script) {
        int skip_first = 0;
        if(strlen(path)>=strlen("wine64-preloader") && strcmp(path+strlen(path)-strlen("wine64-preloader"), "wine64-preloader")==0)
            skip_first++;
        // count argv...
        int n=skip_first;
        while(argv[n]) ++n;
        int toadd = script?2:1;
        const char** newargv = (const char**)alloca((n+1+toadd-skip_first)*sizeof(char*));
        memset(newargv, 0, (n+1+toadd)*sizeof(char*));
        newargv[0] = x86?my_context->box86path:my_context->box64path;
        if(script) newargv[1] = my_context->bashpath; // script needs to be launched with bash
        memcpy(newargv+toadd, argv+skip_first, sizeof(char*)*(n+1-skip_first));
        if(self) newargv[toadd] = my_context->fullpath;
        else {
            // TODO check if envp is not environ and add the value on a copy
            if(strcmp(newargv[toadd], path))
                setenv(x86?"BOX86_ARG0":"BOX64_ARG0", newargv[toadd], 1);
            newargv[toadd] = path;
        }
        printf_log(LOG_DEBUG, " => execve(\"%s\", %p [\"%s\", \"%s\", \"%s\"...:%d], %p)\n", newargv[0], newargv, newargv[0], (n+toadd-skip_first)?newargv[1]:"", ((n+toadd-skip_first)>1)?newargv[2]:"",n, envp);
        int ret = execve(newargv[0], (char* const*)newargv, envp);
        return ret;
    }
    #endif
    if(!strcmp(path + strlen(path) - strlen("/uname"), "/uname")
     && argv[1] && (!strcmp(argv[1], "-m") || !strcmp(argv[1], "-p") || !strcmp(argv[1], "-i"))
     && !argv[2]) {
        // uname -m is redirected to box64 -m
        path = my_context->box64path;
        char *argv2[3] = { my_context->box64path, argv[1], NULL };
        return execve(path, argv2, envp);
    }
    #ifndef NOALIGN
    if(!strcmp(path + strlen(path) - strlen("/grep"), "/grep")
    && argv[1] && argv[2] && (!strcmp(argv[2], "/proc/cpuinfo") || (argv[1][1]=='-' && argv[3] && !strcmp(argv[3], "/proc/cpuinfo")))) {
        // special case of a bash script shell running grep on cpuinfo to extract capacities...
        int cpuinfo = strcmp(argv[2], "/proc/cpuinfo")?3:2;
        int n=0;
        while(argv[n]) ++n;
        const char** newargv = (const char**)alloca((n+1)*sizeof(char*));
        memcpy(newargv, argv, sizeof(char*)*(n+1));
        // create a dummy cpuinfo in temp (that will stay there, sorry)
        const char* tmpdir = GetTmpDir();
        char template[100] = {0};
        sprintf(template, "%s/box64cpuinfoXXXXXX", tmpdir);
        int fd = mkstemp(template);
        CreateCPUInfoFile(fd);
        // get back the name
        char cpuinfo_file[100] = {0};
        sprintf(template, "/proc/self/fd/%d", fd);
        int rl = readlink(template, cpuinfo_file, sizeof(cpuinfo_file));
        (void)rl;
        close(fd);
        chmod(cpuinfo_file, 0666);
        newargv[cpuinfo] = cpuinfo_file;
        printf_log(LOG_DEBUG, " => execve(\"%s\", %p [\"%s\", \"%s\", \"%s\"...:%d], %p)\n", path, newargv, newargv[0], newargv[1], newargv[2],n, envp);
        int ret = execve(path, (char* const*)newargv, envp);
        return ret;
    }
    if(!strcmp(path + strlen(path) - strlen("/cat"), "/cat")
    && argv[1] && !strcmp(argv[1], "/proc/cpuinfo")) {
        // special case of a bash script shell running grep on cpuinfo to extract capacities...
        int cpuinfo = 1;
        int n=0;
        while(argv[n]) ++n;
        const char** newargv = (const char**)alloca((n+1)*sizeof(char*));
        memcpy(newargv, argv, sizeof(char*)*(n+1));
        // create a dummy cpuinfo in temp (that will stay there, sorry)
        const char* tmpdir = GetTmpDir();
        char template[100] = {0};
        sprintf(template, "%s/box64cpuinfoXXXXXX", tmpdir);
        int fd = mkstemp(template);
        CreateCPUInfoFile(fd);
        // get back the name
        char cpuinfo_file[100] = {0};
        sprintf(template, "/proc/self/fd/%d", fd);
        int rl = readlink(template, cpuinfo_file, sizeof(cpuinfo_file));
        (void)rl;
        close(fd);
        chmod(cpuinfo_file, 0666);
        newargv[cpuinfo] = cpuinfo_file;
        printf_log(LOG_DEBUG, " => execve(\"%s\", %p [\"%s\", \"%s\", \"%s\"...:%d], %p)\n", path, newargv, newargv[0], newargv[1], newargv[2],n, envp);
        int ret = execve(path, (char* const*)newargv, envp);
        return ret;
    }
    /*if(!strcmp(path + strlen(path) - strlen("/bwrap"), "/bwrap")) {
        printf_log(LOG_NONE, "\n\n*********\n\nCalling bwrap!\n\n**********\n\n");
    }*/
    #endif

    return execve(path, argv, envp);
}

// execvp should use PATH to search for the program first
EXPORT int32_t my_execvp(const char* path, char* const argv[])
{
    // need to use BOX64_PATH / PATH here...
    char* fullpath = ResolveFile(path, &my_context->box64_path);
    // use fullpath...
    int self = isProcSelf(fullpath, "exe");
    int x64 = FileIsX64ELF(fullpath);
    int x86 = my_context->box86path?FileIsX86ELF(fullpath):0;
    int script = (my_context->bashpath && FileIsShell(fullpath))?1:0;
    printf_log(LOG_DEBUG, "execvp(\"%s\", %p), IsX86=%d / fullpath=\"%s\"\n", path, argv, x64, fullpath);
    if (x64 || x86 || script || self) {
        // count argv...
        int i=0;
        while(argv[i]) ++i;
        int toadd = script?2:1;
        char** newargv = (char**)alloca((i+toadd+1)*sizeof(char*));
        memset(newargv, 0, (i+toadd+1)*sizeof(char*));
        newargv[0] = x86?my_context->box86path:my_context->box64path;
        if(script) newargv[1] = my_context->bashpath; // script needs to be launched with bash
        for (int j=0; j<i; ++j)
            newargv[j+toadd] = argv[j];
        if(self) newargv[1] = my_context->fullpath;
        //else if(script) newargv[2] = fullpath;
        else {
            // TODO check if envp is not environ and add the value on a copy
            if(strcmp(newargv[toadd], path))
                setenv(x86?"BOX86_ARG0":"BOX64_ARG0", newargv[toadd], 1);
            newargv[toadd] = fullpath;
        }

        printf_log(LOG_DEBUG, " => execvp(\"%s\", %p [\"%s\", \"%s\"...:%d])\n", newargv[0], newargv, newargv[1], i?newargv[2]:"", i);
        char** envv = NULL;
        if(my_environ!=my_context->envv) envv = my_environ;
        if(my__environ!=my_context->envv) envv = my__environ;
        if(my___environ!=my_context->envv) envv = my___environ;
        int ret;
        if(envv)
            ret = execvpe(newargv[0], newargv, envv);
        else
            ret = execvp(newargv[0], newargv);
        box_free(fullpath);
        return ret;
    }
    if((!strcmp(path + strlen(path) - strlen("/uname"), "/uname") || !strcmp(path, "uname"))
     && argv[1] && (!strcmp(argv[1], "-m") || !strcmp(argv[1], "-p") || !strcmp(argv[1], "-i"))
     && !argv[2]) {
        // uname -m is redirected to box64 -m
        path = my_context->box64path;
        char *argv2[3] = { my_context->box64path, argv[1], NULL };

        return execvp(path, argv2);
    }

    // fullpath is gone, so the search will only be on PATH, not on BOX64_PATH (is that an issue?)
    return execvp(path, argv);
}

EXPORT int32_t my_execl(const char* path)
{
    int self = isProcSelf(path, "exe");
    int x64 = FileIsX64ELF(path);
    int x86 = my_context->box86path?FileIsX86ELF(path):0;
    int script = (my_context->bashpath && FileIsShell(path))?1:0;
    printf_log(LOG_DEBUG, "execle(\"%s\", ...), IsX86=%d, self=%d\n", path, x64, self);
    // count argv...
    int i=0;
    while(getVargN(i+1)) ++i;
    int toadd = script?2:((x64||self)?1:0);
    char** newargv = (char**)box_calloc(i+toadd+1, sizeof(char*));
    int j=0;
    if ((x64 || x86 || script || self))
        newargv[j++] = x86?my_context->box86path:my_context->box64path;
    if(script) newargv[j++] = my_context->bashpath;
    for (int k=0; k<i; ++k)
        newargv[j++] = getVargN(k+1);
    if(self) newargv[1] = my_context->fullpath;
    printf_log(LOG_DEBUG, " => execle(\"%s\", %p [\"%s\", \"%s\"...:%d])\n", newargv[0], newargv, newargv[1], i?newargv[2]:"", i);
    int ret = 0;
    if (!(x64 || x86 || script || self)) {
        ret = execv(path, newargv);
    } else {
        ret = execv(newargv[0], newargv);
    }
    box_free(newargv);
    return ret;
}

EXPORT int32_t my_execle(const char* path)
{
    int self = isProcSelf(path, "exe");
    int x64 = FileIsX64ELF(path);
    int x86 = my_context->box86path?FileIsX86ELF(path):0;
    int script = (my_context->bashpath && FileIsShell(path))?1:0;
    printf_log(LOG_DEBUG, "execl(\"%s\", ...), IsX86=%d, self=%d\n", path, x64, self);
    // hack to update the environ var if needed
    // count argv...
    int i=0;
    while(getVargN(i+1)) ++i;
    int toadd = script?2:((x64||self)?1:0);
    char** newargv = (char**)box_calloc(i+toadd+1, sizeof(char*));
    char** envp = (char**)getVargN(i+2);
    if(envp == my_context->envv && environ) {
        envp = environ;
    }
    int j=0;
    if ((x64 || x86 || script || self))
        newargv[j++] = x86?my_context->box86path:my_context->box64path;
    if(script) newargv[j++] = my_context->bashpath;
    for (int k=0; k<i; ++k)
        newargv[j++] = getVargN(k+1);
    if(self) newargv[1] = my_context->fullpath;
    printf_log(LOG_DEBUG, " => execle(\"%s\", %p [\"%s\", \"%s\"...:%d], %p)\n", newargv[0], newargv, newargv[1], i?newargv[2]:"", i, envp);
    int ret = execve(newargv[0], newargv, envp);
    box_free(newargv);
    return ret;
}

EXPORT int32_t my_execlp(const char* path)
{
    // need to use BOX64_PATH / PATH here...
    char* fullpath = ResolveFile(path, &my_context->box64_path);
    // use fullpath...
    int self = isProcSelf(fullpath, "exe");
    int x64 = FileIsX64ELF(fullpath);
    int x86 = my_context->box86path?FileIsX86ELF(fullpath):0;
    int script = (my_context->bashpath && FileIsShell(fullpath))?1:0;
    printf_log(LOG_DEBUG, "execlp(\"%s\", ...), IsX86=%d / fullpath=\"%s\"\n", path, x64, fullpath);
    // count argv...
    int i=0;
    while(getVargN(i+1)) ++i;
    int toadd = script?2:((x64||self)?1:0);
    char** newargv = (char**)box_calloc(i+toadd+1, sizeof(char*));
    int j=0;
    if ((x64 || x86 || script || self))
        newargv[j++] = x86?my_context->box86path:my_context->box64path;
    if(script) newargv[j++] = my_context->bashpath;
    for (int k=0; k<i; ++k)
        newargv[j++] = getVargN(k+1);
    if(self) newargv[1] = my_context->fullpath;
    if(script) newargv[2] = fullpath;
    printf_log(LOG_DEBUG, " => execlp(\"%s\", %p [\"%s\", \"%s\"...:%d])\n", newargv[0], newargv, newargv[1], i?newargv[2]:"", i);
    char** envv = NULL;
    if(my_environ!=my_context->envv) envv = my_environ;
    if(my__environ!=my_context->envv) envv = my__environ;
    if(my___environ!=my_context->envv) envv = my___environ;
    int ret;
    if(envv)
        ret = execvpe(newargv[0], newargv, envv);
    else
        ret = execvp(newargv[0], newargv);
    box_free(newargv);
    box_free(fullpath);
    return ret;
}

EXPORT int32_t my_posix_spawn(pid_t* pid, const char* fullpath,
    const posix_spawn_file_actions_t *actions, const posix_spawnattr_t* attrp,  char* const argv[], char* const envp[])
{
    int self = isProcSelf(fullpath, "exe");
    int x64 = FileIsX64ELF(fullpath);
    int x86 = my_context->box86path?FileIsX86ELF(fullpath):0;
    int script = (my_context->bashpath && FileIsShell(fullpath))?1:0;
    int ret;
    printf_log(/*LOG_DEBUG*/LOG_INFO, "posix_spawn(%p, \"%s\", %p, %p, %p[\"%s\", \"%s\", ...], %p), IsX64=%d, IsX86=%d IsScript=%d %s\n", pid, fullpath, actions, attrp, argv, argv[0], argv[1]?argv[1]:"", envp, x64, x86, script, (envp==my_context->envv)?"envp is context->envv":"");
    // hack to update the environ var if needed
    if(envp == my_context->envv && environ) {
        envp = environ;
    }
    if (x64 || x86 || script || self) {
        int n=1;
        while(argv[n]) ++n;
        int toadd = script?2:1;
        const char** newargv = (const char**)alloca((n+1+toadd)*sizeof(char*));
        memset(newargv, 0, (n+1+toadd)*sizeof(char*));
        newargv[0] = x86?my_context->box86path:my_context->box64path;
        if(script) newargv[1] = my_context->bashpath; // script needs to be launched with bash
        memcpy(newargv+toadd, argv, (n+1)*sizeof(char*));
        if(self) newargv[toadd] = my_context->fullpath;
        else {
            // TODO check if envp is not environ and add the value on a copy
            if(strcmp(newargv[toadd], fullpath))
                setenv(x86?"BOX86_ARG0":"BOX64_ARG0", newargv[toadd], 1);
            newargv[toadd] = fullpath;
        }
        printf_log(/*LOG_DEBUG*/LOG_INFO, " => posix_spawn(%p, \"%s\", %p, %p, %p [\"%s\", \"%s\", \"%s\"...:%d], %p)\n", pid, newargv[0], actions, attrp, newargv, newargv[0], newargv[1], newargv[2]?newargv[2]:"", n, envp);
        ret = posix_spawn(pid, newargv[0], actions, attrp, (char* const*)newargv, envp);
        printf_log(/*LOG_DEBUG*/LOG_INFO, "posix_spawn returned %d\n", ret);
        //box_free(newargv);
    } else
        ret = posix_spawn(pid, fullpath, actions, attrp, argv, envp);
    return ret;
}

// execvp should use PATH to search for the program first
EXPORT int32_t my_posix_spawnp(pid_t* pid, const char* path,
    const posix_spawn_file_actions_t *actions, const posix_spawnattr_t* attrp,  char* const argv[], char* const envp[])
{
    // need to use BOX64_PATH / PATH here...
    char* fullpath = ResolveFile(path, &my_context->box64_path);
    // use fullpath...
    int self = isProcSelf(fullpath, "exe");
    int x64 = FileIsX64ELF(fullpath);
    int x86 = my_context->box86path?FileIsX86ELF(path):0;
    int script = (my_context->bashpath && FileIsShell(fullpath))?1:0;
    int ret;
    printf_log(/*LOG_DEBUG*/LOG_INFO, "posix_spawnp(%p, \"%s\", %p, %p, %p, %p), IsX86=%d / fullpath=\"%s\"\n", pid, path, actions, attrp, argv, envp, x64, fullpath);
    // hack to update the environ var if needed
    if(envp == my_context->envv && environ) {
        envp = environ;
    }
    if (x64 || x86 || script || self) {
        int n=1;
        while(argv[n]) ++n;
        int toadd = script?2:1;
        const char** newargv = (const char**)alloca((n+1+toadd)*sizeof(char*));
        memset(newargv, 0, (n+1+toadd)*sizeof(char*));
        newargv[0] = x86?my_context->box86path:my_context->box64path;
        if(script) newargv[1] = my_context->bashpath; // script needs to be launched with bash
        memcpy(newargv+toadd, argv, (n+1)*sizeof(char*));
        if(self) newargv[toadd] = my_context->fullpath;
        else {
            // TODO check if envp is not environ and add the value on a copy
            if(strcmp(newargv[toadd], fullpath))
                setenv(x86?"BOX86_ARG0":"BOX64_ARG0", newargv[toadd], 1);
            newargv[toadd] = fullpath;
        }
        printf_log(/*LOG_DEBUG*/LOG_INFO, " => posix_spawn(%p, \"%s\", %p, %p, %p [\"%s\", \"%s\", \"%s\"...:%d], %p)\n", pid, newargv[0], actions, attrp, newargv, newargv[0], newargv[1], newargv[2]?newargv[2]:"", n, envp);
        ret = posix_spawn(pid, newargv[0], actions, attrp, (char* const*)newargv, envp);
        printf_log(/*LOG_DEBUG*/LOG_INFO, "posix_spawn returned %d\n", ret);
        //box_free(newargv);
    } else
        ret = posix_spawnp(pid, path, actions, attrp, argv, envp);
    box_free(fullpath);
    return ret;
}

EXPORT void my__Jv_RegisterClasses(void) {}
#if 0
EXPORT int32_t my___cxa_thread_atexit_impl(void* dtor, void* obj, void* dso)
{
    //printf_log(LOG_INFO, "Warning, call to __cxa_thread_atexit_impl(%p, %p, %p) ignored\n", dtor, obj, dso);
    AddCleanup1Arg(dtor, obj, dso);
    return 0;

    return 0;
}
#endif
EXPORT int32_t my___register_atfork(void* prepare, void* parent, void* child, void* handle)
{
    // this is partly incorrect, because the emulated funcionts should be executed by actual fork and not by my_atfork...
    if(my_context->atfork_sz==my_context->atfork_cap) {
        my_context->atfork_cap += 4;
        my_context->atforks = (atfork_fnc_t*)box_realloc(my_context->atforks, my_context->atfork_cap*sizeof(atfork_fnc_t));
    }
    my_context->atforks[my_context->atfork_sz].prepare = (uintptr_t)prepare;
    my_context->atforks[my_context->atfork_sz].parent = (uintptr_t)parent;
    my_context->atforks[my_context->atfork_sz].child = (uintptr_t)child;
    my_context->atforks[my_context->atfork_sz].handle = handle;
    return 0;
}

#if 0
EXPORT uint64_t my___umoddi3(uint64_t a, uint64_t b)
{
    return a%b;
}
EXPORT uint64_t my___udivdi3(uint64_t a, uint64_t b)
{
    return a/b;
}
EXPORT int64_t my___divdi3(int64_t a, int64_t b)
{
    return a/b;
}

EXPORT int32_t my___poll_chk(void* a, uint32_t b, int c, int l)
{
    return poll(a, b, c);   // no check...
}
#endif

EXPORT int32_t my_fcntl64(int32_t a, int32_t b, void* c)
{
    if(b==F_SETFL)
        c = (void*)(uintptr_t)of_convert((intptr_t)c);
    #if 0
    if(b==F_GETLK64 || b==F_SETLK64 || b==F_SETLKW64)
    {
        my_flock64_t fl;
        AlignFlock64(&fl, c);
        int ret = fcntl(a, b, &fl);
        UnalignFlock64(c, &fl);
        return ret;
    }
    #endif
    int ret = fcntl(a, b, c);
    if(b==F_GETFL && ret!=-1)
        ret = of_unconvert(ret);

    return ret;
}

EXPORT int32_t my_fcntl(int32_t a, int32_t b, void* c)
{
    if(b==F_SETFL && (intptr_t)c==0xFFFFF7FF) {
        // special case for ~O_NONBLOCK...
        int flags = fcntl(a, F_GETFL);
        if(flags&O_NONBLOCK) {
            flags &= ~O_NONBLOCK;
            return fcntl(a, b, flags);
        }
        return 0;
    }
    if(b==F_SETFL)
        c = (void*)(uintptr_t)of_convert((intptr_t)c);
    #if 0
    if(b==F_GETLK64 || b==F_SETLK64 || b==F_SETLKW64)
    {
        my_flock64_t fl;
        AlignFlock64(&fl, c);
        int ret = fcntl(a, b, &fl);
        UnalignFlock64(c, &fl);
        return ret;
    }
    #endif
    int ret = fcntl(a, b, c);
    if(b==F_GETFL && ret!=-1)
        ret = of_unconvert(ret);

    return ret;
}
EXPORT int32_t my___fcntl(int32_t a, int32_t b, void* c) __attribute__((alias("my_fcntl")));

#if 0
EXPORT int32_t my_preadv64(int32_t fd, void* v, int32_t c, int64_t o)
{
    library_t* lib = my_lib;
    if(!lib) return 0;
    void* f = dlsym(lib->priv.w.lib, "preadv64");
    if(f)
        return ((iFipiI_t)f)(fd, v, c, o);
    return syscall(__NR_preadv, fd, v, c,(uint32_t)(o&0xffffffff), (uint32_t)((o>>32)&0xffffffff));
}

EXPORT int32_t my_pwritev64(int32_t fd, void* v, int32_t c, int64_t o)
{
    library_t* lib = my_lib;
    if(!lib) return 0;
    void* f = dlsym(lib->priv.w.lib, "pwritev64");
    if(f)
        return ((iFipiI_t)f)(fd, v, c, o);
    #ifdef __arm__
    return syscall(__NR_pwritev, fd, v, c, 0, (uint32_t)(o&0xffffffff), (uint32_t)((o>>32)&0xffffffff));
    // on arm, 64bits args needs to be on even/odd register, so need to put a 0 for aligment
    #else
    return syscall(__NR_pwritev, fd, v, c,(uint32_t)(o&0xffffffff), (uint32_t)((o>>32)&0xffffffff));
    #endif
}

EXPORT int32_t my_accept4(int32_t fd, void* a, void* l, int32_t flags)
{
    library_t* lib = my_lib;
    if(!lib) return 0;
    void* f = dlsym(lib->priv.w.lib, "accept4");
    if(f)
        return ((iFippi_t)f)(fd, a, l, flags);
    if(!flags)
        return accept(fd, a, l);
    return syscall(__NR_accept4, fd, a, l, flags);
}

EXPORT  int32_t my_fallocate64(int fd, int mode, int64_t offs, int64_t len)
{
    iFiiII_t f = NULL;
    static int done = 0;
    if(!done) {
        library_t* lib = my_lib;
        f = (iFiiII_t)dlsym(lib->priv.w.lib, "fallocate64");
        done = 1;
    }
    if(f)
        return f(fd, mode, offs, len);
    else
        return syscall(__NR_fallocate, fd, mode, (uint32_t)(offs&0xffffffff), (uint32_t)((offs>>32)&0xffffffff), (uint32_t)(len&0xffffffff), (uint32_t)((len>>32)&0xffffffff));
        //return posix_fallocate64(fd, offs, len);
}

EXPORT struct __processor_model
{
  unsigned int __cpu_vendor;
  unsigned int __cpu_type;
  unsigned int __cpu_subtype;
  unsigned int __cpu_features[1];
} my___cpu_model;

#include "cpu_info.h"
void InitCpuModel()
{
    // some pseudo random cpu info...
    my___cpu_model.__cpu_vendor = VENDOR_INTEL;
    my___cpu_model.__cpu_type = INTEL_PENTIUM_M;
    my___cpu_model.__cpu_subtype = 0; // N/A
    my___cpu_model.__cpu_features[0] = (1<<FEATURE_CMOV)
                                     | (1<<FEATURE_MMX)
                                     | (1<<FEATURE_SSE)
                                     | (1<<FEATURE_SSE2)
                                     | (1<<FEATURE_SSE3)
                                     | (1<<FEATURE_SSSE3)
                                     | (1<<FEATURE_MOVBE)
                                     | (1<<FEATURE_ADX);
}
#endif

#ifdef ANDROID
void ctSetup()
{
}
#else
EXPORT const unsigned short int *my___ctype_b;
EXPORT const int32_t *my___ctype_tolower;
EXPORT const int32_t *my___ctype_toupper;

void ctSetup(void)
{
    my___ctype_b = *(__ctype_b_loc());
    my___ctype_toupper = *(__ctype_toupper_loc());
    my___ctype_tolower = *(__ctype_tolower_loc());
}
#endif

EXPORT void my___register_frame_info(void* a, void* b)
{
    // nothing
    (void)a; (void)b;
}
EXPORT void* my___deregister_frame_info(void* a)
{
    (void)a;
    return NULL;
}
#if 0
EXPORT void* my____brk_addr = NULL;

void EXPORT my_longjmp(/*struct __jmp_buf_tag __env[1]*/void *p, int32_t __val)
{
    jump_buff_x64_t *jpbuff = &((__jmp_buf_tag_t*)p)->__jmpbuf;
    //restore  regs
    R_RBX = jpbuff->save_rbx;
    R_RBP = jpbuff->save_rbp;
    R_R12 = jpbuff->save_r12;
    R_R13 = jpbuff->save_r13;
    R_R14 = jpbuff->save_r14;
    R_R15 = jpbuff->save_r15;
    R_RSP = jpbuff->save_rsp;
    // jmp to saved location, plus restore val to rax
    R_RAX = __val;
    R_RIP = jpbuff->save_rip;
    if(((__jmp_buf_tag_t*)p)->__mask_was_saved) {
        sigprocmask(SIG_SETMASK, &((__jmp_buf_tag_t*)p)->__saved_mask, NULL);
    }
    if(emu->flags.quitonlongjmp) {
        emu->flags.longjmp = 1;
        emu->quit = 1;
    }
}

EXPORT int32_t my___sigsetjmp(/*struct __jmp_buf_tag __env[1]*/void *p, int savesigs)
{
    jump_buff_x64_t *jpbuff = &((__jmp_buf_tag_t*)p)->__jmpbuf;
    // save the buffer
    jpbuff->save_rbx = R_RBX;
    jpbuff->save_rbp = R_RBP;
    jpbuff->save_r12 = R_R12;
    jpbuff->save_r13 = R_R13;
    jpbuff->save_r14 = R_R14;
    jpbuff->save_r15 = R_R15;
    jpbuff->save_rsp = R_RSP+sizeof(uintptr_t); // include "return address"
    jpbuff->save_rip = *(uintptr_t*)(R_RSP);
    if(savesigs) {
        if(sigprocmask(SIG_SETMASK, NULL, &((__jmp_buf_tag_t*)p)->__saved_mask))
            ((__jmp_buf_tag_t*)p)->__mask_was_saved = 0;
        else
            ((__jmp_buf_tag_t*)p)->__mask_was_saved = 1;
    } else
        ((__jmp_buf_tag_t*)p)->__mask_was_saved = 0;
    // quit emulation loop and create a new jumpbuf if needed
    if(!emu->flags.jmpbuf_ready) {
        emu->flags.need_jmpbuf = 1;
        emu->quit = 1;
    }
    return 0;
}
EXPORT int32_t my_sigsetjmp(/*struct __jmp_buf_tag __env[1]*/void *p, int savesigs)
{
    return my___sigsetjmp(p, savesigs);
}
EXPORT int32_t my__setjmp(/*struct __jmp_buf_tag __env[1]*/void *p)
{
    return  my___sigsetjmp(p, 0);
}
EXPORT int32_t my_setjmp(/*struct __jmp_buf_tag __env[1]*/void *p)
{
    return  my___sigsetjmp(p, 1);
}
#endif

EXPORT void my___explicit_bzero_chk(void* dst, uint32_t len, uint32_t dstlen)
{
    (void)dstlen;
    memset(dst, 0, len);
}

EXPORT void* my_realpath(void* path, void* resolved_path)
{
    if(isProcSelf(path, "exe")) {
        return realpath(my_context->fullpath, resolved_path);
    }
    return realpath(path, resolved_path);
}

EXPORT int my_readlinkat(int fd, void* path, void* buf, size_t bufsize)
{
    if(isProcSelf(path, "exe")) {
        strncpy(buf, my_context->fullpath, bufsize);
        size_t l = strlen(my_context->fullpath);
        return (l>bufsize)?bufsize:(l+1);
    }
    return readlinkat(fd, path, buf, bufsize);
}
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE	0x200000
#endif
#ifndef MAP_32BIT
#define MAP_32BIT 0x40
#endif
#if 0
EXPORT void* my_mmap64(void *addr, unsigned long length, int prot, int flags, int fd, int64_t offset)
{
    if(prot&PROT_WRITE)
        prot|=PROT_READ;    // PROT_READ is implicit with PROT_WRITE on i386
    if(emu && (box64_log>=LOG_DEBUG || box64_dynarec_log>=LOG_DEBUG)) {printf_log(LOG_NONE, "mmap64(%p, 0x%lx, 0x%x, 0x%x, %d, %ld) => ", addr, length, prot, flags, fd, offset);}
    int new_flags = flags;
    #ifndef NOALIGN
    void* old_addr = addr;
    new_flags&=~MAP_32BIT;   // remove MAP_32BIT
    if(flags&MAP_32BIT) {
        // MAP_32BIT only exist on x86_64!
        addr = find31bitBlockNearHint(addr, length, 0);
    } else if (box64_wine || 1) {   // other mmap should be restricted to 47bits
        if(!addr)
            addr = find47bitBlock(length);
    }
    #endif
    void* ret = mmap64(addr, length, prot, new_flags, fd, offset);
    #ifndef NOALIGN
    if((ret!=MAP_FAILED) && (flags&MAP_32BIT) &&
      (((uintptr_t)ret>0xffffffffLL) || (box64_wine && ((uintptr_t)ret&0xffff) && (ret!=addr)))) {
        printf_log(LOG_DEBUG, "Warning, mmap on 32bits didn't worked, ask %p, got %p ", addr, ret);
        munmap(ret, length);
        loadProtectionFromMap();    // reload map, because something went wrong previously
        addr = find31bitBlockNearHint(old_addr, length, 0); // is this the best way?
        new_flags = (addr && isBlockFree(addr, length) )? (new_flags|MAP_FIXED) : new_flags;
        if(new_flags&(MAP_FIXED|MAP_FIXED_NOREPLACE)==(MAP_FIXED|MAP_FIXED_NOREPLACE)) new_flags&=~MAP_FIXED_NOREPLACE;
        ret = mmap64(addr, length, prot, new_flags, fd, offset);
        printf_log(LOG_DEBUG, " tried again with %p, got %p\n", addr, ret);
    } else if((ret!=MAP_FAILED) && !(flags&MAP_FIXED) && (box64_wine) && (old_addr) && (addr!=ret) &&
             (((uintptr_t)ret>0x7fffffffffffLL) || ((uintptr_t)ret&~0xffff))) {
        printf_log(LOG_DEBUG, "Warning, mmap on 47bits didn't worked, ask %p, got %p ", addr, ret);
        munmap(ret, length);
        loadProtectionFromMap();    // reload map, because something went wrong previously
        addr = find47bitBlockNearHint(old_addr, length, 0); // is this the best way?
        new_flags = (addr && isBlockFree(addr, length)) ? (new_flags|MAP_FIXED) : new_flags;
        if(new_flags&(MAP_FIXED|MAP_FIXED_NOREPLACE)==(MAP_FIXED|MAP_FIXED_NOREPLACE)) new_flags&=~MAP_FIXED_NOREPLACE;
        ret = mmap64(addr, length, prot, new_flags, fd, offset);
        printf_log(LOG_DEBUG, " tried again with %p, got %p\n", addr, ret);
    }
    #endif
    if((ret!=MAP_FAILED) && (flags&MAP_FIXED_NOREPLACE) && (ret!=addr)) {
        munmap(ret, length);
        errno = EEXIST;
        return MAP_FAILED;
    }
    if(emu && (box64_log>=LOG_DEBUG || box64_dynarec_log>=LOG_DEBUG)) {printf_log(LOG_NONE, "%p\n", ret);}
    #ifdef DYNAREC
    if(box64_dynarec && ret!=MAP_FAILED) {
        /*if(flags&0x100000 && addr!=ret)
        {
            // program used MAP_FIXED_NOREPLACE but the host linux didn't support it
            // and responded with a different address, so ignore it
        } else*/ {
            if(prot& PROT_EXEC)
                addDBFromAddressRange((uintptr_t)ret, length);
            else
                cleanDBFromAddressRange((uintptr_t)ret, length, prot?0:1);
        }
    }
    #endif
    if(ret!=MAP_FAILED) {
        if()
            setProtection_mmap((uintptr_t)ret, length, prot);
        else
            setProtection((uintptr_t)ret, length, prot);
    }
    return ret;
}
EXPORT void* my_mmap(void *addr, unsigned long length, int prot, int flags, int fd, int64_t offset) __attribute__((alias("my_mmap64")));

EXPORT void* my_mremap(void* old_addr, size_t old_size, size_t new_size, int flags, void* new_addr)
{
    if(emu && (box64_log>=LOG_DEBUG || box64_dynarec_log>=LOG_DEBUG)) {printf_log(LOG_NONE, "mremap(%p, %lu, %lu, %d, %p)=>", old_addr, old_size, new_size, flags, new_addr);}
    void* ret = mremap(old_addr, old_size, new_size, flags, new_addr);
    if(emu && (box64_log>=LOG_DEBUG || box64_dynarec_log>=LOG_DEBUG)) {printf_log(LOG_NONE, "%p\n", ret);}
    if(ret!=(void*)-1) {
        uint32_t prot = getProtection((uintptr_t)old_addr)&~PROT_CUSTOM;
        if(ret==old_addr) {
            if(old_size && old_size<new_size) {
                setProtection_mmap((uintptr_t)ret+old_size, new_size-old_size, prot);
                #ifdef DYNAREC
                if(box64_dynarec)
                    addDBFromAddressRange((uintptr_t)ret+old_size, new_size-old_size);
                #endif
            } else if(old_size && new_size<old_size) {
                freeProtection((uintptr_t)ret+new_size, old_size-new_size);
                #ifdef DYNAREC
                if(box64_dynarec)
                    cleanDBFromAddressRange((uintptr_t)ret+new_size, old_size-new_size, 1);
                #endif
            } else if(!old_size) {
                setProtection_mmap((uintptr_t)ret, new_size, prot);
                #ifdef DYNAREC
                if(box64_dynarec)
                    addDBFromAddressRange((uintptr_t)ret, new_size);
                #endif
            }
        } else {
            if(old_size
            #ifdef MREMAP_DONTUNMAP
            && ((flags&MREMAP_DONTUNMAP)==0)
            #endif
            ) {
                freeProtection((uintptr_t)old_addr, old_size);
                #ifdef DYNAREC
                if(box64_dynarec)
                    cleanDBFromAddressRange((uintptr_t)old_addr, old_size, 1);
                #endif
            }
            setProtection_mmap((uintptr_t)ret, new_size, prot); // should copy the protection from old block
            #ifdef DYNAREC
            if(box64_dynarec)
                addDBFromAddressRange((uintptr_t)ret, new_size);
            #endif
        }
    }
    return ret;
}

EXPORT int my_munmap(void* addr, unsigned long length)
{
    if(emu && (box64_log>=LOG_DEBUG || box64_dynarec_log>=LOG_DEBUG)) {printf_log(LOG_NONE, "munmap(%p, %lu)\n", addr, length);}
    int ret = munmap(addr, length);
    #ifdef DYNAREC
    if(!ret && box64_dynarec && length) {
        cleanDBFromAddressRange((uintptr_t)addr, length, 1);
    }
    #endif
    if(!ret) {
        freeProtection((uintptr_t)addr, length);
    }
    return ret;
}
EXPORT int my_mprotect(void *addr, unsigned long len, int prot)
{
    if(emu && (box64_log>=LOG_DEBUG || box64_dynarec_log>=LOG_DEBUG)) {printf_log(LOG_NONE, "mprotect(%p, %lu, 0x%x)\n", addr, len, prot);}
    if(prot&PROT_WRITE)
        prot|=PROT_READ;    // PROT_READ is implicit with PROT_WRITE on x86_64
    int ret = mprotect(addr, len, prot);
    #ifdef DYNAREC
    if(box64_dynarec && !ret && len) {
        if(prot& PROT_EXEC)
            addDBFromAddressRange((uintptr_t)addr, len);
        else
            cleanDBFromAddressRange((uintptr_t)addr, len, 1);
    }
    #endif
    if(!ret && len) {
        if(prot)
            updateProtection((uintptr_t)addr, len, prot);
        else {
            // avoid allocating detailled protection for a no prot 0
            freeProtection((uintptr_t)addr, len);
            setProtection_mmap((uintptr_t)addr, len, prot);
        }
    }
    return ret;
}
#endif
typedef struct mallinfo (*mallinfo_fnc)(void);
EXPORT void* my_mallinfo(void* p)
{
    static mallinfo_fnc f = NULL;
    static int inited = 0;
    if(!inited) {
        inited = 1;
        f = (mallinfo_fnc)dlsym(my_lib->priv.w.lib, "mallinfo");
    }
    if(f)
        *(struct mallinfo*)p=f();
    else
        memset(p, 0, sizeof(struct mallinfo));
    return p;
}

EXPORT int my_getopt(int argc, char* const argv[], const char *optstring)
{
    my_updateGlobalOpt();
    int ret = getopt(argc, argv, optstring);
    my_checkGlobalOpt();
    return ret;
}

EXPORT int my_getopt_long(int argc, char* const argv[], const char* optstring, const struct option *longopts, int *longindex)
{
    my_updateGlobalOpt();
    int ret = getopt_long(argc, argv, optstring, longopts, longindex);
    my_checkGlobalOpt();
    return ret;
}

EXPORT int my_getopt_long_only(int argc, char* const argv[], const char* optstring, const struct option *longopts, int *longindex)
{
    my_updateGlobalOpt();
    int ret = getopt_long_only(argc, argv, optstring, longopts, longindex);
    my_checkGlobalOpt();
    return ret;
}

#ifndef ANDROID
typedef struct {
   void  *read;
   void *write;
   void  *seek;
   void *close;
} my_cookie_io_functions_t;

typedef struct my_cookie_s {
    uintptr_t r, w, s, c;
    void* cookie;
} my_cookie_t;

static ssize_t my_cookie_read(void *p, char *buf, size_t size)
{
    my_cookie_t* cookie = (my_cookie_t*)p;
    return (ssize_t)RunFunctionWithState(cookie->r, 3, cookie->cookie, buf, size)       ;
}
static ssize_t my_cookie_write(void *p, const char *buf, size_t size)
{
    my_cookie_t* cookie = (my_cookie_t*)p;
    return (ssize_t)RunFunctionWithState(cookie->w, 3, cookie->cookie, buf, size)       ;
}
static int my_cookie_seek(void *p, off64_t *offset, int whence)
{
    my_cookie_t* cookie = (my_cookie_t*)p;
    return RunFunctionWithState(cookie->s, 3, cookie->cookie, offset, whence)       ;
}
static int my_cookie_close(void *p)
{
    my_cookie_t* cookie = (my_cookie_t*)p;
    int ret = 0;
    if(cookie->c)
        ret = RunFunctionWithState(cookie->c, 1, cookie->cookie)      ;
    box_free(cookie);
    return ret;
}
EXPORT void* my_fopencookie(void* cookie, void* mode, my_cookie_io_functions_t *s)
{
    cookie_io_functions_t io_funcs = {s->read?my_cookie_read:NULL, s->write?my_cookie_write:NULL, s->seek?my_cookie_seek:NULL, my_cookie_close};
    my_cookie_t *cb = (my_cookie_t*)box_calloc(1, sizeof(my_cookie_t));
    cb->r = (uintptr_t)s->read;
    cb->w = (uintptr_t)s->write;
    cb->s = (uintptr_t)s->seek;
    cb->c = (uintptr_t)s->close;
    cb->cookie = cookie;
    return fopencookie(cb, mode, io_funcs);
}
#endif

#if 0

EXPORT long my_prlimit64(void* pid, uint32_t res, void* new_rlim, void* old_rlim)
{
    return syscall(__NR_prlimit64, pid, res, new_rlim, old_rlim);
}

EXPORT void* my_reallocarray(void* ptr, size_t nmemb, size_t size)
{
    return realloc(ptr, nmemb*size);
}

#ifndef __OPEN_NEEDS_MODE
# define __OPEN_NEEDS_MODE(oflag) \
  (((oflag) & O_CREAT) != 0)
// || ((oflag) & __O_TMPFILE) == __O_TMPFILE)
#endif
EXPORT int my___open_nocancel(void* file, int oflag, int* b)
{
    int mode = 0;
    if (__OPEN_NEEDS_MODE (oflag))
        mode = b[0];
    return openat(AT_FDCWD, file, oflag, mode);
}

EXPORT int my___libc_alloca_cutoff(size_t size)
{
    // not always implemented on old linux version...
    library_t* lib = my_lib;
    if(!lib) return 0;
    void* f = dlsym(lib->priv.w.lib, "__libc_alloca_cutoff");
    if(f)
        return ((iFL_t)f)(size);
    // approximate version but it's better than nothing....
    return (size<=(65536*4));
}

// DL functions from wrappedlibdl.c
void* my_dlopen(void *filename, int flag);
int my_dlclose(void *handle);
void* my_dlsym(void *handle, void *symbol);
EXPORT int my___libc_dlclose(void* handle)
{
    return my_dlclose(handle);
}
EXPORT void* my___libc_dlopen_mode(void* name, int mode)
{
    return my_dlopen(name, mode);
}
EXPORT void* my___libc_dlsym(void* handle, void* name)
{
    return my_dlsym(handle, name);
}

EXPORT int my_nanosleep(const struct timespec *req, struct timespec *rem)
{
    if(!req)
        return 0;   // workaround for some strange calls
    return nanosleep(req, rem);
}
#endif

#ifdef ANDROID
void obstackSetup() {
}
#else
// all obstack function defined in obstack.c file
void obstackSetup(void);
#endif

EXPORT void* my_malloc(unsigned long size)
{
    return calloc(1, size);
}

EXPORT int my_setrlimit(int ressource, const struct rlimit *rlim)
{
    int ret = (ressource==RLIMIT_AS)?0:setrlimit(ressource, rlim);
    if(ressource==RLIMIT_AS) printf_log(LOG_DEBUG, " (ignored) RLIMIT_AS, cur=0x%lx, max=0x%lx ", rlim->rlim_cur, rlim->rlim_max);
    return ret;
}

#if 0
#ifdef PANDORA
#define RENAME_NOREPLACE	(1 << 0)
#define RENAME_EXCHANGE		(1 << 1)
#define RENAME_WHITEOUT		(1 << 2)
EXPORT int my_renameat2(int olddirfd, void* oldpath, int newdirfd, void* newpath, uint32_t flags)
{
    // simulate that function, but
    if(flags&RENAME_NOREPLACE) {
        if(FileExist(newpath, -1)) {
            errno = EEXIST;
            return -1;
        }
        flags &= ~RENAME_NOREPLACE;
    }
    if(!flags) return renameat(olddirfd, oldpath, newdirfd, newpath);
    if(flags&RENAME_WHITEOUT) {
        errno = EINVAL;
        return -1;  // not handling that
    }
    if((flags&RENAME_EXCHANGE) && (olddirfd==-1) && (newdirfd==-1)) {
        // cannot do atomically...
        char* tmp = (char*)box_malloc(strlen(oldpath)+10); // create a temp intermediary
        tmp = strcat(oldpath, ".tmp");
        int ret = renameat(-1, oldpath, -1, tmp);
        if(ret==-1) return -1;
        ret = renameat(-1, newpath, -1, oldpath);
        if(ret==-1) return -1;
        ret = renameat(-1, tmp, -1, newpath);
        box_free(tmp);
        return ret;
    }
    return -1; // unknown flags
}
#endif

#ifndef __NR_memfd_create
#define MFD_CLOEXEC		    0x0001U
#define MFD_ALLOW_SEALING	0x0002U
EXPORT int my_memfd_create(void* name, uint32_t flags)
{
    // try to simulate that function
    uint32_t fl = O_RDWR | O_CREAT;
    if(flags&MFD_CLOEXEC)
        fl |= O_CLOEXEC;
    int tmp = shm_open(name, fl, S_IRWXU);
    if(tmp<0) return -1;
    shm_unlink(name);    // remove the shm file, but it will still exist because it's currently in use
    return tmp;
}
#endif

#ifndef GRND_RANDOM
#define GRND_RANDOM	0x0002
#endif
EXPORT int my_getentropy(void* buffer, size_t length)
{
    library_t* lib = my_lib;
    if(!lib) return 0;
    void* f = dlsym(lib->priv.w.lib, "getentropy");
    if(f)
        return ((iFpL_t)f)(buffer, length);
    // custom implementation
    if(length>256) {
        errno = EIO;
        return -1;
    }
    int ret = my_getrandom(buffer, length, GRND_RANDOM);
    if(ret!=length) {
        errno = EIO;
        return -1;
    }
    return 0;
}

EXPORT void my_mcount(void* frompc, void* selfpc)
{
    // stub doing nothing...
    return;
}
#endif

#ifndef ANDROID
union semun {
  int              val;    /* Value for SETVAL */
  struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
  unsigned short  *array;  /* Array for GETALL, SETALL */
  struct seminfo  *__buf;  /* Buffer for IPC_INFO
                              (Linux-specific) */
};
#endif
#ifndef SEM_STAT_ANY
#define SEM_STAT_ANY 20
#endif

EXPORT int my_semctl(int semid, int semnum, int cmd, union semun b)
{
    struct semid_ds semidds;
    void *backup = NULL;
    if ((cmd == IPC_STAT) || (cmd == IPC_SET) || (cmd == SEM_STAT) || (cmd == SEM_STAT_ANY)) {
        backup = b.buf;
        b.buf = &semidds;
        if (cmd == IPC_SET) {
            AlignSemidDs(&semidds, backup);
        }
    }
    int ret = semctl(semid, semnum, cmd, b);
    if ((cmd == IPC_STAT) || (cmd == IPC_SET) || (cmd == SEM_STAT) || (cmd == SEM_STAT_ANY)) {
        b.buf = backup;
        if (cmd == IPC_STAT) {
            UnalignSemidDs(backup, &semidds);
        }
    }
    return ret;
}

EXPORT uint64_t userdata_sign = 0x1234598765ABCEF0;
EXPORT uint32_t userdata[1024]; 

EXPORT long my_ptrace(int request, pid_t pid, void* addr, uint32_t* data)
{
    if(request == PTRACE_POKEUSER) {
        if(ptrace(PTRACE_PEEKDATA, pid, &userdata_sign, NULL)==userdata_sign  && (uintptr_t)addr < sizeof(userdata)) {
            ptrace(PTRACE_POKEDATA, pid, addr+(uintptr_t)userdata, data);
            return 0;
        }
        // fallback to a generic local faking
        if((uintptr_t)addr < sizeof(userdata))
            *(uintptr_t*)(addr+(uintptr_t)userdata) = (uintptr_t)data;
        // lets just ignore this for now!
        return 0;
    }
    if(request == PTRACE_PEEKUSER) {
        if(ptrace(PTRACE_PEEKDATA, pid, &userdata_sign, NULL)==userdata_sign  && (uintptr_t)addr < sizeof(userdata)) {
            return ptrace(PTRACE_PEEKDATA, pid, addr+(uintptr_t)userdata, data);
        }
        // fallback to a generic local faking
        if((uintptr_t)addr < sizeof(userdata))
            return *(uintptr_t*)(addr+(uintptr_t)userdata);
    }
    return ptrace(request, pid, addr, data);
}

// Backtrace stuff

#ifndef ANDROID
#if 0
EXPORT int my_backtrace(void** buffer, int size)
{
    if (!size) return 0;
    dwarf_unwind_t *unwind = init_dwarf_unwind_registers();
    int idx = 0;
    char success = 0;
    uintptr_t addr = *(uintptr_t*)R_RSP;
    buffer[0] = (void*)addr;
    while (++idx < size) {
        uintptr_t ret_addr = get_parent_registers(unwind, FindElfAddress(my_context, addr), addr, &success);
        if (ret_addr == my_context->exit_bridge) {
            // TODO: do something to be able to get the function name
            buffer[idx] = (void*)ret_addr;
            success = 2;
            // See elfdwarf_private.c for the register mapping
            unwind->regs[7] = unwind->regs[6]; // mov rsp, rbp
            unwind->regs[6] = *(uint64_t*)unwind->regs[7]; // pop rbp
            unwind->regs[7] += 8;
            ret_addr = *(uint64_t*)unwind->regs[7]; // ret
            unwind->regs[7] += 8;
            if (++idx < size) buffer[idx] = (void*)ret_addr;
        } else if (!success) break;
        else buffer[idx] = (void*)ret_addr;
        addr = ret_addr;
    }
    free_dwarf_unwind_registers(&unwind);
    return idx;
}

// special version, called in signal with SHOWBT
EXPORT int my_backtrace_ip(void** buffer, int size)
{
    if (!size) return 0;
    dwarf_unwind_t *unwind = init_dwarf_unwind_registers();
    int idx = 0;
    char success = 1;
    uintptr_t addr = R_RIP;
    buffer[0] = (void*)addr;
    while ((++idx < size) && success) {
        uintptr_t ret_addr = get_parent_registers(unwind, FindElfAddress(my_context, addr), addr, &success);
        if (ret_addr == my_context->exit_bridge) {
            // TODO: do something to be able to get the function name
            buffer[idx] = (void*)ret_addr;
            success = 2;
            // See elfdwarf_private.c for the register mapping
            unwind->regs[7] = unwind->regs[6]; // mov rsp, rbp
            unwind->regs[6] = *(uint64_t*)unwind->regs[7]; // pop rbp
            unwind->regs[7] += 8;
            ret_addr = *(uint64_t*)unwind->regs[7]; // ret
            unwind->regs[7] += 8;
            if (++idx < size) buffer[idx] = (void*)ret_addr;
        } else if (!success) {
            if(getProtection((uintptr_t)addr)&(PROT_READ)) {
                if(getProtection((uintptr_t)addr-19) && *(uint8_t*)(addr-19)==0xCC && *(uint8_t*)(addr-19+1)=='S' && *(uint8_t*)(addr-19+2)=='C') {
                    buffer[idx-1] = (void*)(addr-19);
                    success = 2;
                    if(idx==1)
                        unwind->regs[7] -= 8;
                    ret_addr = *(uint64_t*)unwind->regs[7]; // ret
                    unwind->regs[7] += 8;
                    buffer[idx] = (void*)ret_addr;
                } else {
                    // try a simple end of function epilog
                    unwind->regs[7] = unwind->regs[6]; // mov rsp, rbp
                    if(getProtection(unwind->regs[7])&(PROT_READ)) {
                        unwind->regs[6] = *(uint64_t*)unwind->regs[7]; // pop rbp
                        unwind->regs[7] += 8;
                        ret_addr = *(uint64_t*)unwind->regs[7]; // ret
                        unwind->regs[7] += 8;
                        buffer[idx] = (void*)ret_addr;
                        success = 2;
                    } else
                        break;
                }
            } else
                break;
        } else buffer[idx] = (void*)ret_addr;
        addr = ret_addr;
    }
    free_dwarf_unwind_registers(&unwind);
    return idx;
}
#endif
EXPORT char** my_backtrace_symbols(uintptr_t* buffer, int size)
{
    char** ret = (char**)calloc(1, size*sizeof(char*) + size*200);  // capping each strings to 200 chars, not using box_calloc (program space)
    char* s = (char*)(ret+size);
    for (int i=0; i<size; ++i) {
        uintptr_t start = 0;
        uint64_t sz = 0;
        elfheader_t *hdr = FindElfAddress(my_context, buffer[i]);
        const char* symbname = FindNearestSymbolName(hdr, (void*)buffer[i], &start, &sz);
        if(!sz) sz=0x100;   // arbitrary value...
        if (symbname && buffer[i]>=start && (buffer[i]<(start+sz) || !sz)) {
            snprintf(s, 200, "%s(%s+%lx) [%p]", ElfName(hdr), symbname, buffer[i] - start, (void*)buffer[i]);
        } else if (hdr) {
            snprintf(s, 200, "%s+%lx [%p]", ElfName(hdr), buffer[i] - (uintptr_t)GetBaseAddress(hdr), (void*)buffer[i]);
        } else {
            snprintf(s, 200, "??? [%p]", (void*)buffer[i]);
        }
        ret[i] = s;
        s += 200;
    }
    return ret;
}

EXPORT void my_backtrace_symbols_fd(uintptr_t* buffer, int size, int fd)
{
    char s[200];
    for (int i=0; i<size; ++i) {
        uintptr_t start = 0;
        uint64_t sz = 0;
        const char* symbname = FindNearestSymbolName(FindElfAddress(my_context, buffer[i]), (void*)buffer[i], &start, &sz);
        if(!sz) sz=0x100;   // arbitrary value...
        if(symbname && buffer[i]>=start && (buffer[i]<(start+sz) || !sz))
            snprintf(s, 200, "%s+%ld [%p]\n", symbname, buffer[i] - start, (void*)buffer[i]);
        else
            snprintf(s, 200, "??? [%p]\n", (void*)buffer[i]);
        int dummy = write(fd, s, strlen(s));
        (void)dummy;
    }
}
#endif

EXPORT int my_iopl(int level)
{
    // Set I/O permission (so access IN/OUT opcodes) Default is 0. Can Set to 0..3
    // set permission for all 65536 ports addresses
    // note ioperm can set individual permission
    /*static iFi_t real_iopl = NULL;
    static int searched = 0;
    if(!searched) {
        searched = 1;
        real_iopl = (iFi_t)dlsym(my_lib, "iopl");
    }
    if(real_iopl)
        return real_iopl(level);*/
    // For now, lets just return "unsupported"
    errno = ENOSYS;
    return -1;
}

EXPORT int my_stime(const time_t *t)
{
    // TODO?
    errno = EPERM;
    return -1;
}

int GetTID(void);
#ifdef ANDROID
void updateGlibcTidCache() {}
#else
struct glibc_pthread {
#if defined(NO_ALIGN)
    char header[704];
#else
    void* header[24];
#endif
  void* list[2];
  pid_t tid;
};
pid_t getGlibcCachedTid(void) {
  pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  pthread_mutex_lock(&lock);
  pid_t tid = lock.__data.__owner;
  pthread_mutex_unlock(&lock);
  pthread_mutex_destroy(&lock);
  return tid;
}
void updateGlibcTidCache(void) {
  pid_t real_tid = GetTID();
  pid_t cached_tid = getGlibcCachedTid();
  if (cached_tid != real_tid) {
    pid_t* cached_tid_location =
        &((struct glibc_pthread*)(pthread_self()))->tid;
    *cached_tid_location = real_tid;
  }
}
#endif
#if 0
typedef struct clone_arg_s {
 uintptr_t stack;
 uintptr_t fnc;
 void* args;
 int stack_clone_used;
 void* tls;
} clone_arg_t;
static int clone_fn(void* p)
{
    clone_arg_t* arg = (clone_arg_t*)p;
    updateGlibcTidCache();  // update cache tid if needed
    R_RSP = arg->stack;
    emu->flags.quitonexit = 1;
    thread_set_emu();
    int ret = RunFunctionWithState(arg->fnc, 1, arg->args);
    int exited = (emu->flags.quitonexit==2);
    thread_set_emu(NULL);
    FreeX64Emu(&emu);
    if(arg->stack_clone_used)
        my_context->stack_clone_used = 0;
    box_free(arg);
    /*if(exited)
        exit(ret);*/
    return ret;
}

EXPORT int my_clone(void* fn, void* stack, int flags, void* args, void* parent, void* tls, void* child)
{
    printf_log(LOG_DEBUG, "my_clone(fn:%p(%s), stack:%p, 0x%x, args:%p, %p, %p, %p)", fn, getAddrFunctionName((uintptr_t)fn), stack, flags, args, parent, tls, child);
    void* mystack = NULL;
    clone_arg_t* arg = (clone_arg_t*)box_calloc(1, sizeof(clone_arg_t));
    x64emu_t * newemu = NewX64Emu(emu->context, R_RIP, (uintptr_t)stack, 0, 0);
    SetupX64Emu(newemu);
    //CloneEmu(newemu);
    if(my_context->stack_clone_used) {
        printf_log(LOG_DEBUG, " no free stack_clone ");
        mystack = box_malloc(1024*1024);  // stack for own process... memory leak, but no practical way to remove it
    } else {
        if(!my_context->stack_clone)
            my_context->stack_clone = box_malloc(1024*1024);
        mystack = my_context->stack_clone;
        printf_log(LOG_DEBUG, " using stack_clone ");
        my_context->stack_clone_used = 1;
        arg->stack_clone_used = 1;
    }
    arg->stack = (uintptr_t)stack &~7LL;
    arg->args = args;
    arg->fnc = (uintptr_t)fn;
    arg->tls = tls;
    arg->emu = newemu;
    if((flags|(CLONE_VM|CLONE_VFORK|CLONE_SETTLS))==flags)   // that's difficult to setup, so lets ignore all those flags :S
        flags&=~(CLONE_VM|CLONE_VFORK|CLONE_SETTLS);
    int64_t ret = clone(clone_fn, (void*)((uintptr_t)mystack+1024*1024), flags, arg, parent, NULL, child);
    return (uintptr_t)ret;
}
#endif
EXPORT void my___cxa_pure_virtual(void)
{
    printf_log(LOG_NONE, "Pure virtual function called\n");
    abort();
}

EXPORT size_t my_strlcpy(void* dst, void* src, size_t l)
{
    strncpy(dst, src, l-1);
    ((char*)dst)[l-1] = '\0';
    return strlen(src);
}
EXPORT size_t my_strlcat(void* dst, void* src, size_t l)
{
    size_t s = strlen(dst);
    if(s>=l)
        return l;
    strncat(dst, src, l-1);
    ((char*)dst)[l-1] = '\0';
    return s+strlen(src);
}

EXPORT int my_register_printf_specifier(int c, void* f1, void* f2)
{
    //TODO: defining a new sepcifier for printf, it should also be registered on myStackAlign/myStackAlignW, using f2 to get the type of arg
    return my->register_printf_specifier(c, findprintf_outputFct(f1), findprintf_arginfoFct(f2));
}

EXPORT int my_register_printf_type(void* f)
{
    //TODO: defining a new type, probably needs to also register that for myStackAlign stuffs
    return my->register_printf_type(findprintf_typeFct(f));
}

EXPORT void my___libc_free(void* m)
{
	lsassert(0);//for translate_free_int3
}
EXPORT void my_cfree(void* m)
{
	lsassert(0);//for translate_free_int3
}
EXPORT void my_free(void* m)
{
	lsassert(0);//for translate_free_int3
}
EXPORT void my___free(void* m)
{
	lsassert(0);//for translate_free_int3
}
EXPORT void my_realloc(void* m, void *old, uintptr_t len)
{
    lsassert(0);//for translate_realloc_int3
}

#if 0
extern int box64_quit;
EXPORT void my_exit(int code)
{
    if(emu->flags.quitonexit) {
        emu->quit = 1;
        R_EAX = code;
        emu->flags.quitonexit = 2;
        return;
    }
    emu->quit = 1;
    box64_quit = 1;
    exit(code);
}

EXPORT void my__exit(int code) __attribute__((alias("my_exit")));

EXPORT int my_prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5)
{
    if(option==PR_SET_NAME) {
        printf_log(LOG_DEBUG, "BOX64: set process name to \"%s\"\n", (char*)arg2);
        ApplyParams((char*)arg2);
    }
    if(option==PR_SET_SECCOMP) {
        printf_log(LOG_INFO, "BOX64: ignoring prctl(PR_SET_SECCOMP, ...)\n");
        return 0;
    }
    return prctl(option, arg2, arg3, arg4, arg5);
}

#endif

EXPORT char* my___progname = NULL;
EXPORT char* my___progname_full = NULL;
EXPORT char* my_program_invocation_name = NULL;
EXPORT char* my_program_invocation_short_name = NULL;

// ignoring this for now
EXPORT char my___libc_single_threaded = 0;

#define PRE_INIT\
    if(1)                                                      \
        lib->priv.w.lib = dlopen(NULL, RTLD_LAZY | RTLD_GLOBAL);    \
    else

#ifdef ANDROID
#define NEEDED_LIBS   0
#define NEEDED_LIBS_234 3,  \
    "libpthread.so.0",      \
    "libdl.so.2" ,          \
    "libm.so"
#else
#define NEEDED_LIBS   5,    \
    "ld-linux-x86-64.so.2", \
    "libdl.so.2",           \
    "libutil.so.1",         \
    "librt.so.1"
#define NEEDED_LIBS_234 6,  \
    "ld-linux-x86-64.so.2", \
    "libdl.so.2",           \
    "libutil.so.1",         \
    "libresolv.so.2",       \
    "librt.so.1"
#endif
int box64_isglibc234 = 1;
#define CUSTOM_INIT         \
    box64->libclib = lib;   \
    /*InitCpuModel();*/         \
    ctSetup();              \
    obstackSetup();         \
    my_environ = my__environ = my___environ = box64->envv;                      \
    my___progname_full = my_program_invocation_name = box64->argv[0];           \
    my___progname = my_program_invocation_short_name =                          \
        strrchr(box64->argv[0], '/') + 1;                                       \
    getMy(lib);                                                                 \


    #if 0
    if(box64_isglibc234)                                                        \
        setNeededLibs(lib, NEEDED_LIBS_234);                                    \
    else                                                                        \
        setNeededLibs(lib, NEEDED_LIBS);
        #endif

#define CUSTOM_FINI \
    freeMy();

#pragma GCC diagnostic pop

#ifdef CONFIG_LOONGARCH_NEW_WORLD
#include "elfloader.h"
#include "elfloader_private.h"
#include "callback.h"
#include "myalign.h"
#include "fileutils.h"
#include "library.h"

#define FORWORDBACK 0
dlprivate_t *NewDLPrivate(void) {
    dlprivate_t* dl =  (dlprivate_t*)box_calloc(1, sizeof(dlprivate_t));
    return dl;
}
void FreeDLPrivate(dlprivate_t **lib) {
    box_free((*lib)->last_error);
    box_free(*lib);
}

void* my_dlopen(void *filename, int flag) EXPORT;
void* my_dlmopen(void* mlid, void *filename, int flag) EXPORT;
char* my_dlerror(void) EXPORT;
void* my_dlsym(void *handle, void *symbol) EXPORT;
int my_dlclose(void *handle) EXPORT;
int my_dladdr(void *addr, void *info) EXPORT;
int my_dladdr1(void *addr, void *info, void** extra_info, int flags) EXPORT;
void* my_dlvsym(void *handle, void *symbol, const char *vername) EXPORT;
int my_dlinfo(void* handle, int request, void* info) EXPORT;


#define CLEARERR    if(dl->last_error) box_free(dl->last_error); dl->last_error = NULL;
//#define R_RSP cpu->regs[R_ESP]
static void Push64(CPUX86State *cpu, uint64_t v)
{
    cpu->regs[R_ESP] -= 8;
    *((uint64_t*)cpu->regs[R_ESP]) = v;
}

void kzt_wine_init_x86(void);
int init_x86dlfun(void);
int init_x86dlfun(void)
{
    elfheader_t* h = NULL;
#ifdef CONFIG_LOONGARCH_NEW_WORLD
    char buf[PATH_MAX] = {0};
    snprintf(buf, PATH_MAX, "%s%s", interp_prefix,
             "/usr/lib/glibc-hwcaps/x86-64-v2" /* AOSC OS (Core 12.2.2), glibc 2.40 (EmuKit 20250909~pre20250911T080911Z) */);
    PrependList(&my_context->box64_ld_lib, buf, 1);
#endif
    h = loadElfFromFile("libc.so.6");
    lsassert(h);
    const char* syms[] = {"dlopen", "dlsym", "dlclose", "dladdr", "dladdr1"};
    void *rsyms[5] = {0};
    int rrsyms = 0;
    ResetSpecialCaseElf(h, syms, 5, rsyms, &rrsyms);
    if (rrsyms != 5) {
        h = loadElfFromFile("libdl.so.2");
        ResetSpecialCaseElf(h, syms, 5, rsyms, &rrsyms);
    }
    lsassert(rrsyms == 5);
    my_context->dlprivate->x86dlopen = rsyms[0];
    my_context->dlprivate->x86dlsym = rsyms[1];
    my_context->dlprivate->x86dlclose = rsyms[2];
    my_context->dlprivate->x86dladdr = rsyms[3];
    my_context->dlprivate->x86dladdr1 = rsyms[4];
    kzt_wine_init_x86();
    return 0;
}
static int callx86dlopen(void *filename, int flag, elfheader_t * h, int is_local) {
    struct link_map* ret = (struct link_map*)(uintptr_t)RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlopen, 2, filename, flag);
    if (ret) {
        printf_dlsym(LOG_DEBUG, "latx RunFunctionWithState dlopen %s addr %p\n", (char *)filename, (void *)ret->l_addr);
        h->lib->x86linkmap = ret;
    } else {
        //open error
        return -1;
    }
    h->delta = ret->l_addr;
    linkmap_t* lm = getLinkMapLib(h->lib);
    if (lm) {
        lm->l_addr = ret->l_addr;
    }
    h->latx_hasfix = 1;
    lib_t *maplib = (is_local)?h->lib->maplib:my_context->maplib;
    if(AddSymbolsLibrary(maplib, h->lib)) {   // also add needed libs
        printf_dlsym(LOG_INFO, "Failure to Add lib => fail\n");
        lsassert(0);
    }
    return 0;
}
static void LatxResetElf(elfheader_t * h)
{
    h->latx_hasfix = 0;
    h->had_RelocateElfPlt = 0;
    h->had_RelocateElf = 0;
    h->latx_type = 0;
    h->latx_hasfix = 0;
}
void* my_dlopen(void *filename, int flag){
    // TODO, handling special values for filename, like RTLD_SELF?
    // TODO, handling flags?
    library_t *lib = NULL;
    dlprivate_t *dl = my_context->dlprivate;
    size_t dlopened = 0;
    int is_local = (flag&0x100)?0:1;  // if not global, then local, and that means symbols are not put in the global "pot" for other libs
    CLEARERR
    if (!dl->x86dlopen) {
        init_x86dlfun();
        lsassert(dl->x86dlopen);
    }
    if(filename) {
        char* rfilename = (char*)alloca(MAX_PATH);
        strcpy(rfilename, (char*)filename);
        printf_dlsym(LOG_DEBUG, "Call to dlopen(\"%s\"/%p, %X)\n", rfilename, filename, flag);
        while(strstr(rfilename, "${ORIGIN}")) {
            char* origin = box_strdup(my_context->fullpath);
            char* p = strrchr(origin, '/');
            if(p) *p = '\0';    // remove file name to have only full path, without last '/'
            char* tmp = (char*)box_calloc(1, strlen(rfilename)-strlen("${ORIGIN}")+strlen(origin)+1);
            p = strstr(rfilename, "${ORIGIN}");
            memcpy(tmp, rfilename, p-rfilename);
            strcat(tmp, origin);
            strcat(tmp, p+strlen("${ORIGIN}"));
            strcpy(rfilename, tmp);
            box_free(tmp);
            box_free(origin);
        }
        while(strstr(rfilename, "${PLATFORM}")) {
            char* platform = box_strdup("x86_64");
            char* p = strrchr(platform, '/');
            if(p) *p = '\0';    // remove file name to have only full path, without last '/'
            char* tmp = (char*)box_calloc(1, strlen(rfilename)-strlen("${PLATFORM}")+strlen(platform)+1);
            p = strstr(rfilename, "${PLATFORM}");
            memcpy(tmp, rfilename, p-rfilename);
            strcat(tmp, platform);
            strcat(tmp, p+strlen("${PLATFORM}"));
            strcpy(rfilename, tmp);
            box_free(tmp);
            box_free(platform);
        }
        if (rfilename[0] == '/' && !FileExist(rfilename, IS_FILE)) {
            char filetmp[PATH_MAX] = {0};
            snprintf(filetmp , PATH_MAX, "%s%s", interp_prefix, rfilename);
            strcpy(rfilename, filetmp);
            printf_dlsym(LOG_DEBUG, "dlopen filename change to \"%s\"\n", rfilename);
        }
        // check if alread dlopenned...
        for (size_t i=0; i<dl->lib_sz; ++i) {
            if(IsSameLib(dl->libs[i], rfilename)) {
                if(dl->count[i]==0 && dl->dlopened[i]) {   // need to lauch init again!
                    int idx = GetElfIndex(dl->libs[i]);
                    if(idx!=-1) {
                        printf_dlsym(LOG_DEBUG, "dlopen: Recycling, calling Init for %p (%s)\n", (void*)(i+1), rfilename);
                        //TODO
                        if (IsEmuLib(dl->libs[i])) {
                            elfheader_t * h = my_context->elfs[idx];
                            lsassert(h);
                            LatxResetElf(h);
                            callx86dlopen(rfilename, flag, h, is_local);
                        }
                        ReloadLibrary(dl->libs[i]);    // reset memory image, redo reloc, run inits
                    }
                }
                if(!(flag&0x4))
                    dl->count[i] = dl->count[i]+1;
                printf_dlsym(LOG_DEBUG, "dlopen: Recycling %s/%p count=%ld (dlopened=%ld, elf_index=%d)\n", rfilename, (void*)(i+1), dl->count[i], dl->dlopened[i], GetElfIndex(dl->libs[i]));
                return (void*)(i+1);
            }
        }
        if(strstr(rfilename, "libGL.so")){
            strcpy(rfilename, "libGL.so.1");
        }
        dlopened = (GetLibInternal(rfilename)==NULL);
        // Then open the lib
        const char* libs[] = {rfilename};
        my_context->deferedInit = 1;
        int bindnow = (flag&0x2)?1:0;
        if (!FindLibIsWrapped(basename(rfilename))) {
#if FORWORDBACK
            lsassert(dl->x86dlopen);
            __MY_CPU;
            Push64(cpu, (uint64_t)dl->x86dlopen);
            printf_dlsym(LOG_DEBUG, "warning call x86dlopen filename is %s %x\n", (char *)filename, flag);
            return NULL;
#else
            uint64_t ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlopen, 2, filename, flag);
            printf_dlsym(LOG_DEBUG, "warning call call x86dlopen filename %s %x ret=0x%lx\n",  (char *)filename, flag, ret);
            //lsassert(0);
            if (ret) {
                return (void *)ret;
            }
            if(!dl->last_error)
                dl->last_error = box_malloc(129);
            snprintf(dl->last_error, 129, "filename \"%s\" flag=%x\n", (char *)filename, flag);
            printf_dlsym(LOG_NEVER, "%p return %p\n", dl->last_error, (void*)NULL);
            return NULL;
#endif
        }
        if(AddNeededLib(NULL, NULL, NULL, is_local, bindnow, libs, 1, my_context)) {
            printf_dlsym(strchr(rfilename,'/')?LOG_DEBUG:LOG_INFO, "Warning: Cannot dlopen(\"%s\"/%p, %X)\n", rfilename, filename, flag);
            if(!dl->last_error)
                dl->last_error = box_malloc(129);
            snprintf(dl->last_error, 129, "Cannot dlopen(\"%s\"/%p, %X)\n", rfilename, filename, flag);
            return NULL;
        }
        lib = GetLibInternal(rfilename);
        if (!lib) return NULL;
        lib->x86dlopenflag = flag;
        if (lib && lib->type == LIB_EMULATED) {
            // if dlopened = 0 ---> lib added but not loaded
            int libidx = GetElfIndex(lib);
            lsassert(libidx >= 0);
            elfheader_t * h = my_context->elfs[libidx];
            lsassert(h);
            if (!h->latx_hasfix || !lib->x86linkmap) {//lib->x86linkmap is null ---- this lib has been needed by other elf and opened
                callx86dlopen(rfilename, flag, h, is_local);
            }
        }
        //TODO:RunDeferedElfInit;
    } else {
        // check if already dlopenned...
        for (size_t i=0; i<dl->lib_sz; ++i) {
            if(!dl->libs[i]) {
                dl->count[i] = dl->count[i]+1;
                return (void*)(i+1);
            }
        }
        printf_dlsym(LOG_DEBUG, "Call to dlopen(NULL, %X) forword call x86dlopen \n", flag);
        lsassert(dl->x86dlopen);
        __MY_CPU;
        Push64(cpu, (uint64_t)dl->x86dlopen);
        return NULL;
    }
    //get the lib and add it to the collection

    if(dl->lib_sz == dl->lib_cap) {
        dl->lib_cap += 4;
        dl->libs = (library_t**)box_realloc(dl->libs, sizeof(library_t*)*dl->lib_cap);
        dl->count = (size_t*)box_realloc(dl->count, sizeof(size_t)*dl->lib_cap);
        dl->dlopened = (size_t*)box_realloc(dl->dlopened, sizeof(size_t)*dl->lib_cap);
        // memset count...
        memset(dl->count+dl->lib_sz, 0, (dl->lib_cap-dl->lib_sz)*sizeof(size_t));
    }
    intptr_t idx = dl->lib_sz++;
    dl->libs[idx] = lib;
    dl->count[idx] = dl->count[idx]+1;
    dl->dlopened[idx] = dlopened;
    printf_dlsym(LOG_DEBUG, "dlopen: New handle %p (%s), dlopened=%ld\n", (void*)(idx+1), (char*)filename, dlopened);
    if (lib && lib->type == LIB_EMULATED) {
        return lib->x86linkmap;
    }
    return (void*)(idx+1);
}

void* my_dlmopen(void* lmid, void *filename, int flag)
{
    if(lmid) {
        printf_dlsym(LOG_INFO, "Warning, dlmopen(%p, %p(\"%s\"), 0x%x) called with lmid not LMID_ID_BASE (unsupported)\n", lmid, filename, filename?(char*)filename:"self", flag);
    }
    // lmid is ignored for now...
    return my_dlopen(filename, flag);
}

KHASH_SET_INIT_INT(libs);

static int recursive_dlsym_lib(kh_libs_t* collection, library_t* lib, const char* rsymbol, uintptr_t *start, uintptr_t *end, int version, const char* vername)
{
    if(!lib)
        return 0;
    khint_t k = kh_get(libs, collection, (uintptr_t)lib);
    if(k != kh_end(collection))
        return 0;
    int ret;
    kh_put(libs, collection, (uintptr_t)lib, &ret);
    // look in the library itself
    khint_t pre_k = kh_str_hash_func(rsymbol);
    if(lib->get(lib, rsymbol, pre_k, start, end, version, vername, 1))
        return 1;
    // look in other libs
    int n = GetNeededLibN(lib);
    for (int i=0; i<n; ++i) {
        library_t *l = GetNeededLib(lib, i);
        if(recursive_dlsym_lib(collection, l, rsymbol, start, end, version, vername))
            return 1;
    }

    return 0;
}

static int my_dlsym_lib(library_t* lib, const char* rsymbol, uintptr_t *start, uintptr_t *end, int version, const char* vername)
{
    kh_libs_t *collection = kh_init(libs);
    int ret = recursive_dlsym_lib(collection, lib, rsymbol, start, end, version, vername);
    kh_destroy(libs, collection);

    return ret;
}

void* my_dlsym(void *handle, void *symbol){
    dlprivate_t *dl = my_context->dlprivate;
    uintptr_t start = 0, end = 0;
    char* rsymbol = (char*)symbol;
    CLEARERR
    if (!dl->x86dlsym) {
        init_x86dlfun();
        lsassert(dl->x86dlsym);
    }
    printf_dlsym(LOG_DEBUG, "Call to dlsym(%p, \"%s\")%s\n", handle, rsymbol, dlsym_error?"":"\n");
   //lsassert(!strstr(rsymbol, "XcursorGetDefaultSize"));
    if(handle==NULL) {
        // special case, look globably
        // special case (RTLD_DEFAULT)
#ifdef LATX_RELOCATION_SAVE_SYMBOLS
        if(GetGlobalSymbolStartEnd(my_context->maplib, rsymbol, &start, &end, NULL, -1, NULL)) {
            printf_dlsym(LOG_NEVER, "%p\n", (void*)start);
            return (void*)start;
        }
#endif
#if 0
        lsassert(dl->x86dlsym);
        __MY_CPU;
        Push64(cpu, (uint64_t)dl->x86dlsym);
        printf_dlsym(LOG_DEBUG, "warning call x86dlsym filename is NULL\n");
        return NULL;
#else
        uint64_t ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlsym, 2, handle, symbol);
        printf_dlsym(LOG_DEBUG, "warning call x86dlsym filename is NULL ret=0x%lx\n", ret);
        if (ret) {
            return (void *)ret;
        } else {
            if(GetGlobalSymbolStartEnd(my_context->maplib, rsymbol, &start, &end, NULL, -1, NULL)) {
                printf_dlsym(LOG_NEVER, "%p\n", (void*)start);
                return (void*)start;
            }
            printf_dlsym(LOG_NEVER, "debug my %d\n", __LINE__);
        }
        if(!dl->last_error)
            dl->last_error = box_malloc(129);
        snprintf(dl->last_error, 129, "Symbol \"%s\" not found in %p)\n", rsymbol, handle);
        printf_dlsym(LOG_NEVER, "%p return %p\n", dl->last_error, (void*)NULL);
        return NULL;
#endif
    }
    if(handle==(void*)~0LL) {
        // special case (RTLD_NEXT) -- call x86dlsym
        lsassert(dl->x86dlsym);
        __MY_CPU;
        Push64(cpu, (uint64_t)dl->x86dlsym);
        printf_dlsym(LOG_DEBUG, "warning call x86dlsym filename is RTLD_NEXT\n");
        return NULL;
    }
    size_t nlib = (size_t)handle;
    if(nlib > dl->lib_sz) {
        for (int i = 0; i < dl->lib_sz; i++) {
            if (dl->libs[i] && dl->libs[i]->active && dl->libs[i]->type == LIB_EMULATED && ((size_t)dl->libs[i]->x86linkmap) == nlib) {
                nlib = i + 1;
                break;
            }
        }
    }
    --nlib;
    // size_t is unsigned
    if(nlib>=dl->lib_sz) {
#ifdef LATX_RELOCATION_SAVE_SYMBOLS
        if(GetGlobalSymbolStartEnd(my_context->maplib, rsymbol, &start, &end, NULL, -1, NULL)) {
            printf_dlsym(LOG_NEVER, "%p\n", (void*)start);
            return (void*)start;
        }
#endif
        const char* lmfile = ((struct link_map *)handle)->l_name;
        if (strlen(lmfile)) {
            const char* libs[] = {basename(lmfile)};
            //try to wrapper.
            int iswrapped = 0.;
            if (FindLibIsWrapped((char *)libs[0])) {
                //if file is wrapped.
                iswrapped = 1;
                printf_dlsym(LOG_DEBUG, "find lib \"%s\" shuold be wrapped. init it.\n", libs[0]);
                if(AddNeededLib(NULL, NULL, NULL, 0, 1, libs, 1, my_context)) {
                    printf_dlsym(LOG_DEBUG, "Warning: Cannot AddNeededLib(\"%s\")\n", libs[0]);
                }
                printf_dlsym(LOG_DEBUG, "info: success AddNeededLib(\"%s\")\n", libs[0]);
                if(GetGlobalSymbolStartEnd(my_context->maplib, rsymbol, &start, &end, NULL, -1, NULL)) {
                    printf_dlsym(LOG_NEVER, "%p\n", (void*)start);
                    return (void*)start;
                }
            }
            if (iswrapped) {
                //Perhaps exe want to test func for earch libs, return nil.
                printf_dlsym(LOG_NEVER, "%p\n", (void*)NULL);
                return NULL;
            }
        }
#if !defined(LATX_RELOCATION_SAVE_SYMBOLS)
        else {//dlopen(NULL) --- dlopen self maplink filename is "NULL".
                if(GetGlobalSymbolStartEnd(my_context->maplib, rsymbol, &start, &end, NULL, -1, NULL)) {
                    printf_dlsym(LOG_NEVER, "%p\n", (void*)start);
                    return (void*)start;
                }
        }
#endif
        __MY_CPU;
#if FORWORDBACK
        lsassert(dl->x86dlsym);
        Push64(cpu, (uint64_t)dl->x86dlsym);
        printf_dlsym(LOG_DEBUG, "warning call x86dlsym filename is %s 0x%lx %s\n", strlen(lmfile)?lmfile:"NULL", cpu->regs[R_EDI], (char*)symbol);
        return NULL;
#else
        uint64_t ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlsym, 2, cpu->regs[R_EDI], symbol);
        printf_dlsym(LOG_DEBUG, "warning call call x86dlsym filename is %s handle 0x%lx ret=0x%lx\n", strlen(lmfile)?lmfile:"NULL", cpu->regs[R_EDI], ret);
        if (ret) {
            return (void *)ret;
        }
        if(!dl->last_error)
            dl->last_error = box_malloc(129);
        snprintf(dl->last_error, 129, "Symbol \"%s\" not found in %p)\n", rsymbol, handle);
        printf_dlsym(LOG_NEVER, "%p return %p\n", dl->last_error, (void*)NULL);
        return NULL;
#endif
    }
    if(dl->count[nlib]==0) {
        if(!dl->last_error)
            dl->last_error = box_malloc(129);
        snprintf(dl->last_error, 129, "Bad handle %p (already closed))\n", handle);
        printf_dlsym(LOG_NEVER, "%p return %p\n", dl->last_error, (void*)NULL);
        return NULL;
    }
    if(dl->libs[nlib]) {
        if(my_dlsym_lib(dl->libs[nlib], rsymbol, &start, &end, -1, NULL)==0) {
            // not found
            __MY_CPU;
            #if 1
            if(!dl->libs[nlib]->x86linkmap) {
                //redlopen
                uint64_t ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlopen, 2, dl->libs[nlib]->name, dl->libs[nlib]->x86dlopenflag);
                if (!ret) {//user sometime test for finding a func.
                    printf_dlsym(LOG_NEVER, "redlopen %p return %p\n", rsymbol, (void*)NULL);
                    return NULL;
                }
		lsassert(ret);
                dl->libs[nlib]->x86linkmap = (void *)ret;
                ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlsym, 2, dl->libs[nlib]->x86linkmap , cpu->regs[R_ESI]);
                printf_dlsym(LOG_DEBUG, "call x86dlsym filename %s is wrapped but not find symbol, dlsym(%p, %s) ret=0x%lx\n",
                dl->libs[nlib]->name, dl->libs[nlib]->x86linkmap, (char *)cpu->regs[R_ESI], ret);
                return (void *)ret;
            }
            #endif
            lsassert(dl->x86dlsym);
            if (dl->libs[nlib]->x86linkmap != handle) {
                cpu->regs[R_EDI] = (uintptr_t)dl->libs[nlib]->x86linkmap;
            }
#if FORWORDBACK
            Push64(cpu, (uint64_t)dl->x86dlsym);
            printf_dlsym(LOG_DEBUG, "warning call x86dlsym filename is %s %lx\n", dl->libs[nlib]->x86linkmap->l_name, cpu->regs[R_EDI]);
            return NULL;
#else
            uint64_t ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dlsym, 2, cpu->regs[R_EDI], cpu->regs[R_ESI]);
            printf_dlsym(LOG_DEBUG, "call x86dlsym filename is %s %s ret=0x%lx\n", dl->libs[nlib]->x86linkmap->l_name, (char *)cpu->regs[R_ESI], ret);
            if (ret) {
                return (void *)ret;
            }
            if(!dl->last_error)
                dl->last_error = box_malloc(129);
            snprintf(dl->last_error, 129, "Symbol \"%s\" not found in %p)\n", rsymbol, handle);
            printf_dlsym(LOG_NEVER, "%p return %p\n", dl->last_error, (void*)NULL);
            return NULL;
#endif
        }
    } else {
        // still usefull?
        //  => look globably
#ifdef LATX_RELOCATION_SAVE_SYMBOLS
        if(GetGlobalSymbolStartEnd(my_context->maplib, rsymbol, &start, &end, NULL, -1, NULL)) {
            printf_dlsym(LOG_NEVER, "%p\n", (void*)start);
            return (void*)start;
        }
#endif
        if(!dl->last_error)
            dl->last_error = box_malloc(129);
        snprintf(dl->last_error, 129, "Symbol \"%s\" not found in %p)\n", rsymbol, handle);
        printf_dlsym(LOG_NEVER, "%p\n", NULL);
        lsassertm(0,"%s",dl->last_error);
        return NULL;
    }
    printf_dlsym(LOG_NEVER, "%p\n", (void*)start);
    return (void*)start;
}

int my_dlclose(void *handle)
{
    printf_dlsym(LOG_DEBUG, "Call to dlclose(%p)\n", handle);
    dlprivate_t *dl = my_context->dlprivate;
    CLEARERR
    if (!dl->x86dlclose) {
        init_x86dlfun();
        lsassert(dl->x86dlclose);
    }
    size_t nlib = (size_t)handle;
    if(nlib > dl->lib_sz) {
        for (int i = 0; i < dl->lib_sz; i++) {
            if (dl->libs[i] && dl->libs[i]->active && dl->libs[i]->type == LIB_EMULATED && ((size_t)dl->libs[i]->x86linkmap) == nlib) {
                nlib = i + 1;
                break;
            }
        }
    }
    --nlib;
    // size_t is unsigned
    if(nlib>=dl->lib_sz) {
        int ret = -1;
        if (dl->x86dlclose) {
            __MY_CPU;
            Push64(cpu, (uint64_t)dl->x86dlclose);
            return 0;
        }
        if(!dl->last_error)
            dl->last_error = box_malloc(129);
        snprintf(dl->last_error, 129, "Bad handle %p, ret = %d)\n", handle, ret);
        printf_dlsym(LOG_DEBUG, "dlclose: %s\n", dl->last_error);
        lsassertm(0,"%s",dl->last_error);
        return -1;
    }
    if(dl->count[nlib]==0) {
        if(!dl->last_error)
            dl->last_error = box_malloc(129);
        snprintf(dl->last_error, 129, "Bad handle %p (already closed))\n", handle);
        printf_dlsym(LOG_DEBUG, "dlclose: %s\n", dl->last_error);
        return -1;
    }
    dl->count[nlib] = dl->count[nlib]-1;
    if(dl->count[nlib]==0 && dl->dlopened[nlib]) {   // need to call Fini...
        int idx = GetElfIndex(dl->libs[nlib]);
        if(idx!=-1) {
            printf_dlsym(LOG_DEBUG, "dlclose: Call to Fini for %p\n", handle);
            InactiveLibrary(dl->libs[nlib]);
            if (dl->x86dlclose) {
                __MY_CPU;
                if (dl->libs[nlib]->x86linkmap != handle) {
                    cpu->regs[R_EDI] = (uintptr_t)dl->libs[nlib]->x86linkmap;
                }
                Push64(cpu, (uint64_t)dl->x86dlclose);
                return 0;
            }
        }
    }
    return 0;
}

char* my_dlerror(void)
{
    dlprivate_t *dl = my_context->dlprivate;
    return dl->last_error;
}

int my_dladdr1(void *addr, void *i, void** extra_info, int flags)
{
    //int dladdr(void *addr, Dl_info *info);
    dlprivate_t *dl = my_context->dlprivate;
    CLEARERR
    if (!dl->x86dladdr1) {
        init_x86dlfun();
        lsassert(dl->x86dladdr1);
    }
    Dl_info *info = (Dl_info*)i;
    printf_dlsym(LOG_DEBUG, "Warning: partially unimplement call to dladdr/dladdr1(%p, %p, %p, %d)\n", addr, info, extra_info, flags);
     __MY_CPU;
    uint64_t ret = 0;
    if (extra_info == NULL && flags == 0) {
        ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dladdr, 2, cpu->regs[R_EDI], cpu->regs[R_ESI]);
    } else {
        ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dladdr1, 4, cpu->regs[R_EDI], cpu->regs[R_ESI], cpu->regs[R_EDX], cpu->regs[R_ECX]);
    }
    printf_dlsym(LOG_DEBUG, "     call to x86dladdr1 return saddr=%p, fname=\"%s\", sname=\"%s\" ret=%ld\n", info->dli_saddr, info->dli_sname?info->dli_sname:"", info->dli_fname?info->dli_fname:"", ret);
    if (ret == 1) {
        return ret;
    }
    //emu->quit = 1;
    library_t* lib = NULL;
    info->dli_saddr = NULL;
    info->dli_fname = NULL;
    info->dli_sname = FindSymbolName(my_context->maplib, addr, &info->dli_saddr, NULL, &info->dli_fname, &info->dli_fbase, &lib);
    printf_dlsym(LOG_DEBUG, "     dladdr return saddr=%p, fname=\"%s\", sname=\"%s\"\n", info->dli_saddr, info->dli_sname?info->dli_sname:"", info->dli_fname?info->dli_fname:"");
    if(flags==RTLD_DL_SYMENT) {
        printf_dlsym(LOG_INFO, "Warning, unimplement call to dladdr1 with RTLD_DL_SYMENT flags\n");
    } else if (flags==RTLD_DL_LINKMAP) {
        printf_dlsym(LOG_INFO, "Warning, partially unimplemented call to dladdr1 with RTLD_DL_LINKMAP flags\n");
        *(linkmap_t**)extra_info = getLinkMapLib(lib);
    }
    return (info->dli_sname)?1:0;   // success is non-null here...
}
int my_dladdr(void *addr, void *i)
{
    dlprivate_t *dl = my_context->dlprivate;
    CLEARERR
    if (!dl->x86dladdr) {
        init_x86dlfun();
        lsassert(dl->x86dladdr);
    }
#ifdef CONFIG_LATX_DEBUG
    Dl_info *info = (Dl_info*)i;
#endif
    printf_dlsym(LOG_DEBUG, "Warning: partially unimplement call to dladdr(%p, %p)\n", addr, info);
     __MY_CPU;
    uint64_t ret = RunFunctionWithState((uintptr_t)my_context->dlprivate->x86dladdr, 2, cpu->regs[R_EDI], cpu->regs[R_ESI]);
    printf_dlsym(LOG_DEBUG, "     call to x86dladdr return saddr=%p, fname=\"%s\", sname=\"%s\" ret=%ld\n", info->dli_saddr, info->dli_sname?info->dli_sname:"", info->dli_fname?info->dli_fname:"", ret);
    if (ret == 1) {
        return ret;
    }
    return my_dladdr1(addr, i, NULL, 0);
}
void* my_dlvsym(void *handle, void *symbol, const char *vername)
{
    printf_dlsym(LOG_DEBUG, "Call to dlvsym(%p, \"%s\", %s)", handle, (char *)symbol, vername?vername:"(nil)");
    return my_dlsym(handle, symbol);
}

int my_dlinfo(void* handle, int request, void* info)
{
    printf_dlsym(LOG_DEBUG, "Call to dlinfo(%p, %d, %p)\n", handle, request, info);
    dlprivate_t *dl = my_context->dlprivate;
    CLEARERR
    lsassert(0);//latx not support yet.
    if (!dl->x86dlopen) {
        init_x86dlfun();
        lsassert(dl->x86dlopen);
    }
    size_t nlib = (size_t)handle;
    if(nlib > dl->lib_sz) {
        for (int i = 0; i < dl->lib_sz; i++) {
            if (dl->libs[i] && dl->libs[i]->active && dl->libs[i]->type == LIB_EMULATED && ((size_t)dl->libs[i]->x86linkmap) == nlib) {
                nlib = i + 1;
                break;
            }
        }
    }
    --nlib;
    // size_t is unsigned
    if(nlib>=dl->lib_sz) {
        if(!dl->last_error)
            dl->last_error = box_calloc(1, 129);
        snprintf(dl->last_error, 129, "Bad handle %p)\n", handle);
        printf_dlsym(LOG_DEBUG, "dlinfo: %s\n", dl->last_error);
        return -1;
    }
    #if 0
    if(!dl->dllibs[nlib].count || !dl->dllibs[nlib].full) {
        if(!dl->last_error)
            dl->last_error = box_calloc(1, 129);
        snprintf(dl->last_error, 129, "Bad handle %p (already closed))\n", handle);
        printf_dlsym(LOG_DEBUG, "dlinfo: %s\n", dl->last_error);
        return -1;
    }
    #endif
    library_t *lib = dl->libs[nlib];
    switch(request) {
        case 2: // RTLD_DI_LINKMAP
            {
                *(linkmap_t**)info = getLinkMapLib(lib);
            }
            return 0;
        default:
            printf_dlsym(LOG_NONE, "Warning, unsupported call to dlinfo(%p, %d, %p)\n", handle, request, info);
        if(!dl->last_error)
            dl->last_error = box_calloc(1, 129);
        snprintf(dl->last_error, 129, "unsupported call to dlinfo request:%d\n", request);
    }
    return -1;
}
#endif

#include "wrappedlib_init.h"
