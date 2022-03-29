//
//  kmem_bootstrap.h
//  xpl_kmem
//
//  Created by sreejith on 2/25/22.
//

#ifndef kmem_bootstrap_h
#define kmem_bootstrap_h

#include <stdio.h>

/// Create a new xpl_kmem_backend with read / write capability
/// Use this backend to setup `memory/kmem_fast.c`
/// @param smb_addr Socket address of smbx serer
xpl_kmem_backend_t kmem_bootstrap_create(const struct sockaddr_in* smb_addr);

/// Calculate the kernel address slide and returns the address of
/// mh_execute_header
/// @param backend `xpl_kmem_backend_t` created using `kmem_bootstrap_create`
uintptr_t kmem_bootstrap_get_mh_execute_header(xpl_kmem_backend_t backend);

/// Releases resources
/// @param backend_p `xpl_kmem_backend_t` created using `kmem_bootstrap_create`
void kmem_bootstrap_destroy(xpl_kmem_backend_t* backend_p);

#endif /* kmem_bootstrap_h */
