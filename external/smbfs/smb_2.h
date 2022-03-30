/*
 * Copyright (c) 2011 - 2012 Apple Inc. All rights reserved.
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

#ifndef external_smbfs_smb_2_h
#define external_smbfs_smb_2_h

/* SMB 2/3 Commands, 2.2.1 */
#define SMB2_NEGOTIATE       0x0000
#define SMB2_SESSION_SETUP   0x0001
#define SMB2_LOGOFF          0x0002
#define SMB2_TREE_CONNECT    0x0003
#define SMB2_TREE_DISCONNECT 0x0004
#define SMB2_CREATE          0x0005
#define SMB2_CLOSE           0x0006
#define SMB2_FLUSH           0x0007
#define SMB2_READ            0x0008
#define SMB2_WRITE           0x0009
#define SMB2_LOCK            0x000A
#define SMB2_IOCTL           0x000B
#define SMB2_CANCEL          0x000C
#define SMB2_ECHO            0x000D
#define SMB2_QUERY_DIRECTORY 0x000E
#define SMB2_CHANGE_NOTIFY   0x000F
#define SMB2_QUERY_INFO      0x0010
#define SMB2_SET_INFO        0x0011
#define SMB2_OPLOCK_BREAK    0x0012

/* SMB 2/3 Dialects, 2.2.3 */
#define SMB2_DIALECT_0202   0x0202
#define SMB2_DIALECT_02ff   0x02ff
#define SMB2_DIALECT_0210   0x0210
#define SMB2_DIALECT_0300   0x0300
#define SMB2_DIALECT_0302   0x0302
#define SMB2_DIALECT_0311   0x0311

/* SMB 2/3 Negotiate Capabilities, 2.2.3 */
#define SMB2_GLOBAL_CAP_DFS                 0x00000001
#define SMB2_GLOBAL_CAP_LEASING             0x00000002
#define SMB2_GLOBAL_CAP_LARGE_MTU           0x00000004
#define SMB2_GLOBAL_CAP_MULTI_CHANNEL       0x00000008
#define SMB2_GLOBAL_CAP_PERSISTENT_HANDLES    0x00000010
#define SMB2_GLOBAL_CAP_DIRECTORY_LEASING    0x00000020
#define SMB2_GLOBAL_CAP_ENCRYPTION          0x00000040

#endif /* external_smbfs_smb_2_h */
