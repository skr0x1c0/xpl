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

#ifndef smb_dev_2_h
#define smb_dev_2_h

#include "smb_dev.h"

/*
 * Multi Channel support
 */
struct smbioc_client_interface {
    SMB_IOC_POINTER(struct network_nic_info*, info_array);
    uint32_t interface_instance_count;
    uint32_t total_buffer_size;
    uint32_t ioc_errno;
};

/*
 * Device IOCTLs
 */
/* MC SMBIOC */
#define SMBIOC_UPDATE_CLIENT_INTERFACES   _IOW('n', 127, struct smbioc_client_interface)

#endif /* smb_dev_2_h */
