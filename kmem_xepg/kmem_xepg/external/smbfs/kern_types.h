
#ifndef _KERN_TYPES_H
#define _KERN_TYPES_H

struct __lck_mtx_t__ {
    uintptr_t opaque[2];
};

typedef struct __lck_mtx_t__ lck_mtx_t;

#endif // _KERN_TYPES_H
