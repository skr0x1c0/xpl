//
//  xnu_proc.c
//  xe
//
//  Created by admin on 11/20/21.
//

#include <unistd.h>
#include <sys/errno.h>

#include "xnu_proc.h"
#include "platform_types.h"
#include "platform_variables.h"
#include "kmem.h"
#include "slider.h"


int xe_xnu_proc_find(pid_t proc_id, uintptr_t* proc_out) {
    uint64_t pidhash = xe_kmem_read_uint64(xe_slider_slide(VAR_PIDHASH));
    uintptr_t pidhashtbl = xe_kmem_read_uint64(xe_slider_slide(VAR_PIDHASHTBL));
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
    assert(error == 0);
    return proc;
}

int xe_xnu_proc_find_fd_data(uintptr_t proc, int fd, uintptr_t* out) {
    uintptr_t fdesc = proc + TYPE_PROC_MEM_P_FD_OFFSET;
    int fdesc_nfiles = xe_kmem_read_uint32(fdesc + TYPE_FILEDESC_MEM_FD_NFILES_OFFSET);
    if (fd > fdesc_nfiles) {
        return ENOENT;
    }
    
    uintptr_t fdesc_ofiles = xe_kmem_read_uint64(fdesc + TYPE_FILEDESC_MEM_FD_OFILES_OFFSET);
    uintptr_t fp_p = fdesc_ofiles + (sizeof(uint64_t) * fd);
    uintptr_t fp = xe_kmem_read_uint64(fp_p);
    uintptr_t fp_glob = xe_kmem_read_uint64(fp + TYPE_FILEPROC_MEM_FP_GLOB_OFFSET);
    uintptr_t fg_data = xe_kmem_read_uint64(fp_glob + TYPE_FILEGLOB_MEM_FG_DATA_OFFSET);
    *out = fg_data;
    return 0;
}
