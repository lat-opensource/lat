#ifndef _IR1_H_
#define _IR1_H_

#include "qemu/osdep.h"
#define LA_CAPSTONE
#ifdef CONFIG_LATX_CAPSTONE_GIT
#include "../capstone_git/include/capstone/capstone.h"
#define la_cs_malloc cs_malloc
#define la_cs_reg_name cs_reg_name
#define la_cs_group_name cs_group_name
#define la_cs_option cs_option
#define la_cs_open cs_open
#define la_cs_insn_name cs_insn_name
#else
#include "../capStone/include/capstone/capstone.h"
#endif

#include "latx-disassemble-trace.h"
#include "common.h"
#include "reg-map.h"

/* Mark the CS_MODE is i386 or X64 */
#ifndef TARGET_X86_64
#define CS_MODE CS_MODE_32
#else
#define CS_MODE CS_MODE_64
#endif
extern csh handle[2];


typedef enum {
    IR1_EFLAG_FIRST = 96,
    IR1_EFLAG_CF = IR1_EFLAG_FIRST,
    IR1_EFLAG_PF,
    IR1_EFLAG_AF,
    IR1_EFLAG_ZF,
    IR1_EFLAG_SF,
    IR1_EFLAG_OF,
    IR1_EFLAG_DF,
    IR1_EFLAG_LAST,
} IR1_EFLAG;

typedef struct {
    int8 top_increment;       /* top increment */
    int8 opnd_min_fpr_num[3]; /* [i]: fpu inst has i operands */
} FPU_NEED_MIN_FPR;

#define CF_USEDEF_BIT_INDEX 0
#define PF_USEDEF_BIT_INDEX 1
#define AF_USEDEF_BIT_INDEX 2
#define ZF_USEDEF_BIT_INDEX 3
#define SF_USEDEF_BIT_INDEX 4
#define OF_USEDEF_BIT_INDEX 5
#define DF_USEDEF_BIT_INDEX 6

#define CF_USEDEF_BIT (1 << CF_USEDEF_BIT_INDEX)
#define PF_USEDEF_BIT (1 << PF_USEDEF_BIT_INDEX)
#define AF_USEDEF_BIT (1 << AF_USEDEF_BIT_INDEX)
#define ZF_USEDEF_BIT (1 << ZF_USEDEF_BIT_INDEX)
#define SF_USEDEF_BIT (1 << SF_USEDEF_BIT_INDEX)
#define OF_USEDEF_BIT (1 << OF_USEDEF_BIT_INDEX)
#define DF_USEDEF_BIT (1 << DF_USEDEF_BIT_INDEX)

#define EAX_USEDEF_BIT (1 << eax_index)
#define ECX_USEDEF_BIT (1 << ecx_index)
#define EDX_USEDEF_BIT (1 << edx_index)
#define EBX_USEDEF_BIT (1 << ebx_index)
#define ESP_USEDEF_BIT (1 << esp_index)
#define EBP_USEDEF_BIT (1 << ebp_index)
#define ESI_USEDEF_BIT (1 << esi_index)
#define EDI_USEDEF_BIT (1 << edi_index)

#ifdef TARGET_X86_64
#define R8_USEDEF_BIT  (1 << (r8_index - r8_index))
#define R9_USEDEF_BIT  (1 << (r9_index - r8_index))
#define R10_USEDEF_BIT (1 << (r10_index - r8_index))
#define R11_USEDEF_BIT (1 << (r11_index - r8_index))
#define R12_USEDEF_BIT (1 << (r12_index - r8_index))
#define R13_USEDEF_BIT (1 << (r13_index - r8_index))
#define R14_USEDEF_BIT (1 << (r14_index - r8_index))
#define R15_USEDEF_BIT (1 << (r15_index - r8_index))
#endif

typedef dt_x86_insn IR1_OPCODE;
typedef dt_x86_op_type IR1_OPND_TYPE;
typedef dt_cs_x86_op IR1_OPND;
typedef dt_x86_prefix IR1_PREFIX;
typedef dt_x86_insn_group IR1_OPCODE_TYPE;

#define MAX_IR1_NUM_PER_TB TCG_MAX_INSNS

