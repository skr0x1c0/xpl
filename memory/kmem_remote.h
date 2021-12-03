//
//  kmem_remote.h
//  xe
//
//  Created by admin on 12/2/21.
//

#ifndef kmem_remote_h
#define kmem_remote_h

#include <stdio.h>
#include "kmem.h"

void xe_kmem_remote_server_start(void);

struct xe_kmem_backend* xe_kmem_remote_create(const char* socket_path);
void xe_kmem_remote_destroy(struct xe_kmem_backend* backend);

#endif /* kmem_remote_h */
