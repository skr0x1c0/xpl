#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdatomic.h>

#include <sys/errno.h>

#include "smb_ssn_allocator.h"
#include "smb_client.h"


int smb_ssn_allocator_create(const struct sockaddr_in* addr, uint32_t ioc_saddr_len, smb_ssn_allocator* id_out) {
    int fd = smb_client_open_dev();
    if (fd < 0) {
        return errno;
    }
    struct smbioc_negotiate req;
    bzero(&req, sizeof(req));
    req.ioc_version = SMB_IOC_STRUCT_VERSION;
    req.ioc_saddr = (struct sockaddr*)addr;
    req.ioc_saddr_len = ioc_saddr_len;
    req.ioc_extra_flags |= SMB_FORCE_NEW_SESSION;
    req.ioc_extra_flags |= SMB_SMB1_ENABLED;

    int error = smb_client_ioc(fd, SMBIOC_NEGOTIATE, &req);
    if (error || req.ioc_errno) {
        close(fd);
        return error ? error : req.ioc_errno;
    }
    *id_out = fd;
    return 0;
}

int smb_ssn_allocator_allocate(smb_ssn_allocator id, const char* data1, uint32_t data1_size, const char* data2, uint32_t data2_size, uint32_t user_size, uint32_t password_size, uint32_t domain_size) {
    struct smbioc_setup req;
    bzero(&req, sizeof(req));
    req.ioc_version = SMB_IOC_STRUCT_VERSION;

    if (user_size > sizeof(req.ioc_user)) {
        return EINVAL;
    }

    if (password_size > sizeof(req.ioc_password)) {
        return EINVAL;
    }

    if (domain_size > sizeof(req.ioc_domain)) {
        return EINVAL;
    }

    memset(req.ioc_user, 0x1, user_size - 1);
    req.ioc_user[user_size - 1] = '\0';

    memset(req.ioc_password, 0x1, password_size - 1);
    req.ioc_password[password_size - 1] = '\0';

    memset(req.ioc_domain, 0x1, domain_size - 1);
    req.ioc_domain[domain_size - 1] = '\0';

    req.ioc_gss_client_size = data1_size;
    req.ioc_gss_client_name = (user_addr_t)data1;
    req.ioc_gss_target_size = data2_size;
    req.ioc_gss_target_name = (user_addr_t)data2;

    return smb_client_ioc(id, SMBIOC_SSNSETUP, &req);
}

int smb_ssn_allocator_read(smb_ssn_allocator id, char* data1_out, uint32_t data1_size, char* data2_out, uint32_t data2_size) {
    struct smbioc_auth_info req;
    bzero(&req, sizeof(req));
    req.ioc_version = SMB_IOC_STRUCT_VERSION;

    req.ioc_client_size = data1_size;
    req.ioc_client_name = (user_addr_t)data1_out;
    req.ioc_target_size = data2_size;
    req.ioc_target_name = (user_addr_t)data2_out;

    return smb_client_ioc(id, SMBIOC_AUTH_INFO, &req);
}

int smb_ssn_allocator_destroy(smb_ssn_allocator* id) {
    if (id == NULL || *id < 0) {
        return EINVAL;
    }

    if (close(*id)) {
        return errno;
    }
    *id = -1;
    return 0;
}
