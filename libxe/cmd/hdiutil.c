//
//  cmd_hdiutil.c
//  xe
//
//  Created by admin on 11/26/21.
//

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <spawn.h>

#include <sys/errno.h>

#include "cmd/hdiutil.h"
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


int xe_cmd_hdiutil_attach(const char* image, const char* opts[], size_t num_opts, char* dev_out, size_t dev_size) {
    const char* args[num_opts + 4];
    args[0] = "/usr/bin/hdiutil";
    args[1] = "attach";
    for (int i=0; i<num_opts; i++) {
        args[i + 2] = opts[i];
    }
    args[num_opts + 2] = image;
    args[num_opts + 3] = NULL;
    
    pid_t pid;
    int fd = -1;
    int error = xe_cmd_exec("/usr/bin/hdiutil", args, &pid, &fd);
    if (error) {
        return error;
    }

    FILE* stream = fdopen(fd, "r");
    if (!stream) {
        return errno;
    }

    const char* expected_suffix = "Microsoft Basic Data";
    const int expected_suffix_len = (int)strlen(expected_suffix);
    
    while (1) {
        size_t char_count;
        char* line = fgetln(stream, &char_count);

        if (!char_count) {
            break;
        }

        while (line[char_count - 1] == '\n' || line[char_count - 1] == '\r' || line[char_count - 1] == ' ' || line[char_count - 1] == '\t') {
            char_count--;
        }

        if (char_count <= expected_suffix_len) {
            continue;
        }

        if (memcmp(&line[char_count - expected_suffix_len], expected_suffix, expected_suffix_len)) {
            continue;
        }

        for (int i=0; i<(char_count - sizeof(expected_suffix) + 1); i++) {
            if (i >= dev_size) {
                error = ENOBUFS;
                goto exit;
            }

            if (line[i] == ' ') {
                dev_out[i] = '\0';
                error = 0;
                goto exit;
            } else {
                dev_out[i] = line[i];
            }
        }
    }

    error = ENOTRECOVERABLE;
    
exit:
    if (stream) {
        fclose(stream);
    }
    
    if (fd >= 0) {
        close(fd);
    }
    
    // FIXME: check exit status
    if (pid) {
        waitpid(pid, NULL, 0);
    }
    
    return error;
}


int xe_cmd_hdiutil_detach(const char* dev_path) {
    const char* args[] = {
        "/usr/bin/hdiutil",
        "detach",
        dev_path,
        NULL
    };
    
    pid_t pid;
    int fd;
    int error = xe_cmd_exec("/usr/bin/hdiutil", args, &pid, &fd);
    if (error) {
        return error;
    }
    
    // FIXME: exit status
    waitpid(pid, NULL, 0);
    close(fd);
    return 0;
}



