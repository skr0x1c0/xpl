//
//  kfunc.h
//  libxe
//
//  Created by admin on 2/16/22.
//

#ifndef kfunc_h
#define kfunc_h

#include <stdio.h>

#include "zalloc.h"

typedef struct xe_util_kfunc* xe_util_kfunc_t;

xe_util_kfunc_t xe_util_kfunc_create(uint free_zone_idx);
void xe_util_kfunc_execute_simple(xe_util_kfunc_t util, uintptr_t target_func, uint64_t x0, uint64_t x1, uint64_t x2, uint64_t x3, uint64_t x4, uint64_t x5, uint64_t x6, uint64_t x7);
void xe_util_kfunc_destroy(xe_util_kfunc_t* util_p);

#endif /* kfunc_h */
