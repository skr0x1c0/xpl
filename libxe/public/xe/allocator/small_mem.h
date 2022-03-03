//
//  allocator_pipe.h
//  xe
//
//  Created by admin on 11/22/21.
//

#ifndef xe_allocator_small_mem_h
#define xe_allocator_small_mem_h

#include <stdio.h>

typedef struct xe_allocator_small_mem* xe_allocator_small_mem_t;

xe_allocator_small_mem_t xe_allocator_small_mem_allocate(size_t size, uintptr_t* buffer_ptr_out);
uintptr_t xe_allocator_small_allocate_disowned(size_t size);
void xe_allocator_small_mem_destroy(xe_allocator_small_mem_t* pipe);

#endif /* xe_allocator_small_mem_h */
