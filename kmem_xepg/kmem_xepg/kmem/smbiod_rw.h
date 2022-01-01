//
//  smbiod_rw.h
//  kmem_xepg
//
//  Created by admin on 12/31/21.
//

#ifndef smbiod_rw_h
#define smbiod_rw_h

#include <stdio.h>

typedef struct kmem_smbiod_rw* kmem_smbiod_rw_t;

kmem_smbiod_rw_t kmem_smbiod_rw_create(const struct sockaddr_in* smb_addr, int fd_rw, int fd_iod);
void kmem_smbiod_rw_read_iod(kmem_smbiod_rw_t rw, struct smbiod* out);
void kmem_smbiod_rw_write_iod(kmem_smbiod_rw_t rw, const struct smbiod* value);
void kmem_smbiod_rw_read_data(kmem_smbiod_rw_t rw, void* dst, uintptr_t src, size_t size);
void kmem_smbiod_rw_destroy(kmem_smbiod_rw_t* rw_p);

#endif /* smbiod_rw_h */
