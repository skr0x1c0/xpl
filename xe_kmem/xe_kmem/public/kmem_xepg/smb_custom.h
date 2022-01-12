//
//  smb_custom.h
//  kmem_xepg
//
//  Created by admin on 1/3/22.
//

#ifndef smb_custom_h
#define smb_custom_h

#define KMEM_XEPG_CMD_PADDR_MAGIC 0xe770

#define KMEM_XEPG_CMD_PADDR_FLAG_FAIL (1 << 0)
#define KMEM_XEPG_CMD_PADDR_FLAG_SAVE (1 << 1)

struct kmem_xepg_cmd_paddr {
    uint16_t magic;
    uint16_t flags;
    uint32_t key;
};

#define SMB_CUSTOM_CMD_GET_LAST_NB_SSN_REQUEST  255
#define SMB_CUSTOM_CMD_GET_SAVED_NB_SSN_REQUEST 244

#endif /* smb_custom_h */
