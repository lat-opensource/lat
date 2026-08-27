/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"
#include "qemu/cutils.h"

#include "aot.h"
#include "aot_static_layout.h"
#include "exec/exec-all.h"

struct AOTStaticLayout {
    AOTStaticLayoutNode *nodes;
    uint32_t *pressure;
    uint32_t *scratch;
    bool *placed;
    uint32_t node_count;
    uint32_t component_count;
    uint32_t line_size;
    uint32_t set_count;
    uint32_t ways;
    size_t padding_budget;
    size_t padding_used;
};

struct AOTStaticLazyLayout {
    GHashTable *pressure;
    GHashTable *placed;
    uint32_t *scratch;
    uint32_t line_size;
    uint32_t set_count;
    uint32_t ways;
    size_t padding_budget;
    size_t padding_used;
};

static bool read_uint_file(const char *path, uint32_t *value)
{
    g_autofree char *text = NULL;
    const char *end;
    uint64_t parsed;

    if (!g_file_get_contents(path, &text, NULL, NULL) ||
        qemu_strtou64(text, &end, 10, &parsed) < 0 || parsed > UINT32_MAX) {
        return false;
    }
    while (g_ascii_isspace(*end)) {
        end++;
    }
    if (*end) {
        return false;
    }
    *value = parsed;
    return true;
}

static bool detect_l1i_geometry(uint32_t *line_size, uint32_t *set_count,
                                uint32_t *ways)
{
    for (uint32_t index = 0; index < 16; index++) {
        g_autofree char *base = g_strdup_printf(
            "/sys/devices/system/cpu/cpu0/cache/index%u", index);
        g_autofree char *level_path = g_build_filename(base, "level", NULL);
        g_autofree char *type_path = g_build_filename(base, "type", NULL);
        g_autofree char *line_path = g_build_filename(
            base, "coherency_line_size", NULL);
        g_autofree char *sets_path = g_build_filename(
            base, "number_of_sets", NULL);
        g_autofree char *ways_path = g_build_filename(
            base, "ways_of_associativity", NULL);
        g_autofree char *type = NULL;
        uint32_t level;

        if (!read_uint_file(level_path, &level) || level != 1 ||
            !g_file_get_contents(type_path, &type, NULL, NULL) ||
            (!g_str_has_prefix(type, "Instruction") &&
             !g_str_has_prefix(type, "Unified"))) {
            continue;
        }
        return read_uint_file(line_path, line_size) &&
               read_uint_file(sets_path, set_count) &&
               read_uint_file(ways_path, ways) &&
               *line_size && *set_count && *ways;
    }
    return false;
}

bool aot_static_layout_get_l1i_geometry(uint32_t *line_size,
                                        uint32_t *set_count,
                                        uint32_t *ways)
{
    static gsize initialized;
    static uint32_t cached_line_size;
    static uint32_t cached_set_count;
    static uint32_t cached_ways;
    static bool valid;

    if (g_once_init_enter(&initialized)) {
        valid = detect_l1i_geometry(&cached_line_size, &cached_set_count,
                                    &cached_ways);
        g_once_init_leave(&initialized, 1);
    }
    if (!valid) {
        return false;
    }
    *line_size = cached_line_size;
    *set_count = cached_set_count;
    *ways = cached_ways;
    return true;
}

static uint32_t find_root(uint32_t *parents, uint32_t node)
{
    while (parents[node] != node) {
        parents[node] = parents[parents[node]];
        node = parents[node];
    }
    return node;
}

static void union_nodes(uint32_t *parents, uint32_t left, uint32_t right)
{
    left = find_root(parents, left);
    right = find_root(parents, right);
    if (left != right) {
        parents[right] = left;
    }
}

typedef struct AOTTarjanState {
    const uint32_t *offsets;
    const uint32_t *targets;
    int32_t *index;
    int32_t *lowlink;
    uint32_t *stack;
    uint32_t *scc;
    uint32_t *scc_size;
    bool *on_stack;
    uint32_t stack_size;
    uint32_t next_index;
    uint32_t scc_count;
} AOTTarjanState;

