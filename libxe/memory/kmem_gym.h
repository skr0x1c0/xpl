//
//  kmem_gym.h
//  xe
//
//  Created by admin on 11/26/21.
//

#ifndef kmem_gym_h
#define kmem_gym_h

#include "kmem.h"

struct xe_kmem_backend* xe_kmem_gym_create(void);
void xe_kmem_gym_destroy(struct xe_kmem_backend* backend);

#endif /* kmem_gym_h */
