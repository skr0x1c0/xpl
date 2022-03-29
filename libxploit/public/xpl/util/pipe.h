//
//  pipe.h
//  libxe
//
//  Created by sreejith on 3/20/22.
//

#ifndef pipe_h
#define pipe_h

#include <stdio.h>

typedef struct xpl_util_pipe* xpl_util_pipe_t;

xpl_util_pipe_t xpl_util_pipe_create(size_t size);
uintptr_t xpl_util_pipe_get_address(xpl_util_pipe_t pipe);
void xpl_util_pipe_write(xpl_util_pipe_t pipe, const void* data, size_t size);
void xpl_util_pipe_destroy(xpl_util_pipe_t* pipe_p);

#endif /* pipe_h */
