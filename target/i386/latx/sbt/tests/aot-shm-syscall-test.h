/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AOT_SHM_SYSCALL_TEST_H
#define AOT_SHM_SYSCALL_TEST_H

abi_ulong test_aot_shmat(int shmid);
abi_long test_aot_shmdt(abi_ulong addr);

#endif
