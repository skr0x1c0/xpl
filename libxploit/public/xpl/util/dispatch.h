//
//  utils_dispatch.h
//  xe
//
//  Created by admin on 11/27/21.
//

#ifndef xpl_dispatch_h
#define xpl_dispatch_h

#include <stdio.h>
#include <dispatch/dispatch.h>

typedef int(^dispatch_apply_fn)(void* ctx, void* data, size_t index);

int xpl_dispatch_apply(void* data, size_t element_size, size_t element_count, void* map_fn_ctx, dispatch_apply_fn map_fn);
dispatch_queue_t xpl_dispatch_queue(void);

#endif /* xpl_dispatch_h */
