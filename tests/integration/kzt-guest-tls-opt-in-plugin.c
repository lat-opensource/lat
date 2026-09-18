/* SPDX-License-Identifier: GPL-2.0-or-later */
static __thread int value = 19;

int kzt_opt_in_tls_value(void)
{
    return value++;
}