typedef struct IR1_INST {
    struct la_dt_insn *info;
    /**
     * @brief Current inst eflag information
     *
     * `_eflag_use` and `_eflag_def` define current inst eflag
     * information.
     *
     * - `_eflag_use` define current inst used eflag
     * - `_eflag_def` define current inst generate eflag, this maybe changed by
     * `flag-rdtn` or `flag-pattern`
     */
    union {
        uint16_t _eflag;
        struct {
            uint8_t _eflag_use;  /** < current inst used eflag */
            uint8_t _eflag_def;  /** < current inst generate eflag */
        };
    };

#define IR1_INVALID_MASK  0x01
#define IR1_PATTERN_MASK  0x02
    uint8_t cflag;              /** condition flag */
#ifdef CONFIG_LATX_HBR
#define SHBR_XMM_ZERO    0x00000000
#define SHBR_XMM_OTHER   0x00010000
#define SHBR_XMM_ALL     0x0000ffff
#define SHBR_XMM_MASK    0x0000ffff
#define SHBR_NEED_SRC    0x10000000
#define SHBR_NEED_DES    0x20000000
#define SHBR_UPDATE_DES  0x40000000
#define SHBR_NO_OPT_SRC  0x01000000
#define SHBR_NO_OPT_DES  0x02000000
#define GHBR_GPR_ALL     0x0000ffff
/* #define GHBR_GPR_ALL     0x0 */
    union {
        uint32_t xmm_def;
        uint32_t gpr_def;
    };
    union {
        uint32_t xmm_use;
        uint32_t gpr_use;
    };
#define SHBR_CAN_OPT32    0x02
#define SHBR_CAN_OPT64    0x04
#define GHBR_CAN_OPT      0x08
    uint8_t hbr_flag;
#endif
} IR1_INST;

extern IR1_OPND al_ir1_opnd;
extern IR1_OPND ah_ir1_opnd;
extern IR1_OPND ax_ir1_opnd;
extern IR1_OPND eax_ir1_opnd;
extern IR1_OPND rax_ir1_opnd;

extern IR1_OPND bl_ir1_opnd;
extern IR1_OPND bh_ir1_opnd;
extern IR1_OPND bx_ir1_opnd;
extern IR1_OPND ebx_ir1_opnd;
extern IR1_OPND rbx_ir1_opnd;

extern IR1_OPND cl_ir1_opnd;
extern IR1_OPND ch_ir1_opnd;
extern IR1_OPND cx_ir1_opnd;
extern IR1_OPND ecx_ir1_opnd;
extern IR1_OPND rcx_ir1_opnd;

extern IR1_OPND dl_ir1_opnd;
extern IR1_OPND dh_ir1_opnd;
extern IR1_OPND dx_ir1_opnd;
extern IR1_OPND edx_ir1_opnd;
extern IR1_OPND rdx_ir1_opnd;

extern IR1_OPND esp_ir1_opnd;
extern IR1_OPND rsp_ir1_opnd;

extern IR1_OPND ebp_ir1_opnd;
extern IR1_OPND rbp_ir1_opnd;

extern IR1_OPND si_ir1_opnd;
extern IR1_OPND esi_ir1_opnd;
extern IR1_OPND rsi_ir1_opnd;

extern IR1_OPND di_ir1_opnd;
extern IR1_OPND edi_ir1_opnd;
extern IR1_OPND rdi_ir1_opnd;

extern IR1_OPND eax_mem8_ir1_opnd;
extern IR1_OPND ecx_mem8_ir1_opnd;
extern IR1_OPND edx_mem8_ir1_opnd;
extern IR1_OPND ebx_mem8_ir1_opnd;
extern IR1_OPND esp_mem8_ir1_opnd;
extern IR1_OPND ebp_mem8_ir1_opnd;
extern IR1_OPND esi_mem8_ir1_opnd;
extern IR1_OPND edi_mem8_ir1_opnd;
extern IR1_OPND di_mem8_ir1_opnd;
extern IR1_OPND si_mem8_ir1_opnd;

extern IR1_OPND eax_mem16_ir1_opnd;
extern IR1_OPND ecx_mem16_ir1_opnd;
extern IR1_OPND edx_mem16_ir1_opnd;
extern IR1_OPND ebx_mem16_ir1_opnd;
extern IR1_OPND esp_mem16_ir1_opnd;
extern IR1_OPND ebp_mem16_ir1_opnd;
extern IR1_OPND esi_mem16_ir1_opnd;
extern IR1_OPND edi_mem16_ir1_opnd;

