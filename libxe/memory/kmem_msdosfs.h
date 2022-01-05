//
//  kmem_msdosfs.h
//  xe
//
//  Created by admin on 11/26/21.
//

#ifndef kmem_msdosfs_h
#define kmem_msdosfs_h

#include <limits.h>

#include "platform_params.h"

struct kmem_msdosfs_init_args {
    char helper_mount_point[PATH_MAX];
    char worker_mount_point[PATH_MAX];
    
    char helper_data[TYPE_MSDOSFSMOUNT_SIZE];
    char worker_data[TYPE_MSDOSFSMOUNT_SIZE];
    
    uintptr_t helper_msdosfs;
    uintptr_t worker_msdosfs;
    
    int worker_bridge_fd;
    uintptr_t worker_bridge_vnode;
    
    int helper_bridge_fd;
    uintptr_t helper_bridge_vnode;
    
    void(^helper_mutator)(void* ctx, char data[TYPE_MSDOSFSMOUNT_SIZE]);
    void* helper_mutator_ctx;
};

struct xe_kmem_backend* xe_kmem_msdosfs_create(struct kmem_msdosfs_init_args* args);
void xe_kmem_msdosfs_destroy(struct xe_kmem_backend* backend);

#endif /* kmem_msdosfs_h */
