//
//  platform_types.h
//  xe
//
//  Created by admin on 11/20/21.
//

#ifndef platform_types_h
#define platform_types_h

// STRUCT proc
#define TYPE_PROC_MEM_P_LIST_OFFSET 0
#define TYPE_PROC_MEM_P_PID_OFFSET  104
#define TYPE_PROC_MEM_P_UTHLIST_OFFSET 152
#define TYPE_PROC_MEM_P_FD_OFFSET   224

// STRUCT uthread
#define TYPE_UTHREAD_MEM_SYSCALL_CODE_OFFSET 74
#define TYPE_UTHREAD_MEM_UU_LIST_OFFSET 296
#define TYPE_UTHREAD_MEM_UU_THREAD_OFFSET 224

// STRUCT thread
#define TYPE_THREAD_MEM_STATE_OFFSET 456
#define TYPE_THREAD_MEM_KERNEL_STACK_OFFSET 232
#define TYPE_THREAD_MEM_MACHINE_OFFSET 248

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

// CLASS OSDictionary
#define TYPE_OS_DICTIONARY_MEM_DICT_ENTRY_OFFSET 0x20
#define TYPE_OS_DICTIONARY_MEM_CAPACITY_OFFSET 0x18
#define TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET 0x14

// STRUCT zone
#define TYPE_ZONE_SIZE 168
#define TYPE_ZONE_MEM_Z_ELEMS_RSV_OFFSET 52
#define TYPE_ZONE_MEM_Z_ELEM_SIZE_OFFSET 54
#define TYPE_ZONE_MEM_Z_LOCK_OFFSET 64
#define TYPE_ZONE_MEM_Z_PAGEQ_EMPTY_OFFSET 116
#define TYPE_ZONE_MEM_Z_PAGEQ_PARTIAL_OFFSET 120
#define TYPE_ZONE_MEM_Z_PAGEQ_FULL_OFFSET 124
#define TYPE_ZONE_MEM_Z_ELEMS_FREE_OFFSET 156
#define TYPE_ZONE_MEM_Z_ELEMS_AVAIL_OFFSET 160

#define TYPE_ZONE_MEM_Z_PERCPU_BITFIED_OFFSET 452

// STRUCT zone_info
#define TYPE_ZONE_INFO_MEM_ZI_BITS_RANGE_OFFSET 64
#define TYPE_ZONE_INFO_MEM_ZI_META_BASE_OFFSET 80

// STRUCT zone_page_metadata
#define TYPE_ZONE_PAGE_METADATA_SIZE 16
#define TYPE_ZONE_PAGE_METADATA_MEM_ZM_INDEX_BIT_OFFSET 0
#define TYPE_ZONE_PAGE_METADATA_MEM_ZM_INDEX_BIT_SIZE 11
#define TYPE_ZONE_PAGE_METADATA_MEM_ZM_INLINE_BITMAP_BIT_OFFSET 11
#define TYPE_ZONE_PAGE_METADATA_MEM_ZM_INLINE_BITMAP_BIT_SIZE 1
#define TYPE_ZONE_PAGE_METADATA_MEM_ZM_CHUNK_LEN_BIT_OFFSET 12
#define TYPE_ZONE_PAGE_METADATA_MEM_ZM_CHUNK_LEN_BIT_SIZE 4
#define TYPE_ZONE_PAGE_METADATA_MEM_ZM_ALLOC_SIZE_OFFSET 2
#define TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET 4

// STRUCT zone_map_range
#define TYPE_ZONE_MAP_RANGE_MEM_MIN_ADDRESS_OFFSET 0
#define TYPE_ZONE_MAP_RANGE_MEM_MAX_ADDRESS_OFFSET 8

#endif /* platform_types_h */
