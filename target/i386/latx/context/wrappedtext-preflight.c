/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"

#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "fileutils.h"
#include "kzt-groups.h"
#include "wrappedlib-preflight.h"
#include "wrappedtext-preflight.h"

#define GO(name, signature) #name,
#define GOM(name, signature) #name,
#define GOW(name, signature) #name,
#define GOWM(name, signature) #name,
#define GO2(name, signature, alias) #name,
#define GOS(name, signature) #name,
#define DATA(name, size)
#define DATAV(name, size)
#define DATAB(name, size)
#define DATAM(name, size)
static const char *const fribidi_supported_symbols[] = {
#include "wrappedfribidi_private.h"
};
static const char *const fribidi_required_host_symbols[] = {
    "fribidi_unicode_version",
    "fribidi_version_info",
};
static const char *const harfbuzz_supported_symbols[] = {
#include "wrappedharfbuzz_private.h"
};
static const char *const harfbuzz_subset_supported_symbols[] = {
#include "wrappedharfbuzzsubset_private.h"
};
static const char *const harfbuzz_icu_supported_symbols[] = {
#include "wrappedharfbuzzicu_private.h"
};
static const char *const harfbuzz_cairo_supported_symbols[] = {
#include "wrappedharfbuzzcairo_private.h"
};
/*
 * Keep libharfbuzz-gobject on the guest path until GLib/GObject joins KZT.
 * These get_type entry points register guest GTypes and treat hb_* objects as
 * opaque boxed values; their reference/destroy calls still route through the
 * core HarfBuzz wrapper. Reject a future, broader ABI instead of assuming it
 * has the same safe boundary.
 */
static const char *const harfbuzz_gobject_safe_symbols[] = {
    "hb_gobject_aat_layout_feature_selector_get_type",
    "hb_gobject_aat_layout_feature_type_get_type",
    "hb_gobject_blob_get_type",
    "hb_gobject_buffer_cluster_level_get_type",
    "hb_gobject_buffer_content_type_get_type",
    "hb_gobject_buffer_diff_flags_get_type",
    "hb_gobject_buffer_flags_get_type",
    "hb_gobject_buffer_get_type",
    "hb_gobject_buffer_serialize_flags_get_type",
    "hb_gobject_buffer_serialize_format_get_type",
    "hb_gobject_color_line_get_type",
    "hb_gobject_color_stop_get_type",
    "hb_gobject_direction_get_type",
    "hb_gobject_draw_funcs_get_type",
    "hb_gobject_draw_state_get_type",
    "hb_gobject_face_get_type",
    "hb_gobject_feature_get_type",
    "hb_gobject_font_funcs_get_type",
    "hb_gobject_font_get_type",
    "hb_gobject_glyph_flags_get_type",
    "hb_gobject_glyph_info_get_type",
    "hb_gobject_glyph_position_get_type",
    "hb_gobject_map_get_type",
    "hb_gobject_memory_mode_get_type",
    "hb_gobject_ot_color_palette_flags_get_type",
    "hb_gobject_ot_layout_baseline_tag_get_type",
    "hb_gobject_ot_layout_glyph_class_get_type",
    "hb_gobject_ot_math_constant_get_type",
    "hb_gobject_ot_math_glyph_part_flags_get_type",
    "hb_gobject_ot_math_glyph_part_get_type",
    "hb_gobject_ot_math_glyph_variant_get_type",
    "hb_gobject_ot_math_kern_get_type",
    "hb_gobject_ot_meta_tag_get_type",
    "hb_gobject_ot_metrics_tag_get_type",
    "hb_gobject_ot_name_id_predefined_get_type",
    "hb_gobject_ot_var_axis_flags_get_type",
    "hb_gobject_ot_var_axis_info_get_type",
    "hb_gobject_paint_composite_mode_get_type",
    "hb_gobject_paint_extend_get_type",
    "hb_gobject_paint_funcs_get_type",
    "hb_gobject_script_get_type",
    "hb_gobject_segment_properties_get_type",
    "hb_gobject_set_get_type",
    "hb_gobject_shape_plan_get_type",
    "hb_gobject_style_tag_get_type",
    "hb_gobject_unicode_combining_class_get_type",
    "hb_gobject_unicode_funcs_get_type",
    "hb_gobject_unicode_general_category_get_type",
    "hb_gobject_user_data_key_get_type",
};
#undef GO
#undef GOM
#undef GOW
#undef GOWM
#undef GO2
#undef GOS
#undef DATA
#undef DATAV
#undef DATAB
#undef DATAM

