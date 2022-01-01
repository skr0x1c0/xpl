#ifndef allocator_msdosfs_h
#define allocator_msdosfs_h

#include <stdio.h>

typedef struct xe_allocator_msdosfs* xe_allocator_msdosfs_t;

int xe_allocator_msdosfs_loadkext(void);
int xe_allocator_msdosfs_create(const char* label, xe_allocator_msdosfs_t* mount_out);
int xe_allocator_msdsofs_get_mount_fd(xe_allocator_msdosfs_t allocator);
size_t xe_allocator_msdosfs_get_mountpoint(xe_allocator_msdosfs_t mount, char* dst, size_t dst_size);
int xe_allocator_msdosfs_destroy(xe_allocator_msdosfs_t* mount_p);

#endif /* allocator_msdosfs_h */
