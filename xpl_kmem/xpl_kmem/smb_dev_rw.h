//
//  smb_dev_rw.h
//  xpl_kmem
//
//  Created by admin on 2/22/22.
//

#ifndef xpl_smb_dev_rw_h
#define xpl_smb_dev_rw_h

#include <stdio.h>

typedef struct smb_dev_rw* smb_dev_rw_t;

/// Creates upto two smb devices with read / write access to their
/// `struct smb_dev`
/// @param smb_addr Socket address of xpl_smbx server
/// @param devs Pointer to array for storing the created `smb_dev_rw_t`
void smb_dev_rw_create(const struct sockaddr_in* smb_addr, smb_dev_rw_t devs[2]);

/// Read the `struct smb_dev` assocated with the smb device
/// @param rw `smb_dev_rw_t` created using `smb_dev_rw_create`
/// @param out Memory location for storing the read `struct smb_dev`
int smb_dev_rw_read(smb_dev_rw_t rw, struct smb_dev* out);

/// Write data to `struct smb_dev` associated with the smb device
/// @param rw `smb_dev_rw_t` created using `smb_dev_rw_create`
/// @param dev Data to be written
int smb_dev_rw_write(smb_dev_rw_t rw, const struct smb_dev* dev);

/// Returns the file descriptor to the smb device associated with `smb_dev_rw_t`
/// @param rw `smb_dev_rw_t` created using `smb_dev_rw_create`
int smb_dev_rw_get_dev_fd(smb_dev_rw_t rw);

/// Returns FALSE when this `smb_dev_rw_t` can no longer read / write data
/// from / to `struct smb_dev` associated with the smb device
/// @param dev `smb_dev_rw_t` created using `smb_dev_rw_create`
_Bool smb_dev_rw_is_active(smb_dev_rw_t dev);

/// Returns the member `sd_session` of associated `struct smb_dev`
/// @param dev `smb_dev_rw_t` created using `smb_dev_rw_create`
uintptr_t smb_dev_rw_get_session(smb_dev_rw_t dev);

/// Release resources
/// @param rw_p pointer to `smb_dev_rw_t` created using `smb_dev_rw_create`
void smb_dev_rw_destroy(smb_dev_rw_t* rw_p);

#endif /* xpl_smb_dev_rw_h */
