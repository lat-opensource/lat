/**
 * @file jrra.h
 * @author Hanlu Li <heuleehanlu@gmail.com>
 * @brief JRRA optimization header file
 */
#ifndef _JRRA_H_
#define _JRRA_H_
typedef enum JrraCallType {
    JRRA_CALL_DIRECT,
    JRRA_CALL_INDIRECT,
} JrraCallType;

void jrra_install_smc_sentinel(TranslationBlock *tb);
void jrra_tb_reset(TranslationBlock *);
bool jrra_preserve_current_tb_head(TranslationBlock *tb,
            TranslationBlock *current_tb,
            bool current_tb_modified, uint32_t *inst_out);
void jrra_restore_current_tb_head(TranslationBlock *tb,
            TranslationBlock *current_tb,
            bool current_tb_modified, uint32_t inst);
bool jrra_handle_sigill(CPUArchState *env, ucontext_t *uc);
void jrra_pre_translate(void** list, int num, CPUState *cpu,
                        uint32_t flags, uint32_t cflags);
void jrra_context_switch_bt_to_native(void);
void jrra_relocate_return_target(TranslationBlock *tb, uintptr_t new_base);
#endif
