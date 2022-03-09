//
//  macos_types.h
//  xe
//
//  Created by admin on 11/20/21.
//

#ifndef macos_common_types_h
#define macos_common_types_h

// STRUCT devdirent
#define TYPE_DEVDIRENT_MEM_DE_DNP_OFFSET (256 / 8)

// STRUCT devnode
#define TYPE_DEVNODE_MEM_DN_OPS_OFFSET (576 / 8)

// STRUCT proc
#define TYPE_PROC_MEM_P_LIST_OFFSET 0
#define TYPE_PROC_MEM_TASK_OFFSET 16
#define TYPE_PROC_MEM_P_PROC_RO_OFFSET 32
#define TYPE_PROC_MEM_P_PID_OFFSET  104
#define TYPE_PROC_MEM_P_UTHLIST_OFFSET 152
#define TYPE_PROC_MEM_P_HASH_OFFSET 168
#define TYPE_PROC_MEM_P_FD_OFFSET   224
#define TYPE_PROC_MEM_P_TEXTVP_OFFSET 928

// STRUCT proc_ro
#define TYPE_PROC_RO_MEM_PR_PROC_OFFSET 0
#define TYPE_PROC_RO_MEM_PR_TASK_OFFSET 8
#define TYPE_PROC_RO_SIZE 120

// STRUCT task
#define TYPE_TASK_MEM_THREADS_OFFSET 88
#define TYPE_TASK_MEM_THREAD_COUNT_OFFSET 128
#define TYPE_TASK_MEM_ITK_SPACE_OFFSET 776
#define TYPE_TASK_MEM_BSD_INFO_OFFSET 944

// STRUCT ipc_space
#define TYPE_IPC_SPACE_MEM_IS_TABLE_HASHED_OFFSET 20
#define TYPE_IPC_SPACE_MEM_IS_TABLE_FREE_OFFSET 24
#define TYPE_IPC_SPACE_MEM_IS_TABLE_OFFSET 32
#define TYPE_IPC_SPACE_MEM_IS_TABLE_NEXT_OFFSET 40
#define TYPE_IPC_SPACE_MEM_IS_TASK_OFFSET 48

// STRUCT ipc_entry
#define TYPE_IPC_ENTRY_MEM_IE_OBJECT_OFFSET 0
#define TYPE_IPC_ENTRY_MEM_IE_BITS_OFFSET 8
#define TYPE_IPC_ENTRY_MEM_IE_SIZE_OFFSET 8
#define TYPE_IPC_ENTRY_SIZE 24

// STRUCT ipc_port
#define TYPE_IPC_PORT_MEM_IP_OBJECT_OFFSET 0
#define TYPE_IPC_PORT_MEM_IP_RECEIVER_OFFSET 64

// STRUCT ipc_object
#define TYPE_IPC_OBJECT_MEM_IO_BITS_OFFSET 0
#define TYPE_IPC_OBJECT_MEM_IO_REFERENCES_OFFSET 4

// STRUCT uthread
#define TYPE_UTHREAD_MEM_SYSCALL_CODE_OFFSET 74
#define TYPE_UTHREAD_MEM_UU_LIST_OFFSET 296
#define TYPE_UTHREAD_MEM_UU_THREAD_OFFSET 224

// STRUCT thread
#define TYPE_THREAD_MEM_KERNEL_STACK_OFFSET 232
#define TYPE_THREAD_MEM_MACHINE_OFFSET 248
#define TYPE_THREAD_MEM_STATE_OFFSET 456
#define TYPE_THREAD_MEM_TASK_THREADS_OFFSET 1112
#define TYPE_THREAD_MEM_THREAD_ID_OFFSET 1200
#define TYPE_THREAD_MEM_ITH_VOUCHER_NAME_OFFSET 1292
#define TYPE_THREAD_MEM_THREAD_NAME_OFFSET 2056
#define TYPE_THREAD_SIZE 2080

// STRUCT filedesc
#define TYPE_FILEDESC_MEM_FD_NFILES_OFFSET 20
#define TYPE_FILEDESC_MEM_FD_OFILES_OFFSET 32

// STRUCT fileproc
#define TYPE_FILEPROC_MEM_FP_GLOB_OFFSET 16

// STRUCT fileglob
#define TYPE_FILEGLOB_MEM_FG_DATA_OFFSET 56

// STRUCT socket
#define TYPE_SOCKET_MEM_SO_PCB_OFFSET 16
#define TYPE_SOCKET_MEM_SO_PROTO_OFFSET 24

