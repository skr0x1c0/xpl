//
//  xnu_pipe.c
//  xe
//
//  Created by admin on 11/22/21.
//

#include "xnu_pipe.h"
#include "kmem.h"
#include "platform_types.h"

uint xe_xnu_pipe_pipebuf_get_size(uintptr_t pipe) {
    return xe_kmem_read_int32(pipe + TYPE_PIPE_MEM_PIPE_BUFFER_OFFSET + TYPE_PIPEBUF_MEM_SIZE_OFFSET);
}

uintptr_t xe_xnu_pipe_pipebuf_get_buffer(uintptr_t pipe) {
    uintptr_t auth_ptr = xe_kmem_read_uint64(pipe + TYPE_PIPE_MEM_PIPE_BUFFER_OFFSET + TYPE_PIPEBUF_MEM_BUFFER_OFFSET);
    return auth_ptr | 0xfffff00000000000;
}
