//
//  xe_util_lock.h
//  xe
//
//  Created by admin on 11/21/21.
//

#ifndef xe_util_lock_h
#define xe_util_lock_h

#include <stdio.h>

typedef struct xe_util_lock* xe_util_lock_t;

int xe_util_lock_lock(uintptr_t kernproc, uintptr_t lock, xe_util_lock_t* util_out);
int xe_util_lock_destroy(xe_util_lock_t* util);

#endif /* xe_util_lock_h */