// STRUCT inpcb
#define TYPE_INPCB_MEM_INPCB_MTX_OFFSET 0
#define TYPE_INPCB_MEM_INP_PCBINFO_OFFSET 56
#define TYPE_INPCB_MEM_INP_FPORT_OFFSET 140
#define TYPE_INPCB_MEM_INP_LPORT_OFFSET 142
#define TYPE_INPCB_MEM_INP_VFLAG_OFFSET 172
#define TYPE_INPCB_MEM_INP_DEPENDFADDR_OFFSET 192
#define TYPE_INPCB_MEM_INP_DEPENDLADDR_OFFSET 212

// STRUCT inpcbinfo
#define TYPE_INPCBINFO_MEM_IPI_LOCK_OFFSET 56
#define TYPE_INPCBINFO_MEM_IPI_HASHMASK_OFFSET 120

// STRUCT nstat_control_state
#define TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET 0
#define TYPE_NSTAT_CONTROL_STATE_MEM_NCS_SRC_QUEUE_OFFSET 56

// STRUCT nstat_src
#define TYPE_NSTAT_MEM_NS_CONTROL_LINK_OFFSET 0
#define TYPE_NSTAT_MEM_COOKIE_OFFSET 40

// STRUCT nstat_tucookie
#define TYPE_NSTAT_TUCOOKIE_MEM_INP_OFFSET 0

// STRUCT necp_uuid_id_mapping
#define TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET 0
#define TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET 32

// STRUCT protosw
#define TYPE_PROTOSW_SIZE 168
#define TYPE_PROTOSW_MEM_PR_DOMAIN_OFFSET 16
#define TYPE_PROTOSW_MEM_PR_INPUT_OFFSET 40
#define TYPE_PROTOSW_MEM_PR_CTLOUTPUT_OFFSET 64
#define TYPE_PROTOSW_MEM_PR_LOCK_OFFSET 104
#define TYPE_PROTOSW_MEM_PR_UNLOCK_OFFSET 112
#define TYPE_PROTOSW_MEM_PR_GETLOCK_OFFSET 120

// STRUCT domain
#define TYPE_DOMAIN_SIZE 136
#define TYPE_DOMAIN_MEM_DOM_MTX_OFFSET 16

// STRUCT pipe
#define TYPE_PIPE_MEM_PIPE_BUFFER_OFFSET 0

// STRUCT pipebuf
#define TYPE_PIPEBUF_MEM_SIZE_OFFSET 12
#define TYPE_PIPEBUF_MEM_BUFFER_OFFSET 16

// STRUCT kalloc_heap
#define TYPE_KALLOC_HEAP_MEM_KH_LARGE_MAP_OFFSET 40

// STRUCT vm_map
#define TYPE_VM_MAP_MEM_LCK_RW_OFFSET 0

// STRUCT msdosfsmount
#define TYPE_MSDOSFSMOUNT_SIZE 344
#define TYPE_MSDOSFSMOUNT_MEM_PM_MOUNTP_OFFSET 0
#define TYPE_MSDOSFSMOUNT_MEM_PM_DEVVP_OFFSET 24
#define TYPE_MSDOSFSMOUNT_MEM_PM_BPB_OFFSET 32
#define TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BLOCK_SIZE_OFFSET 108
#define TYPE_MSDOSFSMOUNT_MEM_PM_FAT_MASK_OFFSET 112
#define TYPE_MSDOSFSMOUNT_MEM_PM_FATMULT 132
#define TYPE_MSDOSFSMOUNT_MEM_PM_FLAGS_OFFSET 144
#define TYPE_MSDOSFSMOUNT_MEM_PM_LABEL_OFFSET 168
#define TYPE_MSDOSFSMOUNT_MEM_PM_FAT_ACTIVE_VP_OFFSET 256
#define TYPE_MSDOSFSMOUNT_MEM_PM_FAT_MIRROR_VP_OFFSET 264
#define TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET 280
#define TYPE_MSDOSFSMOUNT_MEM_PM_FAT_BYTES_OFFSET 288
#define TYPE_MSDOSFSMOUNT_MEM_PM_FAT_CACHE_OFFSET_OFFSET 292
#define TYPE_MSDOSFSMOUNT_MEM_PM_FAT_FLAGS_OFFSET 296
#define TYPE_MSDOSFSMOUNT_MEM_PM_SYNC_INCOMPLETE_OFFSET 308
#define TYPE_MSDOSFSMOUNT_MEM_PM_SYNC_TIMER 312

// STRUCT bpb50
#define TYPE_BPB50_MEM_BPB_FATS_OFFSET 6

