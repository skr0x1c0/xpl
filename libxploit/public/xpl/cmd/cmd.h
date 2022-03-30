
#ifndef xpl_cmd_h
#define xpl_cmd_h

#include <stdio.h>

int xpl_cmd_exec(const char* exec_path, const char* argv[], pid_t* pid, int* std_out);
int xpl_cmd_exec_sync(const char* exec_path, const char* argv[], char* buffer, size_t buffer_size);

#endif /* xpl_cmd_h */
