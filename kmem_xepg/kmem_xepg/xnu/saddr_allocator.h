//
//  socket_allocator.h
//  kmem_xepg
//
//  Created by admin on 12/19/21.
//

#ifndef socket_allocator_h
#define socket_allocator_h

#include <stdio.h>

typedef struct xnu_saddr_allocator* xnu_saddr_allocator_t;

xnu_saddr_allocator_t xnu_saddr_allocator_create(int size);
void xnu_saddr_allocator_allocate(xnu_saddr_allocator_t allocator);
void xnu_saddr_allocator_fragment(xnu_saddr_allocator_t allocator, int pf);
int xnu_saddr_allocator_find_modified(xnu_saddr_allocator_t allocator, int* fd_out, size_t* fd_out_len);
void xnu_saddr_allocator_destroy(xnu_saddr_allocator_t* allocator_p);

#endif /* socket_allocator_h */
