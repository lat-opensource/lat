/* SPDX-License-Identifier: GPL-2.0-or-later */
typedef int (*probe_function_t)(void);

int probe_data = 37;

int probe_normal(void)
{
    return 42;
}

static probe_function_t resolve_function(void)
{
    return probe_normal;
}

static probe_function_t resolve_null(void)
{
    return (probe_function_t)0;
}

int probe_ifunc(void) __attribute__((ifunc("resolve_function")));
int probe_null_ifunc(void) __attribute__((ifunc("resolve_null")));

__thread int probe_public_tls = 23;

int *probe_public_tls_address(void)
{
    return &probe_public_tls;
}

/* The host probe returns a different value to prove wrapper execution. */
int XSupportsLocale(void)
{
    return 17;
}
