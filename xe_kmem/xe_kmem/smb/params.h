//
//  constants.h
//  kmem_xepg
//
//  Created by admin on 1/1/22.
//

#ifndef constants_h
#define constants_h

/* Variables */
#define VAR_SMB_SESSION_LIST_OFFSET_DATA 0x1e48

/* Types */
// STRUCT smb_connobj
#define TYPE_SMB_CONNOBJ_MEM_CO_CHILDREN_OFFSET 64
#define TYPE_SMB_CONNOBJ_MEM_CO_NEXT_OFFSET 72

// STRUCT smb_session
#define TYPE_SMB_SESSION_MEM_SESSION_IOD_OFFSET 328
#define TYPE_SMB_SESSION_MEM_SESSION_INTERFACE_TABLE_OFFSET 1056

// STRUCT session_network_interface_info
#define TYPE_SESSION_INTERFACE_INFO_MEM_CLIENT_NIC_COUNT_OFFSET 20
#define TYPE_SESSION_INTERFACE_INFO_MEM_CLIENT_NIC_INFO_LIST_OFFSET 24

#endif /* constants_h */
