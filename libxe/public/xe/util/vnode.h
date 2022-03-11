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

xe_util_vnode_t xe_util_vnode_create(void);
void xe_util_vnode_read(xe_util_vnode_t util, uintptr_t vnode, char* dst, size_t size);
void xe_util_vnode_write(xe_util_vnode_t util, uintptr_t vnode, const char* src, size_t size);
void xe_util_vnode_destroy(xe_util_vnode_t* util_p);

#endif /* vnode_h */
