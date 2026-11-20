/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * User-mode TB invalidation index. Every operation requires mmap_lock.
 * Single-page TBs use per-page lists; crossing TBs retain an interval tree.
 * No caller may depend on address-sorted iteration. Save next before removing
 * the current TB. Empty buckets and radix nodes are reclaimed immediately.
 *
 * For single-page TBs, itree.rb holds the bucket pointer and list links instead
 * of a red-black node. start/last remain valid; subtree_last is not used.
 * This header is internal to translate-all.c and needs TranslationBlock.
 */
#ifndef LATX_TB_PAGE_INDEX_H
#define LATX_TB_PAGE_INDEX_H

#define RX_BITS 9
#define RX_SIZE (1u << RX_BITS)
#define RX_LEVEL 5

struct RXNode {
    void *slot[RX_SIZE];
    uint64_t bits[RX_SIZE / 64];
    unsigned count;
};

struct TBPageBucket {
    uint64_t page;
    RBNode *first;
};

static struct RXNode *rx_root;
static struct TBPageBucket *tb_last_page;
static IntervalTreeRoot tb_cross_root;

static void rx_mark(struct RXNode *n, unsigned i, void *p)
{
    if (!n->slot[i] && p) {
        n->count++;
        n->bits[i / 64] |= 1ULL << (i % 64);
    }
    if (n->slot[i] && !p) {
        n->count--;
        n->bits[i / 64] &= ~(1ULL << (i % 64));
    }
    n->slot[i] = p;
}

static struct TBPageBucket *rx_get(uint64_t page)
{
    struct RXNode *n;
    unsigned i;

    if (!rx_root) {
        rx_root = g_new0(struct RXNode, 1);
    }
    n = rx_root;
    for (int level = RX_LEVEL; level > 0; level--) {
        i = (page >> (level * RX_BITS)) & (RX_SIZE - 1);
        if (!n->slot[i]) {
            rx_mark(n, i, g_new0(struct RXNode, 1));
        }
        n = n->slot[i];
    }
    i = page & (RX_SIZE - 1);
    if (!n->slot[i]) {
        struct TBPageBucket *b = g_new0(struct TBPageBucket, 1);
        b->page = page;
        rx_mark(n, i, b);
    }
    return n->slot[i];
}

static void rx_remove(uint64_t page)
{
    struct RXNode *path[RX_LEVEL + 1], *n = rx_root;
    unsigned index[RX_LEVEL + 1];

    for (int l = RX_LEVEL; l >= 0; l--) {
        unsigned i = (page >> (l * RX_BITS)) & (RX_SIZE - 1);
        path[l] = n;
        index[l] = i;
        if (l) {
            n = n->slot[i];
        }
    }
    g_free(path[0]->slot[index[0]]);
    rx_mark(path[0], index[0], NULL);
    for (int l = 0; l <= RX_LEVEL; l++) {
        if (path[l]->count) {
            break;
        }
        g_free(path[l]);
        if (l == RX_LEVEL) {
            rx_root = NULL;
        } else {
            rx_mark(path[l + 1], index[l + 1], NULL);
        }
    }
}

static struct TBPageBucket *rx_first(struct RXNode *n, int level,
                                    uint64_t prefix, uint64_t lo, uint64_t hi)
{
    unsigned shift = level * RX_BITS;
    uint64_t span = (1ULL << (shift + RX_BITS)) - 1;
    unsigned low, high;

    if (!n || prefix > hi || (prefix | span) < lo) {
        return NULL;
    }
    low = lo > prefix ? (lo - prefix) >> shift : 0;
    high = hi < (prefix | span) ? (hi - prefix) >> shift : RX_SIZE - 1;
    for (unsigned word = low / 64; word <= high / 64; word++) {
        uint64_t bits = n->bits[word];
        if (word == low / 64) {
            bits &= UINT64_MAX << (low % 64);
        }
        if (word == high / 64 && high % 64 != 63) {
            bits &= (1ULL << (high % 64 + 1)) - 1;
        }
        while (bits) {
            unsigned i = word * 64 + __builtin_ctzll(bits);
            struct TBPageBucket *b;
            bits &= bits - 1;
            if (!level) {
                return n->slot[i];
            }
            b = rx_first(n->slot[i], level - 1,
                         prefix | ((uint64_t)i << shift), lo, hi);
            if (b) {
                return b;
            }
        }
    }
    return NULL;
}

