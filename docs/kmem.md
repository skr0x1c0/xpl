# Arbitary kernel memory read write

## Introduction
[++Small intro about SMB]. SMB file sharing is used for sharing files over the network and is natively supported by most operating systems. The macOS provides inbuilt support for both creating (hosting) and mounting SMB shares. Users may use `Finder` or `mount_smbfs` from `Terminal` for mounting SMB shares. Mounting of SMB shares is facilitated by the `smbfs.kext` kernel extension. 



## Building blocks

### Double free in ``smb2_mc_parse_client_interface_array``

In SMB version 2 [verify] and above, clients can use multi channel feature to improve performance by establishing parallel connections to SMB server over different network interfaces. For this, clients needs to know certain informations about various network interfaces in client and server machine (like IPv4/IPv6 address, speed etc). macOS supports mounting SMB shares using the `smbfs.kext` kernel extension. The information about client network interfaces are provided to the kernel extension by the `mc_notifier` process. This process constantly monitors the network interfaces in the client machine and sends up to date information to `smbfs.kext` by executing `ioctl` syscall with command type `SMBIOC_UPDATE_CLIENT_INTERFACES` and following data.

``` c
struct smbioc_client_interface {
    SMB_IOC_POINTER(struct network_nic_info*, info_array);  // Array containing individual network interface information
    uint32_t interface_instance_count;                      // Total number of network interfaces in the array
    uint32_t total_buffer_size;                             // Total size of array in bytes
    uint32_t ioc_errno;                                     // Field used to report back error from kernel to userland
};
```
The `info_array` is the pointer to array of network interfaces storing individual network interface information in the following format.

``` c
struct network_nic_info {
    uint32_t next_offset;                           // Offset in bytes to next network_nic_info entry in array
    uint32_t nic_index;                             // Index of network interface
    uint32_t nic_caps;                              // Capabilities of network interface
    uint64_t nic_link_speed;                        // Link speed of network interface
    uint32_t nic_type;                              // Type of network interface
    in_port_t port;
    union { // alloc the largest sock addr possible
        struct sockaddr         addr;
        struct sockaddr_in      addr_4;
        struct sockaddr_in6     addr_16;
        struct sockaddr_storage addr_strg; // A place holder to make sure enough memory is allocated
                                           // for the largest possible sockaddr (ie NetBIOS's sockaddr_nb)
    };
};
```

The kernel extension handles `ioctl` using `nsmb_dev_ioctl` method, which passes the request down to `smb2_mc_parse_client_interface_array` to update the client network interfaces list `sessionp->session_interface_table`.

``` c
/*
 * Parse the client's network_interface_instance_info array and create a
 * complete_interface_info
 */
int
smb2_mc_parse_client_interface_array(
    struct session_network_interface_info* session_table,
    struct smbioc_client_interface* client_info)
{
    struct network_nic_info *client_info_array = NULL;
    int error = 0;
    bool in_blacklist = false;

    uint32_t array_size = client_info->total_buffer_size;
    
    // Allocates memory for client_info_array
    ...

    // Copy the interface list from user space
    error = copyin((user_addr_t) (void*) client_info->ioc_info_array, (caddr_t) (void*) client_info_array, array_size);
    if (error) {
        SMBERROR("Couldn't copyin the client interface table arguments!");
        goto exit;
    }

    struct network_nic_info * client_info_entry = (struct network_nic_info *) client_info_array;
    for (uint32_t counter = 0; counter < client_info->interface_instance_count; counter++) {
        in_blacklist = false;

        // Check for preventing OOB read
        ...

        // Check and skip adding network interfaces if its nic_index is in blacklist
        ...

        error = smb2_mc_add_new_interface_info_to_list(&session_table->client_nic_info_list,
                                                       &session_table->client_nic_count,
                                                       client_info_entry, 0, in_blacklist);
        if (error) {
            SMBERROR("Adding new interface info ended with error %d!", error);
            smb2_mc_release_interface_list(&session_table->client_nic_info_list);  // NOTE: All client network interfaces are released when an error occurs
            break;
        }

        client_info_entry = (struct network_nic_info *) ((uint8_t*) client_info_entry + client_info_entry->next_offset);
    }

exit:
    // Free resources
    ...
    
    return error;
}
```

