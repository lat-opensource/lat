#ifndef _MACRO_INST_H_
#define _MACRO_INST_H_
#include "aot.h"

/* Load immediate */
void li_d(IR2_OPND opnd2, int64_t value);
void li_w(IR2_OPND opnd2, uint32_t lo32);
void li_wu(IR2_OPND opnd2, uint32_t lo32);

#define li_host_addr(opnd2, value)   li_d(opnd2, (ADDR)value)
#ifdef TARGET_X86_64
#define li_guest_addr(opnd2, value)   li_d(opnd2, value)
#else
#define li_guest_addr(opnd2, value)   li_wu(opnd2, value)
#endif

#ifndef CONFIG_LATX_AOT
#define aot_load_host_addr(opnd2, value, aot_rel_kind, addend)          \
li_host_addr(opnd2, value)
#define aot_load_guest_addr(opnd2, value, aot_rel_kind, addend)         \
li_guest_addr(opnd2, value)
#define aot_la_append_ir2_jmp_far(target, base, aot_rel_kind, addend)   \
la_far_jump(target, base)
#define aot_la_append_ir2_jmp_epilogue(target, base, aot_rel_kind, addend) \
la_jump_epilogue(target, base);

#else
#define aot_load_addr(opnd2, value, aot_rel_kind, addend)                  \
    TranslationBlock *tb = lsenv->tr_data->curr_tb;                        \
    do {                                                                   \
        uint32_t *prev_off, *diff_off;                                     \
        IR2_OPND pre_label = ra_alloc_label();                             \
        IR2_OPND cur_label = ra_alloc_label();                             \
        /* record the num of insts within two label */                     \
        IR2_OPND inst_off = ra_alloc_data();                               \
        la_label(pre_label);                                               \
        load_ireg_from_host_addr(opnd2, (ADDR)value);                      \
        la_label(cur_label);                                               \
        int rel_index =                                                    \
            add_rel_entry(aot_rel_kind, &prev_off, &diff_off, -1, addend); \
        la_inst_diff(inst_off, cur_label, pre_label);                      \
        la_data_st_rel_table(rel_index, (uintptr_t)prev_off -              \
            (uintptr_t)&rel_table[rel_index], pre_label);                  \
        la_data_st_rel_table(rel_index, (uintptr_t)diff_off -              \
            (uintptr_t)&rel_table[rel_index], inst_off);                   \
        TB_SET_REL(tb, rel_index);                                         \
    } while (0)
#define do_aot_load_guest_addr(opnd2, value, aot_rel_kind, addend)         \
    TranslationBlock *tb = lsenv->tr_data->curr_tb;                        \
    do {                                                                   \
        uint32_t *prev_off, *diff_off;                                     \
        IR2_OPND pre_label = ra_alloc_label();                             \
        IR2_OPND cur_label = ra_alloc_label();                             \
        /* record the num of insts within two label */                     \
        IR2_OPND inst_off = ra_alloc_data();                               \
        la_label(pre_label);                                               \
        load_ireg_from_guest_addr(opnd2, (ADDR)value);                      \
        la_label(cur_label);                                               \
        int rel_index =                                                    \
            add_rel_entry(aot_rel_kind, &prev_off, &diff_off, -1, addend); \
        la_inst_diff(inst_off, cur_label, pre_label);                      \
        la_data_st_rel_table(rel_index, (uintptr_t)prev_off -              \
            (uintptr_t)&rel_table[rel_index], pre_label);                  \
        la_data_st_rel_table(rel_index, (uintptr_t)diff_off -              \
            (uintptr_t)&rel_table[rel_index], inst_off);                   \
        TB_SET_REL(tb, rel_index);                                         \
    } while (0)
#define aot_load_host_addr(opnd2, value, aot_rel_kind, addend)          \
    do {                                                                \
        if (option_aot && in_pre_translate) {                           \
            aot_load_addr(opnd2, value, aot_rel_kind, addend);          \
        } else {                                                        \
            li_d(opnd2, (ADDR)value);                                   \
        }                                                               \
    } while (0)
#define aot_load_guest_addr(opnd2, value, aot_rel_kind, addend)         \
    do {                                                                \
        if (option_aot && in_pre_translate) {                           \
            do_aot_load_guest_addr(opnd2, value, aot_rel_kind, addend); \
        } else {                                                        \
            li_guest_addr(opnd2, (ADDR)value);                          \
        }                                                               \
    } while (0)
#define aot_la_append_ir2_jmp_far(target, base, aot_rel_kind, addend)          \
    do {                                                                       \
        TranslationBlock *tb = lsenv->tr_data->curr_tb;                        \
        if (option_aot && in_pre_translate) {                                  \
            uint32_t *prev_off, *diff_off;                                     \
            IR2_OPND pre_label = ra_alloc_label();                             \
            IR2_OPND cur_label = ra_alloc_label();                             \
            /* record the num of insts within two label */                     \
            IR2_OPND inst_off = ra_alloc_data();                               \
            la_label(pre_label);                                               \
            la_far_jump(target, base);                                         \
            la_label(cur_label);                                               \
            int rel_index =                                                    \
                add_rel_entry(aot_rel_kind, &prev_off, &diff_off, -1, addend); \
            la_inst_diff(inst_off, cur_label, pre_label);                      \
            la_data_st_rel_table(rel_index, (uintptr_t)prev_off -              \
                (uintptr_t)&rel_table[rel_index], pre_label);                  \
            la_data_st_rel_table(rel_index, (uintptr_t)diff_off -              \
                (uintptr_t)&rel_table[rel_index], inst_off);                   \
            TB_SET_REL(tb, rel_index);                                         \
        } else {                                                               \
            la_far_jump(target, base);                                         \
        }                                                                      \
    } while (0)
#define aot_la_append_ir2_jmp_epilogue(target, base, aot_rel_kind, addend)     \
    do {                                                                       \
        TranslationBlock *tb = lsenv->tr_data->curr_tb;                        \
        if (option_aot && in_pre_translate) {                                  \
            uint32_t *prev_off, *diff_off;                                     \
            IR2_OPND pre_label = ra_alloc_label();                             \
            IR2_OPND cur_label = ra_alloc_label();                             \
            /* record the num of insts within two label */                     \
            IR2_OPND inst_off = ra_alloc_data();                               \
            la_label(pre_label);                                               \
            la_jump_epilogue(target, base);                                    \
            la_label(cur_label);                                               \
            int rel_index =                                                    \
                add_rel_entry(aot_rel_kind, &prev_off, &diff_off, -1, addend); \
            la_inst_diff(inst_off, cur_label, pre_label);                      \
            la_data_st_rel_table(rel_index, (uintptr_t)prev_off -              \
                (uintptr_t)&rel_table[rel_index], pre_label);                  \
            la_data_st_rel_table(rel_index, (uintptr_t)diff_off -              \
                (uintptr_t)&rel_table[rel_index], inst_off);                   \
            TB_SET_REL(tb, rel_index);                                         \
        } else {                                                               \
            la_jump_epilogue(target, base);                                    \
        }                                                                      \
    } while (0)

#endif

/* load/store by opnd size */
void la_ld_by_op_size(IR2_OPND rd, IR2_OPND rj, int imm_si12, int op_size);
void la_st_by_op_size(IR2_OPND rd, IR2_OPND rj, int imm_si12, int op_size);
#endif
