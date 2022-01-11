//
//  util_lck_rw.h
//  xe
//
//  Created by admin on 12/2/21.
//

#ifndef xe_util_lck_rw_h
#define xe_util_lck_rw_h

#include <stdio.h>

typedef struct xe_util_lck_rw* xe_util_lck_rw_t;

xe_util_lck_rw_t xe_util_lck_rw_lock_exclusive(uintptr_t proc, uintptr_t lock);
void xe_util_lck_rw_lock_done(xe_util_lck_rw_t* util);

#endif /* xe_util_lck_rw_h */
