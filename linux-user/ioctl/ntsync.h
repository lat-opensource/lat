/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Host ntsync ioctl definitions used by linux-user.
 *
 * Keep this file in sync with Linux's uapi/linux/ntsync.h.  The target
 * command encodings live in linux-user/syscall_defs.h so a cross build
 * still uses the target ABI for guest ioctl numbers.
 */

#ifndef QEMU_LINUX_USER_NTSYNC_H
#define QEMU_LINUX_USER_NTSYNC_H

#include <stdint.h>
#include <sys/ioctl.h>

#define NTSYNC_WAIT_REALTIME 0x1
#define NTSYNC_MAX_WAIT_COUNT 64

/* Replace old or partial distribution definitions as one complete ABI. */
#undef NTSYNC_IOC_CREATE_SEM
#undef NTSYNC_IOC_SEM_RELEASE
#undef NTSYNC_IOC_WAIT_ANY
#undef NTSYNC_IOC_WAIT_ALL
#undef NTSYNC_IOC_CREATE_MUTEX
#undef NTSYNC_IOC_MUTEX_UNLOCK
#undef NTSYNC_IOC_MUTEX_KILL
#undef NTSYNC_IOC_CREATE_EVENT
#undef NTSYNC_IOC_EVENT_SET
#undef NTSYNC_IOC_EVENT_RESET
#undef NTSYNC_IOC_EVENT_PULSE
#undef NTSYNC_IOC_SEM_READ
#undef NTSYNC_IOC_MUTEX_READ
#undef NTSYNC_IOC_EVENT_READ

#define NTSYNC_IOC_CREATE_SEM      _IOW('N', 0x80, uint32_t[2])
#define NTSYNC_IOC_SEM_RELEASE     _IOWR('N', 0x81, uint32_t)
#define NTSYNC_IOC_WAIT_ANY        _IOWR('N', 0x82, uint8_t[40])
#define NTSYNC_IOC_WAIT_ALL        _IOWR('N', 0x83, uint8_t[40])
#define NTSYNC_IOC_CREATE_MUTEX    _IOW('N', 0x84, uint32_t[2])
#define NTSYNC_IOC_MUTEX_UNLOCK    _IOWR('N', 0x85, uint32_t[2])
#define NTSYNC_IOC_MUTEX_KILL      _IOW('N', 0x86, uint32_t)
#define NTSYNC_IOC_CREATE_EVENT    _IOW('N', 0x87, uint32_t[2])
#define NTSYNC_IOC_EVENT_SET       _IOR('N', 0x88, uint32_t)
#define NTSYNC_IOC_EVENT_RESET     _IOR('N', 0x89, uint32_t)
#define NTSYNC_IOC_EVENT_PULSE     _IOR('N', 0x8a, uint32_t)
#define NTSYNC_IOC_SEM_READ        _IOR('N', 0x8b, uint32_t[2])
#define NTSYNC_IOC_MUTEX_READ      _IOR('N', 0x8c, uint32_t[2])
#define NTSYNC_IOC_EVENT_READ      _IOR('N', 0x8d, uint32_t[2])

#endif /* QEMU_LINUX_USER_NTSYNC_H */
