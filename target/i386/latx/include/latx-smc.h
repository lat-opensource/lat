#ifndef _LATX_SMC_H_
#define _LATX_SMC_H_

int smc_store_helper_vst(void *env,
        uint64_t address, uint64_t val0, uint64_t val1);
int smc_store_helper_vst_x4(void *env,
        uint64_t address, int r0, int r1, int r2, int r3);
int smc_store_helper_st(void *env,
        uint64_t address, uint64_t val, int s);

void smc_hook_get_xmm(void *_env, int reg, uint64_t *val);

//#define SMC_HELPER_USE_PAGE_UNPROTECT
/* define this to use page_unprotect() directly in smc_store_helper.
 * Otherwise, smc helper will perform tb invalidate by itself and try
 * to make guest page writable after invalidation.
 */

/* Used if current tb modified in smc store helper.
 * To restore guest cpu state, aka. CPUArchState. */
void smc_store_helper_restore_cpu_state(void *_env);

void smc_set_interpret_glue_code(void *uc, unsigned int inst, int rj);

#endif
