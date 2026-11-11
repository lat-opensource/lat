/* SPDX-License-Identifier: GPL-2.0-only */
#include "qemu/osdep.h"

ssize_t __real_readlink(const char *path, char *buf, size_t bufsiz);
ssize_t __wrap_readlink(const char *path, char *buf, size_t bufsiz);
