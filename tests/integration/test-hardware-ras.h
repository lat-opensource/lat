/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef TEST_HARDWARE_RAS_H
#define TEST_HARDWARE_RAS_H

#include <signal.h>

void ras_signal_spin(volatile sig_atomic_t *done);

#endif
