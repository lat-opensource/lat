#ifndef LATX_SMC_RELOAD_GUEST_BASE_TEST_H
#define LATX_SMC_RELOAD_GUEST_BASE_TEST_H

int __wrap_mprotect(void *addr, size_t len, int prot);

#endif
