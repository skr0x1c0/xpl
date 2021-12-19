/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

#ifndef smb2_mc_h
#define smb2_mc_h

#include <net/if.h>
#include <netinet/in.h>

/*
 * The raw NIC's info coming from the client and the server
 * Contains only one IP address
 * will be used to construct the complete_nic_info_entry
 */
struct network_nic_info {
    uint32_t next_offset;
    uint32_t nic_index;
    uint32_t nic_caps;
    uint64_t nic_link_speed;
    uint32_t nic_type;
    in_port_t port;
    union { // alloc the largest sock addr possible
        struct sockaddr         addr;
        struct sockaddr_in      addr_4;
        struct sockaddr_in6     addr_16;
        struct sockaddr_storage addr_strg; // A place holder to make sure enough memory is allocated
        // for the largest possible sockaddr (ie NetBIOS's sockaddr_nb)
    };
};

#endif /* smb2_mc_h */
