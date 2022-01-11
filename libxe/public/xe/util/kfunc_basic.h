//
//  util_kfunc_basic.h
//  xe
//
//  Created by admin on 12/9/21.
//

#ifndef util_kfunc_basic_h
#define util_kfunc_basic_h

#include <stdio.h>

#include "zalloc.h"

typedef struct xe_util_kfunc_basic* xe_util_kfunc_basic_t;

xe_util_kfunc_basic_t xe_util_kfunc_basic_create(uintptr_t proc, xe_util_zalloc_t io_event_source_allocator, xe_util_zalloc_t block_allocator, uint free_zone_idx);
uintptr_t xe_util_kfunc_build_event_source(xe_util_kfunc_basic_t util, uintptr_t target_func, char arg0[8]);
void xe_util_kfunc_reset(xe_util_kfunc_basic_t util);

#endif /* util_kfunc_basic_h */
