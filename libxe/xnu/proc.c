//
//  xnu_proc.c
//  xe
//
//  Created by admin on 11/20/21.
//

#include <unistd.h>
#include <sys/errno.h>

#include "xnu/proc.h"
#include "memory/kmem.h"
#include "slider/kernel.h"
#include "util/assert.h"
#include "util/ptrauth.h"

#include "macos_params.h"


int xe_xnu_proc_find(pid_t proc_id, uintptr_t* proc_out) {
    uint64_t pidhash = xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_PIDHASH));
    uintptr_t pidhashtbl = xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_PIDHASHTBL));
    int index = proc_id & pidhash;
    uintptr_t cursor = xe_kmem_read_uint64(KMEM_OFFSET(pidhashtbl, index * 8));
    while (cursor) {
        pid_t pid = xe_kmem_read_int32(KMEM_OFFSET(cursor, TYPE_PROC_MEM_P_PID_OFFSET));
        if (pid == proc_id) {
            *proc_out = cursor;
            return 0;
        }
        cursor = xe_kmem_read_uint64(KMEM_OFFSET(cursor, TYPE_PROC_MEM_P_HASH_OFFSET));
    }
    return ENOENT;
}

uintptr_t xe_xnu_proc_current_proc() {
    uintptr_t proc;
    int error = xe_xnu_proc_find(getpid(), &proc);
    xe_assert_err(error);
    return proc;
}

uintptr_t xe_xnu_proc_read_fdesc_ofiles(uintptr_t proc, uint32_t* nfiles_out) {
    uintptr_t fdesc = proc + TYPE_PROC_MEM_P_FD_OFFSET;
    if (nfiles_out) {
        *nfiles_out = xe_kmem_read_uint32(fdesc + TYPE_FILEDESC_MEM_FD_NFILES_OFFSET);
    }
    return XE_PTRAUTH_STRIP(xe_kmem_read_uint64(fdesc + TYPE_FILEDESC_MEM_FD_OFILES_OFFSET));
}

uintptr_t xe_xnu_proc_find_fd_data_from_ofiles(uintptr_t fdesc_ofiles, int fd) {
    uintptr_t fp_p = fdesc_ofiles + (sizeof(uint64_t) * fd);
    uintptr_t fp = xe_kmem_read_uint64(fp_p);
    uintptr_t fp_glob = XE_PTRAUTH_STRIP(xe_kmem_read_uint64(fp + TYPE_FILEPROC_MEM_FP_GLOB_OFFSET));
    uintptr_t fg_data = xe_kmem_read_uint64(fp_glob + TYPE_FILEGLOB_MEM_FG_DATA_OFFSET);
    return XE_PTRAUTH_STRIP(fg_data);
}

int xe_xnu_proc_find_fd_data(uintptr_t proc, int fd, uintptr_t* out) {
    uint32_t fdesc_nfiles;
    uintptr_t fdesc_ofiles = xe_xnu_proc_read_fdesc_ofiles(proc, &fdesc_nfiles);
    
    if (fd > fdesc_nfiles) {
        return ENOENT;
    }
    
    *out = xe_xnu_proc_find_fd_data_from_ofiles(fdesc_ofiles, fd);
    return 0;
}
