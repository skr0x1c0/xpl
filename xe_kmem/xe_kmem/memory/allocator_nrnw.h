//
//  allocator_nrnw.h
//  xe_kmem
//
//  Created by admin on 2/22/22.
//

#ifndef allocator_nrnw_h
#define allocator_nrnw_h

#include <stdio.h>
#include <arpa/inet.h>

typedef struct kmem_allocator_nrnw* kmem_allocator_nrnw_t;

kmem_allocator_nrnw_t kmem_allocator_nrnw_create(const struct sockaddr_in* smb_addr);
void kmem_allocator_nrnw_allocate(kmem_allocator_nrnw_t allocator, size_t size, size_t num_allocs);
void kmem_allocator_nrnw_destroy(kmem_allocator_nrnw_t* allocator_p);

#endif /* allocator_nrnw_h */
