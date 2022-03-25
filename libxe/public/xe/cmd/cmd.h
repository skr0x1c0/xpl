//
//  cmd.h
//  libxe
//
//  Created by sreejith on 3/24/22.
//

#ifndef cmd_h
#define cmd_h

#include <stdio.h>

int xe_cmd_exec(const char* exec_path, const char* argv[], pid_t* pid, int* std_out);
int xe_cmd_exec_sync(const char* exec_path, const char* argv[], char* buffer, size_t buffer_size);

#endif /* cmd_h */
