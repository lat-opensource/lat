#include "qemu/osdep.h"

#include "exec/exec-all.h"
#include "exec/fasttb.h"

static void init_tb(TranslationBlock *tb, target_ulong pc,
                    uint32_t flags, uint32_t cflags, uintptr_t code)
{
    memset(tb, 0, sizeof(*tb));
    tb->pc = pc;
    tb->flags = flags;
    tb->cflags = cflags;
    tb->tc.ptr = (void *)code;
}

static unsigned int test_hash(target_ulong pc)
{
    return (pc * LATX_SHADOW_JMP_HASH_MULT) >>
           (64 - LATX_SHADOW_JMP_BITS);
}

static target_ulong find_collision(target_ulong pc, target_ulong start)
{
    unsigned int wanted = test_hash(pc);

    for (target_ulong candidate = start; ; candidate++) {
        if (candidate != pc && test_hash(candidate) == wanted) {
            return candidate;
        }
    }
}

static void test_distinct_keys_and_tombstone(void)
{
    TranslationBlock a, b, c, replacement, collided;
    target_ulong collision_pc = find_collision(0x1000, 0x2000);

    init_tb(&a, 0x1000, 0x11, 0x21, 0x100000);
    init_tb(&b, 0x1000, 0x12, 0x21, 0x200000);
    init_tb(&c, 0x1000, 0x11, 0x22, 0x300000);
    init_tb(&replacement, 0x1000, 0x11, 0x22, 0x400000);
    init_tb(&collided, collision_pc, 0x31, 0x41, 0x500000);

    latx_shadow_jmp_cache_clear_all();
    latx_shadow_jmp_cache_add(&a);
    latx_shadow_jmp_cache_add(&b);
    latx_shadow_jmp_cache_add(&c);

    g_assert_true(latx_shadow_jmp_cache_lookup(a.pc, a.flags,
                                               tb_cflags(&a)) == &a);
    g_assert_true(latx_shadow_jmp_cache_lookup(b.pc, b.flags,
                                               tb_cflags(&b)) == &b);
    g_assert_true(latx_shadow_jmp_cache_lookup(c.pc, c.flags,
                                               tb_cflags(&c)) == &c);
    g_assert_null(latx_shadow_jmp_cache_lookup(a.pc, 0xff, tb_cflags(&a)));

    c.cflags |= CF_INVALID;
    g_assert_null(latx_shadow_jmp_cache_lookup(c.pc, c.flags, 0x22));
    c.cflags &= ~CF_INVALID;

    latx_shadow_jmp_cache_remove(&b);
    g_assert_null(latx_shadow_jmp_cache_lookup(b.pc, b.flags, tb_cflags(&b)));
    g_assert_true(latx_shadow_jmp_cache_lookup(c.pc, c.flags,
                                               tb_cflags(&c)) == &c);

    latx_shadow_jmp_cache_add(&collided);
    g_assert_true(latx_shadow_jmp_cache_lookup(
                      collided.pc, collided.flags,
                      tb_cflags(&collided)) == &collided);

    latx_shadow_jmp_cache_add(&replacement);
    g_assert_true(latx_shadow_jmp_cache_lookup(
                      replacement.pc, replacement.flags,
                      tb_cflags(&replacement)) == &replacement);
    latx_shadow_jmp_cache_remove(&c);
    g_assert_true(latx_shadow_jmp_cache_lookup(
                      replacement.pc, replacement.flags,
                      tb_cflags(&replacement)) == &replacement);
}

static void test_full_table(void)
{
    TranslationBlock tb[LATX_SHADOW_JMP_SIZE + 1];

    latx_shadow_jmp_cache_clear_all();
    for (unsigned int i = 0; i < LATX_SHADOW_JMP_SIZE; i++) {
        init_tb(&tb[i], 0x10000 + i, i, 0x80 + i, 0x100000 + i * 16);
        latx_shadow_jmp_cache_add(&tb[i]);
    }
    for (unsigned int i = 0; i < LATX_SHADOW_JMP_SIZE; i++) {
        g_assert_true(latx_shadow_jmp_cache_lookup(
                          tb[i].pc, tb[i].flags,
                          tb_cflags(&tb[i])) == &tb[i]);
    }

    init_tb(&tb[LATX_SHADOW_JMP_SIZE], 0x20000, 0xaa, 0xbb, 0x200000);
    g_assert_null(latx_shadow_jmp_cache_lookup(0x30000, 0xcc, 0xdd));
    latx_shadow_jmp_cache_add(&tb[LATX_SHADOW_JMP_SIZE]);
    g_assert_null(latx_shadow_jmp_cache_lookup(
        tb[LATX_SHADOW_JMP_SIZE].pc,
        tb[LATX_SHADOW_JMP_SIZE].flags,
        tb_cflags(&tb[LATX_SHADOW_JMP_SIZE])));

    latx_shadow_jmp_cache_remove(&tb[3]);
    latx_shadow_jmp_cache_add(&tb[LATX_SHADOW_JMP_SIZE]);
    g_assert_true(latx_shadow_jmp_cache_lookup(
                      tb[LATX_SHADOW_JMP_SIZE].pc,
                      tb[LATX_SHADOW_JMP_SIZE].flags,
                      tb_cflags(&tb[LATX_SHADOW_JMP_SIZE])) ==
                  &tb[LATX_SHADOW_JMP_SIZE]);
}

int main(void)
{
    test_distinct_keys_and_tombstone();
    test_full_table();
    return 0;
}
