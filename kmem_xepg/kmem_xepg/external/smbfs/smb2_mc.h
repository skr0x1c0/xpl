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

typedef enum _SMB2_MC_NIC_STATE {
    SMB2_MC_STATE_IDLE          = 0x00, /* This NIC is ready to be used */
    SMB2_MC_STATE_ON_TRIAL      = 0x01, /* This NIC was sent to connection trial */
    SMB2_MC_STATE_USED          = 0x02, /* This NIC is being used */
    SMB2_MC_STATE_DISCONNECTED  = 0x03, /* An update found this NIC to be disconnected */

}_SMB2_MC_NIC_STATE;

#define SMB2_MC_NIC_IN_BLACKLIST 0x0001


/*
 * The complete nic's info. Contains all available IP addresses.
 * Will be saved in the session, in order to create alternative channels
 */
struct complete_nic_info_entry {
    uint64_t nic_index;       /* the nic's index */
    uint64_t nic_link_speed;  /* the link speed of the nic */
    uint32_t nic_caps;        /* the nic's capabilities */
    uint32_t nic_type;        /* indicates the type of the nic */
    uint8_t  nic_ip_types;    /* indicates the types of IPs this nic has */
    uint32_t nic_flags;       /* Black-listed, etc */
    _SMB2_MC_NIC_STATE nic_state;

    struct {
        void* tqh_first;
        void** tqh_last;
    } addr_list;
    
    struct {
        struct complete_nic_info_entry* next;
        struct complete_nic_info_entry** prev;
    } next;
    
    struct {
        void* tqh_first;
        void** tqh_last;
    } possible_connections;
};

#endif /* smb2_mc_h */
