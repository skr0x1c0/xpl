//
//  large_mem.h
//  libxe
//
//  Created by admin on 2/15/22.
//

#ifndef large_mem_h
#define large_mem_h

#include <stdio.h>

typedef struct xe_allocator_large_mem* xe_allocator_large_mem_t;

xe_allocator_large_mem_t xe_allocator_large_mem_allocate(size_t size, uintptr_t* addr_out);
uintptr_t xe_allocator_large_mem_allocate_disowned(size_t size);
void xe_allocator_large_mem_free(xe_allocator_large_mem_t* allocator_p);

#endif /* large_mem_h */
