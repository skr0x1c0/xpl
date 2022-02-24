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
#include "kmem_neighbor_reader.h"


/*
 * --- Allocates memory (upto 256 bytes) from `KHEAP_KEXT` zone
 * --- Address of allocated memory will be returned
 * --- Data after first 8 bytes in allocated memory can be controlled
 */


struct kmem_zkext_alloc_small_entry {
    uintptr_t address;
    kmem_allocator_prpw_t element_allocator;
};

struct kmem_zkext_alloc_small_entry kmem_zkext_alloc_small(const struct sockaddr_in* smb_addr, uint8_t alloc_size, uint8_t sa_family, char* data, uint8_t data_size);

#endif /* zalloc_kext_small_h */
