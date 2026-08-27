/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_AOT_STATIC_LAYOUT_H
#define LATX_AOT_STATIC_LAYOUT_H

#include "qemu-def.h"

#define AOT_STATIC_LAYOUT_NO_COMPONENT UINT32_MAX
#define AOT_STATIC_LAYOUT_PADDING_PERCENT 1

typedef enum AOTStaticLayoutEdgeKind {
    AOT_LAYOUT_EDGE_FLOW,
    AOT_LAYOUT_EDGE_CALL,
} AOTStaticLayoutEdgeKind;

typedef struct AOTStaticLayoutNode {
    target_ulong guest_pc;
    uint32_t code_size;
    uint32_t component;
    bool can_pad;
} AOTStaticLayoutNode;

typedef struct AOTStaticLayoutEdge {
    uint32_t from;
    uint32_t to;
    AOTStaticLayoutEdgeKind kind;
} AOTStaticLayoutEdge;

typedef struct AOTStaticLayout AOTStaticLayout;
struct aot_segment;
struct aot_tb;
struct aot_header;

uint32_t aot_static_layout_find_components(AOTStaticLayoutNode *nodes,
                                           uint32_t node_count,
                                           const AOTStaticLayoutEdge *edges,
                                           uint32_t edge_count);

AOTStaticLayout *aot_static_layout_new(const AOTStaticLayoutNode *nodes,
                                       uint32_t node_count,
                                       uint32_t component_count,
                                       uint32_t line_size,
                                       uint32_t set_count,
                                       uint32_t ways);

uintptr_t aot_static_layout_place(AOTStaticLayout *layout,
                                  uint32_t node_index,
                                  uintptr_t host_code,
                                  size_t available_padding);

bool aot_static_layout_get_l1i_geometry(uint32_t *line_size,
                                        uint32_t *set_count,
                                        uint32_t *ways);

bool aot_static_layout_store(const struct aot_segment *segment,
                             struct aot_tb *tbs, uint32_t cflags,
                             target_ulong segment_base,
                             uint32_t line_size, uint32_t set_count,
                             uint32_t ways);

bool aot_static_layout_store_header(struct aot_header *header,
                                    struct aot_segment *segments);

bool aot_static_layout_header_matches(const struct aot_header *header,
                                      uint32_t line_size,
                                      uint32_t set_count, uint32_t ways);

uintptr_t aot_static_layout_stored_padding(const struct aot_header *header,
                                           const struct aot_tb *tb,
                                           uint32_t line_size,
                                           uint32_t set_count, uint32_t ways,
                                           size_t available_padding);

uint64_t aot_static_layout_cost(const AOTStaticLayout *layout,
                                uint32_t component);

size_t aot_static_layout_padding_used(const AOTStaticLayout *layout);

void aot_static_layout_free(AOTStaticLayout *layout);

#endif
