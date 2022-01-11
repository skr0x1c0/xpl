//
//  allocator_pipe.h
//  xe
//
//  Created by admin on 11/22/21.
//

#ifndef xe_allocator_pipe_h
#define xe_allocator_pipe_h

#include <stdio.h>

typedef struct xe_allocator_pipe* xe_allocator_pipe_t;

int xe_allocator_pipe_allocate(size_t size, xe_allocator_pipe_t* allocator_out, uintptr_t* buffer_ptr_out);
int xe_allocator_pipe_destroy(xe_allocator_pipe_t* pipe);

#endif /* xe_allocator_pipe_h */
