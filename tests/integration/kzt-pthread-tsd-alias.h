/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef KZT_PTHREAD_TSD_ALIAS_H
#define KZT_PTHREAD_TSD_ALIAS_H

#include <pthread.h>

int direct_pthread_key_create(pthread_key_t *key,
                              void (*destructor)(void *));
int direct_pthread_setspecific(pthread_key_t key,
                               const void *value);
void *direct_pthread_getspecific(pthread_key_t key);

__asm__(".symver direct_pthread_key_create,"
        "__pthread_key_create@GLIBC_2.2.5");
__asm__(".symver direct_pthread_setspecific,"
        "__pthread_setspecific@GLIBC_2.2.5");
__asm__(".symver direct_pthread_getspecific,"
        "__pthread_getspecific@GLIBC_2.2.5");

#endif
