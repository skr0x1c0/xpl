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
void xe_util_kfunc_exec(xe_util_kfunc_t util, uintptr_t target_func, uint64_t args[8]);
void xe_util_kfunc_destroy(xe_util_kfunc_t* util_p);

#endif /* kfunc_h */
