
#ifndef xpl_kmem_fast_h
#define xpl_kmem_fast_h

#include <stdio.h>

#include "../memory/kmem.h"

/// Create a fast aribitary kernel memory read / write backend. A kmem backend should
/// already be setup with moderate read capabiility and atleast single write capability
xpl_kmem_backend_t xpl_memory_kmem_fast_create(void);

/// Release resources
/// @param backend_p pointer to `xpl_kmem_backend_t` created using
/// `xpl_memory_kmem_fast_create`
void xpl_memory_kmem_fast_destroy(xpl_kmem_backend_t* backend_p);

#endif /* xpl_kmem_fast_h */
