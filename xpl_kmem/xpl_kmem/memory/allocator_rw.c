#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#include <sys/errno.h>
#include <arpa/inet.h>

#include <xpl/util/dispatch.h>
#include <xpl/util/misc.h>
#include <xpl/util/assert.h>
#include <xpl/util/log.h>

#include "../smb/ssn_allocator.h"

#include "allocator_rw.h"
#include <macos/kernel.h>


#define MAX_BACKEND_COUNT 1023
#define DEFAULT_SSN_ALLOCATOR_IOC_SADDR_LEN 128
#define DEFAULT_SSN_ALLOCATOR_USER_SIZE 16
#define DEFAULT_SSN_ALLOCATOR_PASSWORD_SIZE 16
#define DEFAULT_SSN_ALLOCATOR_DOMAIN_SIZE 16


struct xpl_allocator_rw {
    struct sockaddr_in addr;
    smb_ssn_allocator* backends;
    int backend_count;
    _Atomic int write_index;
};


xpl_allocator_rw_t xpl_allocator_rw_create(const struct sockaddr_in* addr, int count) {
    xpl_assert(count <= MAX_BACKEND_COUNT);

    smb_ssn_allocator* backends = malloc(sizeof(smb_ssn_allocator) * count);
    int error = xpl_util_dispatch_apply(backends, sizeof(smb_ssn_allocator), count, NULL, ^(void* ctx, void* data, size_t index) {
        smb_ssn_allocator* allocator = (smb_ssn_allocator*)data;
        *allocator = smb_ssn_allocator_create(addr, DEFAULT_SSN_ALLOCATOR_IOC_SADDR_LEN);
        return 0;
    });
    xpl_assert_err(error);

    xpl_allocator_rw_t allocator = malloc(sizeof(struct xpl_allocator_rw));
    memcpy(&allocator->addr, addr, xpl_min(sizeof(struct sockaddr_in), addr->sin_len));
    allocator->backends = backends;
    allocator->backend_count = count;
    atomic_init(&allocator->write_index, 0);
    return allocator;
}

int xpl_allocator_rw_allocate(xpl_allocator_rw_t allocator, int count, xpl_allocator_rw_data_reader data_reader, void* reader_ctx) {
    if (count == -1) {
        count = allocator->backend_count;
    }

    size_t write_idx = atomic_fetch_add(&allocator->write_index, count);
    if (write_idx + count > allocator->backend_count) {
        atomic_fetch_sub(&allocator->write_index, count);
        return ERANGE;
    }

    int error = xpl_util_dispatch_apply(&allocator->backends[write_idx], sizeof(smb_ssn_allocator), count, NULL, ^(void* ctx, void* data, size_t index) {
        smb_ssn_allocator* id = (smb_ssn_allocator*)data;
        char* data1 = NULL;
        char* data2 = NULL;
        uint32_t data1_size = 0;
        uint32_t data2_size = 0;
        data_reader(reader_ctx, &data1, &data1_size, &data2, &data2_size, index);

        int error = smb_ssn_allocator_allocate(*id, data1, data1_size, data2, data2_size, DEFAULT_SSN_ALLOCATOR_USER_SIZE, DEFAULT_SSN_ALLOCATOR_PASSWORD_SIZE, DEFAULT_SSN_ALLOCATOR_DOMAIN_SIZE);
        
        if (error) {
            xpl_log_warn("rw allocate failed for fd: %d, err: %d", *id, errno);
        }
        
        return error;
    });

    return error;
}

