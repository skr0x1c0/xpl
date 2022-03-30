
#ifndef xpl_kmem_smb_custom_h
#define xpl_kmem_smb_custom_h

#define XPL_SMBX_NB_LADDR_CMD_MAGIC 0xe770

#define XPL_SMBX_NB_LADDR_CMD_FLAG_FAIL (1 << 0)
#define XPL_SMBX_NB_LADDR_CMD_FLAG_SAVE (1 << 1)

struct xpl_smbx_nb_laddr_cmd {
    uint16_t magic;
    uint16_t flags;
    uint32_t key;
};

#define XPL_SMBX_SMB_CMD_GET_LAST_NB_SSN_REQUEST  255
#define XPL_SMBX_SMB_CMD_GET_SAVED_NB_SSN_REQUEST 244

#define XPL_SMBX_HOST   "127.0.0.1"
#define XPL_SMBX_PORT   8090

#endif /* xpl_kmem_smb_custom_h */
