#ifndef smb_nic_allocator_h
#define smb_nic_allocator_h

#include <arpa/inet.h>

#include "../external/smbfs/smb_dev.h"
#include "../external/smbfs/smb_dev_2.h"
#include "../external/smbfs/smb2_mc.h"

typedef int smb_nic_allocator;

int smb_nic_allocator_create(const struct sockaddr_in* addr, uint32_t ioc_saddr_len, smb_nic_allocator* id_out);
int smb_nic_allocator_allocate(smb_nic_allocator id, const struct network_nic_info* infos, uint32_t info_count, uint32_t buffer_size);
int smb_nic_allocator_read(smb_nic_allocator id, struct smbioc_nic_info* info_out);
int smb_nic_allocator_destroy(smb_nic_allocator* id);

#endif /* smb_nic_allocator_h */
