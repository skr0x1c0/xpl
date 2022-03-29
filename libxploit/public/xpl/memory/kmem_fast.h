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

/// Create a fast aribitary kernel memory read / write backend. A kmem backend should
/// already be setup with moderate read capabiility and atleast single write capability
xpl_kmem_backend_t xpl_memory_kmem_fast_create(void);

/// Release resources
/// @param backend_p pointer to `xpl_kmem_backend_t` created using
/// `xpl_memory_kmem_fast_create`
void xpl_memory_kmem_fast_destroy(xpl_kmem_backend_t* backend_p);

#endif /* kmem_fast_h */
