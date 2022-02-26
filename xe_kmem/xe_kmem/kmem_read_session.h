//
//  kmem_read_session.h
//  xe_kmem
//
//  Created by admin on 2/22/22.
//

#ifndef kmem_read_session_h
#define kmem_read_session_h

#include <stdio.h>


static const struct sockaddr_in FAKE_SESSION_NIC_ADDR = {
    .sin_len = sizeof(struct sockaddr_in),
    .sin_family = AF_INET,
    .sin_port = 1234,
    .sin_addr = { 123456 },
};

#define FAKE_SESSION_NIC_INDEX   4321

typedef struct kmem_read_session* kmem_read_session_t;

kmem_read_session_t kmem_read_session_create(const struct sockaddr_in* smb_addr);
uintptr_t kmem_read_session_get_addr(kmem_read_session_t session);
void kmem_read_session_destroy(kmem_read_session_t* session_p);

#endif /* kmem_read_session_h */
