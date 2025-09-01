#ifndef _FLAG_REDUCTION_H_
#define _FLAG_REDUCTION_H_

#include "latx-types.h"
#include "lsenv.h"

#define MAX_DEPTH 5 
#define FLAG_DEFINE(opcode, _use, _def, _undef) \
[dt_X86_INS_##opcode] = (IR1_EFLAG_USEDEF) \
{.use = _use, .def = _def, .undef = _undef}

typedef struct {
    uint8 use;
    uint8 def;
    uint8 undef;
} IR1_EFLAG_USEDEF;

#define __INVALID    (1 << 7)
#define __VALID      (0)

#define __CF         ((1 << CF_USEDEF_BIT_INDEX) | __VALID)
#define __PF         ((1 << PF_USEDEF_BIT_INDEX) | __VALID)
#define __AF         ((1 << AF_USEDEF_BIT_INDEX) | __VALID)
#define __ZF         ((1 << ZF_USEDEF_BIT_INDEX) | __VALID)
#define __SF         ((1 << SF_USEDEF_BIT_INDEX) | __VALID)
#define __OF         ((1 << OF_USEDEF_BIT_INDEX) | __VALID)
#define __DF         ((1 << DF_USEDEF_BIT_INDEX) | __VALID)

#define __NONE       (__VALID)
#define __OSAPF      (__OF | __SF | __AF | __PF)
#define __OSZPF      (__OF | __SF | __ZF | __PF)
#define __SZAPF      (__SF | __ZF | __AF | __PF)
#define __SZAPCF     (__SZAPF | __CF)
#define __ALL_EFLAGS (__OF | __SF | __ZF | __AF | __PF | __CF)

void flag_gen(IR1_INST *pir1);

#ifdef CONFIG_LATX_FLAG_REDUCTION
uint8 pending_use_of_succ(void *tb, int indirect_depth, int max_depth);
void flag_reduction(IR1_INST *pir1, uint8 *pending_use);
uint8 flag_reduction_check(TranslationBlock *tb);

#define DEF_FLAG_RDTN(_prex) \
        uint8 _prex##_pending_use = __ALL_EFLAGS;
#define CHK_FLAG_RDTN(_prex, _tb) \
        _prex##_pending_use = flag_reduction_check(_tb)
#ifdef CONFIG_LATX_DECODE_DEBUG
#define OPT_FLAG_RDTN(_prex, _inst) \
        do { \
            if (_inst->decode_engine == OPT_DECODE_BY_CAPSTONE) { \
                if (option_flag_reduction) { \
                    flag_reduction(_inst, &_prex##_pending_use); \
                } else { \
                    flag_gen(_inst); \
                } \
            } else { \
                if (option_flag_reduction) { \
                    flag_reduction_bd(_inst, &_prex##_pending_use); \
                } else { \
                    flag_gen_bd(_inst); \
                } \
            } \
        } while (0)
#else
#define OPT_FLAG_RDTN(_prex, _inst) \
        do { \
            if (option_flag_reduction) { \
                flag_reduction_bd(_inst, &_prex##_pending_use); \
            } else { \
                flag_gen_bd(_inst); \
            } \
        } while (0)
#endif
#define SAVE_FLAG_TO_TB(_prex, _tb) \
        do { \
            _tb->eflag_use = (_prex##_pending_use); \
        } while (0)

#else /* !CONFIG_LATX_FLAG_REDUCTION */

#define DEF_FLAG_RDTN(_prex)        ((void)0)
#define CHK_FLAG_RDTN(_prex, _tb)   ((void)0)
#define OPT_FLAG_RDTN(_prex, _inst) flag_gen(_inst)
#define SAVE_FLAG_TO_TB(_prex, _tb) ((void)0)
#endif

void flag_gen_bd(IR1_INST *pir1);
void flag_reduction_bd(IR1_INST *pir1, uint8 *pending_use);
const IR1_EFLAG_USEDEF *ir1_opcode_to_eflag_usedef_bd(IR1_INST *ir1);
uint8 pending_use_of_succ_bd(void *tb, int indirect_depth, int max_depth);
#endif
