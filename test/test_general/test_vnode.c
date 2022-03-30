
#include <stdlib.h>
#include <limits.h>

#include <sys/fcntl.h>

#include <xpl/memory/kmem.h>
#include <xpl/util/vnode.h>
#include <xpl/util/log.h>
#include <xpl/util/assert.h>
#include <xpl/xnu/proc.h>
#include <xpl/util/msdosfs.h>

#include "test_vnode.h"


void test_vnode() {
    xpl_msdosfs_loadkext();
    
    char directory[] = "/tmp/test_vnode.XXXXXXX";
    xpl_assert(mkdtemp(directory) != NULL);
    
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/file", directory);
    int file = open(path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    xpl_assert(file >= 0);
    
    uintptr_t proc = xpl_proc_current_proc();
    uintptr_t vnode;
    int error = xpl_proc_find_fd_data(proc, file, &vnode);
    xpl_assert_err(error);
    
    char data[32];
    for (int i=0; i<sizeof(data); i++) {
        data[i] = random() % (UINT8_MAX + 1);
    }
    
    int res = ftruncate(file, sizeof(data));
    xpl_assert_errno(res);
    xpl_vnode_t util = xpl_vnode_create();
    xpl_vnode_write_user(util, vnode, data, sizeof(data));
    
    char buffer[sizeof(data)];
    bzero(buffer, sizeof(buffer));
    ssize_t read_bytes = read(file, buffer, sizeof(buffer));
    xpl_assert_cond(read_bytes, ==, sizeof(buffer));
    res = memcmp(data, buffer, sizeof(data));
    xpl_assert(res == 0);
    
    bzero(buffer, sizeof(buffer));
    xpl_vnode_read_user(util, vnode, buffer, sizeof(buffer));
    res = memcmp(data, buffer, sizeof(data));
    xpl_assert(res == 0);
    
    xpl_vnode_destroy(&util);
    close(file);
    unlink(path);
    rmdir(directory);
}
