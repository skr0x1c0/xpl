//
//  kmem_remote.h
//  xe
//
//  Created by admin on 12/2/21.
//

#ifndef xpl_kmem_remote_h
#define xpl_kmem_remote_h

#include <stdio.h>
#include "./kmem.h"

#define xpl_DEFAULT_KMEM_SOCKET "/tmp/xpl_kmem.sock"

int xpl_kmem_remote_server_start(uintptr_t mh_execute_header, const char* uds_path);

int xpl_kmem_remote_client_create(const char* socket_path, xpl_kmem_backend_t* backend);
uintptr_t xpl_kmem_remote_client_get_mh_execute_header(xpl_kmem_backend_t client);
void xpl_kmem_remote_client_destroy(xpl_kmem_backend_t* backend_p);

#endif /* xpl_kmem_remote_h */
