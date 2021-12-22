//
//  allocator_nic_parallel.h
//  kmem_xepg
//
//  Created by admin on 12/21/21.
//

#ifndef allocator_nic_parallel_h
#define allocator_nic_parallel_h

#include <stdio.h>

typedef struct kmem_allocator_nic_parallel* kmem_allocator_nic_parallel_t;

kmem_allocator_nic_parallel_t kmem_allocator_nic_parallel_create(const struct sockaddr_in* smb_addr, size_t allocs);
void kmem_allocator_nic_parallel_alloc(kmem_allocator_nic_parallel_t allocator, char* data, size_t data_size);
void kmem_allocator_nic_parallel_destroy(kmem_allocator_nic_parallel_t* allocator_p);

#endif /* allocator_nic_parallel_h */
