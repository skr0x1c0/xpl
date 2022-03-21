//
//  vnode.h
//  libxe
//
//  Created by sreejith on 3/10/22.
//

#ifndef vnode_h
#define vnode_h

#include <stdio.h>

typedef struct xe_util_vnode* xe_util_vnode_t;

/// Create a new `xe_util_vnode_t` for reading / writing data from arbitary vnode
xe_util_vnode_t xe_util_vnode_create(void);

/// Read data from arbitary vnode to kernel memory
/// @param util `xe_util_vnode_t` created using `xe_util_vnode_create`
/// @param vnode vnode from which data is to be read
/// @param dst buffer for storing read data
/// @param size size of data to be read (This should be less than file size of vnode)
void xe_util_vnode_read_kernel(xe_util_vnode_t util, uintptr_t vnode, uintptr_t dst, size_t size);

/// Read data from arbitary vnode to user memory
/// @param util `xe_util_vnode_t` created using `xe_util_vnode_create`
/// @param vnode vnode from which data is to be read
/// @param dst buffer for storing read data
/// @param size size of data to be read (This should be less than file size of vnode)
void xe_util_vnode_read_user(xe_util_vnode_t util, uintptr_t vnode, char* dst, size_t size);

/// Write data to arbitary vnode from kernel memory
/// @param util `xe_util_vnode_t` created using `xe_util_vnode_create`
/// @param vnode vnode to which data is to be written
/// @param src buffer containing data to be written
/// @param size size of data to be written (This should be less than file size of vnode)
void xe_util_vnode_write_kernel(xe_util_vnode_t util, uintptr_t vnode, uintptr_t src, size_t size);

/// Write data to arbitary vnode from user memory
/// @param util `xe_util_vnode_t` created using `xe_util_vnode_create`
/// @param vnode vnode to which data is to be written
/// @param src buffer containing data to be written
/// @param size size of data to be written (This should be less than file size of vnode)
void xe_util_vnode_write_user(xe_util_vnode_t util, uintptr_t vnode, const char* src, size_t size);

/// Release resources
/// @param util_p  pointer to `xe_util_vnode_t` created using `xe_util_vnode_create`
void xe_util_vnode_destroy(xe_util_vnode_t* util_p);

#endif /* vnode_h */
