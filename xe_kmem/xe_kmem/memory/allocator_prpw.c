#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <sys/errno.h>

#include <xe/util/dispatch.h>
#include <xe/util/misc.h>
#include <xe/util/assert.h>

#include "../smb/nic_allocator.h"

#include "allocator_prpw.h"
#include <macos/kernel.h>


#define MAX_NICS_PER_BACKEND 16
#define MAX_IPS_PER_NIC 8
#define MAX_ALLOCS_PER_BACKEND (MAX_NICS_PER_BACKEND * MAX_IPS_PER_NIC)

struct kmem_allocator_prpw {
    smb_nic_allocator* backends;
    size_t backend_count;
    _Atomic size_t alloc_cursor;
};

kmem_allocator_prpw_t kmem_allocator_prpw_create(const struct sockaddr_in* addr, size_t num_allocs) {
    size_t backends_required = (num_allocs + MAX_ALLOCS_PER_BACKEND - 1) / MAX_ALLOCS_PER_BACKEND;
    smb_nic_allocator* backends = malloc(sizeof(smb_nic_allocator) * backends_required);
    xe_util_dispatch_apply(backends, sizeof(smb_nic_allocator), backends_required, NULL, ^(void* ctx, void* data, size_t index) {
        smb_nic_allocator* allocator = (smb_nic_allocator*)data;
        *allocator = smb_nic_allocator_create(addr, sizeof(*addr));
        return 0;
    });

    kmem_allocator_prpw_t allocator = (kmem_allocator_prpw_t)malloc(sizeof(struct kmem_allocator_prpw));
    allocator->backends = backends;
    allocator->backend_count = backends_required;
    atomic_init(&allocator->alloc_cursor, 0);

    int error = xe_util_dispatch_apply(backends, sizeof(smb_nic_allocator), backends_required, NULL, ^(void* ctx, void* data, size_t index) {
        smb_nic_allocator* allocator = (smb_nic_allocator*)data;
        struct network_nic_info infos[MAX_NICS_PER_BACKEND];
        bzero(infos, sizeof(infos));
        for (int i=0; i<MAX_NICS_PER_BACKEND; i++) {
            infos[i].nic_index = i;
            infos[i].nic_link_speed = 1024 * 1024;
            infos[i].addr.sa_family = AF_LOCAL;
            infos[i].addr.sa_len = sizeof(infos[i].addr);
            infos[i].next_offset = sizeof(struct network_nic_info);
        }
        return smb_nic_allocator_allocate(*allocator, infos, MAX_NICS_PER_BACKEND, sizeof(infos));
    });
    
    xe_assert_err(error);
    return allocator;
}

size_t kmem_allocator_prpw_get_capacity(kmem_allocator_prpw_t allocator) {
    return allocator->backend_count * MAX_ALLOCS_PER_BACKEND;
}

int kmem_allocator_prpw_allocate(kmem_allocator_prpw_t allocator, uint8_t address_len, sa_family_t address_family, const char* arbitary_data, int arbitary_data_len, int arbitary_data_offset) {
    size_t index = atomic_fetch_add(&allocator->alloc_cursor, 1);
    size_t backend = index / MAX_ALLOCS_PER_BACKEND;
    
    int min_offset = offsetof(struct sockaddr, sa_data);
    if (address_family == AF_INET) {
        min_offset = offsetof(struct sockaddr_in, sin_addr) + sizeof(struct in_addr);
    } else if (address_family == AF_INET6) {
        min_offset = offsetof(struct sockaddr_in6, sin6_addr) + sizeof(struct in6_addr);
    }
    
    if (arbitary_data_offset < min_offset) {
        return EINVAL;
    }
    
    if (arbitary_data_len + arbitary_data_offset > address_len) {
        return EINVAL;
    }
    
    /// Size of `struct network_nic_info` can hold only socket addresses upto 128 bytes size.
    /// Allocate bigger memory if required
    size_t info_size = sizeof(struct network_nic_info) - offsetof(struct network_nic_info, addr) + address_len;
    struct network_nic_info* info = alloca(info_size);
    bzero(info, info_size);
    
    info->nic_index = (index % MAX_ALLOCS_PER_BACKEND) / MAX_IPS_PER_NIC;
    info->addr.sa_len = address_len;
    info->addr.sa_family = address_family;
    memcpy((char*)&info->addr + arbitary_data_offset, arbitary_data, arbitary_data_len);
    
    uint32_t addr = (index % MAX_ALLOCS_PER_BACKEND) % MAX_IPS_PER_NIC;
    if (address_family == AF_INET) {
        info->addr_4.sin_addr.s_addr = addr;
    } else if (address_family == AF_INET6) {
        memcpy(&info->addr_16.sin6_addr, &addr, sizeof(addr));
    }
    
    return smb_nic_allocator_allocate(allocator->backends[backend], info, (uint32_t)1, (uint32_t)info_size);
}


