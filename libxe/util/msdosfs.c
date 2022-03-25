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


struct xe_util_msdosfs {
    char mount_point[sizeof(XE_MOUNT_TEMP_DIR)];
    int fd;
    int disk;
};


typedef struct xe_util_msdosfs* xe_util_msdosfs_t;


int xe_util_msdosfs_loadkext(void) {
    struct vfsconf conf;
    int res = getvfsbyname("msdos", &conf);
    if (res) {
        return KextManagerLoadKextWithIdentifier(CFSTR("com.apple.filesystems.msdosfs"), NULL);
    }
    return 0;
}


int xe_util_msdosfs_ramfs(size_t block_size, int* dev) {
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
    
    int error = xe_cmd_exec_sync("/usr/bin/hdiutil", argv, name, sizeof(name));
    if (error) {
        return error;
    }
    xe_assert_cond(memcmp(name, "/dev/disk", 9), ==, 0);
    
    int disk = 0;
    char* endp;
    for (endp = &name[9]; endp < &name[sizeof(name) - 1] && *endp >= '0' && *endp <= '9'; endp++) {
        disk = disk * 10 + (*endp - '0');
    }
    xe_assert(*endp == ' ' || *endp == '\0' || *endp == '\n');
    
    *dev = disk;
    return 0;
}


int xe_util_msdosfs_newfs(int disk) {
    xe_assert_cond(disk, <=, UINT16_MAX);
    char dev[16];
    snprintf(dev, sizeof(dev), "/dev/rdisk%d", disk);
    
    const char* argv[] = {
        "/sbin/newfs_msdos",
        dev,
        NULL
    };
    
    return xe_cmd_exec_sync("/sbin/newfs_msdos", argv, NULL, 0);
}


xe_util_msdosfs_t xe_util_msdosfs_mount(size_t blocks, int mount_options) {
    xe_util_msdosfs_loadkext();
    
    int disk;
    int error = xe_util_msdosfs_ramfs(blocks, &disk);
    xe_assert_err(error);
    
    error = xe_util_msdosfs_newfs(disk);
    xe_assert_err(error);
    
    xe_util_msdosfs_t util = malloc(sizeof(struct xe_util_msdosfs));
    static_assert(sizeof(util->mount_point) == sizeof(XE_MOUNT_TEMP_DIR), "");
    memcpy(util->mount_point, XE_MOUNT_TEMP_DIR, sizeof(util->mount_point));
    mkdtemp(util->mount_point);
    util->disk = disk;
    
    int fd = open(util->mount_point, O_RDONLY);
    xe_assert_cond(fd, >=, 0);
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
    xe_assert_errno(res);
    
    return util;
}


void xe_util_msdosfs_update(xe_util_msdosfs_t util, int flags) {
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
    xe_assert_errno(res);
}


int xe_util_msdosfs_mount_fd(xe_util_msdosfs_t util) {
    return util->fd;
}


void xe_util_msdosfs_mount_point(xe_util_msdosfs_t util, char buffer[sizeof(XE_MOUNT_TEMP_DIR)]) {
    static_assert(sizeof(util->mount_point) == sizeof(XE_MOUNT_TEMP_DIR), "");
    memcpy(buffer, util->mount_point, sizeof(XE_MOUNT_TEMP_DIR));
}


void xe_util_msdosfs_unmount(xe_util_msdosfs_t* util_p) {
    xe_util_msdosfs_t util = *util_p;
    char dev[16];
    snprintf(dev, sizeof(dev), "/dev/disk%d", util->disk);
    
    const char* argv[] = {
        "/usr/bin/hdiutil",
        "detach",
        "-force",
        dev,
        NULL
    };
    
    int error = xe_cmd_exec_sync("/usr/bin/hdiutil", argv, NULL, 0);
    xe_assert_err(error);
    
    rmdir(util->mount_point);
    free(util);
    *util_p = NULL;
}
