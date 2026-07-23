#include "qemu/osdep.h"
#include "qemu/log.h"

#if defined(CONFIG_LATX_KZT)
#include "qemu.h"
#endif

#include "accel/tcg/internal.h"
#include "aot.h"
#include "exec/fasttb.h"
#include "latx-options.h"
#include "smc_reload.h"
#include "sysemu/tcg.h"
#include "ts.h"

static run_on_cpu_func queued_func;
static run_on_cpu_data queued_data;
static bool mmap_locked;

TBContext tb_ctx;
CPUTailQ cpus = QTAILQ_HEAD_INITIALIZER(cpus);
bool tcg_allowed = true;
int option_smc_reload;
int option_aot;
int qemu_loglevel;
__thread int in_pre_translate;
#if defined(CONFIG_LATX_KZT)
int option_kzt;
struct image_info info1;
#endif

int qemu_log(const char *fmt G_GNUC_UNUSED, ...)
{
    return 0;
}

void aot_exit_entry(CPUState *cpu G_GNUC_UNUSED, int is_end G_GNUC_UNUSED)
{
}

#ifdef CONFIG_PLUGIN
void qemu_plugin_flush_cb(void)
{
}
#endif

#if defined(CONFIG_LATX_KZT)
void init_tb_callback_bridge(CPUState *cpu G_GNUC_UNUSED,
                             void *opaque G_GNUC_UNUSED)
{
}
#endif

void async_safe_run_on_cpu(CPUState *cpu G_GNUC_UNUSED,
                           run_on_cpu_func func, run_on_cpu_data data)
{
    g_assert(queued_func == NULL);
    queued_func = func;
    queued_data = data;
}

void mmap_lock(void)
{
    g_assert(!mmap_locked);
    mmap_locked = true;
}

void mmap_unlock(void)
{
    g_assert(mmap_locked);
    mmap_locked = false;
}

bool have_mmap_lock(void)
{
    return mmap_locked;
}

bool qht_reset_size(struct qht *ht G_GNUC_UNUSED,
                    size_t n_elems G_GNUC_UNUSED)
{
    return false;
}

void tb_flush_remove_all(void)
{
    g_assert(mmap_locked);
}

void tcg_region_reset_all(void)
{
    g_assert(mmap_locked);
}

void latx_fast_jmp_cache_clear_all(CPUState *cpu G_GNUC_UNUSED)
{
}

static SMCReloadInfo *new_reload_node(target_ulong page_addr)
{
    SMCReloadInfo *node = g_new0(SMCReloadInfo, 1);

    node->page_addr = page_addr;
    return node;
}

static void run_queued_flush(CPUState *cpu)
{
    run_on_cpu_func func = queued_func;

    g_assert(func != NULL);
    queued_func = NULL;
    func(cpu, queued_data);
}

int main(void)
{
    CPUState cpu = { 0 };

    option_smc_reload = true;
    tb_ctx.tb_flush_count = 1;

    tb_flush(&cpu);
    g_assert(queued_func != NULL);
    smc_reload_tree_insert(new_reload_node(0x1000));
    run_queued_flush(&cpu);
    g_assert(smc_reload_tree_get_node_count() == 0);

    smc_reload_tree_insert(new_reload_node(0x2000));
    tb_flush(&cpu);
    g_assert(queued_func != NULL);
    tb_ctx.tb_flush_count++;
    run_queued_flush(&cpu);
    g_assert(smc_reload_tree_get_node_count() == 1);
    smc_reload_tree_clear();
    return 0;
}
