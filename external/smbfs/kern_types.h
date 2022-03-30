
#ifndef EXTERNAL_SMBFS_KERN_TYPES_H
#define EXTERNAL_SMBFS_KERN_TYPES_H

struct __lck_mtx_t__ {
    uintptr_t opaque[2];
};

struct __lck_rw_t__ {
    uintptr_t opaque[2];
};

typedef struct __lck_mtx_t__ lck_mtx_t;
typedef struct __lck_rw_t__ lck_rw_t;

#endif // _KERN_TYPES_H
