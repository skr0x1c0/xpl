//
//  xnu_proc.h
//  xe
//
//  Created by admin on 11/20/21.
//

#ifndef xnu_proc_h
#define xnu_proc_h

#include <stdio.h>

int xe_xnu_proc_find(pid_t proc_id, uintptr_t* proc_out);
uintptr_t xe_xnu_proc_current_proc(void);
uintptr_t xe_xnu_proc_read_fdesc_ofiles(uintptr_t proc, uint32_t* nfiles_out);
uintptr_t xe_xnu_proc_find_fd_data_from_ofiles(uintptr_t fdesc_ofiles, int fd);
int xe_xnu_proc_find_fd_data(uintptr_t proc, int fd, uintptr_t* out);

#endif /* xnu_proc_h */