// STRUCT machine_thread
#define TYPE_MACHINE_THREAD_MEM_KSTACKPTR_OFFSET 80

// CLASS OSObject
#define TYPE_OS_OBJECT_MEM_VTABLE_OFFSET 0x0
#define TYPE_OS_OBJECT_MEM_RETAIN_COUNT_OFFSET 0x8

// CLASS OSDictionary
#define TYPE_OS_DICTIONARY_MEM_DICT_ENTRY_OFFSET 0x20
#define TYPE_OS_DICTIONARY_MEM_CAPACITY_OFFSET 0x18
#define TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET 0x14
#define TYPE_OS_DICTIONARY_MEM_CAPACITY_INCREMENT_OFFSET 0x1c

// CLASS OSString
#define TYPE_OS_STRING_MEM_STRING_OFFSET 0x10
#define TYPE_OS_STRING_MEM_LENGTH_BIT_OFFSET 0x6e
#define TYPE_OS_STRING_MEM_LENGTH_BIT_SIZE 0x12

// CLASS IORegistryEntry
#define TYPE_IO_REGISTRY_ENTRY_MEM_F_REGISTRY_TABLE_OFFSET 0x18
#define TYPE_IO_REGISTRY_ENTRY_MEM_F_PROPERTY_TABLE_OFFSET 0x20

// CLASS OSArray
#define TYPE_OS_ARRAY_MEM_ARRAY_OFFSET 0x20
#define TYPE_OS_ARRAY_MEM_CAPACITY_OFFSET 0x18
#define TYPE_OS_ARRAY_MEM_COUNT_OFFSET 0x14

// STRUCT zone
#define TYPE_ZONE_SIZE 168
#define TYPE_ZONE_MEM_Z_PCPU_CACHE_OFFSET 40
#define TYPE_ZONE_MEM_Z_CHUNK_PAGES_OFFSET 48
#define TYPE_ZONE_MEM_Z_ELEMS_RSV_OFFSET 52
#define TYPE_ZONE_MEM_Z_ELEM_SIZE_OFFSET 54
#define TYPE_ZONE_MEM_Z_LOCK_OFFSET 64
#define TYPE_ZONE_MEM_Z_RECIRC_OFFSET 80
#define TYPE_ZONE_MEM_Z_PAGEQ_EMPTY_OFFSET 116
#define TYPE_ZONE_MEM_Z_PAGEQ_PARTIAL_OFFSET 120
#define TYPE_ZONE_MEM_Z_PAGEQ_FULL_OFFSET 124
#define TYPE_ZONE_MEM_Z_CONTENTION_WMA_OFFSET 132
#define TYPE_ZONE_MEM_Z_RECIRC_CUR_OFFSET 140
#define TYPE_ZONE_MEM_Z_ELEMS_FREE_OFFSET 156
#define TYPE_ZONE_MEM_Z_ELEMS_AVAIL_OFFSET 160

#define TYPE_ZONE_BF0_OFFSET 58
#define TYPE_ZONE_BF0_MEM_Z_ASYNC_REFILLING_OFFSET 1
#define TYPE_ZONE_BF0_MEM_Z_ASYNC_REFILLING_SIZE 1
#define TYPE_ZONE_BF0_MEM_Z_PERCPU_BIT_OFFSET 4
#define TYPE_ZONE_BF0_MEM_Z_PERCPU_BIT_SIZE 1
#define TYPE_ZONE_BF0_MEM_Z_NOCACHING_BIT_OFFSET 6
#define TYPE_ZONE_BF0_MEM_Z_NOCACHING_BIT_SIZE 1

// STRUCT zone_info
#define TYPE_ZONE_INFO_MEM_ZI_META_RANGE_OFFSET 48
#define TYPE_ZONE_INFO_MEM_ZI_BITS_RANGE_OFFSET 64
#define TYPE_ZONE_INFO_MEM_ZI_META_BASE_OFFSET 80

// STRUCT zone_page_metadata
#define TYPE_ZONE_PAGE_METADATA_SIZE 16
#define TYPE_ZONE_PAGE_METADATA_BF0_OFFSET 0
#define TYPE_ZONE_PAGE_METADATA_BF0_MEM_ZM_INDEX_BIT_OFFSET 0
#define TYPE_ZONE_PAGE_METADATA_BF0_MEM_ZM_INDEX_BIT_SIZE 11
#define TYPE_ZONE_PAGE_METADATA_BF0_MEM_ZM_INLINE_BITMAP_BIT_OFFSET 11
#define TYPE_ZONE_PAGE_METADATA_BF0_MEM_ZM_INLINE_BITMAP_BIT_SIZE 1
#define TYPE_ZONE_PAGE_METADATA_BF0_MEM_ZM_CHUNK_LEN_BIT_OFFSET 12
#define TYPE_ZONE_PAGE_METADATA_BF0_MEM_ZM_CHUNK_LEN_BIT_SIZE 4
#define TYPE_ZONE_PAGE_METADATA_MEM_ZM_ALLOC_SIZE_OFFSET 2
#define TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET 4
#define TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_NEXT_OFFSET 8
#define TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_PREV_OFFSET 12