The `smb2_mc_parse_client_interface_array` reads the `info_array` from user land and uses a for loop to insert / update each interface in `info_array` to `client_nic_info_list` by calling `smb2_mc_add_new_interface_info_to_list`. **As noted in the code snippet above, entire interfaces in the `client_nic_info_list` is released when the `smb2_mc_add_new_interface_info_to_list` returns an error.** 

The `smb2_mc_add_new_interface_info_to_list` method is responsible for checking if an nic with same index is already present in the `client_nic_info_list`. If present, it updates the existing entry, else a new entry in created and added to the list. 

``` c
static int
smb2_mc_add_new_interface_info_to_list(
    struct interface_info_list* list,
    uint32_t* list_counter,
    struct network_nic_info* new_info,
    uint32_t rss_val,
    bool in_black_list)
{
    int error = 0;
    uint64_t nic_index = ((uint64_t)rss_val << SMB2_IF_RSS_INDEX_SHIFT) | new_info->nic_index;

    // If there is already a nic with same nic_index, use it
    struct complete_nic_info_entry* nic_info = smb2_mc_get_nic_by_index(list, nic_index);
    bool new_nic = false;
    if (nic_info == NULL) { // Need to create a new nic element
        new_nic = true;
        // Allocates and intializes and new complete_nic_info_entry
        ...
    }

    /* Update with the most recent data */
    nic_info->nic_caps = new_info->nic_caps;
    nic_info->nic_index = nic_index;
    nic_info->nic_link_speed = new_info->nic_link_speed;
    nic_info->nic_type = new_info->nic_type;

    if (in_black_list) {
        nic_info->nic_flags |= SMB2_MC_NIC_IN_BLACKLIST;
    }

    error = smb2_mc_update_info_with_ip(nic_info, &new_info->addr, NULL);
    if (error) {
        SMBERROR("failed to smb2_mc_update_info_with_ip!");
        smb2_mc_release_interface(NULL, nic_info, NULL);        // NOTE: Individual network interface is release when an error occurs but it is not removed from the list
        nic_info = NULL;
    }
    
    if (!error && new_nic) {
        // If it is a new nic, add it to the list
        smb2_mc_insert_new_nic_by_speed(list, list_counter, nic_info, NULL);
    }

    return error;
}
```

**As noted in the code snippet, when `smb2_mc_update_info_with_ip` returns an error, the individual network interface info (`nic_info`) is released and the error is passed down to the caller.** When the `nic_info` is released, it is not removed from the `interface_info_list`. For new nic (`new_nic == TRUE`) this won't create any problem because the new `nic_info` is added to the `interface_info_list` only if `smb2_mc_update_info_with_ip` succeds. But for existing nic (`new_nic == FALSE`), the `nic_info` is already present in the tailq `interface_info_list` and when `nic_info` is released, it is not removed from the tailq. Since the `smb2_mc_add_new_interface_info_to_list` returns an error, the caller `smb2_mc_parse_client_interface_array` will try to release all the interfaces in the `client_interface_info_list` including the previously released `nic_info` resulting in the `nic_info` being freed twice. 

The method `smb2_mc_update_info_with_ip` returns error `ENOMEM` when `MALLOC` of size `addr->sa_len` returns `NULL`. Since `addr->sa_len` is not validated, it can be zero and `MALLOC` of size zero will return `NULL`. This will make the `smb2_mc_update_info_with_ip` return an error which would trigger the double free vulnerability.
 
``` c
static int
smb2_mc_update_info_with_ip(
    struct complete_nic_info_entry* nic_info,
    struct sockaddr *addr,
    bool *changed)
{
    ...
    SMB_MALLOC(new_addr->addr, struct sockaddr *, addr->sa_len, M_NSMBDEV, M_WAITOK | M_ZERO);
    if (new_addr->addr == NULL) {
        SMB_FREE(new_addr, M_NSMBDEV);
        SMBERROR("failed to allocate struct sockaddr!");
        return ENOMEM;
    }
    ...
}
```

This double free vulnerability may be exploited to free any memory allocated from `KHEAP_KEXT`. [Attach minimal POC that will trigger double free]

## Out of bound write in `smb_dup_sockaddr`

