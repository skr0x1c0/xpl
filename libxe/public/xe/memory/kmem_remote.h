//
//  kmem_remote.h
//  xe
//
//  Created by admin on 12/2/21.
//

#ifndef xe_kmem_remote_h
#define xe_kmem_remote_h

#include <stdio.h>
#include "kmem.h"

struct xe_kmem_remote_server_ctx {
    uintptr_t mh_execute_header;
};

void xe_kmem_remote_server_start(const struct xe_kmem_remote_server_ctx* ctx);

struct xe_kmem_backend* xe_kmem_remote_client_create(const char* socket_path);
uintptr_t xe_kmem_remote_client_get_mh_execute_header(struct xe_kmem_backend* client);
void xe_kmem_remote_client_destroy(struct xe_kmem_backend* backend);

#endif /* xe_kmem_remote_h */
