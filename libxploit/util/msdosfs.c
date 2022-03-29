//
//  msdosfs_ramfs.c
//  libxe
//
//  Created by sreejith on 3/24/22.
//

#include <assert.h>
#include <string.h>

#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <IOKit/kext/KextManager.h>

#include "util/msdosfs.h"
#include "cmd/cmd.h"
#include "util/assert.h"

#include "../external/msdosfs/msdosfs.h"


struct xpl_util_msdosfs {
    char mount_point[sizeof(xpl_MOUNT_TEMP_DIR)];
    int fd;
    int disk;
};


typedef struct xpl_util_msdosfs* xpl_util_msdosfs_t;


int xpl_util_msdosfs_loadkext(void) {
    struct vfsconf conf;
    int res = getvfsbyname("msdos", &conf);
    if (res) {
        return KextManagerLoadKextWithIdentifier(CFSTR("com.apple.filesystems.msdosfs"), NULL);
    }
    return 0;
}


int xpl_util_msdosfs_ramfs(size_t block_size, int* dev) {
    char name[64];
    bzero(name, sizeof(name));
    
    char url[27];
    snprintf(url, sizeof(url), "ram://%lu", block_size);
    
    const char* argv[] = {
        "/usr/bin/hdiutil",
        "attach",
        "-nomount",
        url,
        NULL
    };
    
    int error = xpl_cmd_exec_sync("/usr/bin/hdiutil", argv, name, sizeof(name));
    if (error) {
        return error;
    }
    xpl_assert_cond(memcmp(name, "/dev/disk", 9), ==, 0);
    
    int disk = 0;
    char* endp;
    for (endp = &name[9]; endp < &name[sizeof(name) - 1] && *endp >= '0' && *endp <= '9'; endp++) {
        disk = disk * 10 + (*endp - '0');
    }
    xpl_assert(*endp == ' ' || *endp == '\0' || *endp == '\n');
    
    *dev = disk;
    return 0;
}


int xpl_util_msdosfs_newfs(int disk) {
    xpl_assert_cond(disk, <=, UINT16_MAX);
    char dev[16];
    snprintf(dev, sizeof(dev), "/dev/rdisk%d", disk);
    
    const char* argv[] = {
        "/sbin/newfs_msdos",
        dev,
        NULL
    };
    
    return xpl_cmd_exec_sync("/sbin/newfs_msdos", argv, NULL, 0);
}


xpl_util_msdosfs_t xpl_util_msdosfs_mount(size_t blocks, int mount_options) {
    xpl_util_msdosfs_loadkext();
    
    int disk;
    int error = xpl_util_msdosfs_ramfs(blocks, &disk);
    xpl_assert_err(error);
    
    error = xpl_util_msdosfs_newfs(disk);
    xpl_assert_err(error);
    
    xpl_util_msdosfs_t util = malloc(sizeof(struct xpl_util_msdosfs));
    static_assert(sizeof(util->mount_point) == sizeof(xpl_MOUNT_TEMP_DIR), "");
    memcpy(util->mount_point, xpl_MOUNT_TEMP_DIR, sizeof(util->mount_point));
    mkdtemp(util->mount_point);
    util->disk = disk;
    
    int fd = open(util->mount_point, O_RDONLY);
    xpl_assert_cond(fd, >=, 0);
    util->fd = fd;
    
    struct msdosfs_args args;
    bzero(&args, sizeof(args));
    
    char dev[16];
    snprintf(dev, sizeof(dev), "/dev/disk%d", disk);
    
    args.mask = ACCESSPERMS;
    args.uid = getuid();
    args.gid = getgid();
    args.magic = MSDOSFS_ARGSMAGIC;
    args.fspec = dev;

    int res = fmount("msdos", fd, mount_options, &args);
    xpl_assert_errno(res);
    
    return util;
}


void xpl_util_msdosfs_update(xpl_util_msdosfs_t util, int flags) {
    struct msdosfs_args args;
    bzero(&args, sizeof(args));
    
    char dev[16];
    snprintf(dev, sizeof(dev), "/dev/disk%d", util->disk);
    
    args.mask = ACCESSPERMS;
    args.uid = getuid();
    args.gid = getgid();
    args.magic = MSDOSFS_ARGSMAGIC;
    args.fspec = dev;

    int res = mount("msdos", util->mount_point, MNT_UPDATE | flags, &args);
    xpl_assert_errno(res);
}


int xpl_util_msdosfs_mount_fd(xpl_util_msdosfs_t util) {
    return util->fd;
}


void xpl_util_msdosfs_mount_point(xpl_util_msdosfs_t util, char buffer[sizeof(xpl_MOUNT_TEMP_DIR)]) {
    static_assert(sizeof(util->mount_point) == sizeof(xpl_MOUNT_TEMP_DIR), "");
    memcpy(buffer, util->mount_point, sizeof(xpl_MOUNT_TEMP_DIR));
}


void xpl_util_msdosfs_unmount(xpl_util_msdosfs_t* util_p) {
    xpl_util_msdosfs_t util = *util_p;
    char dev[16];
    snprintf(dev, sizeof(dev), "/dev/disk%d", util->disk);
    
    const char* argv[] = {
        "/usr/bin/hdiutil",
        "detach",
        "-force",
        dev,
        NULL
    };
    
    int error = xpl_cmd_exec_sync("/usr/bin/hdiutil", argv, NULL, 0);
    xpl_assert_err(error);
    
    rmdir(util->mount_point);
    free(util);
    *util_p = NULL;
}