The method `smb_dup_sockaddr` is used in `smbfs.kext` to duplicate a socket address. This method allocates a memory of size `sizeof(struct sockaddr_nb)` and type `M_SONAME` and copies `sa_len` bytes from socket address to allocated memory. 

``` c
struct sockaddr *
smb_dup_sockaddr(struct sockaddr *sa, int canwait)
{
    struct sockaddr *sa2;

    SMB_MALLOC(sa2, struct sockaddr *, sizeof(struct sockaddr_nb), M_SONAME,
           canwait ? M_WAITOK : M_NOWAIT);
    if (sa2)
        bcopy(sa, sa2, sa->sa_len);
    return (sa2);
}
```

The size of `struct sockaddr_nb` is 56. `MALLOC` with type `M_SONAME` is allocated from `KHEAP_DEFAULT`. Thus the allocated memory will be of size 64 from `default.kalloc.64` zone. Since `sa->sa_len` is of type `uint8_t` and `sa->sa_len` bytes will be copied from the socket address to the allocated memory, an out of bound write of upto `255 - 64` bytes is possible on `default.kalloc.64` zone.

There are several code paths to this method which can be directly triggered from user land where `smb_dup_sockaddr` is called for duplicating socket addresses provided from user land without validating their `sa_len` field. See attached POC [Attach minimal POC that will panic]


## Out of bound read in `nb_put_name`

Netbios names are composed of different segments with with each segment prefixed by its segment length. A segment with zero length is used to represent the end of netbios name. 

An example encoded netbios name with two segments "first" and "second" is shown below:

![Netbios name encoding](/images/nb_put_name.drawio.png) 

In `smbfs.kext`, the `nb_pub_name` method is responsible for copying the netbios name in the field `snb_name` from socket address of type `struct sockaddr_nb` to a message buffer which would be sent over network to the SMB server. The method `nb_put_name` is defined as follows:

``` c
static int
nb_put_name(struct mbchain *mbp, struct sockaddr_nb *snb)
{
    int error;
    u_char seglen, *cp;

    cp = snb->snb_name;
    if (*cp == 0)
        return (EINVAL);
    NBDEBUG("[%s]\n", cp);
    
    // NOTE: this loop will keep on adding segments to message buffer until
    //       it encounters a segment with zero length
    for (;;) {
        seglen = (*cp) + 1;
        error = mb_put_mem(mbp, (const char *)cp, seglen, MB_MSYSTEM);
        if (error)
            return (error);
        if (seglen == 1)
            break;
        cp += seglen;
    }
    return (0);
}
```

The field `snb_name` is of size 34. But the method `nb_put_name` does not take this size into account and will keep on adding segments to the message buffer until it encounters a segment with zero length. 

The method `nb_put_name` is used in the method `nbssn_rq_request` to send netbios request containing local netbios name and SMB server netbios name over network as shown below:

``` c
static int
nbssn_rq_request(struct nbpcb *nbp)
{
    struct mbchain mb, *mbp = &mb;
    ...

    error = mb_init(mbp);
    if (error)
        return (error);
    
    // Add request parameters to mbp
    mb_put_uint32le(mbp, 0);
    nb_put_name(mbp, nbp->nbp_paddr);
    nb_put_name(mbp, nbp->nbp_laddr);
    
    // Add other parameters to mbp
    ...
    // Send request
    ...
    // Read and process response
    ...
    return (error);
}
```

The socket addresses `nbp->nbp_paddr` and `nbp->nbp_laddr` are socket address duplicated using `smb_dup_sockaddr` method. As discussed before, the `smb_dup_sockaddr` allocates 64 bytes of memory from `default.kalloc.64` zone for the duplicated addresses. The vulnerability in `nb_put_name` will allow us to read zone elements succeeding the `nbp->nbp_paddr` and `nbp->nbp_laddr` socket addresses.

[Attach minimal POC]


## Out of bound read in `smb_sm_negotiate`

Before mouting an SMB share using the `mount` syscall, the user process must first open a smb block device using `open('/dev/nsbm0xx')` syscall. Then it must execute an `ioctl` request on the opened smb block device with request of type `SMBIOC_NEGOTIATE` and data of type `struct smbioc_negotiate` as shown below: 

