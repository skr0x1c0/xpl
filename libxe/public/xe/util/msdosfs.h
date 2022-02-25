#ifndef xe_util_msdosfs_h
#define xe_util_msdosfs_h

#include <stdio.h>

typedef struct xe_util_msdosfs* xe_util_msdosfs_t;

int xe_util_msdosfs_loadkext(void);
int xe_util_msdosfs_mount(const char* base_img_path, const char* label, xe_util_msdosfs_t* mount_out);
int xe_util_msdosfs_get_mount_fd(xe_util_msdosfs_t mount);
size_t xe_util_msdosfs_get_mountpoint(xe_util_msdosfs_t mount, char* dst, size_t dst_size);
int xe_util_msdosfs_unmount(xe_util_msdosfs_t* mount_p);

#endif /* xe_util_msdosfs_h */
