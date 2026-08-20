/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FD_TRANS_H
#define FD_TRANS_H

typedef abi_long (*TargetFdDataFunc)(void *, size_t);
typedef abi_long (*TargetFdAddrFunc)(void *, abi_ulong, socklen_t);
typedef struct TargetFdTrans {
    TargetFdDataFunc host_to_target_data;
    TargetFdDataFunc target_to_host_data;
    TargetFdAddrFunc target_to_host_addr;
} TargetFdTrans;

TargetFdDataFunc fd_trans_target_to_host_data(int fd);
TargetFdDataFunc fd_trans_host_to_target_data(int fd);
TargetFdAddrFunc fd_trans_target_to_host_addr(int fd);
void fd_trans_register(int fd, TargetFdTrans *trans);
void fd_trans_unregister(int fd);
void fd_trans_dup(int oldfd, int newfd);

extern TargetFdTrans target_packet_trans;
#ifdef CONFIG_RTNETLINK
extern TargetFdTrans target_netlink_route_trans;
#endif
extern TargetFdTrans target_netlink_audit_trans;
extern TargetFdTrans target_netlink_crypto_trans;
extern TargetFdTrans target_signalfd_trans;
extern TargetFdTrans target_eventfd_trans;
#if (defined(TARGET_NR_inotify_init) && defined(__NR_inotify_init)) || \
    (defined(CONFIG_INOTIFY1) && defined(TARGET_NR_inotify_init1) && \
     defined(__NR_inotify_init1))
extern TargetFdTrans target_inotify_trans;
#endif
#if (defined(TARGET_NR_fanotify_init) && defined(__NR_fanotify_init) && \
     defined(CONFIG_FANOTIFY))
extern TargetFdTrans target_fanotify_trans;
#endif
#endif
