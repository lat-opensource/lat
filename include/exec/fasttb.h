#ifndef FASTTB_H
#define FASTTB_H

#define FASTTB_INVALID_PC ((unsigned long)-1)

struct FastTB {
    unsigned long pc;
    const void *ptr;    /* pointer to the translated code */
};

typedef struct LatxAotV2FastTB {
    unsigned long pc;
    const void *ptr;
    const void *context;
    const uint64_t *generation_address;
    uint64_t generation;
    const uint64_t *guest_slots_end;
    uint64_t guest_slot_count;
    uint64_t reserved;
} LatxAotV2FastTB;

_Static_assert(sizeof(LatxAotV2FastTB) == 64,
               "AOT v2 fast cache entries must remain power-of-two sized");

#define FASTTB_ILLINST_MAGIC 0x88888888

void latx_fast_jmp_cache_add(CPUState *cs, int h, struct TranslationBlock *tb);
void latx_fast_jmp_cache_clear(CPUState *cs, int h);
void latx_fast_jmp_cache_clear_all(CPUState *cs);
bool latx_fast_jmp_cache_init(void *env);
void latx_fast_jmp_cache_free_rcu(void *ptr);
void latx_aot_v2_fast_jmp_cache_add(CPUState *cs, int h,
                                    unsigned long guest_pc, const void *ptr,
                                    const void *context,
                                    const uint64_t *generation_address,
                                    uint64_t generation,
                                    const uint64_t *guest_slots_end,
                                    uint64_t guest_slot_count);
void latx_aot_v2_fast_jmp_cache_set_context(CPUState *cs,
                                            const void *context);
#endif
