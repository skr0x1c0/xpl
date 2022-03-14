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

xe_util_lck_rw_t xe_util_lck_rw_lock_exclusive(uintptr_t lock);
uintptr_t xe_util_lck_rw_locking_thread(xe_util_lck_rw_t util);
int xe_util_lck_rw_wait_for_contention(xe_util_lck_rw_t util, uintptr_t thread, uint64_t timeout_ms, uintptr_t* lr_stack_ptr);
void xe_util_lck_rw_lock_done(xe_util_lck_rw_t* util);

#endif /* xe_util_lck_rw_h */
