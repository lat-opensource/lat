/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef WOWLATX_UNIX_BRIDGE_H
#define WOWLATX_UNIX_BRIDGE_H

#include "latx-wow64.h"

enum wowlatx_unix_call {
    WOWLATX_INIT,
    WOWLATX_RUN,
    WOWLATX_THREAD_EXIT,
    WOWLATX_MEMORY,
    WOWLATX_PROCESS_TERM,
};

struct wowlatx_run {
    LatWow64Context context;
    LatWow64Exit exit;
};

#endif
