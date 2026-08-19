/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"

#include "wrappedlibs.h"

#include "box64context.h"
#include "bridge.h"
#include "library_private.h"
#include "wrappedharfbuzz-interop.h"
#include "wrappedtext-preflight.h"
#include "wrapper.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

const char *harfbuzzsubsetName = "libharfbuzz-subset.so.0";
#define LIBNAME harfbuzzsubset

#include "generated/wrappedharfbuzzsubsettypes.h"
#include "wrappercallback.h"

#define DEFINE_HB_SUBSET_USER_DATA(name) \
    EXPORT int32_t my_##name##_set_user_data( \
        void *object, void *key, void *data, void *destroy, int32_t replace) \
    { \
        return latx_hb_set_user_data( \
            (LatxHbSetUserDataFunc)my->name##_set_user_data, \
            object, key, data, destroy, replace); \
    } \
    EXPORT void *my_##name##_get_user_data(void *object, void *key) \
    { \
        return latx_hb_get_user_data( \
            (LatxHbGetUserDataFunc)my->name##_get_user_data, \
            object, key); \
    }

DEFINE_HB_SUBSET_USER_DATA(hb_subset_input)
DEFINE_HB_SUBSET_USER_DATA(hb_subset_plan)

#undef DEFINE_HB_SUBSET_USER_DATA

#define PRE_INIT_GUEST \
    do { \
        if (latx_harfbuzz_preflight_or_disable( \
                &box64->box64_ld_lib, harfbuzzsubsetName) != 0) { \
            return -1; \
        } \
    } while (0);

#define CUSTOM_INIT \
    getMy(lib);

#define CUSTOM_FINI \
    freeMy();

#include "wrappedlib_init.h"

#pragma GCC diagnostic pop