int xpl_allocator_rw_filter(xpl_allocator_rw_t allocator, int offset, int count, uint32_t data1_size, uint32_t data2_size, xpl_allocator_rw_data_filter filter, void* filter_ctx, int* found_idx_out) {
    if ((offset + count) > allocator->write_index) {
        return ERANGE;
    }

    _Atomic int found_idx;
    atomic_init(&found_idx, -1);

    int error = xpl_util_dispatch_apply(&allocator->backends[offset], sizeof(smb_ssn_allocator), count, &found_idx, ^(void* ctx, void* data, size_t index) {
        smb_ssn_allocator* id = (smb_ssn_allocator*)data;

        char data1[data1_size];
        char data2[data2_size];
        int error = smb_ssn_allocator_read(*id, data1, data1_size, data2, data2_size);
        if (error) {
            return error;
        }

        _Bool result = filter(filter_ctx, data1, data2, index);
        if (result) {
            int expected = -1;
            atomic_compare_exchange_strong((_Atomic int*)ctx, &expected, (int)index + offset);
        }

        return 0;
    });

    if (error) {
        return error;
    }

    *found_idx_out = found_idx;
    return 0;
}

int xpl_allocator_rw_read(xpl_allocator_rw_t allocator, int index, char* data1_out, uint32_t data1_size, char* data2_out, uint32_t data2_size) {
    if (index >= allocator->write_index) {
        return ERANGE;
    }
    return smb_ssn_allocator_read(allocator->backends[index], data1_out, data1_size, data2_out, data2_size);
}

int xpl_allocator_rw_release_backends(xpl_allocator_rw_t allocator, int offset, int count) {
    if (offset < 0 || count < 0) {
        return EINVAL;
    }

    if ((offset + count) > atomic_fetch_sub(&allocator->write_index, count)) {
        atomic_fetch_add(&allocator->write_index, count);
        return EINVAL;
    }

    int error = xpl_util_dispatch_apply(&allocator->backends[offset], sizeof(allocator->backends[0]), count, NULL, ^(void* ctx, void* data, size_t index) {
        return smb_ssn_allocator_destroy((smb_ssn_allocator*)data);
    });

    if (error) {
        atomic_fetch_add(&allocator->write_index, count);
        return error;
    }

    memcpy(&allocator->backends[offset], &allocator->backends[offset + count], sizeof(allocator->backends[0]) * (allocator->backend_count - (count + offset)));
    allocator->backend_count -= count;
    
    return 0;
}

int xpl_allocator_rw_disown_backend(xpl_allocator_rw_t allocator, int index) {
    xpl_assert(index < allocator->backend_count);
    int fd = allocator->backends[index];
    memcpy(&allocator->backends[index], &allocator->backends[index + 1], sizeof(allocator->backends[0]) * (allocator->backend_count - index - 1));
    allocator->backend_count--;
    return fd;
}

int xpl_allocator_rw_grow_backend_count(xpl_allocator_rw_t allocator, int count) {
    smb_ssn_allocator* backends = (smb_ssn_allocator*)malloc(sizeof(smb_ssn_allocator) * (allocator->backend_count + count));
    if (!backends) {
        return ENOMEM;
    }

    int error = xpl_util_dispatch_apply(&backends[allocator->backend_count], sizeof(backends[0]), count, NULL, ^(void* ctx, void* data, size_t index) {
        smb_ssn_allocator* ssn_allocator = (smb_ssn_allocator*)data;
        *ssn_allocator =  smb_ssn_allocator_create(&allocator->addr, DEFAULT_SSN_ALLOCATOR_IOC_SADDR_LEN);
        return 0;
    });

    if (error) {
        free(backends);
        return error;
    }

    memcpy(backends, allocator->backends, sizeof(smb_ssn_allocator) * allocator->backend_count);
    free(allocator->backends);
    allocator->backends = backends;
    allocator->backend_count += count;
    return 0;
}

int xpl_allocator_rw_get_backend_count(xpl_allocator_rw_t allocator) {
    return allocator->backend_count;
}

int xpl_allocator_rw_destroy(xpl_allocator_rw_t* allocator) {
    xpl_allocator_rw_t arg = *allocator;
    int error = xpl_util_dispatch_apply(arg->backends, sizeof(smb_ssn_allocator), arg->backend_count, NULL, ^(void* ctx, void* data, size_t index) {
        return smb_ssn_allocator_destroy((smb_ssn_allocator*)data);
    });
    if (error) {
        return error;
    }
    free(arg->backends);
    free(arg);
    *allocator = NULL;
    return 0;
}
