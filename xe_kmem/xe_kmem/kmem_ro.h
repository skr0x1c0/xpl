//
//  kmem_ro.h
//  xe_kmem
//
//  Created by admin on 2/23/22.
//

#ifndef kmem_ro_h
#define kmem_ro_h

#include <stdio.h>

#include <xe/memory/kmem.h>

xe_kmem_backend_t kmem_ro_create(const struct sockaddr_in* smb_addr);
uintptr_t kmem_ro_get_mach_execute_header(xe_kmem_backend_t backend);
void kem_ro_destroy(xe_kmem_backend_t backend);

#endif /* kmem_ro_h */
