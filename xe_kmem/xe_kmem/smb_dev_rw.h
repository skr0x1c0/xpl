//
//  smb_dev_rw.h
//  xe_kmem
//
//  Created by admin on 2/22/22.
//

#ifndef smb_dev_rw_h
#define smb_dev_rw_h

#include <stdio.h>

typedef struct smb_dev_rw* smb_dev_rw_t;

smb_dev_rw_t smb_dev_rw_create(const struct sockaddr_in* smb_addr);
void smb_dev_rw_read(smb_dev_rw_t rw, struct smb_dev* out);
void smb_dev_rw_write(smb_dev_rw_t rw, struct smb_dev* dev);
int smb_dev_get_active_dev(smb_dev_rw_t rw);
void smb_dev_rw_destroy(smb_dev_rw_t rw);

#endif /* smb_dev_rw_h */