static void tarjan_visit(AOTTarjanState *state, uint32_t node)
{
    state->index[node] = state->next_index;
    state->lowlink[node] = state->next_index++;
    state->stack[state->stack_size++] = node;
    state->on_stack[node] = true;

    for (uint32_t i = state->offsets[node];
         i < state->offsets[node + 1]; i++) {
        uint32_t target = state->targets[i];

        if (state->index[target] < 0) {
            tarjan_visit(state, target);
            state->lowlink[node] = MIN(state->lowlink[node],
                                       state->lowlink[target]);
        } else if (state->on_stack[target]) {
            state->lowlink[node] = MIN(state->lowlink[node],
                                       state->index[target]);
        }
    }

    if (state->lowlink[node] == state->index[node]) {
        uint32_t member;

        do {
            member = state->stack[--state->stack_size];
            state->on_stack[member] = false;
            state->scc[member] = state->scc_count;
            state->scc_size[state->scc_count]++;
        } while (member != node);
        state->scc_count++;
    }
}

uint32_t aot_static_layout_find_components(AOTStaticLayoutNode *nodes,
                                           uint32_t node_count,
                                           const AOTStaticLayoutEdge *edges,
                                           uint32_t edge_count)
{
    AOTTarjanState tarjan = { 0 };
    uint32_t *offsets;
    uint32_t *targets;
    uint32_t *cursor;
    uint32_t *parents;
    uint32_t *root_component;
    uint32_t *scc_first;
    bool *hot;
    uint32_t component_count = 0;
    uint32_t flow_edge_count = 0;

    if (!node_count) {
        return 0;
    }

    offsets = g_new0(uint32_t, node_count + 1);
    parents = g_new(uint32_t, node_count);
    root_component = g_new(uint32_t, node_count);
    scc_first = g_new(uint32_t, node_count);
    hot = g_new0(bool, node_count);

    for (uint32_t i = 0; i < node_count; i++) {
        parents[i] = i;
        root_component[i] = AOT_STATIC_LAYOUT_NO_COMPONENT;
        scc_first[i] = AOT_STATIC_LAYOUT_NO_COMPONENT;
        nodes[i].component = AOT_STATIC_LAYOUT_NO_COMPONENT;
    }

    for (uint32_t i = 0; i < edge_count; i++) {
        if (edges[i].from >= node_count || edges[i].to >= node_count ||
            edges[i].kind != AOT_LAYOUT_EDGE_FLOW) {
            continue;
        }
        offsets[edges[i].from + 1]++;
        flow_edge_count++;
    }
    for (uint32_t i = 1; i <= node_count; i++) {
        offsets[i] += offsets[i - 1];
    }
    targets = g_new(uint32_t, flow_edge_count);
    cursor = g_new(uint32_t, node_count);
    memcpy(cursor, offsets, sizeof(*offsets) * node_count);
    for (uint32_t i = 0; i < edge_count; i++) {
        if (edges[i].from < node_count && edges[i].to < node_count &&
            edges[i].kind == AOT_LAYOUT_EDGE_FLOW) {
            targets[cursor[edges[i].from]++] = edges[i].to;
        }
    }
    g_free(cursor);

    tarjan.offsets = offsets;
    tarjan.targets = targets;
    tarjan.index = g_new(int32_t, node_count);
    tarjan.lowlink = g_new(int32_t, node_count);
    tarjan.stack = g_new(uint32_t, node_count);
    tarjan.scc = g_new(uint32_t, node_count);
    tarjan.scc_size = g_new0(uint32_t, node_count);
    tarjan.on_stack = g_new0(bool, node_count);
    memset(tarjan.index, -1, sizeof(*tarjan.index) * node_count);

    for (uint32_t i = 0; i < node_count; i++) {
        if (tarjan.index[i] < 0) {
            tarjan_visit(&tarjan, i);
        }
    }

    for (uint32_t i = 0; i < node_count; i++) {
        if (tarjan.scc_size[tarjan.scc[i]] > 1) {
            hot[i] = true;
        }
    }
    for (uint32_t i = 0; i < edge_count; i++) {
        if (edges[i].from < node_count && edges[i].to < node_count &&
            edges[i].kind == AOT_LAYOUT_EDGE_FLOW &&
            edges[i].from == edges[i].to) {
            hot[edges[i].from] = true;
        }
    }
    for (uint32_t i = 0; i < node_count; i++) {
        if (hot[i]) {
            uint32_t scc = tarjan.scc[i];

            if (scc_first[scc] == AOT_STATIC_LAYOUT_NO_COMPONENT) {
                scc_first[scc] = i;
            } else {
                union_nodes(parents, scc_first[scc], i);
            }
        }
    }

    for (uint32_t i = 0; i < edge_count; i++) {
        uint32_t from = edges[i].from;
        uint32_t to = edges[i].to;

        if (from >= node_count || to >= node_count ||
            edges[i].kind != AOT_LAYOUT_EDGE_CALL || !hot[from]) {
            continue;
        }
        hot[to] = true;
        union_nodes(parents, from, to);
    }

    for (uint32_t i = 0; i < node_count; i++) {
        uint32_t root;

        if (!hot[i]) {
            continue;
        }
        root = find_root(parents, i);
        if (root_component[root] == AOT_STATIC_LAYOUT_NO_COMPONENT) {
            root_component[root] = component_count++;
        }
        nodes[i].component = root_component[root];
    }

    g_free(hot);
    g_free(scc_first);
    g_free(root_component);
    g_free(parents);
    g_free(tarjan.on_stack);
    g_free(tarjan.scc_size);
    g_free(tarjan.scc);
    g_free(tarjan.stack);
    g_free(tarjan.lowlink);
    g_free(tarjan.index);
    g_free(targets);
    g_free(offsets);
    return component_count;
}

