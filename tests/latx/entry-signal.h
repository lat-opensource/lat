/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LATX_TEST_ENTRY_SIGNAL_H
#define LATX_TEST_ENTRY_SIGNAL_H

#include <signal.h>

void entry_spin(volatile sig_atomic_t *value);

#endif
