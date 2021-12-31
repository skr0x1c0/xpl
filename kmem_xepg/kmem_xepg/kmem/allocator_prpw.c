#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdatomic.h>
#include <sys/errno.h>

#include "../smb/nic_allocator.h"

#include "allocator_prpw.h"
#include "platform_constants.h"
#include "util_dispatch.h"
#include "util_misc.h"

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
    
    assert(error == 0);
    return allocator;
}

size_t kmem_allocator_prpw_get_capacity(kmem_allocator_prpw_t allocator) {
    return allocator->backend_count * MAX_ALLOCS_PER_BACKEND;
}

int kmem_allocator_prpw_allocate(kmem_allocator_prpw_t allocator, size_t count, kmem_allocator_prpw_data_reader reader, void* reader_ctx) {
    size_t start_idx = atomic_fetch_add(&allocator->alloc_cursor, count);
    if ((start_idx + count) > (allocator->backend_count * MAX_ALLOCS_PER_BACKEND)) {
        atomic_fetch_sub(&allocator->alloc_cursor, count);
        return ERANGE;
    }

    size_t end_idx = start_idx + count - 1;
    size_t backend_start_idx = start_idx / MAX_ALLOCS_PER_BACKEND;
    size_t backend_end_idx = end_idx / MAX_ALLOCS_PER_BACKEND;

    return xe_util_dispatch_apply(&allocator->backends[backend_start_idx], sizeof(smb_nic_allocator), backend_end_idx - backend_start_idx + 1, NULL, ^(void* ctx, void* data, size_t index) {

        size_t backend_idx = backend_start_idx + index;
        size_t backend_alloc_start_idx = backend_idx * MAX_ALLOCS_PER_BACKEND;
        size_t backend_alloc_end_idx = ((backend_idx + 1) * MAX_ALLOCS_PER_BACKEND) - 1;
        size_t alloc_start_idx = XE_MAX(backend_alloc_start_idx, start_idx);
        size_t alloc_end_idx = XE_MIN(backend_alloc_end_idx, end_idx);
        size_t alloc_count = alloc_end_idx - alloc_start_idx + 1;

        char buffer[sizeof(struct network_nic_info) * alloc_count + UINT8_MAX];
        bzero(buffer, sizeof(buffer));
        struct network_nic_info* infos = (struct network_nic_info*)buffer;

        for (size_t i=0; i<alloc_count; i++)  {
            size_t alloc_idx = alloc_start_idx + i;
            infos[i].nic_index = (alloc_idx % MAX_ALLOCS_PER_BACKEND) / MAX_IPS_PER_NIC;
            infos[i].nic_caps = 1;
            infos[i].nic_type = 1;
            infos[i].nic_link_speed = 1024 * 1024;
            infos[i].port = 1;
            infos[i].next_offset = sizeof(struct network_nic_info);
            uint8_t len = 0;
            sa_family_t family = 0;
            char* data = NULL;
            size_t data_len = 0;
            reader(reader_ctx, &len, &family, &data, &data_len, alloc_idx - start_idx);
            if (family == AF_INET) {
                infos[i].addr_4.sin_len = len;
                infos[i].addr_4.sin_addr.s_addr = (uint32_t)alloc_idx;
                infos[i].addr_4.sin_family = family;
                assert(data_len <= UINT8_MAX - offsetof(struct sockaddr_in, sin_addr) - sizeof(struct in_addr));
                memcpy((char*)&infos[i].addr_4 + offsetof(struct sockaddr_in, sin_addr) + sizeof(struct in_addr), data, data_len);
            } else if (family == AF_INET6) {
                infos[i].addr_16.sin6_len = len;
                memcpy(&infos[i].addr_16.sin6_addr, &alloc_idx, sizeof(alloc_idx));
                infos[i].addr_16.sin6_family = family;
                assert(data_len <= UINT8_MAX - offsetof(struct sockaddr_in6, sin6_addr) - sizeof(struct in6_addr));
                memcpy((char*)&infos[i].addr_16 + offsetof(struct sockaddr_in6, sin6_addr) + sizeof(struct in6_addr), data, data_len);
            } else {
                infos[i].addr.sa_len = len;
                memcpy(infos[i].addr.sa_data, &alloc_idx, sizeof(alloc_idx));
                infos[i].addr.sa_family = family;
                assert(data_len <= UINT8_MAX - offsetof(struct sockaddr, sa_data) - sizeof(infos[i].addr.sa_data));
                memcpy((char*)&infos[i].addr + offsetof(struct sockaddr, sa_data) + sizeof(infos[i].addr.sa_data), data, data_len);
            }
        }

        return smb_nic_allocator_allocate(allocator->backends[backend_idx], infos, (uint32_t)alloc_count, (uint32_t)sizeof(buffer));
    });
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
        size_t backend_alloc_end_idx = XE_MIN(backend_alloc_start_idx + MAX_ALLOCS_PER_BACKEND, alloc_cursor) - 1;
        size_t backend_num_allocs = backend_alloc_end_idx - backend_alloc_start_idx + 1;
        size_t backend_num_nics = (backend_num_allocs + MAX_IPS_PER_NIC - 1) / MAX_IPS_PER_NIC;
        smb_nic_allocator* backend = (smb_nic_allocator*)data;
        struct smbioc_nic_info infos;
        int error = smb_nic_allocator_read(*backend, &infos);
        if (error) {
            return error;
        }
        assert(infos.num_of_nics == MAX_NICS_PER_BACKEND);
        for (int i=MAX_NICS_PER_BACKEND - 1; i>=(int)(MAX_NICS_PER_BACKEND - backend_num_nics); i--) {
            struct nic_properties props = infos.nic_props[i];
            uint32_t nic_idx = props.if_index & UINT32_MAX;
            assert(nic_idx == MAX_NICS_PER_BACKEND - i - 1);
            size_t nic_alloc_start_idx = backend_alloc_start_idx +  (nic_idx * MAX_IPS_PER_NIC);
            size_t nic_alloc_end_idx = XE_MIN(nic_alloc_start_idx + MAX_IPS_PER_NIC, alloc_cursor) - 1;
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
    size_t nic_alloc_count = XE_MIN(MAX_IPS_PER_NIC, alloc_cursor - ((backend_idx * MAX_ALLOCS_PER_BACKEND) + (nic_idx * MAX_IPS_PER_NIC)));
    size_t local_idx = nic_alloc_count - (alloc_index % MAX_IPS_PER_NIC) - 1;

    struct smbioc_nic_info info;
    int error = smb_nic_allocator_read(allocator->backends[backend_idx], &info);
    if (error) {
        return error;
    }

    assert(info.num_of_nics == MAX_NICS_PER_BACKEND);
    assert(nic_idx < info.num_of_nics);

    struct nic_properties props = info.nic_props[MAX_NICS_PER_BACKEND - nic_idx - 1];
    assert(props.if_index == nic_idx);
    assert(props.num_of_addrs >= nic_alloc_count);

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
        size_t num_allocated = XE_MIN(XE_MAX(0, allocator->alloc_cursor - (backend_idx * MAX_ALLOCS_PER_BACKEND)), MAX_ALLOCS_PER_BACKEND);
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
    memcpy(&allocator->backends[offset], &allocator->backends[offset + count], sizeof(allocator->backends[0] * (allocator->backend_count - offset - count)));
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