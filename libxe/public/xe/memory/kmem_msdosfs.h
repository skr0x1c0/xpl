//
//  kmem_msdosfs.h
//  xe
//
//  Created by admin on 11/26/21.
//

#ifndef xe_kmem_msdosfs_h
#define xe_kmem_msdosfs_h

#include <limits.h>

#include "macos_params.h"

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

xe_kmem_backend_t xe_kmem_msdosfs_create(struct kmem_msdosfs_init_args* args);
void xe_kmem_msdosfs_destroy(xe_kmem_backend_t* backend_p);

#endif /* xe_kmem_msdosfs_h */
