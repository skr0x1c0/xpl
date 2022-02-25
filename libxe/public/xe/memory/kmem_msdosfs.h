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

#define KMEM_MSDOSFS_DEFAULT_BASE_IMAGE "Resources/libxe/memory/kmem_msdosfs_img.dmg"

xe_kmem_backend_t xe_kmem_msdosfs_create(const char* base_img_path);
void xe_kmem_msdosfs_destroy(xe_kmem_backend_t* backend_p);

#endif /* xe_kmem_msdosfs_h */
