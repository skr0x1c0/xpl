//
//  smb_custom.h
//  kmem_xepg
//
//  Created by admin on 1/3/22.
//

#ifndef xe_kmem_smb_custom_h
#define xe_kmem_smb_custom_h

#define XE_SMBX_NB_LADDR_CMD_MAGIC 0xe770

#define XE_SMBX_NB_LADDR_CMD_FLAG_FAIL (1 << 0)
#define XE_SMBX_NB_LADDR_CMD_FLAG_SAVE (1 << 1)

struct xe_smbx_nb_laddr_cmd {
    uint16_t magic;
    uint16_t flags;
    uint32_t key;
};

#define XE_SMBX_SMB_CMD_GET_LAST_NB_SSN_REQUEST  255
#define XE_SMBX_SMB_CMD_GET_SAVED_NB_SSN_REQUEST 244

#define XE_SMBX_HOST   "127.0.0.1"
#define XE_SMBX_PORT   8090

#endif /* xe_kmem_smb_custom_h */