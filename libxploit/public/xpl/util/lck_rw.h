//
//  util_lck_rw.h
//  xe
//
//  Created by admin on 12/2/21.
//

#ifndef xpl_lck_rw_h
#define xpl_lck_rw_h

#include <stdio.h>

typedef struct xpl_lck_rw* xpl_lck_rw_t;

/// Acquire exclusive lock on a read write lock in kernel memory
/// @param lock address of the read write lock in kernel memory
xpl_lck_rw_t xpl_lck_rw_lock_exclusive(uintptr_t lock);

/// Returns the address of thread which is locking the read write lock
/// @param util `xpl_lck_rw_t` created using `xpl_lck_rw_lock_exclusive`
uintptr_t xpl_lck_rw_locking_thread(xpl_lck_rw_t util);

/// Wait until provided thread tries to acquire the read write lock using `lck_rw_lock_exclusive` or
/// `IORWLockWrite` method
/// @param util `xpl_lck_rw_t` created using `xpl_lck_rw_lock_exclusive`
/// @param thread address of thread in kernel memory
/// @param lr_stack_ptr buffer to store the address of kernel stack where link register
///        for branch with link call from `lck_rw_lock_exclusive` to `lck_rw_lock_exclusive_gen`
///        is saved
int xpl_lck_rw_wait_for_contention(xpl_lck_rw_t util, uintptr_t thread, uintptr_t* lr_stack_ptr);

/// Release the read write lock
/// @param util pointer to  `xpl_lck_rw_t` created using `xpl_lck_rw_lock_exclusive`
void xpl_lck_rw_lock_done(xpl_lck_rw_t* util);

#endif /* xpl_lck_rw_h */
