#include <string.h>
#include <unistd.h>

#include <sys/errno.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <IOKit/kext/KextManager.h>

#include "../external/msdosfs/msdosfs.h"

#include "util/msdosfs.h"
#include "util/assert.h"
#include "util/cp.h"
#include "cmd/hdiutil.h"

#define BASE_IMAGE_NAME "exp_msdosfs_base.dmg"


struct xe_util_msdosfs {
    char directory[PATH_MAX];
    char dev[PATH_MAX];
    int fd_mount;
};


int xe_util_msdosfs_loadkext(void) {
    return KextManagerLoadKextWithIdentifier(CFSTR("com.apple.filesystems.msdosfs"), NULL);
}


int xe_util_msdosfs_mount(const char* base_img_path, const char* label, xe_util_msdosfs_t* mount_out) {
    char temp_dir[] = "/tmp/exp_msdos.XXXXXXXX";
    if (!mkdtemp(temp_dir)) {
        return errno;
    }
    
    struct stat st;
    if (stat(temp_dir, &st)) {
        printf("[ERROR] temp directory problem, err: %d\n", errno);
        abort();
    }

    char mount_point[PATH_MAX];
    snprintf(mount_point, sizeof(mount_point), "%s/mount", temp_dir);

    if (mkdir(mount_point, 0700)) {
        return errno;
    }

    int error = 0;
    char image_path[PATH_MAX];
    if (snprintf(image_path, sizeof(image_path), "%s/%s", temp_dir, BASE_IMAGE_NAME) >= sizeof(image_path)) {
        error = E2BIG;
        goto exit_error;
    }
    
    error = xe_util_cp(base_img_path, image_path);
    if (error) {
        goto exit_error;
    }
    
    const char* opts[] = {
        "-nomount",
    };
    char dev_path[PATH_MAX];
    error = xe_cmd_hdiutil_attach(image_path, opts, 1, dev_path, sizeof(dev_path));
    if (error) {
        goto exit_error;
    }

    struct msdosfs_args args;
    bzero(&args, sizeof(args));
    
    if (strlcpy((char*)args.label, label, sizeof(args.label)) >= sizeof(args.label)) {
        error = E2BIG;
        goto exit_error;
    }
    
    int fd_mount = open(mount_point, O_RDONLY);
    xe_assert(fd_mount >= 0);
    
    args.mask = ACCESSPERMS;
    args.uid = getuid();
    args.gid = getgid();
    args.magic = MSDOSFS_ARGSMAGIC;
    args.fspec = dev_path;
    args.flags = MSDOSFSMNT_LABEL | MNT_UNKNOWNPERMISSIONS;

    if (fmount("msdos", fd_mount, MNT_WAIT | MNT_SYNCHRONOUS | MNT_DONTBROWSE, &args)) {
        error = errno;
        goto exit_error;
    }

    xe_util_msdosfs_t mount = (xe_util_msdosfs_t)malloc(sizeof(struct xe_util_msdosfs));
    strncpy(mount->directory, temp_dir, sizeof(mount->directory));
    strncpy(mount->dev, dev_path, sizeof(mount->dev));
    mount->fd_mount = fd_mount;
    *mount_out = mount;

    return 0;

exit_error:
    rmdir(mount_point);
    unlink(image_path);

    if (rmdir(temp_dir)) {
        printf("[WARN] failed to remove temporary directory %s, err: %d", temp_dir, errno);
    }

    return error;
}

size_t xe_util_msdosfs_get_mountpoint(xe_util_msdosfs_t mount, char* dst, size_t dst_size) {
    return snprintf(dst, dst_size, "%s/mount", mount->directory);
}

int xe_util_msdosfs_get_mount_fd(xe_util_msdosfs_t allocator) {
    return allocator->fd_mount;
}

int xe_util_msdosfs_unmount(xe_util_msdosfs_t* mount_p) {
    xe_util_msdosfs_t mount = *mount_p;

    int error = xe_cmd_hdiutil_detach(mount->dev);
    if (error) {
        return error;
    }
    
    char buffer[PATH_MAX];
    snprintf(buffer, sizeof(buffer), "%s/mount", mount->directory);
    rmdir(buffer);

    snprintf(buffer, sizeof(buffer), "%s/%s", mount->directory, BASE_IMAGE_NAME);
    unlink(buffer);

    rmdir(mount->directory);

    free(mount);
    *mount_p = NULL;
    return 0;
}

