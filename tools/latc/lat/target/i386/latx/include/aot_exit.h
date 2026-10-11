/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * AOT exit worker policy.
 */
#ifndef LATX_AOT_EXIT_H
#define LATX_AOT_EXIT_H

static inline bool aot_exit_worker_should_daemonize(
    bool ipc_namespace_isolated)
{
    return !ipc_namespace_isolated;
}

#endif
