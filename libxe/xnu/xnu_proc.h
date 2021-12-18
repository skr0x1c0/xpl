//
//  xnu_proc.h
//  xe
//
//  Created by admin on 11/20/21.
//

#ifndef xnu_proc_h
#define xnu_proc_h

#include <stdio.h>

int xe_xnu_proc_find(uintptr_t kernproc, pid_t proc_id, uintptr_t* proc_out);
uintptr_t xe_xnu_proc_current_proc(uintptr_t kernproc);
int xe_xnu_proc_find_fd_data(uintptr_t proc, int fd, uintptr_t* out);

#endif /* xnu_proc_h */
