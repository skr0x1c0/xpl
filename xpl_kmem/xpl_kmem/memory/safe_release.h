
#ifndef xpl_safe_release_h
#define xpl_safe_release_h

#include <stdio.h>

void xpl_safe_release_reset_client_nics(int fd, xpl_slider_kext_t smbfs_slider);
void xpl_safe_release_reset_auth_info(int fd, xpl_slider_kext_t smbfs_slider);
void xpl_safe_release_restore_smb_dev(const struct sockaddr_in* addr, int fd, const struct smb_dev* backup, _Bool valid_mem, xpl_slider_kext_t slider);

#endif /* xpl_safe_release_h */