static uint32_t line_set(const AOTStaticLayout *layout, uintptr_t address)
{
    return address / layout->line_size % layout->set_count;
}

static void add_footprint(const AOTStaticLayout *layout, uint32_t *pressure,
                          uintptr_t host_code, uint32_t code_size)
{
    uint32_t lines = DIV_ROUND_UP(code_size, layout->line_size);
    uint32_t first_set = line_set(layout, host_code);

    for (uint32_t i = 0; i < lines; i++) {
        pressure[(first_set + i) % layout->set_count]++;
    }
}

static uint64_t pressure_cost(const AOTStaticLayout *layout,
                              const uint32_t *pressure)
{
    uint64_t cost = 0;

    for (uint32_t i = 0; i < layout->set_count; i++) {
        if (pressure[i] > layout->ways) {
            uint64_t overflow = pressure[i] - layout->ways;

            cost += overflow * overflow;
        }
    }
    return cost;
}

static uint64_t candidate_cost(AOTStaticLayout *layout,
                               uint32_t component, uintptr_t host_code,
                               uint32_t code_size)
{
    const uint32_t *current = layout->pressure +
                              component * layout->set_count;

    memcpy(layout->scratch, current,
           sizeof(*layout->scratch) * layout->set_count);
    add_footprint(layout, layout->scratch, host_code, code_size);
    return pressure_cost(layout, layout->scratch);
}

AOTStaticLayout *aot_static_layout_new(const AOTStaticLayoutNode *nodes,
                                       uint32_t node_count,
                                       uint32_t component_count,
                                       uint32_t line_size,
                                       uint32_t set_count,
                                       uint32_t ways)
{
    AOTStaticLayout *layout;
    size_t code_size = 0;

    if (!line_size || !set_count || !ways) {
        return NULL;
    }

    layout = g_new0(AOTStaticLayout, 1);
    layout->nodes = g_malloc(sizeof(*nodes) * node_count);
    memcpy(layout->nodes, nodes, sizeof(*nodes) * node_count);
    layout->placed = g_new0(bool, node_count);
    layout->pressure = g_new0(uint32_t,
                              (size_t)component_count * set_count);
    layout->scratch = g_new(uint32_t, set_count);
    layout->node_count = node_count;
    layout->component_count = component_count;
    layout->line_size = line_size;
    layout->set_count = set_count;
    layout->ways = ways;

    for (uint32_t i = 0; i < node_count; i++) {
        code_size += nodes[i].code_size;
    }
    layout->padding_budget = ROUND_DOWN(code_size /
                                        AOT_STATIC_LAYOUT_PADDING_PERCENT /
                                        100, line_size);
    return layout;
}

