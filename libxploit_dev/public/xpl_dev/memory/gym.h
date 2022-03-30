
#ifndef xpl_dev_kmem_gym_h
#define xpl_dev_kmem_gym_h

#include <xpl/memory/kmem.h>

xpl_kmem_backend_t xpl_kmem_gym_create(void);
void xpl_kmem_gym_destroy(xpl_kmem_backend_t* backend_p);

#endif /* xpl_dev_kmem_gym_h */