static void rx_free(struct RXNode *n, int level)
{
    if (!n) {
        return;
    }
    for (unsigned w = 0; w < RX_SIZE / 64; w++) {
        for (uint64_t bits = n->bits[w]; bits; bits &= bits - 1) {
            unsigned i = w * 64 + __builtin_ctzll(bits);
            if (level) {
                rx_free(n->slot[i], level - 1);
            } else {
                g_free(n->slot[i]);
            }
        }
    }
    g_free(n);
}

static void tb_index_clear(void)
{
    rx_free(rx_root, RX_LEVEL);
    rx_root = NULL;
    tb_last_page = NULL;
    memset(&tb_cross_root, 0, sizeof(tb_cross_root));
}

static void tb_index_insert(TranslationBlock *tb)
{
    uint64_t page = tb->itree.start >> TARGET_PAGE_BITS;
    RBNode *n = &tb->itree.rb;
    struct TBPageBucket *b;

    if ((tb->itree.last >> TARGET_PAGE_BITS) != page) {
        interval_tree_insert(&tb->itree, &tb_cross_root);
        return;
    }
    b = tb_last_page;
    if (!b || b->page != page) {
        tb_last_page = b = rx_get(page);
    }
    n->rb_parent_color = (uintptr_t)b;
    n->rb_left = NULL;
    n->rb_right = b->first;
    if (b->first) {
        b->first->rb_left = n;
    }
    b->first = n;
}

static void tb_index_remove(TranslationBlock *tb)
{
    RBNode *n = &tb->itree.rb;
    struct TBPageBucket *b;

    if (((tb->itree.start ^ tb->itree.last) >> TARGET_PAGE_BITS) != 0) {
        interval_tree_remove(&tb->itree, &tb_cross_root);
        return;
    }
    b = (void *)n->rb_parent_color;
    if (n->rb_left) {
        n->rb_left->rb_right = n->rb_right;
    } else {
        b->first = n->rb_right;
    }
    if (n->rb_right) {
        n->rb_right->rb_left = n->rb_left;
    }
    if (!b->first) {
        if (tb_last_page == b) {
            tb_last_page = NULL;
        }
        rx_remove(b->page);
    }
}

static TranslationBlock *tb_index_scan(RBNode *n, struct TBPageBucket *b,
                                       uint64_t start, uint64_t end)
{
    IntervalTreeNode *i;

    while (b) {
        uint64_t page;
        for (; n; n = n->rb_right) {
            TranslationBlock *tb = container_of(n, TranslationBlock, itree.rb);
            if (tb->itree.start <= end - 1 && tb->itree.last >= start) {
                return tb;
            }
        }
        page = b->page + 1;
        b = rx_first(rx_root, RX_LEVEL, 0, page,
                     (end - 1) >> TARGET_PAGE_BITS);
        n = b ? b->first : NULL;
    }
    i = interval_tree_iter_first(&tb_cross_root, start, end - 1);
    return i ? container_of(i, TranslationBlock, itree) : NULL;
}

static TranslationBlock *tb_index_first(uint64_t start, uint64_t end)
{
    struct TBPageBucket *b;

    /* end == 0 represents the exclusive limit past UINT64_MAX. */
    if (end && start >= end) {
        return NULL;
    }
    b = rx_first(rx_root, RX_LEVEL, 0, start >> TARGET_PAGE_BITS,
                 (end - 1) >> TARGET_PAGE_BITS);
    return tb_index_scan(b ? b->first : NULL, b, start, end);
}

static TranslationBlock *tb_index_next(TranslationBlock *tb,
                                      uint64_t start, uint64_t end)
{
    IntervalTreeNode *i;

    if (((tb->itree.start ^ tb->itree.last) >> TARGET_PAGE_BITS) == 0) {
        return tb_index_scan(tb->itree.rb.rb_right,
                             (void *)tb->itree.rb.rb_parent_color, start, end);
    }
    i = interval_tree_iter_next(&tb->itree, start, end - 1);
    return i ? container_of(i, TranslationBlock, itree) : NULL;
}
#endif
