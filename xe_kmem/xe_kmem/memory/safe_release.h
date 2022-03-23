//
//  safe_release.h
//  xe_kmem
//
//  Created by sreejith on 3/22/22.
//

#ifndef safe_release_h
#define safe_release_h

#include <stdio.h>

void xe_safe_release_reset_client_nics(int fd, xe_slider_kext_t smbfs_slider);
void xe_safe_release_reset_auth_info(int fd, xe_slider_kext_t smbfs_slider);
void xe_safe_release_restore_smb_dev(const struct sockaddr_in* addr, int fd, const struct smb_dev* backup, _Bool valid_mem, xe_slider_kext_t slider);

#endif /* safe_release_h */
