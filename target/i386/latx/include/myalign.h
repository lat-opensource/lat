#ifndef __MY_ALIGN__H_
#define __MY_ALIGN__H_
#include <stdint.h>
#include "lsenv.h"
#include "fpu/softfloat.h"
#include "box64context.h"
#include "qemu.h"
#include "elfloader_private.h"
#include "elf.h"
#include "fileutils.h"
#include "elfloader.h"
#include "khash.h"
#include "elfload_dump.h"
#include "librarian.h"
#include "wrapper.h"
#include "library.h"
#include "translate.h"
#include "wrappertbbridge.h"
#include "callback.h"

extern elfheader_t* elf_header;
extern const char *interp_prefix;
extern int latx_wine;
extern int is_user_map;
#define M_F_FD_WINE 0
#define M_F_FD_LD 1
typedef struct {
    int type;
    int inited;
    const char *match;
    void (*mhanle)(char *, abi_ulong);
} my_foundfd_t;

typedef struct x64_va_list_s {
   unsigned int gp_offset;
   unsigned int fp_offset;
   void *overflow_arg_area;
   void *reg_save_area;
} x64_va_list_t[1];

#define X64_VA_MAX_REG  (6*8)
#define X64_VA_MAX_XMM  ((6*8)+(8*16))

#if defined(__loongarch64) || defined(__powerpc64__) || defined(__riscv)
#define __MY_CPU CPUX86State *cpu = (CPUX86State *)lsenv->cpu_state
#define __MY_R_RSP cpu->regs[R_ESP]

#define CREATE_SYSV_VALIST(A) \
  va_list sysv_varargs = (va_list)A
// not creating CONVERT_VALIST(A) on purpose
// this one will create a VA_LIST from x64_va_list using only GPRS and 100 stack element
#define CREATE_VALIST_FROM_VALIST(VA, SCRATCH)                          \
  va_list sysv_varargs;                                                 \
  {                                                                     \
    if((VA)->fp_offset!=X64_VA_MAX_XMM) printf_log(LOG_DEBUG, "Warning: %s: CREATE_VALIST_FROM_VALIST with %d XMM register!\n", __FUNCTION__, (X64_VA_MAX_XMM - (VA)->fp_offset)/16);\
    uintptr_t *p = (uintptr_t*)(SCRATCH);                               \
    int n = (X64_VA_MAX_REG - (VA)->gp_offset)/8;                       \
    if(n) memcpy(&p[0], (VA)->reg_save_area+X64_VA_MAX_REG-n*8, n*8);   \
    memcpy(&p[n], (VA)->overflow_arg_area, 20*8);                       \
    sysv_varargs = (va_list)p;                                          \
  }
// this is an approximation, and if the va_list have some float/double, it will fail!
// if the funciton needs more than 100 args, it will also fail
#define CREATE_VALIST_FROM_VAARG(STACK, SCRATCH, N)                     \
  va_list sysv_varargs;                                                 \
  {                                                                     \
    uintptr_t *p = (uintptr_t*)(SCRATCH);                               \
    __MY_CPU;                                                        \
    p[0]=cpu->regs[R_EDI]; p[1]=cpu->regs[R_ESI]; p[2]=cpu->regs[R_EDX];    \
    p[3]=cpu->regs[R_ECX]; p[4]=cpu->regs[R_R8]; p[5]=cpu->regs[R_R9];     \
    memcpy(&p[6], STACK, 20*8);                                         \
    sysv_varargs = (va_list)&p[N];                                      \
  }
#else
#error Unknown architecture!
#endif

