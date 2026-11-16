/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LATX_AOT_DEFER_H
#define LATX_AOT_DEFER_H
#include <stdint.h>
#define AOT_DEFER_ENV "LATX_QUICKTRANS_AOT_CONTROL"
#define AOT_DEFER_MAGIC UINT32_C(0x51414431)
typedef struct AOTDeferReady {
    uint32_t magic;
    int32_t pid;
} AOTDeferReady;
int aot_defer_connect(const char *path);
/* Close inherited application descriptors, register, then await permission. */
int aot_defer_wait(int control_fd);
#endif
