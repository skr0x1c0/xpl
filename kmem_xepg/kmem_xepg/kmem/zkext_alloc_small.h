//
//  zalloc_kext_small.h
//  kmem_xepg
//
//  Created by admin on 12/27/21.
//

#ifndef zalloc_kext_small_h
#define zalloc_kext_small_h

#include <stdio.h>

struct kmem_zkext_alloc_small_entry {
    uintptr_t address;
    smb_nic_allocator element_allocator;
};

struct kmem_zkext_alloc_small_entry kmem_zalloc_kext_small(const struct sockaddr_in* smb_addr, char* data, size_t data_size);

#endif /* zalloc_kext_small_h */
