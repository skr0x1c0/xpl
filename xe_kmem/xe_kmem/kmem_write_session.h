//
//  kmem_write_session.h
//  xe_kmem
//
//  Created by sreejith on 2/25/22.
//

#ifndef kmem_write_session_h
#define kmem_write_session_h

#include <stdio.h>


typedef struct kmem_write_session* kmem_write_session_t;

kmem_write_session_t kmem_write_session_create(const struct sockaddr_in* smb_addr);
void kmem_write_session_write_block(kmem_write_session_t session, int smb_dev, uintptr_t dst, char data[MAXTHREADNAMESIZE]);
uintptr_t kmem_write_session_get_addr(kmem_write_session_t session);
void kmem_write_session_destroy(kmem_write_session_t* session_p);

#endif /* kmem_write_session_h */
