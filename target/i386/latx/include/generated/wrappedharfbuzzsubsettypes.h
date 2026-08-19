/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef __wrappedharfbuzzsubsetTYPES_H_
#define __wrappedharfbuzzsubsetTYPES_H_

#ifndef LIBNAME
#error You should only #include this file inside a wrapped*.c file
#endif
#ifndef ADDED_FUNCTIONS
#define ADDED_FUNCTIONS()
#endif

typedef int32_t (*iFppppi_t)(void*, void*, void*, void*, int32_t);
typedef void* (*pFpp_t)(void*, void*);

#define SUPER() ADDED_FUNCTIONS() \
    GO(hb_subset_input_get_user_data, pFpp_t) \
    GO(hb_subset_input_set_user_data, iFppppi_t) \
    GO(hb_subset_plan_get_user_data, pFpp_t) \
    GO(hb_subset_plan_set_user_data, iFppppi_t)

#endif /* __wrappedharfbuzzsubsetTYPES_H_ */
