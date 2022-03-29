//
//  util_lck_rw.h
//  xe
//
//  Created by admin on 12/2/21.
//

#ifndef xpl_util_lck_rw_h
#define xpl_util_lck_rw_h

#include <stdio.h>

typedef struct xpl_util_lck_rw* xpl_util_lck_rw_t;

xpl_util_lck_rw_t xpl_util_lck_rw_lock_exclusive(uintptr_t lock);
uintptr_t xpl_util_lck_rw_locking_thread(xpl_util_lck_rw_t util);
int xpl_util_lck_rw_wait_for_contention(xpl_util_lck_rw_t util, uintptr_t thread, uintptr_t* lr_stack_ptr);
void xpl_util_lck_rw_lock_done(xpl_util_lck_rw_t* util);

#endif /* xpl_util_lck_rw_h */
