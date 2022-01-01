//
//  kmem_smbiod_rw.h
//  kmem_xepg
//
//  Created by admin on 1/1/22.
//

#ifndef kmem_smbiod_h
#define kmem_smbiod_h

#include <stdio.h>

#include "kmem.h"
#include "smbiod_rw.h"

struct xe_kmem_backend* xe_kmem_smbiod_create(kmem_smbiod_rw_t rw);
void xe_kmem_smbiod_destroy(struct xe_kmem_backend** backend_p);

#endif /* kmem_smbiod_h */
