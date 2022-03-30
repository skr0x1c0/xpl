//
//  large_mem.h
//  libxe
//
//  Created by admin on 2/15/22.
//

#ifndef xpl_large_mem_h
#define xpl_large_mem_h

#include <stdio.h>

typedef struct xpl_allocator_large_mem* xpl_allocator_large_mem_t;

xpl_allocator_large_mem_t xpl_allocator_large_mem_allocate(size_t size, uintptr_t* addr_out);
uintptr_t xpl_allocator_large_mem_allocate_disowned(size_t size);
void xpl_allocator_large_mem_free(xpl_allocator_large_mem_t* allocator_p);

#endif /* xpl_large_mem_h */