extern IR1_OPND eax_mem32_ir1_opnd;
extern IR1_OPND ecx_mem32_ir1_opnd;
extern IR1_OPND edx_mem32_ir1_opnd;
extern IR1_OPND ebx_mem32_ir1_opnd;
extern IR1_OPND esp_mem32_ir1_opnd;
extern IR1_OPND esp_mem64_ir1_opnd;
extern IR1_OPND ebp_mem32_ir1_opnd;
extern IR1_OPND esi_mem32_ir1_opnd;
extern IR1_OPND edi_mem32_ir1_opnd;
extern IR1_OPND si_mem16_ir1_opnd;
extern IR1_OPND di_mem16_ir1_opnd;
extern IR1_OPND si_mem32_ir1_opnd;
extern IR1_OPND di_mem32_ir1_opnd;
#ifdef TARGET_X86_64
extern IR1_OPND rax_mem64_ir1_opnd;
extern IR1_OPND rcx_mem64_ir1_opnd;
extern IR1_OPND rdx_mem64_ir1_opnd;
extern IR1_OPND rbx_mem64_ir1_opnd;
extern IR1_OPND rsp_mem64_ir1_opnd;
extern IR1_OPND rbp_mem64_ir1_opnd;
extern IR1_OPND rsi_mem64_ir1_opnd;
extern IR1_OPND rdi_mem64_ir1_opnd;
extern IR1_OPND rsi_mem8_ir1_opnd;
extern IR1_OPND rdi_mem8_ir1_opnd;
extern IR1_OPND rsi_mem32_ir1_opnd;
extern IR1_OPND rdi_mem32_ir1_opnd;
extern IR1_OPND rsi_mem16_ir1_opnd;
extern IR1_OPND rdi_mem16_ir1_opnd;
extern IR1_OPND esi_mem64_ir1_opnd;
extern IR1_OPND edi_mem64_ir1_opnd;
#endif

ADDRX ir1_disasm(IR1_INST *ir1, uint8_t *addr, ADDRX t_pc,
                    int ir1_num, void *pir1_base);

// TODO : avx_bcast
void ir1_opnd_build_reg(IR1_OPND * opnd, int size, dt_x86_reg reg);
void ir1_opnd_build_imm(IR1_OPND *opnd, int size, int64_t imm);
void ir1_opnd_build_mem(IR1_OPND *opnd, int size,
    dt_x86_reg base, int64_t disp);
void ir1_opnd_build_fp(IR1_OPND *, IR1_OPND_TYPE opnd_type, int size,
                       double fp);  // not used current
// not used current
// IR1_OPND ir1_opnd_new(IR1_OPND_TYPE opnd_type, int size, int reg_num, longx
// imm);
void ir1_opnd_check(IR1_OPND *);
IR1_OPND_TYPE ir1_opnd_type(IR1_OPND *opnd);
int ir1_opnd_size(IR1_OPND *opnd);
int ir1_addr_size(IR1_INST *inst);
int ir1_get_opnd_size(IR1_INST *inst);
int ir1_opnd_base_reg_bits_start(IR1_OPND *opnd);
dt_x86_reg ir1_opnd_index_reg(IR1_OPND *opnd);
dt_x86_reg ir1_opnd_base_reg(IR1_OPND *opnd);
int ir1_opnd_scale(IR1_OPND *opnd);
longx ir1_opnd_simm(IR1_OPND *opnd);
ulongx ir1_opnd_uimm(IR1_OPND *opnd);
/**
 * @brief get sign-imm from unsign-imm opnd
 *
 * @param opnd unsign-imm opnd (x86)
 * @return longx sign-imm
 */
longx ir1_opnd_u2simm(IR1_OPND *opnd);

/**
 * @brief get unsign-imm from sign-imm opnd
 *
 * @param opnd sign-imm opnd (x86)
 * @return ulongx unsign-imm
 */
ulongx ir1_opnd_s2uimm(IR1_OPND *opnd);

ulongx ir1_opnd_uimm_addr(IR1_OPND *opnd);
dt_x86_reg ir1_opnd_seg_reg(IR1_OPND *opnd);
int ir1_opnd_is_imm(IR1_OPND *opnd);
int ir1_opnd_is_8h(const IR1_OPND *opnd);
int ir1_opnd_is_8l(const IR1_OPND *opnd);
int ir1_opnd_is_16bit(const IR1_OPND *opnd);
int ir1_opnd_is_32bit(const IR1_OPND *opnd);
#ifdef TARGET_X86_64
int ir1_opnd_is_64bit(const IR1_OPND *opnd);
#endif
int ir1_opnd_is_same_reg(const IR1_OPND *opnd0, const IR1_OPND *opnd1);
bool ir1_opnd_is_uimm12(IR1_OPND *opnd);
bool ir1_opnd_is_simm12(IR1_OPND *opnd);
bool ir1_opnd_is_s2uimm12(IR1_OPND *opnd);
bool ir1_opnd_is_u2simm12(IR1_OPND *opnd);
int ir1_opnd_is_gpr_used(IR1_OPND *opnd, uint8 gpr_index);
int ir1_opnd_is_reg(const IR1_OPND *opnd);
int ir1_opnd_is_gpr(const IR1_OPND *opnd);
int ir1_opnd_is_fpr(const IR1_OPND *opnd);
int ir1_opnd_is_seg(const IR1_OPND *opnd);
int ir1_opnd_is_mmx(const IR1_OPND *opnd);
int ir1_opnd_is_xmm(const IR1_OPND *opnd);
int ir1_opnd_is_ymm(const IR1_OPND *opnd);
int ir1_opnd_is_mem(const IR1_OPND *opnd);
int ir1_opnd_has_base(IR1_OPND *opnd);
int ir1_opnd_has_index(IR1_OPND *opnd);
int ir1_opnd_has_seg(IR1_OPND *opnd);
int ir1_opnd_get_seg_index(IR1_OPND *opnd);
int ir1_index_reg_is_ymm(IR1_OPND *opnd);
IR1_PREFIX ir1_prefix(IR1_INST *ir1);
IR1_PREFIX ir1_prefix_opnd_size(IR1_INST *ir1);
#ifdef TARGET_X86_64
uint8_t ir1_rex(IR1_INST *ir1);
uint8_t ir1_rex_w(IR1_INST *ir1);
#endif
int ir1_opnd_num(IR1_INST *ir1);
ADDRX ir1_addr(IR1_INST *ir1);
ADDRX ir1_addr_next(IR1_INST *ir1);
ADDRX ir1_target_addr(IR1_INST *ir1);
IR1_OPCODE ir1_opcode(const IR1_INST *ir1);

