//
//  xnu_pipe.h
//  xe
//
//  Created by admin on 11/22/21.
//

#ifndef xnu_pipe_h
#define xnu_pipe_h

#include <stdio.h>

uint xe_xnu_pipe_pipebuf_get_size(uintptr_t pipe);
uintptr_t xe_xnu_pipe_pipebuf_get_buffer(uintptr_t pipe);

#endif /* xnu_pipe_h */
