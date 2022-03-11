//
//  test_vnode.c
//  test_general_gym
//
//  Created by sreejith on 3/10/22.
//

#include <stdlib.h>
#include <limits.h>

#include <sys/fcntl.h>

#include <xe/memory/kmem.h>
#include <xe/util/vnode.h>
#include <xe/util/log.h>
#include <xe/util/assert.h>
#include <xe/xnu/proc.h>
#include <xe/util/msdosfs.h>

#include "test_vnode.h"


void test_vnode() {
    xe_util_msdosfs_loadkext();
    
    char directory[] = "/tmp/test_vnode.XXXXXXX";
    xe_assert(mkdtemp(directory) != NULL);
    
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/file", directory);
    int file = open(path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    xe_assert(file >= 0);
    
    uintptr_t proc = xe_xnu_proc_current_proc();
    uintptr_t vnode;
    int error = xe_xnu_proc_find_fd_data(proc, file, &vnode);
    xe_assert_err(error);
    
    char data[32];
    for (int i=0; i<sizeof(data); i++) {
        data[i] = random() % (UINT8_MAX + 1);
    }
    
    int res = ftruncate(file, sizeof(data));
    xe_assert_errno(res);
    xe_util_vnode_t util = xe_util_vnode_create();
    xe_util_vnode_write(util, vnode, data, sizeof(data));
    
    char buffer[sizeof(data)];
    bzero(buffer, sizeof(buffer));
    xe_util_vnode_read(util, vnode, buffer, sizeof(buffer));
    
    res = memcmp(data, buffer, sizeof(data));
    xe_assert(res == 0);
    
    xe_util_vnode_destroy(&util);
    close(file);
    unlink(path);
    rmdir(directory);
}
