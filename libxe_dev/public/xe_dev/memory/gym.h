//
//  kmem_gym.h
//  xe
//
//  Created by admin on 11/26/21.
//

#ifndef xe_dev_kmem_gym_h
#define xe_dev_kmem_gym_h

#include <xe/memory/kmem.h>

xe_kmem_backend_t xe_kmem_gym_create(void);
void xe_kmem_gym_destroy(xe_kmem_backend_t* backend_p);

#endif /* xe_dev_kmem_gym_h */
