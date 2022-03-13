//
//  kmem_fast.h
//  libxe
//
//  Created by sreejith on 3/12/22.
//

#ifndef kmem_fast_h
#define kmem_fast_h

#include <stdio.h>

#include "../memory/kmem.h"

#define XE_KMEM_FAST_DEFAULT_IMAGE "Resources/libxe/memory/kmem_fast_img.dmg"

xe_kmem_backend_t xe_memory_kmem_fast_create(const char* base_img_path);
void xe_memory_kmem_fast_destroy(xe_kmem_backend_t* backend_p);

#endif /* kmem_fast_h */