static bool fribidi_symbol_filter(const char *name)
{
    return !strncmp(name, "fribidi_", 8);
}

static bool harfbuzz_symbol_filter(const char *name)
{
    return !strncmp(name, "hb_", 3);
}

typedef struct HarfBuzzLibraryPreflight {
    const char *soname;
    const char *label;
    const char *const *supported_symbols;
    size_t supported_symbol_count;
    bool required;
} HarfBuzzLibraryPreflight;

bool latx_harfbuzz_preflight_guest(path_collection_t *guest_paths,
                                   char *reason, size_t reason_size)
{
    static const HarfBuzzLibraryPreflight libraries[] = {
        {
            .soname = "libharfbuzz.so.0",
            .label = "HarfBuzz",
            .supported_symbols = harfbuzz_supported_symbols,
            .supported_symbol_count =
                ARRAY_SIZE(harfbuzz_supported_symbols),
            .required = true,
        },
        {
            .soname = "libharfbuzz-subset.so.0",
            .label = "HarfBuzz subset",
            .supported_symbols = harfbuzz_subset_supported_symbols,
            .supported_symbol_count =
                ARRAY_SIZE(harfbuzz_subset_supported_symbols),
        },
        {
            .soname = "libharfbuzz-icu.so.0",
            .label = "HarfBuzz ICU",
            .supported_symbols = harfbuzz_icu_supported_symbols,
            .supported_symbol_count =
                ARRAY_SIZE(harfbuzz_icu_supported_symbols),
        },
        {
            .soname = "libharfbuzz-cairo.so.0",
            .label = "HarfBuzz Cairo",
            .supported_symbols = harfbuzz_cairo_supported_symbols,
            .supported_symbol_count =
                ARRAY_SIZE(harfbuzz_cairo_supported_symbols),
        },
        {
            .soname = "libharfbuzz-gobject.so.0",
            .label = "HarfBuzz GObject",
            .supported_symbols = harfbuzz_gobject_safe_symbols,
            .supported_symbol_count =
                ARRAY_SIZE(harfbuzz_gobject_safe_symbols),
        },
    };

    if (!guest_paths || !reason || !reason_size) {
        return false;
    }
    reason[0] = '\0';
    for (size_t i = 0; i < ARRAY_SIZE(libraries); i++) {
        const HarfBuzzLibraryPreflight *library = &libraries[i];
        char *guest_path = ResolveFile(library->soname, guest_paths);
        bool exists = guest_path && access(guest_path, R_OK) == 0;
        bool safe;

        if (!exists && !library->required) {
            box_free(guest_path);
            continue;
        }
        safe = latx_wrappedlib_preflight_guest(
            guest_path, library->soname, library->label,
            harfbuzz_symbol_filter, library->supported_symbols,
            library->supported_symbol_count, NULL, 0, NULL, 0, NULL,
            reason, reason_size);
        box_free(guest_path);
        if (!safe) {
            return false;
        }
    }
    return true;
}

int latx_harfbuzz_preflight_or_disable(path_collection_t *guest_paths,
                                       const char *requesting_soname)
{
    char reason[256];

    if (latx_harfbuzz_preflight_guest(guest_paths, reason,
                                      sizeof(reason))) {
        return 0;
    }
    kzt_groups_log_wrapper_rejection(requesting_soname, reason);
    kzt_group_disable(KZT_GROUP_TEXT, reason);
    return -1;
}

bool latx_text_preflight_guest(path_collection_t *guest_paths,
                               char *reason, size_t reason_size)
{
    char *guest_path;
    bool safe;

    if (!guest_paths || !reason || !reason_size) {
        return false;
    }
    reason[0] = '\0';
    guest_path = ResolveFile("libfribidi.so.0", guest_paths);
    safe = latx_wrappedlib_preflight_guest(
        guest_path, "libfribidi.so.0", "FriBidi", fribidi_symbol_filter,
        fribidi_supported_symbols, ARRAY_SIZE(fribidi_supported_symbols),
        fribidi_required_host_symbols,
        ARRAY_SIZE(fribidi_required_host_symbols), NULL, 0, NULL,
        reason, reason_size);
    box_free(guest_path);
    return safe;
}

int latx_text_preflight_or_disable(path_collection_t *guest_paths,
                                   const char *requesting_soname)
{
    char reason[256];

    if (latx_text_preflight_guest(guest_paths, reason, sizeof(reason))) {
        return 0;
    }
    kzt_groups_log_wrapper_rejection(requesting_soname, reason);
    kzt_group_disable(KZT_GROUP_TEXT, reason);
    return -1;
}