// STRUCT zone_map_range
#define TYPE_ZONE_MAP_RANGE_MEM_MIN_ADDRESS_OFFSET 0
#define TYPE_ZONE_MAP_RANGE_MEM_MAX_ADDRESS_OFFSET 8

// STRUCT vnode
#define TYPE_VNODE_MEM_VN_UN_OFFSET 120
#define TYPE_VNODE_MEM_VN_DATA_OFFSET 224

// STRUCT ubc_info
#define TYPE_UBC_INFO_MEM_CS_BLOBS_OFFSET 80

// STRUCT cs_blob
#define TYPE_CS_BLOB_MEM_CSB_NEXT_OFFSET 0
#define TYPE_CS_BLOB_MEM_CSB_HASHTYPE_OFFSET 112
#define TYPE_CS_BLOB_MEM_CSB_DER_ENTITLEMENTS_BLOB_OFFSET 192
#define TYPE_CS_BLOB_MEM_CSB_ENTITLEMENTS_OFFSET 200

// STRUCT cs_hash
#define TYPE_CS_HASH_SIZE 48
#define TYPE_CS_HASH_MEM_CS_SIZE_OFFSET 8

// CLASS OSEntitlements
#define TYPE_OS_ENTITLEMENTS_MEM_ENTITLEMENT_DICT_OFFSET 0x88

// STRUCT mount
#define TYPE_MOUNT_MEM_MNT_DATA_OFFSET 2296

// CLASS IOSurfaceRootUserClient
#define TYPE_IOSURFACE_ROOT_USER_CLIENT_MEM_CLIENT_COUNT_OFFSET 0x120
#define TYPE_IOSURFACE_ROOT_USER_CLIENT_MEM_CLIENT_ARRAY_OFFSET 0x118

// CLASS IOSurfaceClient
#define TYPE_IOSURFACE_CLIENT_MEM_IOSURFACE_OFFSET 0x40

// CLASS IOSurface
#define TYPE_IOSURFACE_MEM_PROPS_OFFSET 0xe8

// CLASS IOEventSource
#define TYPE_IO_EVENT_SOURCE_MEM_VTABLE_DESCRIMINATOR 0xcda1
#define TYPE_IO_EVENT_SOURCE_MEM_RESERVED_OFFSET 0x40
#define TYPE_IO_EVENT_SOURCE_MEM_FLAGS_OFFSET 0x2a
#define TYPE_IO_EVENT_SOURCE_MEM_ACTION_BLOCK_OFFSET 0x20
#define TYPE_BLOCK_LAYOUT_MEM_DESCRIPTOR_DESCRIMINATOR 0xc0bb
#define TYPE_IO_EVENT_SOURCE_MEM_EXPANSION_DATA_SIZE 16

// STRUCT kalloc_heap
#define TYPE_KALLOC_HEAP_MEM_KH_ZONES_OFFSET 0

// STRUCT kheap_zones
#define TYPE_KHEAP_ZONES_MEM_CFG_OFFSET 0
#define TYPE_KHEAP_ZONES_MEM_MAX_K_ZONE_OFFSET 20
#define TYPE_KHEAP_ZONES_MEM_K_ZONE_OFFSET 152

// STRUCT kalloc_zone_cfg
#define TYPE_KALLOC_ZONE_CFG_SIZE 40
#define TYPE_KALLOC_ZONE_CFG_MEM_KZC_SIZE_OFFSET 4

// STRUCT Block_layout
#define TYPE_BLOCK_LAYOUT_MEM_FLAGS_OFFSET 8
#define TYPE_BLOCK_LAYOUT_MEM_DESCRIPTOR_OFFSET 24

// STRUCT Block_descriptor_small
#define TYPE_BLOCK_DESCRIPTOR_SMALL_MEM_DISPOSE_OFFSET 16

// STRUCT mac_policy_list
#define TYPE_MAC_POLICY_LIST_MEM_NUMLOADED_OFFSET 0
#define TYPE_MAC_POLICY_LIST_MEM_MAXINDEX_OFFSET 8
#define TYPE_MAC_POLICY_LIST_MEM_STATICMAX_OFFSET 12
#define TYPE_MAC_POLICY_LIST_MEM_ENTRIES_OFFSET 24

