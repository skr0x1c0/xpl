//
//  kmem_smbiod_rw.h
//  kmem_xepg
//
//  Created by admin on 1/1/22.
//

#ifndef kmem_smbiod_h
#define kmem_smbiod_h

#include <stdio.h>

#include <xe/memory/kmem.h>

typedef struct xe_kmem_backend* kmem_smbiod_backend_t;

kmem_smbiod_backend_t kmem_smbiod_create(const struct sockaddr_in* smb_addr, int fd_rw, int fd_iod);
void kmem_smbiod_read_iod(kmem_smbiod_backend_t rw, struct smbiod* out);
void kmem_smbiod_write_iod(kmem_smbiod_backend_t rw, const struct smbiod* value);
void kmem_smbiod_destroy(kmem_smbiod_backend_t* backend_p);

#endif /* kmem_smbiod_h */
