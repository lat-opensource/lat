#ifndef FASTTB_H
#define FASTTB_H

#define FASTTB_INVALID_PC ((unsigned long)-1)

struct FastTB {
    unsigned long pc;
    const void *ptr;    /* pointer to the translated code */
};

#define FASTTB_ILLINST_MAGIC 0x88888888

#ifndef LATX_SHADOW_JMP_BITS
#define LATX_SHADOW_JMP_BITS 20
#endif
#define LATX_SHADOW_JMP_SIZE (1U << LATX_SHADOW_JMP_BITS)
#define LATX_SHADOW_JMP_HASH_MULT UINT64_C(11400714819323198485)
#define LATX_SHADOW_JMP_TOMBSTONE ((struct TranslationBlock *)1)

typedef struct LatxShadowJmpEntry {
    struct TranslationBlock *tb;
} LatxShadowJmpEntry;

extern LatxShadowJmpEntry latx_shadow_jmp_entries[];

void latx_fast_jmp_cache_add(CPUState *cs, int h, struct TranslationBlock *tb);
void latx_fast_jmp_cache_clear(CPUState *cs, int h);
void latx_fast_jmp_cache_clear_all(CPUState *cs);
bool latx_fast_jmp_cache_init(void *env);
void latx_fast_jmp_cache_free_rcu(void *ptr);
void latx_shadow_jmp_cache_add(struct TranslationBlock *tb);
void latx_shadow_jmp_cache_remove(struct TranslationBlock *tb);
void latx_shadow_jmp_cache_clear_all(void);
struct TranslationBlock *latx_shadow_jmp_cache_lookup(
    uint64_t pc, uint32_t flags, uint32_t cflags);
#endif