``` c
struct smbioc_negotiate {
    ...
    int32_t     ioc_saddr_len;                  // Size of sockaddr pointed by saddr user pointer
    int32_t     ioc_laddr_len;                  // Size of sockaddr pointer by laddr user pointer
    ...
    SMB_IOC_POINTER(struct sockaddr *, saddr);  // Socket address of SMB server
    SMB_IOC_POINTER(struct sockaddr *, laddr);  // Local netbios socket address (Only used when saddr is of family AF_NETBIOS)
    ...
};
```

The pointers `saddr` and `laddr` are pointers to memory in user land containing the socket address of SMB server and local netbios address respectively. The type `struct sockaddr` is of the following format:

``` c
/*
 * [XSI] Structure used by kernel to store most addresses.
 */
struct sockaddr {
    __uint8_t       sa_len;         /* total length */
    sa_family_t     sa_family;      /* [XSI] address family */
    char            sa_data[14];    /* [XSI] addr value (actually larger) */
};

```

Note: The field `sa_len` of type `uint8_t` is used for storing the length of the socket address. This allows representing socket addresses of different address families (like IPV4 or IPV6) and addresses with variable length (like unix domain socket)

This `ioctl` request is handled by `nsmb_dev_ioctl` which just performs a couple of validations and pass the request down to `smb_usr_negotiate` which will also pass the request down to `smb_sm_negotiate`. 

The `smb_sm_negotiate` method is responsible for copying in the `saddr` and `laddr` from user memory. This method calls `smb_memdupin` method which allocates a memory of provided length (`ioc_saddr_len` or `ioc_laddr_len`) in `KHEAP_KEXT` and calls `copyin` method to copy the socket address data from user memory to the allocated address.

``` c
int smb_sm_negotiate(struct smbioc_negotiate *session_spec,
                     vfs_context_t context, struct smb_session **sessionpp,
                     struct smb_dev *sdp, int searchOnly, uint32_t *matched_dns)
{
    struct sockaddr *saddr = NULL, *laddr = NULL;
    
    // Initial state validations
    ...
    
    // Copy in `saddr` from user memory
    saddr = smb_memdupin(session_spec->ioc_kern_saddr, session_spec->ioc_saddr_len); // NOTE1: copied in saddr may have sa_len greater than ioc_saddr_len
    if (saddr == NULL) {
        return ENOMEM;
    }

    if (session_spec->ioc_saddr_len < 3) {
        /* Paranoid check - Have to have at least sa_len and sa_family */
        SMBERROR("invalid ioc_saddr_len (%d) \n", session_spec->ioc_saddr_len);
        SMB_FREE(saddr, M_SMBDATA);
        return EINVAL;
    }

    // NOTE: No validation done to verify saddr->sa_len <= ioc_saddr_len
    
    
    // Search for session
    ...
        
    if ((error == 0) || (searchOnly)) {
        SMB_FREE(saddr, M_SMBDATA);
        saddr = NULL;
        session_spec->ioc_extra_flags |= SMB_SHARING_SESSION;
    } else {
        /* NetBIOS connections require a local address */
        if (saddr->sa_family == AF_NETBIOS) {
            // Copy in `laddr` from user memory
            laddr = smb_memdupin(session_spec->ioc_kern_laddr, session_spec->ioc_laddr_len); // NOTE1: copied in laddr may have sa_len greater than ioc_laddr_len
            if (laddr == NULL) {
                SMB_FREE(saddr, M_SMBDATA);
                saddr = NULL;
                return ENOMEM;
            }
        }
        //NOTE: No validation done to verify laddr->sa_len <= ioc_laddr_len
        ...
        error = smb_session_create(session_spec, saddr, laddr, context, &sessionp);
        if (error == 0) {
            /* Flags used to cancel the connection */
            sessionp->connect_flag = &sdp->sd_flags;
            error = smb_session_negotiate(sessionp, context);
            sessionp->connect_flag = NULL;
            if (error) {
                SMBDEBUG("saddr is %s.\n", str);
                /* Remove the lock and reference */
                smb_session_put(sessionp, context);
            }
        }       
    }
    ...
    return error;
}
```

As noted in the code snippet, the copied in `saddr` and `laddr` values may have `sa_len` field value greater than the copied in memory size `ioc_saddr_len` and `ioc_laddr_len` respectively. Subsequent use of these copied in socket address values may lead to out of bound read. 

