#include <string.h>
#include <unistd.h>

#include <sys/errno.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <IOKit/kext/KextManager.h>

#include "allocator_msdosfs.h"
#include "msdosfs.h"
#include "util_assert.h"

#define BASE_IMAGE_NAME "exp_msdosfs_base.dmg"


struct xe_allocator_msdosfs {
    char directory[PATH_MAX];
    char dev[PATH_MAX];
    int fd_mount;
};


int xe_allocator_msdosfs_loadkext(void) {
    return KextManagerLoadKextWithIdentifier(CFSTR("com.apple.filesystems.msdosfs"), NULL);
}


int msdosfs_hdiutil_attach_internal(const char* image, char* dev_out, size_t dev_size) {
    dev_out[0] = 0;
    
    char buffer[1024];
    if (snprintf(buffer, sizeof(buffer), "hdiutil attach -nomount \"%s\"", image) >= sizeof(buffer)) {
        return E2BIG;
    }
    
    FILE* stream = popen(buffer, "r");
    if (!stream) {
        return errno;
    }

    int error = 0;
    const char* expected_suffix = "Microsoft Basic Data";
    const int expected_suffix_len = (int)strlen(expected_suffix);

    while (TRUE) {
        size_t char_count;
        
        if (feof(stream)) {
            break;
        }
        
        if (ferror(stream)) {
            error = errno;
            goto done;
        }
        
        char* line = fgetln(stream, &char_count);

        if (!char_count) {
            continue;
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
                goto done;
            }

            if (line[i] == ' ') {
                dev_out[i] = '\0';
                error = 0;
                goto done;
            } else {
                dev_out[i] = line[i];
            }
        }
    }
    
    error = ENOTRECOVERABLE;
    
done:
    pclose(stream);
    return error;
}

int msdosfs_hdiutil_attach(const char* image, char* dev_out, size_t dev_size) {
    int error;
    do {
        error = msdosfs_hdiutil_attach_internal(image, dev_out, dev_size);
    } while (error == EINTR);
    return error;
}


int xe_allocator_msdosfs_create(const char* label, xe_allocator_msdosfs_t* mount_out) {
    char* base_img_path = "allocators/msdosfs/"BASE_IMAGE_NAME;
    char temp_dir[PATH_MAX] = "/tmp/exp_msdos.XXXXXXXX";

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
    char cmd[PATH_MAX];
    if (snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", base_img_path, temp_dir) >= sizeof(cmd)) {
        error = E2BIG;
        goto exit_error;
    }

    if (system(cmd)) {
        error = errno;
        goto exit_error;
    }

    char image_path[PATH_MAX];
    if (snprintf(image_path, sizeof(image_path), "%s/"BASE_IMAGE_NAME, temp_dir) >= sizeof(image_path)) {
        error = E2BIG;
        goto exit_error;
    }

    char dev_path[PATH_MAX];
    error = msdosfs_hdiutil_attach(image_path, dev_path, sizeof(dev_path));
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
    args.uid = 501;
    args.gid = 20;
    args.magic = MSDOSFS_ARGSMAGIC;
    args.fspec = dev_path;
    args.flags = MSDOSFSMNT_LABEL | MNT_UNKNOWNPERMISSIONS;

    if (fmount("msdos", fd_mount, MNT_WAIT | MNT_SYNCHRONOUS | MNT_LOCAL, &args)) {
        error = errno;
        goto exit_error;
    }
    
    int res = fcntl(fd_mount, F_FULLFSYNC);
    assert(res == 0);

    xe_allocator_msdosfs_t mount = (xe_allocator_msdosfs_t)malloc(sizeof(struct xe_allocator_msdosfs));
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

size_t xe_allocator_msdosfs_get_mountpoint(xe_allocator_msdosfs_t mount, char* dst, size_t dst_size) {
    return snprintf(dst, dst_size, "%s/mount", mount->directory);
}

int xe_allocator_msdsofs_get_mount_fd(xe_allocator_msdosfs_t allocator) {
    return allocator->fd_mount;
}

int xe_allocator_msdosfs_destroy(xe_allocator_msdosfs_t* mount_p) {
    xe_allocator_msdosfs_t mount = *mount_p;

    char buffer[PATH_MAX];
    snprintf(buffer, sizeof(buffer), "hdiutil detach -force \"%s\"", mount->dev);
    if (system(buffer)) {
        return errno;
    }

    snprintf(buffer, sizeof(buffer), "%s/mount", mount->directory);
    rmdir(buffer);

    snprintf(buffer, sizeof(buffer), "%s/%s", mount->directory, BASE_IMAGE_NAME);
    unlink(buffer);

    rmdir(mount->directory);

    free(mount);
    return 0;
}

