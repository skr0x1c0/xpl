#include <unistd.h>
#include <strings.h>
#include <limits.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/errno.h>

#include <IOKit/kext/KextManager.h>

#include "../external/smbfs/smb_dev.h"
#include "../external/smbfs/netbios.h"
#include "../external/smbfs/smb_dev_2.h"
#include "../external/smbfs/smb2_mc.h"

#include "smb_client.h"


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


int smb_client_ioc(int fd_dev, unsigned long cmd, void* data) {
    if (ioctl(fd_dev, cmd, data)) {
        return errno;
    }
    return 0;
}


int smb_client_ioc_negotiate(int fd_dev, struct sockaddr_in* addr, int32_t ioc_saddr_len, _Bool need_multichannel) {
    struct smbioc_negotiate req;
    bzero(&req, sizeof(req));
    req.ioc_version = SMB_IOC_STRUCT_VERSION;
    req.ioc_saddr = (struct sockaddr*)addr;
    req.ioc_saddr_len = ioc_saddr_len;
    req.ioc_extra_flags |= SMB_FORCE_NEW_SESSION;
    if (need_multichannel) {
        req.ioc_extra_flags |= SMB_SMB3_ENABLED;
        req.ioc_extra_flags |= SMB_MULTICHANNEL_ENABLE;
    } else {
        req.ioc_extra_flags |= SMB_SMB1_ENABLED;
    }

    if (ioctl(fd_dev, SMBIOC_NEGOTIATE, &req)) {
        return errno;
    }
    return req.ioc_errno;
}


int smb_client_ioc_ssn_setup(int fd_dev, char* gss_cpn, uint32_t gss_cpn_size, char* gss_spn, uint32_t gss_spn_size) {
    struct smbioc_setup req;
    bzero(&req, sizeof(req));
    req.ioc_version = SMB_IOC_STRUCT_VERSION;

    char user[] = "adminxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    static_assert(sizeof(user) <= sizeof(req.ioc_user), "");
    char password[] = "secretxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    static_assert(sizeof(password) <= sizeof(req.ioc_password), "");
    char domain[] = "nt";
    static_assert(sizeof(domain) <= sizeof(req.ioc_domain), "");

    memcpy(req.ioc_user, user, sizeof(user));
    memcpy(req.ioc_password, password, sizeof(password));
    memcpy(req.ioc_domain, domain, sizeof(domain));

    req.ioc_gss_client_size = gss_cpn_size;
    req.ioc_gss_client_name = (user_addr_t)gss_cpn;
    req.ioc_gss_target_size = gss_spn_size;
    req.ioc_gss_target_name = (user_addr_t)gss_spn;

    if (ioctl(fd_dev, SMBIOC_SSNSETUP, &req)) {
        return errno;
    }

    return 0;
}


int smb_client_ioc_update_client_interface(int fd_dev, struct network_nic_info* info, size_t info_count) {
    struct smbioc_client_interface req;
    req.ioc_info_array = info;
    req.total_buffer_size = (uint32_t)(sizeof(struct network_nic_info) * info_count);
    req.interface_instance_count = (uint32_t)info_count;

    if (ioctl(fd_dev, SMBIOC_UPDATE_CLIENT_INTERFACES, &req)) {
        return errno;
    }
    return 0;
}


int smb_client_ioc_multichannel_properties(int fd_dev, struct smbioc_multichannel_properties* out) {
    bzero(out, sizeof(*out));
    out->ioc_version = SMB_IOC_STRUCT_VERSION;
    if (ioctl(fd_dev, SMBIOC_MULTICHANNEL_PROPERTIES, out)) {
        return errno;
    }
    return 0;
}


int smb_client_ioc_nic_info(int fd_dev, struct smbioc_nic_info* out) {
    out->ioc_version = SMB_IOC_STRUCT_VERSION;
    if (ioctl(fd_dev, SMBIOC_NIC_INFO, out)) {
        return errno;
    }
    return 0;
}


int smb_client_ioc_auth_info(int fd_dev, char* gss_cpn, uint32_t gss_cpn_size, char* gss_spn, uint32_t gss_spn_size) {
    struct smbioc_auth_info req;
    bzero(&req, sizeof(req));
    req.ioc_version = SMB_IOC_STRUCT_VERSION;

    req.ioc_client_size = gss_cpn_size;
    req.ioc_client_name = (user_addr_t)gss_cpn;
    req.ioc_target_size = gss_spn_size;
    req.ioc_target_name = (user_addr_t)gss_spn;

    if (ioctl(fd_dev, SMBIOC_AUTH_INFO, &req)) {
        return errno;
    }

    return 0;
}