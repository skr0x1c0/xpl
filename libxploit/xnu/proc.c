//
//  xnu_proc.c
//  xe
//
//  Created by admin on 11/20/21.
//

#include <unistd.h>
#include <libproc.h>

#include <sys/errno.h>

#include "xnu/proc.h"
#include "memory/kmem.h"
#include "slider/kernel.h"
#include "util/assert.h"
#include "util/ptrauth.h"

#include <macos/kernel.h>


int xpl_proc_find(pid_t proc_id, uintptr_t* proc_out) {
    uint64_t pidhash = xpl_kmem_read_uint64(xpl_slider_kernel_slide(VAR_PIDHASH), 0);
    uintptr_t pidhashtbl = xpl_kmem_read_uint64(xpl_slider_kernel_slide(VAR_PIDHASHTBL), 0);
    int index = proc_id & pidhash;
    uintptr_t cursor = xpl_kmem_read_uint64(pidhashtbl, index * 8);
    while (cursor) {
        pid_t pid = xpl_kmem_read_int32(cursor, TYPE_PROC_MEM_P_PID_OFFSET);
        if (pid == proc_id) {
            *proc_out = cursor;
            return 0;
        }
        cursor = xpl_kmem_read_uint64(cursor, TYPE_PROC_MEM_P_HASH_OFFSET);
    }
    return ENOENT;
}

uintptr_t xpl_proc_current_proc() {
    uintptr_t proc;
    int error = xpl_proc_find(getpid(), &proc);
    xpl_assert_err(error);
    return proc;
}

uintptr_t xpl_proc_read_fdesc_ofiles(uintptr_t proc, uint32_t* nfiles_out) {
    uintptr_t fdesc = proc + TYPE_PROC_MEM_P_FD_OFFSET;
    if (nfiles_out) {
        *nfiles_out = xpl_kmem_read_uint32(fdesc, TYPE_FILEDESC_MEM_FD_NFILES_OFFSET);
    }
    return xpl_ptrauth_strip(xpl_kmem_read_uint64(fdesc, TYPE_FILEDESC_MEM_FD_OFILES_OFFSET));
}

uintptr_t xpl_proc_find_fd_data_from_ofiles(uintptr_t fdesc_ofiles, int fd) {
    uintptr_t fp_p = fdesc_ofiles + (sizeof(uint64_t) * fd);
    uintptr_t fp = xpl_kmem_read_uint64(fp_p, 0);
    uintptr_t fp_glob = xpl_ptrauth_strip(xpl_kmem_read_uint64(fp, TYPE_FILEPROC_MEM_FP_GLOB_OFFSET));
    uintptr_t fg_data = xpl_kmem_read_uint64(fp_glob, TYPE_FILEGLOB_MEM_FG_DATA_OFFSET);
    return xpl_ptrauth_strip(fg_data);
}

int xpl_proc_find_fd_data(uintptr_t proc, int fd, uintptr_t* out) {
    uint32_t fdesc_nfiles;
    uintptr_t fdesc_ofiles = xpl_proc_read_fdesc_ofiles(proc, &fdesc_nfiles);
    
    if (fd > fdesc_nfiles) {
        return ENOENT;
    }
    
    *out = xpl_proc_find_fd_data_from_ofiles(fdesc_ofiles, fd);
    return 0;
}

void xpl_proc_iter_pids(_Bool(^callback)(pid_t pid)) {
    int num_pids = proc_listallpids(NULL, 0);
    pid_t* pids = malloc(sizeof(pid_t) * num_pids);
    int res = proc_listallpids(pids, num_pids * sizeof(pid_t));
    xpl_assert(res >= 0);
    
    for (int i=0; i<num_pids; i++) {
        if (callback(pids[i])) {
            break;
        }
        if (pids[i] == 0) {
            break;
        }
    }
    
    free(pids);
}

void xpl_proc_iter_procs(_Bool(^callback)(uintptr_t proc)) {
    xpl_proc_iter_pids(^_Bool(pid_t pid) {
        uintptr_t proc;
        int error = xpl_proc_find(pid, &proc);
        xpl_assert_err(error);
        return callback(proc);
    });
}

void xpl_proc_iter_pids_with_binary(const char* binary_path, _Bool(^callback)(pid_t pid)) {
    xpl_proc_iter_pids(^_Bool(pid_t pid) {
        char path[PATH_MAX];
        int res = proc_pidpath(pid, &path, sizeof(path));
        xpl_assert(res >= 0);
        if (strncmp(path, binary_path, sizeof(path)) == 0) {
            return callback(pid);
        } else {
            return 0;
        }
    });
}
