//
//  allocator_pipe.h
//  xe
//
//  Created by admin on 11/22/21.
//

#ifndef xpl_allocator_small_mem_h
#define xpl_allocator_small_mem_h

#include <stdio.h>

typedef struct xpl_allocator_small_mem* xpl_allocator_small_mem_t;

xpl_allocator_small_mem_t xpl_allocator_small_mem_allocate(size_t size, uintptr_t* buffer_ptr_out);
uintptr_t xpl_allocator_small_allocate_disowned(size_t size);
void xpl_allocator_small_mem_destroy(xpl_allocator_small_mem_t* pipe);

#endif /* xpl_allocator_small_mem_h */
