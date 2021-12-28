/*
 * Copyright (c) 2009 - 2020 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _SMBCLIENT_INTERNAL_H_
#define _SMBCLIENT_INTERNAL_H_

/* Optional smbfs mount flags */
#define SMBFS_MNT_SOFT              0x0001
#define SMBFS_MNT_NOTIFY_OFF        0x0002
#define SMBFS_MNT_STREAMS_ON        0x0004
#define SMBFS_MNT_DEBUG_ACL_ON      0x0008
#define SMBFS_MNT_DFS_SHARE         0x0010
#define SMBFS_MNT_COMPOUND_ON       0x0020
#define SMBFS_MNT_TIME_MACHINE      0x0040
#define SMBFS_MNT_READDIRATTR_OFF   0x0080
#define SMBFS_MNT_KERBEROS_OFF      0x0100  /* tmp until <12991970> is fixed */
#define SMBFS_MNT_FILE_IDS_OFF      0x0200
#define SMBFS_MNT_AAPL_OFF          0x0400
#define SMBFS_MNT_VALIDATE_NEG_OFF  0x0800
#define SMBFS_MNT_SUBMOUNTS_OFF     0x1000
#define SMBFS_MNT_DIR_LEASE_OFF     0x2000
#define SMBFS_MNT_FILE_DEF_CLOSE_OFF 0x4000
#define SMBFS_MNT_DIR_CACHE_OFF     0x8000
#define SMBFS_MNT_HIGH_FIDELITY     0x10000
#define SMBFS_MNT_DATACACHE_OFF     0x20000
#define SMBFS_MNT_MDATACACHE_OFF    0x40000
#define SMBFS_MNT_MULTI_CHANNEL_ON  0x80000
#define SMBFS_MNT_SNAPSHOT          0x100000
#define SMBFS_MNT_MC_PREFER_WIRED   0x200000
#define SMBFS_MNT_DISABLE_311       0x400000

#endif // _SMBCLIENT_INTERNAL_H_
