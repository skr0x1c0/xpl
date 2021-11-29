//
//  utils_dispatch.h
//  xe
//
//  Created by admin on 11/27/21.
//

#ifndef util_dispatch_h
#define util_dispatch_h

#include <stdio.h>

typedef int(^dispatch_apply_fn)(void* ctx, void* data, size_t index);

int xe_util_dispatch_apply(void* data, size_t element_size, size_t element_count, void* map_fn_ctx, dispatch_apply_fn map_fn);

#endif /* utils_dispatch_h */
