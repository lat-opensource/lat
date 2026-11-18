/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef LATX_WOW64_PE_HOST_H
#define LATX_WOW64_PE_HOST_H

/* Host operations used by the shared user-mode TCG runtime. */
void lat_wow64_pe_rcu_init(void);
void lat_wow64_pe_prepare_exit(void);

#endif
