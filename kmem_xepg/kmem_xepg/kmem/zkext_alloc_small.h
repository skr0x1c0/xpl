//
//  zalloc_kext_small.h
//  kmem_xepg
//
//  Created by admin on 12/27/21.
//

#ifndef zalloc_kext_small_h
#define zalloc_kext_small_h

#include <stdio.h>

#include "../smb/nic_allocator.h"

#include "allocator_prpw.h"
#include "zkext_neighbor_reader_xs.h"


struct kmem_zkext_alloc_small_entry {
    uintptr_t address;
    kmem_allocator_prpw_t element_allocator;
};

struct kmem_zkext_alloc_small_entry kmem_zkext_alloc_small(const struct sockaddr_in* smb_addr, kmem_zkext_neighbor_reader_xs_t reader, char* data, size_t data_size);

#endif /* zalloc_kext_small_h */