uintptr_t aot_static_layout_place(AOTStaticLayout *layout,
                                  uint32_t node_index,
                                  uintptr_t host_code,
                                  size_t available_padding)
{
    AOTStaticLayoutNode *node;
    uint32_t *pressure;
    uintptr_t best_padding = 0;
    uintptr_t max_padding;
    uint64_t best_cost;

    if (!layout || node_index >= layout->node_count ||
        layout->placed[node_index]) {
        return 0;
    }
    layout->placed[node_index] = true;
    node = &layout->nodes[node_index];
    if (node->component == AOT_STATIC_LAYOUT_NO_COMPONENT ||
        node->component >= layout->component_count) {
        return 0;
    }

    pressure = layout->pressure + node->component * layout->set_count;
    best_cost = candidate_cost(layout, node->component, host_code,
                               node->code_size);
    max_padding = node->can_pad ?
        MIN((uintptr_t)layout->line_size * layout->ways,
            layout->padding_budget - layout->padding_used) : 0;
    max_padding = MIN(max_padding, available_padding);

    for (uintptr_t padding = layout->line_size;
         padding <= max_padding; padding += layout->line_size) {
        uint64_t cost = candidate_cost(layout, node->component,
                                       host_code + padding,
                                       node->code_size);

        if (cost < best_cost) {
            best_cost = cost;
            best_padding = padding;
        }
    }

    add_footprint(layout, pressure, host_code + best_padding,
                  node->code_size);
    layout->padding_used += best_padding;
    return best_padding;
}

uint64_t aot_static_layout_cost(const AOTStaticLayout *layout,
                                uint32_t component)
{
    if (!layout || component >= layout->component_count) {
        return 0;
    }
    return pressure_cost(layout, layout->pressure +
                         component * layout->set_count);
}

size_t aot_static_layout_padding_used(const AOTStaticLayout *layout)
{
    return layout ? layout->padding_used : 0;
}

void aot_static_layout_free(AOTStaticLayout *layout)
{
    if (!layout) {
        return;
    }
    g_free(layout->pressure);
    g_free(layout->scratch);
    g_free(layout->placed);
    g_free(layout->nodes);
    g_free(layout);
}

static uint32_t *lazy_component_pressure(AOTStaticLazyLayout *layout,
                                         uint16_t component)
{
    gpointer key = (gpointer)(uintptr_t)(component + 1);
    uint32_t *pressure = g_hash_table_lookup(layout->pressure, key);

    if (!pressure) {
        pressure = g_new0(uint32_t, layout->set_count);
        g_hash_table_insert(layout->pressure, key, pressure);
    }
    return pressure;
}

static void lazy_add_footprint(const AOTStaticLazyLayout *layout,
                               uint32_t *pressure, uintptr_t host_code,
                               uint32_t code_size)
{
    uint32_t lines = DIV_ROUND_UP(code_size, layout->line_size);
    uint32_t first_set = host_code / layout->line_size % layout->set_count;

    for (uint32_t i = 0; i < lines; i++) {
        pressure[(first_set + i) % layout->set_count]++;
    }
}

static uint64_t lazy_pressure_cost(const AOTStaticLazyLayout *layout,
                                   const uint32_t *pressure)
{
    uint64_t cost = 0;

    for (uint32_t i = 0; i < layout->set_count; i++) {
        if (pressure[i] > layout->ways) {
            uint64_t overflow = pressure[i] - layout->ways;

            cost += overflow * overflow;
        }
    }
    return cost;
}

static uint64_t lazy_candidate_cost(AOTStaticLazyLayout *layout,
                                    const uint32_t *pressure,
                                    uintptr_t host_code, uint32_t code_size)
{
    memcpy(layout->scratch, pressure,
           sizeof(*layout->scratch) * layout->set_count);
    lazy_add_footprint(layout, layout->scratch, host_code, code_size);
    return lazy_pressure_cost(layout, layout->scratch);
}

