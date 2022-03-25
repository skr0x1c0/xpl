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

/// Create a fast aribitary kernel memory read / write backend
xe_kmem_backend_t xe_memory_kmem_fast_create(void);

/// Release resources
/// @param backend_p pointer to `xe_kmem_backend_t` created using
/// `xe_memory_kmem_fast_create`
void xe_memory_kmem_fast_destroy(xe_kmem_backend_t* backend_p);

#endif /* kmem_fast_h */
