#ifndef LATX_DISASSEMBLE_TRACE_H
#define LATX_DISASSEMBLE_TRACE_H

#define LA_CAPSTONE
#include <stddef.h>
#include "x86.h"
#include "qemu/osdep.h"
#include "qemu/log.h"
#include "error.h"

#define LA_STATIC_CAST(x, y) ((x) (y))

#define OPT_V1LACAPSTONE        (1 << 0)
#define OPT_V1NEXTCAPSTONE      (1 << 1)
#define OPT_V1LAXED             (1 << 2)
#define OPT_V1LAZYDIS             (1 << 3)
#define OPT_V2LACAPSTONE        (1 << 16)
#define OPT_V2NEXTCAPSTONE      (1 << 17)
#define OPT_V2LAXED             (1 << 18)
#define OPT_V2LAZYDIS             (1 << 19)

#ifdef CONFIG_LATX_DEBUG
#define LATX_DISASSEMBLE_TRACE_DEBUG
extern int dt_debug_enable;
#define dtassert(cond)                                                  \
    do {                                                                \
        if (!(cond)) {                                                  \
            fprintf(stderr,                                             \
                    "\033[31m [DT]%s failed in <%s> %s:%d \033[m\n",    \
                    __func__, #cond, __FILE__, __LINE__);               \
            while (dt_debug_enable) {                                   \
                fprintf(stderr, "%d: wait for debug!\n", getpid());     \
                sleep(10);                                              \
            }                                                           \
        }                                                               \
    } while (0)
#define LA_X86_INSN_ENUM(a, pre, dtpre, p)          \
{                                                   \
    a[pre##p].name = #p;                            \
    a[pre##p].id = dt_##dtpre##p;                   \
}

#define LA_X86_INSN_REENUM(a, pre, p)           \
{                                               \
    a[pre].name = #p;                           \
    a[pre].id = p;                              \
}

#define LA_X86_INSN_DEF(s, pre, ins)    \
{                                       \
    s[pre##ins].name = #ins;            \
    s[pre##ins].id = dt_X86_INS_##ins;  \
}

#define LA_X86_INSN_REDEF(s, pre, ins)          \
{                                               \
    s[pre].name = #ins;                         \
    s[pre].id = ins;                            \
}

#define LA_X86_INSN_DEF_LOCK(s, pre, ins)       \
{                                               \
    s[pre##ins].id = dt_X86_INS_##ins;          \
    s[pre##ins##_LOCK].id = dt_X86_INS_##ins;   \
    s[pre##ins].name = #ins;                    \
    s[pre##ins##_LOCK].name = #ins;             \
}

#define dtcmpmask(cond, mask, message)                  \
do {                                                    \
    if (!(cond)) {                                      \
                mask = 1;                               \
                message = #cond;                        \
    }                                                   \
} while (0)

#define dtcmpmask5(cond, mask, message, index, i)       \
do {                                                    \
    if (!(cond)) {                                      \
                mask = 1;                               \
                index = i;                              \
                message = #cond;                        \
    }                                                   \
} while (0)

#else
#define dtassert(cond)          ((void)0)
#define dtcmpmask(cond, mask, message)          ((void)0)
#define dtcmpmask5(cond, mask, message)          ((void)0)
#define LA_X86_INSN_ENUM(a, pre, dtpre, p)          \
{                                                   \
    a[pre##p].id = dt_##dtpre##p;                   \
}

#define LA_X86_INSN_REENUM(a, pre, p)               \
{                                                   \
    a[pre].id = p;                                  \
}

#define LA_X86_INSN_DEF(s, pre, ins)                \
{                                                   \
    s[pre##ins].id = dt_X86_INS_##ins;              \
}

#define LA_X86_INSN_REDEF(s, pre, ins)              \
{                                                   \
    s[pre].id = ins;                                \
}

#define LA_X86_INSN_DEF_LOCK(s, pre, ins)           \
{                                                   \
    s[pre##ins].id = dt_X86_INS_##ins;              \
    s[pre##ins##_LOCK].id = dt_X86_INS_##ins;       \
}
#endif
struct dt_x86 {
    uint8_t prefix[4];
    uint8_t opcode[4];
    uint8_t addr_size;
    uint8_t rex;
    uint8_t op_count;
    uint8_t modrm;
    dt_cs_x86_op operands[8];
    dt_x86_avx_cc avx_cc;
};
struct la_dt_insn{
    union {
        struct dt_x86 x86;
    };
    uint16_t size;
    uint64_t address;
    uint8_t bytes[24];
    unsigned int id;
#ifdef LATX_DISASSEMBLE_TRACE_DEBUG
    char mnemonic[32];
    char op_str[160];
#endif
};
typedef struct {
    const char* name;
    int id;
} la_name_enum_t;
typedef enum dt_cs_ac_type {
    dt_CS_AC_INVALID = 0,        ///< Uninitialized/invalid access type.
    dt_CS_AC_READ    = 1 << 0,   ///< Operand read from memory or register.
    dt_CS_AC_WRITE   = 1 << 1,   ///< Operand write to memory or register.
} dt_cs_ac_type; 
void disassemble_trace_init(int abi_bits, int args);
void disassemble_trace_loop(const uint8_t *code, size_t code_size,
    uint64_t address, size_t count, struct la_dt_insn *inputinsn, int mode);
void lacapstone_init(int abi_bits);
void nextcapstone_init(int abi_bits);
void gitcapstone_init(int abi_bits);
void laxed_init(int abi_bits);
void lazydis_init(int abi_bits);
int lacapstone_get(const uint8_t *code, size_t code_size,
        uint64_t address,
        size_t count, struct la_dt_insn **insn,
        int ir1_num, void *pir1_base);
int gitcapstone_get(const uint8_t *code, size_t code_size,
        uint64_t address,
        size_t count, struct la_dt_insn **insn,
        int ir1_num, void *pir1_base, int mode);
int nextcapstone_get(const uint8_t *code, size_t code_size,
        uint64_t address,
        size_t count, struct la_dt_insn **insn,
        int ir1_num, void *pir1_base);
int laxed_get(const uint8_t *code, size_t code_size,
        uint64_t address,
        size_t count, struct la_dt_insn **insn,
        int ir1_num, void *pir1_base, int mode);
int lazydis_get(const uint8_t *code, size_t code_size,
        uint64_t address,
        size_t count, struct la_dt_insn **insn,
        int ir1_num, void *pir1_base, int mode);
static inline void disassemble_trace_cmp_nop(
        const uint8_t *code, size_t code_size,
        uint64_t address,
        size_t count,
        struct la_dt_insn *inputinsn, int mode)
{

}
extern int (*la_disa_v1)(const uint8_t *code, size_t code_size,
        uint64_t address,
        size_t count, struct la_dt_insn **insn,
        int ir1_num, void *pir1_base, int mode);
extern void (*disassemble_trace_cmp)(const uint8_t *code, size_t code_size,
        uint64_t address,
        size_t count,
        struct la_dt_insn *inputinsn, int mode);

#endif
