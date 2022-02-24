//
//  fake_session.h
//  xe_kmem
//
//  Created by admin on 2/22/22.
//

#ifndef fake_session_h
#define fake_session_h

#include <stdio.h>


static const struct sockaddr_in FAKE_SESSION_NIC_SADDR = {
    .sin_len = sizeof(struct sockaddr_in),
    .sin_family = AF_INET,
    .sin_port = 1234,
    .sin_addr = { 123456 },
};

#define FAKE_SESSION_NIC_INDEX   4321

typedef struct fake_session* fake_session_t;

fake_session_t fake_session_create(const struct sockaddr_in* smb_addr);
uintptr_t fake_session_get_addr(fake_session_t session);
void fake_session_destroy(fake_session_t* session_p);

#endif /* fake_session_h */
