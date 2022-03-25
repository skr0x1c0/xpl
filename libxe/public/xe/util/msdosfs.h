#ifndef xe_util_msdosfs_h
#define xe_util_msdosfs_h

#include <stdio.h>

#define XE_MOUNT_TEMP_DIR "/tmp/xe_msdosfs_XXXXXXXXX"


typedef struct xe_util_msdosfs* xe_util_msdosfs_t;

int xe_util_msdosfs_loadkext(void);
xe_util_msdosfs_t xe_util_msdosfs_mount(size_t blocks, int mount_options);
int xe_util_msdosfs_mount_fd(xe_util_msdosfs_t util);
void xe_util_msdosfs_mount_point(xe_util_msdosfs_t util, char buffer[sizeof(XE_MOUNT_TEMP_DIR)]);
void xe_util_msdosfs_update(xe_util_msdosfs_t util, int flags);
void xe_util_msdosfs_unmount(xe_util_msdosfs_t* util_p);

#endif /* xe_util_msdosfs_h */