#define VARARGS sysv_varargs
#define PREPARE_VALIST CREATE_SYSV_VALIST(__MY_R_RSP)
#define VARARGS_(A) sysv_varargs
#define PREPARE_VALIST_(A) CREATE_SYSV_VALIST(A)
uintptr_t getVArgs(int pos, uintptr_t* b, int N);
void* getVargN(int n);
void myStackAlign(const char* fmt, uint64_t* st, uint64_t* mystack, int xmm, int pos);
void myStackAlignW(const char* fmt, uint64_t* st, uint64_t* mystack, int xmm, int pos);
void myStackAlignScanf(const char* fmt, uint64_t* st, uint64_t* mystack, int pos);
void myStackAlignScanfW(const char* fmt, uint64_t* st, uint64_t* mystack, int pos);
#ifndef CONVERT_VALIST
void myStackAlignValist(const char* fmt, uint64_t* mystack, x64_va_list_t va);
void myStackAlignWValist(const char* fmt, uint64_t* mystack, x64_va_list_t va);
void myStackAlignScanfValist(const char* fmt, uint64_t* mystack, x64_va_list_t va);
void myStackAlignScanfWValist(const char* fmt, uint64_t* mystack, x64_va_list_t va);
#endif
long double LD2localLD(void* ld);
double FromLD(void* ld);
int of_convert(int);    // x86->arm

struct x64_stat64 {                   /* x86_64       arm64 */
    uint64_t st_dev;                    /* 0   */   /* 0   */
    uint64_t st_ino;                    /* 8   */   /* 8   */
    uint64_t st_nlink;                  /* 16  */   /* 20  */
    uint32_t st_mode;                   /* 24  */   /* 16  */
    uint32_t st_uid;                    /* 28  */   /* 24  */
    uint32_t st_gid;                    /* 32  */   /* 28  */
    int __pad0;                         /* 36  */   /* --- */
    uint64_t st_rdev;                   /* 40  */   /* 32  */
    int64_t st_size;                    /* 48  */   /* 48  */
    int64_t st_blksize;                 /* 56  */   /* 56  */
    uint64_t st_blocks;                 /* 64  */   /* 64  */
    struct timespec st_atim;            /* 72  */   /* 72  */
    struct timespec st_mtim;            /* 88  */   /* 88  */
    struct timespec st_ctim;            /* 104 */   /* 104 */
    uint64_t __glibc_reserved[3];       /* 120 */   /* 120 */
} __attribute__((packed));              /* 144 */   /* 128 */

static inline floatx80 longdouble_to_floatx80(CPUX86State *env, long double a)
{
    union {
        float128 f128;
        long double ld;
    } u;

    u.ld = a;
    return float128_to_floatx80(u.f128, &env->fp_status);
}
static inline floatx80 double_to_floatx80(CPUX86State *env, double a)
{
    union {
        float64 f64;
        double d;
    } u;

    u.d = a;
    return float64_to_floatx80(u.f64, &env->fp_status);
}
void UnalignStat64(const void* source, void* dest);
void AlignStat64(const void* source, void* dest);
void UnalignSemidDs(void *dest, const void* source);
void AlignSemidDs(void *dest, const void* source);
int getNCpu(void);
const char* getBoxCpuName(void);    // defined in my_cpuid.c
const char* getCpuName(void); // defined in my_cpu_id.c
double getBogoMips(void); // defined in my_cpu_id.c
int get_cpuMhz(void); // defined in my_cpu_id.c
void CreateMemorymapFile(box64context_t* context, int fd);
void UnalignEpollEvent(void* dest, void* source, int nbr); // Arm -> x86
void AlignEpollEvent(void* dest, void* source, int nbr); // x86 -> Arm
void* align_xcb_connection(void* src);
void unalign_xcb_connection(void* src, void* dst);
void* add_xcb_connection(void* src);
void del_xcb_connection(void* src);
int sync_xcb_connection(void* src);
int kzt_init(char** argv, int argc,char** target_argv, int  target_argc,
    struct linux_binprm* bprm);
int collectX86free(elfheader_t* h);
TranslationBlock * kzt_tb_find_exp(
            CPUState *cpu,
            TranslationBlock *last_tb,
            int tb_exit, uint32_t cflags);
void kzt_bridge_init(void);
void kzt_wine_bridge(abi_ulong start, int fd);
int latx_dpy_xcb_sync(void *v1);
elfheader_t* loadElfFromFile(const char* name);
#endif  //__MY_ALIGN__H_