AOTStaticLazyLayout *aot_static_lazy_layout_new(uint32_t line_size,
                                                uint32_t set_count,
                                                uint32_t ways,
                                                size_t padding_budget)
{
    AOTStaticLazyLayout *layout;

    if (!line_size || !set_count || !ways) {
        return NULL;
    }
    layout = g_new0(AOTStaticLazyLayout, 1);
    layout->pressure = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                             NULL, g_free);
    layout->placed = g_hash_table_new(g_direct_hash, g_direct_equal);
    layout->scratch = g_new(uint32_t, set_count);
    layout->line_size = line_size;
    layout->set_count = set_count;
    layout->ways = ways;
    layout->padding_budget = ROUND_DOWN(padding_budget, line_size);
    return layout;
}

uintptr_t aot_static_lazy_layout_place(AOTStaticLazyLayout *layout,
                                       uint16_t component, bool movable,
                                       target_ulong guest_pc,
                                       uintptr_t host_code,
                                       uint32_t code_size,
                                       size_t available_padding)
{
    gpointer guest_key = (gpointer)(uintptr_t)guest_pc;
    uint32_t *pressure;
    uintptr_t best_padding = 0;
    uintptr_t max_padding;
    uint64_t best_cost;

    if (!layout || component == AOT_LAYOUT_COMPONENT_NONE ||
        g_hash_table_contains(layout->placed, guest_key)) {
        return 0;
    }
    g_hash_table_add(layout->placed, guest_key);
    pressure = lazy_component_pressure(layout, component);
    if (!movable) {
        lazy_add_footprint(layout, pressure, host_code, code_size);
        return 0;
    }

    best_cost = lazy_candidate_cost(layout, pressure, host_code, code_size);
    max_padding = MIN((uintptr_t)layout->line_size * layout->ways,
                      layout->padding_budget - layout->padding_used);
    max_padding = MIN(max_padding, available_padding);
    for (uintptr_t padding = layout->line_size;
         padding <= max_padding; padding += layout->line_size) {
        uint64_t cost = lazy_candidate_cost(layout, pressure,
                                            host_code + padding, code_size);
        uint64_t score = cost + padding / layout->line_size;

        if (score < best_cost) {
            best_cost = score;
            best_padding = padding;
        }
    }
    lazy_add_footprint(layout, pressure, host_code + best_padding, code_size);
    layout->padding_used += best_padding;
    return best_padding;
}

void aot_static_lazy_layout_free(AOTStaticLazyLayout *layout)
{
    if (!layout) {
        return;
    }
    g_hash_table_destroy(layout->placed);
    g_hash_table_destroy(layout->pressure);
    g_free(layout->scratch);
    g_free(layout);
}

static bool stored_matching_cflags(const aot_tb *tb, uint32_t cflags)
{
    return (tb->cflags & CF_PARALLEL) == (cflags & CF_PARALLEL);
}

static void stored_add_edge(AOTStaticLayoutEdge *edges, uint32_t *edge_count,
                            GHashTable *tb_nodes, uint32_t from,
                            target_ulong target, AOTStaticLayoutEdgeKind kind)
{
    gpointer value = g_hash_table_lookup(tb_nodes,
                                         (gpointer)(uintptr_t)target);

    if (!value) {
        return;
    }
    edges[*edge_count].from = from;
    edges[*edge_count].to = (uintptr_t)value - 1;
    edges[*edge_count].kind = kind;
    (*edge_count)++;
}

