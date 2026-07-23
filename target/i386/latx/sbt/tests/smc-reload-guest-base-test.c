#include "qemu/osdep.h"

#include "optimize-config.h"
#include "aot_recover_tb.h"
#include "latx-options.h"
#include "smc_reload.h"
#include "exec/translate-all.h"

static unsigned int aot_register_count;

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
    return PAGE_VALID | PAGE_READ | PAGE_WRITE | PAGE_EXEC;
}

bool page_is_shadow_not_shmm(target_ulong address G_GNUC_UNUSED)
{
    return false;
}

int main(void)
{
    static TCGContext test_tcg_ctx;
    const size_t code_size = 1;
    const size_t mapping_size = 2 * qemu_real_host_page_size;
    uint8_t *mapping;
    target_ulong guest_page;
    uint8_t *host_page;
    TranslationBlock tb = { 0 };
    SMCReloadInfo *reload = g_new0(SMCReloadInfo, 1);

    qemu_host_page_size = qemu_real_host_page_size;
    qemu_host_page_mask = -qemu_host_page_size;
    mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    g_assert(mapping != MAP_FAILED);
    guest_page = (target_ulong)(uintptr_t)mapping;
    host_page = mapping + qemu_host_page_size;
    guest_base = qemu_host_page_size;
    mapping[0] = 0xcc;
    host_page[0] = 0x90;
    g_assert(mprotect(mapping, qemu_host_page_size, PROT_NONE) == 0);

    tcg_ctx = &test_tcg_ctx;
    option_smc_reload = 1;
    tb.pc = guest_page;
    tb.size = code_size;
    tb.bool_flags = IS_TU_JMP;
    qemu_spin_init(&tb.jmp_lock);

    reload->tb_count = 1;
    reload->page_addr = guest_page;
    reload->tb_vector = g_new(TranslationBlock *, 1);
    reload->tb_vector[0] = &tb;
    reload->ir1_code_buffer = g_malloc(code_size);
    memcpy(reload->ir1_code_buffer, host_page, code_size);

    smc_reload_tree_insert(reload);
    g_assert(smc_page_reload(reload->page_addr, 0) == 1);
    g_assert(aot_register_count == 1);
    g_assert(smc_reload_tree_get_node_count() == 0);
    qemu_spin_destroy(&tb.jmp_lock);
    g_assert(munmap(mapping, mapping_size) == 0);
    return 0;
}
