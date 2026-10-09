/* SPDX-License-Identifier: GPL-2.0-only */
#include "qemu/osdep.h"
#include <sys/personality.h>
#include <sys/shm.h>

#include "qemu.h"
#include "segment.h"
#include "latx-options.h"

/* Unmodified production IPC code, without unrelated syscall tables. */
#include "aot-shm-syscall-test-body.h"

#include "aot-shm-syscall-test.h"

abi_ulong test_aot_shmat(int shmid)
{
    X86CPU cpu = { 0 };
    struct image_info info = { 0 };
    TaskState ts = { .info = &info };
    CPUState *cs = CPU(&cpu);

    cs->opaque = &ts;
    cs->tcg_cflags = CF_PARALLEL;
    return do_shmat(&cpu.env, shmid, 0, SHM_EXEC);
}

abi_long test_aot_shmdt(abi_ulong addr)
{
    return do_shmdt(addr);
}