bool aot_static_layout_store(aot_segment *segment, aot_tb *tbs,
                             uint32_t cflags, target_ulong segment_base,
                             uint32_t line_size, uint32_t set_count,
                             uint32_t ways)
{
    AOTStaticLayoutNode *nodes;
    AOTStaticLayoutEdge *edges;
    AOTStaticLayout *layout = NULL;
    GHashTable *tb_nodes;
    uint32_t *tb_node;
    uint32_t *first_tb;
    uint32_t *tb_count;
    uint32_t node_count = 0;
    uint32_t next_node = 0;
    uint32_t edge_count = 0;
    uint32_t component_count;
    uint32_t current_node = AOT_STATIC_LAYOUT_NO_COMPONENT;
    uintptr_t cursor = 0;
    size_t total_code_size = 0;
    uint32_t parallel = cflags & CF_PARALLEL ? 1 : 0;

    if (!line_size || !set_count || !ways) {
        return false;
    }
    for (uint32_t i = 0; i < segment->segment_tbs_num; i++) {
        if (stored_matching_cflags(&tbs[i], cflags) && tbs[i].is_first_tb) {
            node_count++;
        }
    }
    if (!node_count) {
        return true;
    }

    nodes = g_new0(AOTStaticLayoutNode, node_count);
    edges = g_new(AOTStaticLayoutEdge, segment->segment_tbs_num * 2);
    tb_node = g_new(uint32_t, segment->segment_tbs_num);
    first_tb = g_new(uint32_t, node_count);
    tb_count = g_new0(uint32_t, node_count);
    tb_nodes = g_hash_table_new(g_direct_hash, g_direct_equal);

    for (uint32_t i = 0; i < segment->segment_tbs_num; i++) {
        aot_tb *tb = &tbs[i];
        target_ulong guest;

        tb_node[i] = AOT_STATIC_LAYOUT_NO_COMPONENT;
        if (!stored_matching_cflags(tb, cflags)) {
            continue;
        }
        guest = segment_base + tb->offset_in_segment;
        if (tb->is_first_tb) {
            assert(next_node < node_count);
            current_node = next_node++;
            first_tb[current_node] = i;
            nodes[current_node].guest_pc = guest;
            nodes[current_node].code_size = tb->tu_size;
            nodes[current_node].component = AOT_STATIC_LAYOUT_NO_COMPONENT;
            tb->layout_padding_lines = 0;
            tb->layout_component = AOT_LAYOUT_COMPONENT_NONE;
            tb->layout_flags = 0;
        }
        if (current_node == AOT_STATIC_LAYOUT_NO_COMPONENT ||
            tb->tu_id != (uint32_t)nodes[current_node].guest_pc) {
            continue;
        }
        tb_node[i] = current_node;
        tb_count[current_node]++;
        g_hash_table_insert(tb_nodes, (gpointer)(uintptr_t)guest,
                            (gpointer)(uintptr_t)(current_node + 1));
    }
    assert(next_node == node_count);

    for (uint32_t i = 0; i < segment->segment_tbs_num; i++) {
        const aot_tb *tb = &tbs[i];
        uint32_t from = tb_node[i];
        target_ulong guest;

        if (from == AOT_STATIC_LAYOUT_NO_COMPONENT) {
            continue;
        }
        guest = segment_base + tb->offset_in_segment;
        switch (tb->last_ir1_type) {
        case IR1_TYPE_BRANCH:
            if (tb->next_tb_pc_offset != -1) {
                stored_add_edge(edges, &edge_count, tb_nodes, from,
                                segment_base + tb->next_tb_pc_offset,
                                AOT_LAYOUT_EDGE_FLOW);
            }
            if (tb->target_tb_pc_offset != -1) {
                stored_add_edge(edges, &edge_count, tb_nodes, from,
                                segment_base + tb->target_tb_pc_offset,
                                AOT_LAYOUT_EDGE_FLOW);
            }
            break;
        case IR1_TYPE_JUMP:
            if (tb->target_tb_pc_offset != -1) {
                stored_add_edge(edges, &edge_count, tb_nodes, from,
                                segment_base + tb->target_tb_pc_offset,
                                AOT_LAYOUT_EDGE_FLOW);
            }
            break;
        case IR1_TYPE_CALL:
            if (tb->target_tb_pc_offset != -1) {
                stored_add_edge(edges, &edge_count, tb_nodes, from,
                                segment_base + tb->target_tb_pc_offset,
                                AOT_LAYOUT_EDGE_CALL);
            }
            stored_add_edge(edges, &edge_count, tb_nodes, from,
                            guest + tb->size, AOT_LAYOUT_EDGE_FLOW);
            break;
        case IR1_TYPE_NORMAL:
        case IR1_TYPE_CALLIN:
            stored_add_edge(edges, &edge_count, tb_nodes, from,
                            guest + tb->size, AOT_LAYOUT_EDGE_FLOW);
            break;
        default:
            break;
        }
    }

    component_count = aot_static_layout_find_components(nodes, node_count,
                                                         edges, edge_count);
    for (uint32_t i = 0; i < edge_count; i++) {
        if (edges[i].kind == AOT_LAYOUT_EDGE_CALL) {
            nodes[edges[i].to].can_pad = true;
        }
    }
    if (component_count >= AOT_LAYOUT_COMPONENT_NONE) {
        component_count = 0;
    }
    if (component_count) {
        layout = aot_static_layout_new(nodes, node_count, component_count,
                                       line_size, set_count, ways);
    }

    for (uint32_t i = 0; i < node_count; i++) {
        uintptr_t table;
        uintptr_t code;
        uintptr_t padding = 0;

        total_code_size += nodes[i].code_size;
        if (layout &&
            nodes[i].component != AOT_STATIC_LAYOUT_NO_COMPONENT) {
            tbs[first_tb[i]].layout_component = nodes[i].component;
            if (nodes[i].can_pad) {
                tbs[first_tb[i]].layout_flags |= AOT_LAYOUT_MOVABLE;
            }
        }

        cursor = ROUND_UP(cursor, line_size);
        cursor += (uintptr_t)ROUND_UP(sizeof(TranslationBlock), line_size) *
                  tb_count[i];
        table = ROUND_UP(cursor, CODE_GEN_ALIGN);
        code = ROUND_UP(table + sizeof(TBMini) * (tb_count[i] + 1),
                        line_size);
        if (layout) {
            padding = aot_static_layout_place(layout, i, code, SIZE_MAX);
        }
        assert(!(padding % line_size));
        assert(padding / line_size <= UINT8_MAX);
        tbs[first_tb[i]].layout_padding_lines = padding / line_size;
        cursor = ROUND_UP(code + padding + nodes[i].code_size,
                          CODE_GEN_ALIGN);
    }
    segment->layout_padding_budget[parallel] = ROUND_DOWN(
        total_code_size / 100, line_size);

    aot_static_layout_free(layout);
    g_hash_table_destroy(tb_nodes);
    g_free(tb_count);
    g_free(first_tb);
    g_free(tb_node);
    g_free(edges);
    g_free(nodes);
    return true;
}

