#include "qemu/osdep.h"

#include "optimize-config.h"
#include "aot_recover_tb.h"
#include "latx-options.h"
#include "smc_reload.h"
#include "exec/translate-all.h"

static unsigned int aot_register_count;
static int test_page_flags;

__thread TCGContext *tcg_ctx;
int option_smc_reload;
uintptr_t tcg_splitwx_diff;
uintptr_t qemu_host_page_size;
intptr_t qemu_host_page_mask;
uintptr_t guest_base;
bool have_guest_base;

bool use_tu_jmp(TranslationBlock *tb)
{
    return tb->bool_flags & IS_TU_JMP;
}

void tb_set_jmp_target(TranslationBlock *tb G_GNUC_UNUSED,
                       int n G_GNUC_UNUSED, uintptr_t addr G_GNUC_UNUSED)
{
    g_assert_not_reached();
}

void tb_eflag_recover(TranslationBlock *tb G_GNUC_UNUSED,
                      int n G_GNUC_UNUSED)
{
    g_assert_not_reached();
}

void tb_target_set_nop(uintptr_t tc_ptr G_GNUC_UNUSED,
                       uintptr_t jmp_rx G_GNUC_UNUSED,
                       uintptr_t jmp_rw G_GNUC_UNUSED,
                       uintptr_t addr G_GNUC_UNUSED)
{
    g_assert_not_reached();
}

void aot_tb_register(TranslationBlock *tb)
{
    assert(tb != NULL);
    aot_register_count++;
}

int page_get_flags(target_ulong address G_GNUC_UNUSED)
{
    return test_page_flags;
}

bool page_is_shadow_not_shmm(target_ulong address G_GNUC_UNUSED)
{
    return false;
}

static void test_smc_reload_segment_candidate(void)
{
    static uint8_t aot_buffer;
    seg_info segment = { 0 };

    g_assert(smc_reload_segment_is_candidate(NULL));
    g_assert(smc_reload_segment_is_candidate(&segment));
    segment.buffer = &aot_buffer;
    g_assert(!smc_reload_segment_is_candidate(&segment));
}

int main(void)
{
    static TCGContext test_tcg_ctx;
    static uint8_t guest_code[] = { 0x90 };
    TranslationBlock tb = { 0 };
    SMCReloadInfo *reload = g_new0(SMCReloadInfo, 1);

    test_smc_reload_segment_candidate();
    tcg_ctx = &test_tcg_ctx;
    option_smc_reload = 1;
    tb.pc = (uintptr_t)guest_code;
    tb.size = sizeof(guest_code);
    tb.bool_flags = IS_TU_JMP;
    tb.tu_jmp[TU_TB_INDEX_NEXT] = 4;
    tb.tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;

    reload->tb_count = 1;
    reload->page_addr = (uintptr_t)guest_code & TARGET_PAGE_MASK;
    reload->tb_vector = g_new(TranslationBlock *, 1);
    reload->tb_vector[0] = &tb;
    reload->ir1_code_buffer = g_malloc(sizeof(guest_code));
    memcpy(reload->ir1_code_buffer, guest_code, sizeof(guest_code));

    test_page_flags = PAGE_READ;
    smc_reload_tree_insert(reload);
    g_assert(smc_page_reload(reload->page_addr, 0) == 1);
    g_assert(aot_register_count == 1);
    g_assert(tb.tu_jmp[TU_TB_INDEX_NEXT] == 4);
    g_assert(smc_reload_tree_get_node_count() == 0);
    tb.cflags = CF_INVALID;
    reload = g_new0(SMCReloadInfo, 1);
    reload->tb_count = 1;
    reload->page_addr = (uintptr_t)guest_code & TARGET_PAGE_MASK;
    reload->tb_vector = g_new(TranslationBlock *, 1);
    reload->tb_vector[0] = &tb;
    reload->ir1_code_buffer = g_malloc(sizeof(guest_code));
    memcpy(reload->ir1_code_buffer, guest_code, sizeof(guest_code));

    test_page_flags = PAGE_VALID | PAGE_EXEC;
    smc_reload_tree_insert(reload);
    g_assert(smc_page_reload(reload->page_addr, 0) == 1);
    g_assert(aot_register_count == 2);
    g_assert(tb.tu_jmp[TU_TB_INDEX_NEXT] == 4);
    g_assert(smc_reload_tree_get_node_count() == 0);
    return 0;
}
