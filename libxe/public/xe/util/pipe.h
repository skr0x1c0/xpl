//
//  pipe.h
//  libxe
//
//  Created by sreejith on 3/20/22.
//

#ifndef pipe_h
#define pipe_h

#include <stdio.h>

typedef struct xe_util_pipe* xe_util_pipe_t;

xe_util_pipe_t xe_util_pipe_create(size_t size);
uintptr_t xe_util_pipe_get_address(xe_util_pipe_t pipe);
void xe_util_pipe_write(xe_util_pipe_t pipe, const void* data, size_t size);
void xe_util_pipe_destroy(xe_util_pipe_t* pipe_p);

#endif /* pipe_h */
