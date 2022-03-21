//
//  pipe.c
//  libxe
//
//  Created by sreejith on 3/20/22.
//

#include <stdlib.h>

#include "util/pipe.h"
#include "util/assert.h"
#include "xnu/proc.h"
#include "memory/kmem.h"


struct xe_util_pipe {
    int         read_fd;
    int         write_fd;
    uintptr_t   buffer;
    size_t      size;
    char*       reset_buffer;
};


#define MAX_BUFFER_SIZE (4 * 16384)


xe_util_pipe_t xe_util_pipe_create(size_t size) {
    xe_assert_cond(size, <=, MAX_BUFFER_SIZE);
    
    int pfds[2];
    int res = pipe(pfds);
    xe_assert_errno(res);
    
    /// Make pipe allocate a buffer with size larger than required size
    char* buffer = malloc(size);
    ssize_t wrote_bytes = write(pfds[1], buffer, size);
    xe_assert_cond(wrote_bytes, ==, size);
    
    /// Reset write position to zero
    ssize_t read_bytes = read(pfds[0], buffer, size);
    xe_assert_cond(read_bytes, ==, size);
    
    /// Read address of allocated buffer
    uintptr_t pipe_buffer;
    int error = xe_xnu_proc_find_fd_data(xe_xnu_proc_current_proc(), pfds[0], &pipe_buffer);
    xe_assert_err(error);
    uintptr_t allocated_buffer = xe_kmem_read_ptr(pipe_buffer, TYPE_PIPEBUF_MEM_BUFFER_OFFSET);
    xe_assert_cond(xe_kmem_read_uint32(pipe_buffer, TYPE_PIPEBUF_MEM_SIZE_OFFSET), >=, size);
    
    xe_util_pipe_t pipe = malloc(sizeof(struct xe_util_pipe));
    pipe->read_fd = pfds[0];
    pipe->write_fd = pfds[1];
    pipe->buffer = allocated_buffer;
    pipe->size = size;
    pipe->reset_buffer = buffer;
    
    return pipe;
}


uintptr_t xe_util_pipe_get_address(xe_util_pipe_t pipe) {
    return pipe->buffer;
}


void xe_util_pipe_write(xe_util_pipe_t pipe, const void* data, size_t size) {
    xe_assert_cond(pipe->size, >=, size);
    ssize_t bytes_written = write(pipe->write_fd, data, size);
    xe_assert_cond(bytes_written, ==, size);
    /// Reset write position to zero
    ssize_t bytes_read = read(pipe->read_fd, pipe->reset_buffer, size);
    xe_assert_cond(bytes_read, ==, size);
}


void xe_util_pipe_destroy(xe_util_pipe_t* pipe_p) {
    xe_util_pipe_t pipe = *pipe_p;
    close(pipe->read_fd);
    close(pipe->write_fd);
    free(pipe->reset_buffer);
    free(pipe);
    *pipe_p = NULL;
}
