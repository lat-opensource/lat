#include "qemu/osdep.h"

#include "optimize-config.h"
#include "aot_recover_tb.h"
#include "latx-options.h"
#include "smc_reload.h"
#include "exec/translate-all.h"
#include "smc-reload-guest-base-test.h"

static unsigned int aot_register_count;
static void *reload_mprotect_addr;
static int reload_mprotect_prot;
static bool capture_reload_mprotect;

__thread TCGContext *tcg_ctx;
int option_smc_reload;
uintptr_t tcg_splitwx_diff;
uintptr_t qemu_host_page_size;
intptr_t qemu_host_page_mask;
uintptr_t guest_base;
bool have_guest_base;

int __wrap_mprotect(void *addr, size_t len G_GNUC_UNUSED, int prot)
{
    g_assert(capture_reload_mprotect);
    reload_mprotect_addr = addr;
    reload_mprotect_prot = prot;
    return 0;
}

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
    smc_reload_save_tb_code(reload->ir1_code_buffer, guest_page, code_size);
    g_assert(reload->ir1_code_buffer[0] == host_page[0]);

    smc_reload_tree_insert(reload);
    capture_reload_mprotect = true;
    g_assert(smc_page_reload(reload->page_addr, 0) == 1);
    capture_reload_mprotect = false;
    g_assert(reload_mprotect_addr == host_page);
    g_assert(reload_mprotect_prot == PROT_READ);
    g_assert(aot_register_count == 1);
    g_assert(smc_reload_tree_get_node_count() == 0);
    qemu_spin_destroy(&tb.jmp_lock);
    g_assert(munmap(mapping, mapping_size) == 0);
    return 0;
}