int ir1_is_branch(IR1_INST *ir1);
int ir1_is_jump(IR1_INST *ir1);
int ir1_is_call(IR1_INST *ir1);
int ir1_is_return(IR1_INST *ir1);
int ir1_is_indirect(IR1_INST *ir1);
int ir1_is_syscall(IR1_INST *ir1);
bool ir1_is_tb_ending(IR1_INST *ir1);

bool ir1_is_cf_use(IR1_INST *ir1);
bool ir1_is_pf_use(IR1_INST *ir1);
bool ir1_is_af_use(IR1_INST *ir1);
bool ir1_is_zf_use(IR1_INST *ir1);
bool ir1_is_sf_use(IR1_INST *ir1);
bool ir1_is_of_use(IR1_INST *ir1);

bool ir1_is_cf_def(IR1_INST *ir1);
bool ir1_is_pf_def(IR1_INST *ir1);
bool ir1_is_af_def(IR1_INST *ir1);
bool ir1_is_zf_def(IR1_INST *ir1);
bool ir1_is_sf_def(IR1_INST *ir1);
bool ir1_is_of_def(IR1_INST *ir1);

uint8 ir1_get_eflag_use(IR1_INST *ir1);
uint8 ir1_get_eflag_def(IR1_INST *ir1);
void ir1_set_eflag_use(IR1_INST *, uint8 use);
void ir1_set_eflag_def(IR1_INST *, uint8 def);

void ir1_make_ins_JMP(IR1_INST *ir1, ADDRX addr, int32 off);
void ir1_make_ins_NOP(IR1_INST *ir1, ADDRX addr);
void ir1_make_ins_RET(IR1_INST *ir1, ADDRX addr);
void ir1_make_ins_LIBFUNC(IR1_INST *ir1, ADDRX addr);
int ir1_opnd_index_reg_num(IR1_OPND *opnd);
int ir1_opnd_base_reg_num(const IR1_OPND *opnd);
int ir1_opnd_vsib_index_reg_num(IR1_OPND *opnd);

int ir1_dump(IR1_INST *ir1);
int ir1_opcode_dump(IR1_INST *ir1);
const char * ir1_name(IR1_OPCODE op);
const char *ir1_group_name(dt_x86_insn_group grp);

bool ir1_need_calculate_of(IR1_INST *ir1);
bool ir1_need_calculate_cf(IR1_INST *ir1);
bool ir1_need_calculate_pf(IR1_INST *ir1);
bool ir1_need_calculate_af(IR1_INST *ir1);
bool ir1_need_calculate_zf(IR1_INST *ir1);
bool ir1_need_calculate_sf(IR1_INST *ir1);
bool ir1_need_calculate_any_flag(IR1_INST *ir1);
bool tr_opt_simm12(IR1_INST *ir1);
bool tr_opt_uimm12(IR1_INST *ir1);

bool ir1_translate(IR1_INST *ir1);

uint8_t ir1_get_opnd_num(const IR1_INST *);
IR1_OPND *ir1_get_opnd(const IR1_INST *, int i);

bool ir1_is_indirect_call(IR1_INST *);
bool ir1_is_indirect_jmp(IR1_INST *);
bool ir1_is_prefix_lock(IR1_INST *ir1);

const char *ir1_reg_name(dt_x86_reg reg);
IR1_INST *tb_ir1_inst(TranslationBlock *tb, const int i);
IR1_INST *tb_ir1_inst_last(TranslationBlock *tb);
int tb_ir1_num(TranslationBlock *tb);

#endif
