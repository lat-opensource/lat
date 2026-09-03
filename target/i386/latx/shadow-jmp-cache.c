#include "qemu/osdep.h"

#include "exec/exec-all.h"
#include "exec/fasttb.h"

LatxShadowJmpEntry latx_shadow_jmp_entries[LATX_SHADOW_JMP_SIZE];

static inline unsigned int latx_shadow_jmp_hash(uint64_t pc)
{
    return (pc * LATX_SHADOW_JMP_HASH_MULT) >>
           (64 - LATX_SHADOW_JMP_BITS);
}

static bool latx_shadow_jmp_key_equal(const TranslationBlock *tb,
                                      uint64_t pc, uint32_t flags,
                                      uint32_t cflags)
{
    return tb->pc == pc && tb->flags == flags && tb_cflags(tb) == cflags;
}

TranslationBlock *latx_shadow_jmp_cache_lookup(uint64_t pc,
                                               uint32_t flags,
                                               uint32_t cflags)
{
    unsigned int h = latx_shadow_jmp_hash(pc);

    for (unsigned int i = 0; i < LATX_SHADOW_JMP_SIZE; i++) {
        LatxShadowJmpEntry *entry =
            &latx_shadow_jmp_entries[(h + i) &
                                     (LATX_SHADOW_JMP_SIZE - 1)];
        TranslationBlock *tb = qatomic_read(&entry->tb);

        if (!tb) {
            return NULL;
        }
        if (tb != LATX_SHADOW_JMP_TOMBSTONE &&
            latx_shadow_jmp_key_equal(tb, pc, flags, cflags)) {
            return tb;
        }
    }
    return NULL;
}

void latx_shadow_jmp_cache_add(TranslationBlock *tb)
{
    unsigned int h = latx_shadow_jmp_hash(tb->pc);
    LatxShadowJmpEntry *first_tombstone = NULL;
    uint32_t cflags = tb_cflags(tb);

    for (unsigned int i = 0; i < LATX_SHADOW_JMP_SIZE; i++) {
        LatxShadowJmpEntry *entry =
            &latx_shadow_jmp_entries[(h + i) &
                                     (LATX_SHADOW_JMP_SIZE - 1)];
        TranslationBlock *cached = qatomic_read(&entry->tb);

        if (cached == LATX_SHADOW_JMP_TOMBSTONE) {
            if (!first_tombstone) {
                first_tombstone = entry;
            }
            continue;
        }
        if (!cached) {
            entry = first_tombstone ? first_tombstone : entry;
            qatomic_set(&entry->tb, tb);
            return;
        }
        if (cached == tb ||
            latx_shadow_jmp_key_equal(cached, tb->pc, tb->flags, cflags)) {
            qatomic_set(&entry->tb, tb);
            return;
        }
    }

    if (first_tombstone) {
        qatomic_set(&first_tombstone->tb, tb);
    }
}

void latx_shadow_jmp_cache_remove(TranslationBlock *tb)
{
    unsigned int h = latx_shadow_jmp_hash(tb->pc);

    for (unsigned int i = 0; i < LATX_SHADOW_JMP_SIZE; i++) {
        LatxShadowJmpEntry *entry =
            &latx_shadow_jmp_entries[(h + i) &
                                     (LATX_SHADOW_JMP_SIZE - 1)];
        TranslationBlock *cached = qatomic_read(&entry->tb);

        if (!cached) {
            return;
        }
        if (cached == tb) {
            qatomic_set(&entry->tb, LATX_SHADOW_JMP_TOMBSTONE);
            return;
        }
    }
}

void latx_shadow_jmp_cache_clear_all(void)
{
    memset(latx_shadow_jmp_entries, 0, sizeof(latx_shadow_jmp_entries));
}