int kmem_allocator_prpw_filter(kmem_allocator_prpw_t allocator, size_t offset, size_t count, kmem_allocator_prpw_data_filter filter, void* filter_ctx, int64_t* found_idx_out) {
    size_t alloc_cursor = allocator->alloc_cursor;
    size_t start_idx = offset;
    size_t end_idx = offset + count - 1;

    if (end_idx >= alloc_cursor) {
        return ERANGE;
    }

    size_t backend_start_idx = start_idx / MAX_ALLOCS_PER_BACKEND;
    size_t backend_end_idx = end_idx / MAX_ALLOCS_PER_BACKEND;
    size_t backend_count = backend_end_idx - backend_start_idx + 1;

    _Atomic int64_t found_idx;
    atomic_init(&found_idx, -1);

    int error = xe_util_dispatch_apply(&allocator->backends[backend_start_idx], sizeof(smb_nic_allocator), backend_count, &found_idx, ^(void* ctx, void* data, size_t index) {
        size_t backend_alloc_start_idx = (backend_start_idx + index) * MAX_ALLOCS_PER_BACKEND;
        size_t backend_alloc_end_idx = xe_min(backend_alloc_start_idx + MAX_ALLOCS_PER_BACKEND, alloc_cursor) - 1;
        size_t backend_num_allocs = backend_alloc_end_idx - backend_alloc_start_idx + 1;
        size_t backend_num_nics = (backend_num_allocs + MAX_IPS_PER_NIC - 1) / MAX_IPS_PER_NIC;
        smb_nic_allocator* backend = (smb_nic_allocator*)data;
        struct smbioc_nic_info infos;
        int error = smb_nic_allocator_read(*backend, &infos);
        if (error) {
            return error;
        }
        xe_assert(infos.num_of_nics == MAX_NICS_PER_BACKEND);
        for (int i=MAX_NICS_PER_BACKEND - 1; i>=(int)(MAX_NICS_PER_BACKEND - backend_num_nics); i--) {
            struct nic_properties props = infos.nic_props[i];
            uint32_t nic_idx = props.if_index & UINT32_MAX;
            xe_assert(nic_idx == MAX_NICS_PER_BACKEND - i - 1);
            size_t nic_alloc_start_idx = backend_alloc_start_idx +  (nic_idx * MAX_IPS_PER_NIC);
            size_t nic_alloc_end_idx = xe_min(nic_alloc_start_idx + MAX_IPS_PER_NIC, alloc_cursor) - 1;
            size_t nic_num_allocs = nic_alloc_end_idx - nic_alloc_start_idx + 1;
            for (size_t j=0; j<nic_num_allocs; j++) {
                size_t local_idx = nic_num_allocs - j - 1;
                struct address_propreties addr_props = props.addr_list[local_idx];
                size_t alloc_idx = nic_alloc_start_idx + j;
                if (filter(filter_ctx, addr_props.addr_family, alloc_idx)) {
                    int64_t expected = -1;
                    atomic_compare_exchange_strong((_Atomic int64_t*)ctx, &expected, alloc_idx);
                }
            }
        }
        return 0;
    });

    *found_idx_out = found_idx;
    return error;
}

