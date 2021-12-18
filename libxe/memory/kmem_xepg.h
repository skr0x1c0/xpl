//
//  kmem_xepg.h
//  xe
//
//  Created by admin on 12/15/21.
//

#ifndef kmem_xepg_h
#define kmem_xepg_h

typedef struct xe_kmem_xepg* xe_kmem_xepg_t;

int xe_kmem_xepg_create(xe_kmem_xepg_t* out);
int xe_kmem_xepg_execute(xe_kmem_xepg_t xepg, int index);
int xe_kmem_xepg_spin(xe_kmem_xepg_t xepg);
int xe_kmem_xepg_get_fd(xe_kmem_xepg_t xepg);
int xe_kmem_xepg_destroy(xe_kmem_xepg_t* xepgp);

#endif /* kmem_xepg_h */
