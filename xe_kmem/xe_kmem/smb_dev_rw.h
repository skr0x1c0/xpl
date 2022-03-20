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

void smb_dev_rw_create(const struct sockaddr_in* smb_addr, smb_dev_rw_t devs[2]);
int smb_dev_rw_read(smb_dev_rw_t rw, struct smb_dev* out);
int smb_dev_rw_write(smb_dev_rw_t rw, struct smb_dev* dev);
int smb_dev_rw_get_dev_fd(smb_dev_rw_t rw);
_Bool smb_dev_rw_is_active(smb_dev_rw_t dev);
uintptr_t smb_dev_rw_get_session(smb_dev_rw_t dev);
void smb_dev_rw_destroy(smb_dev_rw_t* rw_p);

#endif /* smb_dev_rw_h */
