/* SPDX-License-Identifier: GPL-2.0-or-later */
static __thread int value __attribute__((tls_model("global-dynamic"))) = 17;
int *review_tls_cell(void);
int *review_tls_cell(void) { return &value; }
