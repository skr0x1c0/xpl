//
//  zfree_kext.h
//  kmem_xepg
//
//  Created by admin on 12/20/21.
//

#ifndef zfree_kext_h
#define zfree_kext_h

#include <stdio.h>
#include "../external/smbfs/smb2_mc.h"

int kmem_zfree_kext(struct sockaddr_in* smb_addr, uintptr_t tailq);

#endif /* zfree_kext_h */