int kmem_allocator_prpw_read(kmem_allocator_prpw_t allocator, size_t alloc_index, sa_family_t* family) {
    size_t alloc_cursor = allocator->alloc_cursor;
    if (alloc_index >= alloc_cursor) {
        return ERANGE;
    }
    size_t backend_idx = alloc_index / MAX_ALLOCS_PER_BACKEND;
    size_t nic_idx = (alloc_index % MAX_ALLOCS_PER_BACKEND) / MAX_IPS_PER_NIC;
    size_t nic_alloc_count = xe_min(MAX_IPS_PER_NIC, alloc_cursor - ((backend_idx * MAX_ALLOCS_PER_BACKEND) + (nic_idx * MAX_IPS_PER_NIC)));
    size_t local_idx = nic_alloc_count - (alloc_index % MAX_IPS_PER_NIC) - 1;

    struct smbioc_nic_info info;
    int error = smb_nic_allocator_read(allocator->backends[backend_idx], &info);
    if (error) {
        return error;
    }

    xe_assert(info.num_of_nics == MAX_NICS_PER_BACKEND);
    xe_assert(nic_idx < info.num_of_nics);

    struct nic_properties props = info.nic_props[MAX_NICS_PER_BACKEND - nic_idx - 1];
    xe_assert(props.if_index == nic_idx);
    xe_assert(props.num_of_addrs >= nic_alloc_count);

    *family = props.addr_list[local_idx].addr_family;
    return 0;
}

int kmem_allocator_prpw_trim_backend_count(kmem_allocator_prpw_t allocator, size_t offset, size_t count) {
    if ((offset + count) > allocator->backend_count) {
        return ERANGE;
    }

    _Atomic size_t num_released;
    atomic_init(&num_released, 0);

    int error = xe_util_dispatch_apply(&allocator->backends[offset], sizeof(allocator->backends[0]), count, &num_released, ^(void* ctx, void* data, size_t index) {
        size_t backend_idx = offset + index;
        size_t num_allocated = xe_min(xe_max(0, allocator->alloc_cursor - (backend_idx * MAX_ALLOCS_PER_BACKEND)), MAX_ALLOCS_PER_BACKEND);
        int error = smb_nic_allocator_destroy((smb_nic_allocator*)data);
        if (error) {
            return error;
        }
        atomic_fetch_add((_Atomic size_t*)ctx, num_allocated);
        return 0;
    });

    if (error) {
        return error;
    }

    atomic_fetch_sub(&allocator->alloc_cursor, num_released);
    memcpy(&allocator->backends[offset], &allocator->backends[offset + count], sizeof(allocator->backends[0]) * (allocator->backend_count - offset - count));
    allocator->backend_count -= count;

    return 0;
}

int kmem_allocator_prpw_release_containing_backend(kmem_allocator_prpw_t allocator, int64_t alloc_index) {
    if (alloc_index < 0 || alloc_index >= allocator->alloc_cursor) {
        return EINVAL;
    }

    return kmem_allocator_prpw_trim_backend_count(allocator, alloc_index / MAX_ALLOCS_PER_BACKEND, 1);
}

int kmem_allocator_prpw_destroy(kmem_allocator_prpw_t* id) {
    if (id == NULL) {
        return EINVAL;
    }

    kmem_allocator_prpw_t allocator = *id;
    int error = xe_util_dispatch_apply(allocator->backends, sizeof(smb_nic_allocator), allocator->backend_count, NULL, ^(void* ctx, void* data, size_t index) {
        return smb_nic_allocator_destroy((smb_nic_allocator*)data);
    });

    if (error) {
        return error;
    }

    free(allocator->backends);
    free(allocator);
    *id = NULL;
    return 0;
}
