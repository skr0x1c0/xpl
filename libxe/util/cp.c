//
//  cp.c
//  libxe
//
//  Created by sreejith on 2/24/22.
//

#include <unistd.h>

#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/types.h>

#include "cp.h"


int xe_util_cp(const char* src, const char* dst) {
    int fd_src = open(src, O_RDONLY);
    if (fd_src < 0) {
        return errno;
    }
    
    int fd_dst = open(dst, O_CREAT | O_EXCL | O_RDWR);
    if (fd_dst < 0) {
        close(fd_src);
        return errno;
    }
    
    char buffer[4096];
    int error = 0;
    while (1) {
        ssize_t read_bytes = read(fd_src, buffer, sizeof(buffer));
        if (read_bytes == 0) {
            goto exit;
        } else if (read_bytes < 0) {
            error = errno;
            goto exit;
        }
        ssize_t wrote_bytes = write(fd_dst, buffer, read_bytes);
        if (wrote_bytes != read_bytes) {
            error = wrote_bytes < 0 ? errno : EIO;
            goto exit;
        }
    }
    
exit:
    close(fd_src);
    close(fd_dst);
    return error;
}