// STRUCT mac_policy_conf
#define TYPE_MAC_POLICY_CONF_SIZE 80
#define TYPE_MAC_POLICY_CONF_MEM_MPC_NAME_OFFSET 0
#define TYPE_MAC_POLICY_CONF_MEM_MPC_OPS_OFFSET 32

// STRUCT mac_policy_ops
#define TYPE_MAC_POLICY_OPS_SIZE 2680
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_CHANGE_OFFSET 208
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_CREATE 216
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_DUP 224
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_FCNTL 232
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_GET_OFFSET 240
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_GET 248
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_INHERIT 256
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_IOCTL 264
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_LOCK 272
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_MMAP_DOWNGRADE 280
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_MMAP 288
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_RECEIVE 296
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_SET 304
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_LABEL_INIT 312
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_LABEL_DESTROY 320
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_LABEL_ASSOCIATE 328
#define TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_NOTIFY_CLOSE 336
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_SETACL (4160 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_SETATTRLIST (4224 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_SETEXTATTR (4288 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_SETFLAGS (4352 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_SETMODE (4416 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_SETOWNER (4480 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_SETUTIMES (4544 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_TRUNCATE (4608 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_GETATTRLISTBULK (4672 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_SWAP (4864 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_COPYFILE (5248 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_RENAME (7680 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_TRIGGER_RESOLVE (8576 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_RECLAIM (8768 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_GETATTR (15680 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_CLONE (15872 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_ACCESS (16128 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_CHDIR (16192 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_CHROOT (16256 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_CREATE (16320 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_DELETEEXTATTR (16384 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_EXCHANGEDATA (16448 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_EXEC (16512 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_GETATTRLIST (16576 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_GETEXTATTR (16640 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_IOCTL (16704 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_KQFILTER (16768 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_LABLE_UPDATE (16832 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_LINK (16896 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_LISTEXTATTR (16960 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_LOOKUP (17024 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_OPEN (17088 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_READ (17152 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_READDIR (17216 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_READLINK (17280 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_RENAME_FROM (17344 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_RENAME_TO (17408 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_REVOKE (17472 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SELECT (17536 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SETATTRLIST (17600 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SETEXTATTR (17664 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SETFLAGS (17728 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SETMODE (17792 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SETOWNER (17856 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SETUTIMES (17920 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_STAT (17984 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_TRUNCATE (18048 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_UNLINK (18112 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_WRITE (18176 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_ASSOCIATE_DEVFS (18240 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_ASSOCIATE_EXTATTR (18304 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_ASSOCIATE_FILE (18368 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_ASSOCIATE_PIPE (18432 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_ASSOCIATE_POSIXSEM (18496 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_ASSOCIATE_POSIXSHM (18560 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_ASSOCIATE_SINGLELABEL (18624 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_ASSOCIATE_SOCKET (18688 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_COPY (18752 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_DESTROY (18816 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_EXTERNALIZE_AUDIT (18880 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_EXTERNALIZE (18944 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_INIT (19008 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_INTERNALIZE (19072 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_RECYCLE (19136 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_STORE (19200 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_UPDATE_EXTATTR (19264 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_UPDATE (19328 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_CREATE (19392 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SIGNATURE (19456 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_UIPC_BIND (19520 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_UIPC_CONNECT (19584 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SUPPLEMENTAL_SIGNATURE (19904 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SEARCHFS (19968 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_FSGETPATH (20224 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_RENAME (20416 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SETACL (20480 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_DELETEEXTATTR (20544 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_LOOKUP_PREFLIGHT (20672 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_OPEN (20736 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_FIND_SIGS (20992 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_LINK (21248 / 8)

#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_QUOTACTL (5312 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_FSCTL (5376 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_GETATTR (5440 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_LABEL_UPDATE (5504 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_MOUNT (5568 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_REMOUNT (5632 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_SETATTR (5696 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_STAT (5760 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_UMOUNT (5824 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_LABEL_ASSOCIATE (5888 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_LABEL_DESTROY (5952 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_LABEL_EXTERNALIZE (6016 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_LABEL_INIT (6080 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_LABEL_INTERNALIZE (6144 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_MOUNT_LATE (8640 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_SNAPSHOT_MOUNT (15616 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_SNAPSHOT_REVERT (15616 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_SNAPSHOT_CREATE (15744 / 8)
#define TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_SNAPSHOT_DELETE (15808 / 8)

#endif /* macos_common_types_h */
