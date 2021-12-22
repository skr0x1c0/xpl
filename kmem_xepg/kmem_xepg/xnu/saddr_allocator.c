//
//  socket_allocator.c
//  kmem_xepg
//
//  Created by admin on 12/19/21.
//

#include <assert.h>
#include <stdlib.h>
#include <limits.h>
#include <libproc.h>
#include <stdatomic.h>

#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "saddr_allocator.h"
#include "util_dispatch.h"

#define SADDR_SIZE 64
#define SADDR_PATH_SIZE (64 - offsetof(struct sockaddr_un, sun_path))

struct xnu_saddr_allocator {
    int* fds;
    size_t fd_count;
    char root_dir[SADDR_PATH_SIZE];
    
    enum {
        STATE_CREATED,
        STATE_ALLOCATED,
        STATE_FRAGMENTED,
    } state;
};

xnu_saddr_allocator_t xnu_saddr_allocator_create(int size) {
    int* fds = malloc(sizeof(int) * size);
    int error = xe_util_dispatch_apply(fds, sizeof(fds[0]), size, NULL, ^(void* ctx, void* data, size_t index) {
        int fd = socket(PF_LOCAL, SOCK_STREAM, 0);
        if (fd < 0) {
            return errno;
        }
        *((int*)data) = fd;
        return 0;
    });
    assert(error == 0);
    
    xnu_saddr_allocator_t allocator = malloc(sizeof(struct xnu_saddr_allocator));
    allocator->fds = fds;
    allocator->fd_count = size;
    strcpy(allocator->root_dir, "/tmp/saddr_XXXXXX");
    mkdtemp(allocator->root_dir);
    allocator->state = STATE_CREATED;
    
    return allocator;
}


void xnu_saddr_allocator_allocate(xnu_saddr_allocator_t allocator) {
    assert(allocator->state == STATE_CREATED);
    int error = xe_util_dispatch_apply(allocator->fds, sizeof(allocator->fds[0]), allocator->fd_count, NULL, ^(void* ctx, void* data, size_t index) {
        struct sockaddr_un addr;
        bzero(&addr, sizeof(addr));
        
        addr.sun_len = SADDR_SIZE;
        addr.sun_family = AF_LOCAL;
        size_t sun_path_max_len = SADDR_PATH_SIZE;
        size_t required = snprintf(addr.sun_path, sun_path_max_len, "%s/%lu", allocator->root_dir, index);
        assert(required <= sun_path_max_len);
        
        if (bind(allocator->fds[index], (struct sockaddr*)&addr, 64)) {
            return errno;
        }
        return 0;
    });
    assert(error == 0);
    allocator->state = STATE_ALLOCATED;
}


void xnu_saddr_allocator_fragment(xnu_saddr_allocator_t allocator, int pf) {
    assert(allocator->state == STATE_ALLOCATED);
    dispatch_apply(allocator->fd_count / pf, DISPATCH_APPLY_AUTO, ^(size_t index) {
        size_t alloc_idx = pf * index;
        char path[SADDR_PATH_SIZE];
        snprintf(path, sizeof(path), "%s/%lu", allocator->root_dir, alloc_idx);
        int res = unlink(path);
        assert(res == 0);
        res = close(allocator->fds[alloc_idx]);
        assert(res == 0);
        allocator->fds[alloc_idx] = -1;
    });
    allocator->state = STATE_FRAGMENTED;
};


int xnu_saddr_allocator_find_modified(xnu_saddr_allocator_t allocator, int* fd_out, size_t* fd_out_len) {
    assert(allocator->state == STATE_ALLOCATED || allocator->state == STATE_FRAGMENTED);
    _Atomic int* found_idx = alloca(sizeof(int));
    *found_idx = 0;
    
    int error = xe_util_dispatch_apply(allocator->fds, sizeof(allocator->fds[0]), allocator->fd_count, NULL, ^(void* ctx, void* data, size_t index) {
        int fd = *((int*)data);
        
        if (fd < 0) {
            return 0;
        }
        
        struct socket_fdinfo info;
        int res = proc_pidfdinfo(getpid(), fd, PROC_PIDFDSOCKETINFO, &info, sizeof(info));
        assert(res != 0);
        
        uint8_t expected_sun_family = AF_LOCAL;
        uint8_t expected_sun_len = SADDR_SIZE;
        char expected_sun_path[SADDR_PATH_SIZE];
        snprintf(expected_sun_path, sizeof(expected_sun_path), "%s/%lu", allocator->root_dir, index);
        
        struct sockaddr_un* found = &info.psi.soi_proto.pri_un.unsi_addr.ua_sun;
        if (found->sun_family != expected_sun_family || found->sun_len != expected_sun_len || strncmp(found->sun_path, expected_sun_path, sizeof(expected_sun_path)) != 0) {
            int idx = atomic_fetch_add(found_idx, 1);
            if (idx >= *fd_out_len) {
                return ENOBUFS;
            }
            fd_out[idx] = fd;
        }
        return 0;
    });
    
    *fd_out_len = *found_idx;
    return error;
}


void xnu_saddr_allocator_destroy(xnu_saddr_allocator_t* allocator_p) {
    xnu_saddr_allocator_t allocator = *allocator_p;
    
    dispatch_apply(allocator->fd_count, DISPATCH_APPLY_AUTO, ^(size_t index){
        int fd = allocator->fds[index];
        if (fd >= 0) {
            char path[SADDR_PATH_SIZE];
            snprintf(path, sizeof(path), "%s/%lu", allocator->root_dir, index);
            unlink(path);
            close(fd);
        }
    });
    
    rmdir(allocator->root_dir);
    free(allocator->fds);
    free(allocator);
    *allocator_p = NULL;
}
