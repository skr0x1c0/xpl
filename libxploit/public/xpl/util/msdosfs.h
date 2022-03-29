#ifndef xpl_util_msdosfs_h
#define xpl_util_msdosfs_h

#include <stdio.h>

#define xpl_MOUNT_TEMP_DIR "/tmp/xpl_msdosfs_XXXXXXXXX"


typedef struct xpl_util_msdosfs* xpl_util_msdosfs_t;

int xpl_util_msdosfs_loadkext(void);
xpl_util_msdosfs_t xpl_util_msdosfs_mount(size_t blocks, int mount_options);
int xpl_util_msdosfs_mount_fd(xpl_util_msdosfs_t util);
void xpl_util_msdosfs_mount_point(xpl_util_msdosfs_t util, char buffer[sizeof(xpl_MOUNT_TEMP_DIR)]);
void xpl_util_msdosfs_update(xpl_util_msdosfs_t util, int flags);
void xpl_util_msdosfs_unmount(xpl_util_msdosfs_t* util_p);

#endif /* xpl_util_msdosfs_h */
