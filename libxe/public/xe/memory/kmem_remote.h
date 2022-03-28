//
//  kmem_remote.h
//  xe
//
//  Created by admin on 12/2/21.
//

#ifndef xe_kmem_remote_h
#define xe_kmem_remote_h

#include <stdio.h>
#include "./kmem.h"

#define XE_DEFAULT_KMEM_SOCKET "/tmp/xe_kmem.sock"

int xe_kmem_remote_server_start(uintptr_t mh_execute_header, const char* uds_path);

int xe_kmem_remote_client_create(const char* socket_path, xe_kmem_backend_t* backend);
uintptr_t xe_kmem_remote_client_get_mh_execute_header(xe_kmem_backend_t client);
void xe_kmem_remote_client_destroy(xe_kmem_backend_t* backend_p);

#endif /* xe_kmem_remote_h */
