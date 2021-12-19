#ifndef smb_client_h
#define smb_client_h

#include "../external/smbfs/smb2_mc.h"
#include "../external/smbfs/smb_dev.h"

int smb_client_load_kext(void);
int smb_client_open_dev(void);
int smb_client_ioc(int fd_dev, unsigned long cmd, void* data);
int smb_client_ioc_negotiate(int fd_dev, struct sockaddr_in* addr, int32_t ioc_saddr_len, _Bool need_multichannel);
int smb_client_ioc_ssn_setup(int fd_dev, char* gss_cpn, uint32_t gss_cpn_size, char* gss_spn, uint32_t gss_spn_size);
int smb_client_ioc_auth_info(int fd_dev, char* gss_cpn, uint32_t gss_cpn_size, char* gss_spn, uint32_t gss_spn_size);
int smb_client_ioc_nic_info(int fd_dev, struct smbioc_nic_info* out);
int smb_client_ioc_update_client_interface(int fd_dev, struct network_nic_info* info, size_t info_count);
int smb_client_ioc_multichannel_properties(int fd_dev, struct smbioc_multichannel_properties* out);


#endif /* smb_client_h */