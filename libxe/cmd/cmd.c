//
//  cmd.c
//  libxe
//
//  Created by sreejith on 3/24/22.
//

#include <spawn.h>

#include <sys/fcntl.h>

#include "cmd/cmd.h"
#include "util/assert.h"


int xe_cmd_exec(const char* exec_path, const char* argv[], pid_t* pid, int* std_out) {
    posix_spawn_file_actions_t file_actions;
    int res = posix_spawn_file_actions_init(&file_actions);
    xe_assert_errno(res);
    
    int pfds[2];
    res = pipe(pfds);
    xe_assert_errno(res);
    
    res = posix_spawn_file_actions_addclose(&file_actions, pfds[0]);
    xe_assert_errno(res);
    
    res = posix_spawn_file_actions_adddup2(&file_actions, pfds[1], STDOUT_FILENO);
    xe_assert_errno(res);
    
    int error = posix_spawn(pid, exec_path, &file_actions, NULL, (char**)argv, NULL);
    if (error) {
        return error;
    }
    
    res = close(pfds[1]);
    xe_assert_errno(res);
    
    *std_out = pfds[0];
    return 0;
}


int xe_cmd_exec_sync(const char* exec_path, const char* argv[], char* buffer, size_t buffer_size) {
    pid_t pid;
    int fd;
    int error = xe_cmd_exec(exec_path, argv, &pid, &fd);
    if (error) {
        return error;
    }
    
    if (!buffer_size) {
        goto waitpid;
    }
    
    error = ENOBUFS;
    for (size_t cursor = 0; cursor < buffer_size; ) {
        ssize_t read_bytes = read(fd, &buffer[cursor], buffer_size - cursor);
        if (read_bytes == 0) {
            error = 0;
            break;
        }
        
        if (read_bytes < 0) {
            error = errno;
            break;
        }
        
        cursor += read_bytes;
    }
    
waitpid:
    while (0) {};
    
    int stat_loc = 0;
    int res;
    
    do {
        res = waitpid(pid, &stat_loc, 0);
    } while ((res < 0 && errno == EINTR) || (res >= 0 && !WIFEXITED(stat_loc)));
    
    if (res < 0) {
        return errno;
    }
    
    if (error) {
        return error;
    }
    
    return WEXITSTATUS(stat_loc);
}
