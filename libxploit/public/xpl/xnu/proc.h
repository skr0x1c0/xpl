
#ifndef xpl_proc_h
#define xpl_proc_h

#include <stdio.h>

int xpl_proc_find(pid_t proc_id, uintptr_t* proc_out);
uintptr_t xpl_proc_current_proc(void);
uintptr_t xpl_proc_read_fdesc_ofiles(uintptr_t proc, uint32_t* nfiles_out);
uintptr_t xpl_proc_find_fd_data_from_ofiles(uintptr_t fdesc_ofiles, int fd);
int xpl_proc_find_fd_data(uintptr_t proc, int fd, uintptr_t* out);
void xpl_proc_iter_pids(_Bool(^callback)(pid_t pid));
void xpl_proc_iter_procs(_Bool(^callback)(uintptr_t proc));
void xpl_proc_iter_pids_with_binary(const char* binary_path, _Bool(^callback)(pid_t pid));

#endif /* xpl_proc_h */
