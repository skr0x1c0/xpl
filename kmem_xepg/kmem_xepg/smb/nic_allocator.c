#include <string.h>
#include <unistd.h>
#include <sys/errno.h>

#include "nic_allocator.h"
#include "client.h"
#include "assert.h"

int smb_nic_allocator_create(const struct sockaddr_in* addr, uint32_t ioc_saddr_len, smb_nic_allocator* id_out) {
    int error = 0;
    int fd = smb_client_open_dev();

    if (fd < 0) {
        error = errno;
        goto exit;
    }

    struct smbioc_negotiate req;
    bzero(&req, sizeof(req));
    req.ioc_version = SMB_IOC_STRUCT_VERSION;
    req.ioc_saddr = (struct sockaddr*)addr;
    req.ioc_saddr_len = ioc_saddr_len;
    req.ioc_extra_flags |= SMB_FORCE_NEW_SESSION;
    req.ioc_extra_flags |= SMB_SMB3_ENABLED;
    req.ioc_extra_flags |= SMB_MULTICHANNEL_ENABLE;
    if ((error = smb_client_ioc(fd, SMBIOC_NEGOTIATE, &req))) {
        goto exit;
    }

    *id_out = fd;
exit:
    if (error && fd >= 0) {
        close(fd);
    }
    return error;
}


int smb_nic_allocator_allocate(smb_nic_allocator id, const struct network_nic_info* infos, uint32_t info_count, uint32_t buffer_size) {
    struct smbioc_client_interface req;
    bzero(&req, sizeof(req));
    req.interface_instance_count = info_count;
    req.ioc_info_array = (struct network_nic_info*)infos;
    req.total_buffer_size = buffer_size;

    int error = smb_client_ioc(id, SMBIOC_UPDATE_CLIENT_INTERFACES, &req);
    if (error) {
        return error;
    }

    return req.ioc_errno;
}


int smb_nic_allocator_read(smb_nic_allocator id, struct smbioc_nic_info* info_out) {
    bzero(info_out, sizeof(*info_out));
    info_out->ioc_version = SMB_IOC_STRUCT_VERSION;
    info_out->flags = 1;
    return smb_client_ioc(id, SMBIOC_NIC_INFO, info_out);
}


int smb_nic_allocator_destroy(smb_nic_allocator* id) {
    if (id == NULL || *id < 0) {
        return EINVAL;
    }

    if (close(*id)) {
        return errno;
    }
    
    *id = -1;
    return 0;
}