The `smb_sm_negotiate` method calls `smb_session_create` to create a new smb session (of type `struct smb_session`), which assigns the copied in `saddr` and `laddr` to `session->session_iod->iod_saddr` and `session->session_iod->iod_laddr` respectively. The server socket address `saddr` is also duplicated by calling `smb_dup_sockaddr` and the result is assigned to `sessionp->session_saddr`

``` c
static int smb_session_create(struct smbioc_negotiate *session_spec, 
                         struct sockaddr *saddr, struct sockaddr *laddr,
                         vfs_context_t context, struct smb_session **sessionpp)
{
    struct smb_session *sessionp;
    ...
    // create session
    
    SMB_MALLOC(sessionp, struct smb_session *, sizeof(*sessionp), M_SMBCONN, M_WAITOK | M_ZERO);

    // setup other fields of sessionp
    ...
    
    /*
     * <72239144> Save the original IP address that we connected to so we can
     * return it later. Used when checking for IP address matching for sharing
     * a session.
     */
    sessionp->session_saddr = smb_dup_sockaddr(saddr, 1);

    // continue setup other fields
    ...

    struct smbiod *iod = NULL;
    if (!error)
        error = smb_iod_create(sessionp, &iod);
    if (error) {
        smb_session_put(sessionp, context);
        return error;
    }
    iod->iod_message_id = 1;
    iod->iod_saddr  = saddr;
    iod->iod_laddr  = laddr;

    // setup other fields of iod
    ...
    // continue create session
    ...
    return 0;
}
```

Now that we know socket address in `session->session_iod->iod_saddr`, `session->session_iod->iod_laddr` and `sessionp->session_saddr` may have `sa_len` value greater than their allocated memory size, all we need is a way to read any of these socket addresses back to user land. 

One way to achieve this is by executing `fcntl` syscall of type `smbfsGetSessionSockaddrFSCTL` or `smbfsGetSessionSockaddrFSCTL2` on any file inside the SMB mount point. This sysctl is handled by `smbfs_vnop_ioctl` method and it directly copies out `sessionp->session_saddr` memory of size `sa_len` back to user land. But this method has a disadvantage, we need user approval to open a file inside SMB share because files in network shares are protected by TCC. 

Another way to achive this is by reading the `session->session_iod->iod_laddr.snb_name` value sent to SMB server during netbios session setup. While handling the `SMBIOC_NEGOTIATE` ioctl, the method `smb_session_negotiate` is called which would create and submit an iod request of type `SMBIOD_EV_NEGOTIATE`. The iod requests are handled in a separate thread by `smb_iod_main` method which will call `smb_iod_negotiate` to handle this request. For server socket addresses with address family `AF_NETBIOS`, this method indirectly calls `nb_ssn_request` to setup netbios session. As discussed previously, this would send the `snb_name` field of socket address in `session->session_iod->iod_tdata->nbp_paddr` duplicated from `session->session_iod->iod_laddr` to the server. So for example, by executing `ioctl` syscall of type `SMBIOC_NEGOTIATE` using `ioc_laddr_len = 32`, `laddr->sa_len = 64` and `saddr->sa_family = AF_NETBIOS`, the successor of `session->session_iod->iod_laddr` from `kext.kalloc.32` zone will be sent along with the local netbios name sent to the SMB server. By running a fake local SMB server, this value can be read by a user land program.

[Attach minimal POC]

## Reading neighbor from `KHEAP_KEXT` zones

### Zone element size upto 32 bytes
![KHEAP_KEXT neighbor reader (z_elem_size <= 32 byte)](/images/kext_neighbor_read_upto_32.drawio.png)

### Zone element size upto 128 bytes
![KHEAP_KEXT neighbor reader (z_elem_size <= 128 byte)](/images/kext_neighbor_read_upto_128.drawio.png)

### Zone element greater than 128 bytes
![KHEAP_KEXT neighbor reader (z_elem_size > 128 byte)](/images/todo.png)


## Kernel memory fast reader / writer

### Reading memory
![Kernel memory fast reader](/images/kmem_fast_read.drawio.png)

### Writing memory
![Kernel memory fast write](/images/kmem_fast_write.drawio.png)
