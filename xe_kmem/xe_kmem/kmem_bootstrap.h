//
//  kmem_bootstrap.h
//  xe_kmem
//
//  Created by sreejith on 2/25/22.
//

#ifndef kmem_bootstrap_h
#define kmem_bootstrap_h

#include <stdio.h>

xe_kmem_backend_t kmem_boostrap_create(const struct sockaddr_in* smb_addr);
uintptr_t kmem_boostrap_get_mach_execute_header(xe_kmem_backend_t backend);
void kem_boostrap_destroy(xe_kmem_backend_t* backend_p);

#endif /* kmem_bootstrap_h */
