#ifndef smb_client_h
#define smb_client_h

#include "../external/smbfs/smb2_mc.h"
#include "../external/smbfs/smb_dev.h"
#include "../external/smbfs/netbios.h"

int smb_client_load_kext(void);
int smb_client_open_dev(void);
int smb_client_ioc(int fd_dev, unsigned long cmd, void* data);
int smb_client_ioc_negotiate(int fd_dev, const struct sockaddr_in* addr, int32_t ioc_saddr_len, _Bool need_multichannel);
int smb_client_ioc_negotiate_nb(int fd_dev, const struct sockaddr_nb* saddr, int32_t ioc_saddr_len, const struct sockaddr_nb* laddr, int32_t ioc_laddr_len);
int smb_client_ioc_ssn_setup(int fd_dev, char* gss_cpn, uint32_t gss_cpn_size, char* gss_spn, uint32_t gss_spn_size);
int smb_client_ioc_auth_info(int fd_dev, char* gss_cpn, uint32_t gss_cpn_size, char* gss_spn, uint32_t gss_spn_size, uint32_t* gss_target_nt);
int smb_client_ioc_nic_info(int fd_dev, struct smbioc_nic_info* out);
int smb_client_ioc_update_client_interface(int fd_dev, struct network_nic_info* info, size_t info_count);
int smb_client_ioc_notifier_update_interfaces(int fd_dev, struct network_nic_info* info, size_t info_count);
int smb_client_ioc_multichannel_properties(int fd_dev, struct smbioc_multichannel_properties* out);
int smb_client_ioc_tcon(int fd_dev, char* share_name);
int smb_client_ioc_read_saved_nb_ssn_request(int fd_dev, uint32_t key, char* dst, uint32_t dst_size);
int smb_client_ioc_read_last_nb_ssn_request(int fd_dev, char* dst, uint32_t dst_size);

#endif /* smb_client_h */
