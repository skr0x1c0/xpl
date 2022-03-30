
#ifndef xpl_msdosfs_h
#define xpl_msdosfs_h

#include <stdio.h>

#define XPL_MOUNT_TEMP_DIR "/tmp/xpl_msdosfs_XXXXXXXXX"


typedef struct xpl_msdosfs* xpl_msdosfs_t;

/// Loads the `com.apple.filesystems.msdosfs` kernel extension
int xpl_msdosfs_loadkext(void);

/// Creates a disk in ram with provided number of blocks, formats it as FAT32 and mounts the
/// disk on a temporary directory. Block size of created disk is 512 bytes
/// @param blocks number of blocks in the created disk that will back the mount
/// @param mount_options mount options from `sys/mount.h`
xpl_msdosfs_t xpl_msdosfs_mount(size_t blocks, int mount_options);

/// Returns the file descriptor of the mount point
/// @param util `xpl_msdosfs_t` created using `xpl_msdosfs_mount`
int xpl_msdosfs_mount_fd(xpl_msdosfs_t util);

/// Copies the path of mount point to `buffer`
/// @param util `xpl_msdosfs_t` created using `xpl_msdosfs_mount`
void xpl_msdosfs_mount_point(xpl_msdosfs_t util, char buffer[sizeof(XPL_MOUNT_TEMP_DIR)]);

/// Updates the mount options of the mount
/// @param util `xpl_msdosfs_t` created using `xpl_msdosfs_mount`
/// @param flags mount options from `sys/mount.h`
void xpl_msdosfs_update(xpl_msdosfs_t util, int flags);

/// Unmounts the mount
/// @param util_p pointer to `xpl_msdosfs_t` created using `xpl_msdosfs_mount`
void xpl_msdosfs_unmount(xpl_msdosfs_t* util_p);

#endif /* xpl_msdosfs_h */