bool aot_static_layout_store_header(aot_header *header,
                                    aot_segment *segments)
{
    uint32_t line_size;
    uint32_t set_count;
    uint32_t ways;

    if (!aot_static_layout_get_l1i_geometry(&line_size, &set_count, &ways) ||
        !is_power_of_2(line_size) || !is_power_of_2(set_count) ||
        ways > UINT8_MAX) {
        return false;
    }

    for (uint32_t i = 0; i < header->segments_num; i++) {
        aot_segment *segment = &segments[i];
        aot_tb *tbs = (void *)header + segment->segment_tbs_offset;

        if (!aot_static_layout_store(segment, tbs, 0,
                                     segment->details.seg_begin, line_size,
                                     set_count, ways) ||
            !aot_static_layout_store(segment, tbs, CF_PARALLEL,
                                     segment->details.seg_begin, line_size,
                                     set_count, ways)) {
            return false;
        }
    }

    header->layout_line_log2 = ctz32(line_size);
    header->layout_set_log2 = ctz32(set_count);
    header->layout_ways = ways;
    header->layout_magic = AOT_STATIC_LAYOUT_MAGIC;
    return true;
}

bool aot_static_layout_header_matches(const aot_header *header,
                                      uint32_t line_size,
                                      uint32_t set_count, uint32_t ways)
{
    return header && header->layout_magic == AOT_STATIC_LAYOUT_MAGIC &&
           line_size && set_count && ways && is_power_of_2(line_size) &&
           is_power_of_2(set_count) &&
           header->layout_line_log2 == ctz32(line_size) &&
           header->layout_set_log2 == ctz32(set_count) &&
           header->layout_ways == ways;
}

uintptr_t aot_static_layout_stored_padding(const aot_header *header,
                                           const aot_tb *tb,
                                           uint32_t line_size,
                                           uint32_t set_count, uint32_t ways,
                                           size_t available_padding)
{
    uintptr_t padding;

    if (!tb || !aot_static_layout_header_matches(header, line_size,
                                                  set_count, ways)) {
        return 0;
    }
    padding = (uintptr_t)tb->layout_padding_lines * line_size;
    return padding <= available_padding ? padding : 0;
}
