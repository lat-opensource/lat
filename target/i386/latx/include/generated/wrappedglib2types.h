/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef __wrappedglib2TYPES_H_
#define __wrappedglib2TYPES_H_

#ifndef LIBNAME
#error You should only #include this file inside a wrapped*.c file
#endif
#ifndef ADDED_FUNCTIONS
#define ADDED_FUNCTIONS()
#endif

#define SUPER() ADDED_FUNCTIONS()

#endif /* __wrappedglib2TYPES_H_ */
