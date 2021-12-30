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

typedef struct kmem_zkext_free_session* kmem_zkext_free_session_t;
kmem_zkext_free_session_t kmem_zkext_free_session_create(const struct sockaddr_in* smb_addr);
struct complete_nic_info_entry kmem_zkext_free_session_prepare(kmem_zkext_free_session_t session);
void kmem_zkext_free_session_execute(kmem_zkext_free_session_t session, const struct complete_nic_info_entry* entry);
void kmem_zkext_free_session_destroy(kmem_zkext_free_session_t* session_p);

#endif /* zfree_kext_h */
